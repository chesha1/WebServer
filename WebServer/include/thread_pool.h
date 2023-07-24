#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <condition_variable>
#include <coroutine>
#include <cstddef>
#include <list>
#include <mutex>
#include <queue>
#include <stop_token>
#include <thread>

namespace WebServer {
    class thread_pool {
    public:
        // 创建指定数量的线程，每个线程都在运行 thread_loop()
        explicit thread_pool(std::size_t thread_count);

        ~thread_pool();

        // 决定了协程如何在多线程环境下被挂起和恢复
        class schedule_awaiter {
        public:
            explicit schedule_awaiter(thread_pool &thread_pool);

            [[nodiscard]] bool await_ready() const noexcept;

            void await_resume() const noexcept;

            void await_suspend(std::coroutine_handle<> handle) const noexcept;

        private:
            thread_pool &thread_pool_;
        };

        // 通过 co_await thread_pool.schedule() 挂起协程，将控制权交给线程池
        schedule_awaiter schedule();

        [[nodiscard]] size_t size() const noexcept;

    private:
        std::stop_source stop_source_; // 停止信号源
        std::list<std::jthread> thread_list_;

        std::mutex mutex_;
        std::condition_variable condition_variable_;
        std::queue<std::coroutine_handle<>> coroutine_queue_; // 存放协程句柄的队列

        void thread_loop();

        // 将协程句柄加入到队列中，然后唤醒一个线程来执行协程
        void enqueue(std::coroutine_handle<> coroutine);
    };
}

#endif