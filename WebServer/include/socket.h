#ifndef SOCKET_H
#define SOCKET_H

#include <coroutine>
#include <optional>
#include <span>
#include <tuple>
#include <sys/socket.h>

#include "file_descriptor.h"
#include "io_uring.h"
#include "task.h"


namespace WebServer {

    // 扩展了 file_descriptor 类，表示可接受客户端的监听套接字
    // 提供了一个 accept() 方法，记录是否在 io_uring 中存在现有的 multishot accept 请求
    // 并在不存在时提交一个新的请求
    class server_socket : public file_descriptor {
    public:
        server_socket();

        void bind(const char *port);

        void listen() const;

        // 用于管理多次接收的网络连接请求，它是一个协程对象
        class multishot_accept_guard {
        public:
            multishot_accept_guard(
                    int raw_file_descriptor, sockaddr_storage *client_address,
                    socklen_t *client_address_size
            );

            ~multishot_accept_guard();

            [[nodiscard]] bool await_ready() const;

            void await_suspend(std::coroutine_handle<> coroutine);

            int await_resume();

        private:
            bool initial_await_ = true;
            const int raw_file_descriptor_;
            sockaddr_storage *client_address_;
            socklen_t *client_address_size_;
            sqe_data sqe_data_;
        };

        multishot_accept_guard &
        accept(sockaddr_storage *client_address = nullptr,
               socklen_t *client_address_size = nullptr);

    private:
        std::optional<multishot_accept_guard> multishot_accept_guard_;
    };

    class client_socket : public file_descriptor {
    public:
        explicit client_socket(int raw_file_descriptor);

        class recv_awaiter {
        public:
            recv_awaiter(int raw_file_descriptor, size_t length);

            [[nodiscard]] bool await_ready() const;

            void await_suspend(std::coroutine_handle<> coroutine);

            std::tuple<unsigned int, ssize_t> await_resume();

        private:
            const int raw_file_descriptor_;
            const size_t length_;
            sqe_data sqe_data_;
        };

        recv_awaiter recv(size_t length);

        class send_awaiter {
        public:
            send_awaiter(int raw_file_descriptor, const std::span<char> &buffer, size_t length);

            [[nodiscard]] bool await_ready() const;

            void await_suspend(std::coroutine_handle<> coroutine);

            [[nodiscard]] ssize_t await_resume() const;

        private:
            const int raw_file_descriptor_;
            const size_t length_;
            const std::span<char> &buffer_;
            sqe_data sqe_data_;
        };

        task<ssize_t> send(const std::span<char> &buffer, size_t length);
    };

}

#endif