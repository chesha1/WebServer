#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <cstddef>
#include <thread>
#include "socket.h"
#include "task.h"
#include "thread_pool.h"

namespace WebServer {
    // 这个类负责处理与客户端的交互。它的构造函数会启动两个协程：accept_client 和 event_loop
    class thread_worker {
    public:
        explicit thread_worker(const char *port);

        // 在一个循环中通过调用 server_socket::accept() 来提交一个 multishot accept 请求到 io_uring.
        // 由于 multishot accept 请求的持久性, server_socket::accept() 只有当之前的请求失效时才会提交新的请求到 io_uring.
        // 当新的客户端建立连接后, 它会启动 thread_worker::handle_client() 协程处理该客户端发来的 HTTP 请求
        task<> accept_client();

        // 调用 client_socket::recv() 来接收 HTTP 请求, 并且用 http_parser (http_parser.hpp) 解析 HTTP 请求
        // 等请求解析完毕后, 它会构造一个 http_response 并调用 client_socket::send() 将响应发给客户端
        task<> handle_client(client_socket client_socket);

        // 在一个无限循环中处理来自 io_uring 的完成队列中的事件，并继续运行等待该事件的协程
        // 这样做的目的是让服务器能够异步地处理各种 I/O 操作，包括读写套接字、文件操作等
        task<> event_loop();

    private:
        server_socket server_socket_;
    };

    // 初始化一个线程池，然后为线程池中的每个线程创建一个无限循环的 thread_worker 任务
    class http_server {
    public:
        explicit http_server(size_t thread_count = std::thread::hardware_concurrency());

        void listen(const char *port);

    private:
        thread_pool thread_pool_;
    };
}

#endif