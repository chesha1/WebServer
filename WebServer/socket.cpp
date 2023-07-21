#include <cstring>
#include <span>
#include <stdexcept>
#include <liburing/io_uring.h>
#include <netdb.h>

#include "constant.h"
#include "file_descriptor.h"
#include "socket.h"

namespace WebServer {
    server_socket::server_socket() = default;

    void server_socket::bind(const char *port) {
        addrinfo address_hints;
        addrinfo *socket_address;

        std::memset(&address_hints, 0, sizeof(addrinfo));
        address_hints.ai_family = AF_UNSPEC;
        address_hints.ai_socktype = SOCK_STREAM;
        address_hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(nullptr, port, &address_hints, &socket_address) != 0) {
            throw std::runtime_error("failed to invoke 'getaddrinfo'");
        }

        // 遍历 socket_address 链表中的每个节点
        for (auto *node = socket_address; node != nullptr; node = node->ai_next) {
            // 调用socket函数创建新的套接字
            raw_file_descriptor_ = socket(node->ai_family, node->ai_socktype, node->ai_protocol);
            if (raw_file_descriptor_.value() == -1) {
                throw std::runtime_error("failed to invoke 'socket'");
            }

            const int flag = 1;
            // 将套接字选项 SO_REUSEADDR 和 SO_REUSEPORT 设置为1，允许重用端口
            if (setsockopt(raw_file_descriptor_.value(), SOL_SOCKET,
                           SO_REUSEADDR, &flag, sizeof(flag)) == -1) {
                throw std::runtime_error("failed to invoke 'setsockopt'");
            }

            if (setsockopt(raw_file_descriptor_.value(), SOL_SOCKET,
                           SO_REUSEPORT, &flag, sizeof(flag)) == -1) {
                throw std::runtime_error("failed to invoke 'setsockopt'");
            }

            // 调用 bind 函数将新套接字绑定到当前节点的地址
            if (::bind(raw_file_descriptor_.value(), node->ai_addr, node->ai_addrlen) == -1) {
                throw std::runtime_error("failed to invoke 'bind'");
            }
            break;
        }
        freeaddrinfo(socket_address);
    }

    void server_socket::listen() const {
        if (!raw_file_descriptor_.has_value()) {
            throw std::runtime_error("the file descriptor is invalid");
        }

        if (::listen(raw_file_descriptor_.value(), SOCKET_LISTEN_QUEUE_SIZE) == -1) {
            throw std::runtime_error("failed to invoke 'listen'");
        }
    }

    server_socket::multishot_accept_guard::multishot_accept_guard(
            const int raw_file_descriptor, sockaddr_storage *client_address,
            socklen_t *client_address_size
    )
            : raw_file_descriptor_{raw_file_descriptor}, client_address_{client_address},
              client_address_size_{client_address_size} {}

    server_socket::multishot_accept_guard::~multishot_accept_guard() {
        io_uring::get_instance().submit_cancel_request(&sqe_data_);
    }

    bool server_socket::multishot_accept_guard::await_ready() const { return false; }

    void server_socket::multishot_accept_guard::await_suspend(std::coroutine_handle<> coroutine) {
        sqe_data_.coroutine = coroutine.address();
        if (initial_await_) {
            io_uring::get_instance().submit_multishot_accept_request(
                    &sqe_data_, raw_file_descriptor_,
                    reinterpret_cast<sockaddr *>(client_address_),
                    client_address_size_
            );
            initial_await_ = false;
        }
    }

    int server_socket::multishot_accept_guard::await_resume() {
        // 这个方法检查 sqe_data_.cqe_flags 是否包含 IORING_CQE_F_MORE 标志
        // 这个标志表示是否有更多的事件需要处理
        // 如果没有（即该标志位未被设置），那么会再次提交一个接收新连接的请求
        if (!(sqe_data_.cqe_flags & IORING_CQE_F_MORE)) {
            io_uring::get_instance().submit_multishot_accept_request(
                    &sqe_data_, raw_file_descriptor_,
                    reinterpret_cast<sockaddr *>(client_address_),
                    client_address_size_
            );
        }
        return sqe_data_.cqe_res;
    }

    server_socket::multishot_accept_guard &
    server_socket::accept(sockaddr_storage *client_address, socklen_t *client_address_size) {
        if (!raw_file_descriptor_.has_value()) {
            throw std::runtime_error("the file descriptor is invalid");
        }

        // 检查 multishot_accept_guard_ 是否已经初始化。如果没有（即它没有值），
        // 那么创建一个 multishot_accept_guard 实例
        if (!multishot_accept_guard_.has_value()) {
            multishot_accept_guard_.emplace(
                    raw_file_descriptor_.value(), client_address, client_address_size
            );
        }

        // 这个方法返回 multishot_accept_guard_ 的值（即刚创建的 multishot_accept_guard 实例）
        // 返回的这个实例可以用来开始一个新的协程，等待新的连接请求
        return multishot_accept_guard_.value();
    }

    client_socket::client_socket(const int raw_file_descriptor)
            : file_descriptor{raw_file_descriptor} {}

    client_socket::recv_awaiter::recv_awaiter(const int raw_file_descriptor, const size_t length)
            : raw_file_descriptor_{raw_file_descriptor}, length_{length} {}

    bool client_socket::recv_awaiter::await_ready() const { return false; }

    void client_socket::recv_awaiter::await_suspend(std::coroutine_handle<> coroutine) {
        sqe_data_.coroutine = coroutine.address();
        io_uring::get_instance().submit_recv_request(&sqe_data_, raw_file_descriptor_, length_);
    }

    std::tuple<unsigned int, ssize_t> client_socket::recv_awaiter::await_resume() {
        if (sqe_data_.cqe_flags | IORING_CQE_F_BUFFER) {
            const unsigned int buffer_id = sqe_data_.cqe_flags >> IORING_CQE_BUFFER_SHIFT;
            return {buffer_id, sqe_data_.cqe_res};
        }
        return {};
    }

    client_socket::recv_awaiter client_socket::recv(const size_t length) {
        if (raw_file_descriptor_.has_value()) {
            return {raw_file_descriptor_.value(), length};
        }
        throw std::runtime_error("the file descriptor is invalid");
    }

    client_socket::send_awaiter::send_awaiter(
            const int raw_file_descriptor, const std::span<char> &buffer, const size_t length
    )
            : raw_file_descriptor_{raw_file_descriptor}, length_{length}, buffer_{buffer} {};

    bool client_socket::send_awaiter::await_ready() const { return false; }

    void client_socket::send_awaiter::await_suspend(std::coroutine_handle<> coroutine) {
        sqe_data_.coroutine = coroutine.address();

        io_uring::get_instance().submit_send_request(&sqe_data_, raw_file_descriptor_, buffer_, length_);
    }

    ssize_t client_socket::send_awaiter::await_resume() const { return sqe_data_.cqe_res; }

    task<ssize_t> client_socket::send(const std::span<char> &buffer, const size_t length) {
        if (!raw_file_descriptor_.has_value()) {
            throw std::runtime_error("the file descriptor is invalid");
        }

        size_t bytes_sent = 0;
        while (bytes_sent < length) {
            ssize_t result =
                    co_await send_awaiter(raw_file_descriptor_.value(), buffer.subspan(bytes_sent), length);
            if (result < 0) {
                co_return -1;
            }
            bytes_sent += result;
        }
        co_return bytes_sent;
    }

} 