#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#include <liburing.h>
#include <liburing/io_uring.h>
#include "buffer_ring.h"
#include "constant.h"
#include "file_descriptor.h"
#include "http_message.h"
#include "http_parser.h"
#include "io_uring.h"
#include "socket.h"
#include "sync_wait.h"
#include "http_server.h"

namespace WebServer {
    thread_worker::thread_worker(const char *port) {
        // 获取 buffer_ring 的实例并注册缓冲区
        buffer_ring::get_instance().register_buffer_ring(BUFFER_RING_SIZE, BUFFER_SIZE);

        // 绑定服务器套接字，开始监听绑定的端口
        server_socket_.bind(port);
        server_socket_.listen();

        // 创建 accept_client() 任务并启动这个任务
        // accept_client() 是一个无限循环的协程，它不断地接收新的客户端连接，并为每个连接创建一个处理客户端任务
        // 使用 resume() 函数启动协程，然后使用 detach() 函数将协程设为分离状态。
        // 这使得协程可以在后台运行，而主线程可以继续执行其他操作，而不必等待协程结束
        task<> accept_client_task = accept_client();
        accept_client_task.resume();
        accept_client_task.detach();
    }

    task<> thread_worker::accept_client() {
        while (true) {
            // server_socket_.accept() 这个函数的作用是异步地接收新的客户端连接
            // 它会返回一个文件描述符（ file descriptor ）表示新的客户端套接字
            const int raw_file_descriptor = co_await server_socket_.accept();
            if (raw_file_descriptor == -1) {
                continue;
            }

            // 创建一个新的handle_client任务，用于处理新的客户端连接
            task<> handle_client_task = handle_client(client_socket(raw_file_descriptor));
            handle_client_task.resume();
            handle_client_task.detach();
        }
    }

    task<> thread_worker::handle_client(client_socket client_socket) {
        http_parser http_parser;
        buffer_ring &buffer_ring = buffer_ring::get_instance();
        while (true) {
            const auto [recv_buffer_id, recv_buffer_size] = co_await client_socket.recv(BUFFER_SIZE);
            if (recv_buffer_size == 0) {
                break;
            }

            const std::span<char> recv_buffer = buffer_ring.borrow_buffer(recv_buffer_id, recv_buffer_size);
            if (const auto parse_result = http_parser.parse_packet(recv_buffer); parse_result.has_value()) {
                const http_request &http_request = parse_result.value();
                const std::filesystem::path file_path = std::filesystem::relative(http_request.url, "/");

                http_response http_response;
                http_response.version = http_request.version;
                if (std::filesystem::exists(file_path) && std::filesystem::is_regular_file(file_path)) {
                    http_response.status = "200";
                    http_response.status_text = "OK";
                    const uintmax_t file_size = std::filesystem::file_size(file_path);
                    http_response.header_list.emplace_back("content-length", std::to_string(file_size));

                    std::string send_buffer = http_response.serialize();
                    if (co_await client_socket.send(send_buffer, send_buffer.size()) == -1) {
                        throw std::runtime_error("failed to invoke 'send'");
                    }

                    const file_descriptor file_descriptor = open(file_path);
                    if (co_await splice(file_descriptor, client_socket, file_size) == -1) {
                        throw std::runtime_error("failed to invoke 'splice'");
                    }
                } else {
                    http_response.status = "404";
                    http_response.status_text = "Not Found";
                    http_response.header_list.emplace_back("content-length", "0");

                    std::string send_buffer = http_response.serialize();
                    if (co_await client_socket.send(send_buffer, send_buffer.size()) == -1) {
                        throw std::runtime_error("failed to invoke 'send'");
                    }
                }
            }

            buffer_ring.return_buffer(recv_buffer_id);
        }
    }

    task<> thread_worker::event_loop() {
        // 首先获取io_uring实例的引用
        io_uring &io_uring = io_uring::get_instance();

        while (true) {
            // 提交所有挂起的请求，并等待至少一个事件完成。这个函数会阻塞，直到有至少一个事件完成
            io_uring.submit_and_wait(1);

            // 遍历 io_uring 中的所有完成队列项
            for (io_uring_cqe *const cqe: io_uring) {
                // 获取关联的数据，将这些数据转换为 sqe_data 结构
                auto *sqe_data = reinterpret_cast<struct sqe_data *>(io_uring_cqe_get_data(cqe));
                sqe_data->cqe_res = cqe->res;
                sqe_data->cqe_flags = cqe->flags;
                void *const coroutine_address = sqe_data->coroutine;

                // 告诉io_uring这个完成队列项已经被处理
                io_uring.cqe_seen(cqe);

                // 如果协程地址非空，使用 std::coroutine_handle<>::from_address 将其转换为协程句柄
                // 并恢复（即继续执行）这个协程
                if (coroutine_address != nullptr) {
                    std::coroutine_handle<>::from_address(coroutine_address).resume();
                }

                // 通过这种方式，event_loop 函数可以处理所有的 IO 事件，并恢复等待这些事件的协程
                // 这使得异步 IO 操作看起来像同步操作一样直观
            };
        }
    }

    http_server::http_server(const size_t thread_count) : thread_pool_{thread_count} {}

    void http_server::listen(const char *port) {
        const auto construct_task = [&]() -> task<> {
            co_await thread_pool_.schedule();
            co_await thread_worker(port).event_loop();
        };

        std::vector<task<>> thread_worker_list;
        for (size_t _ = 0; _ < thread_pool_.size(); ++_) {
            task<> thread_worker = construct_task();
            thread_worker.resume();
            thread_worker_list.emplace_back(std::move(thread_worker));
        }
        sync_wait_all(thread_worker_list);
    }
}