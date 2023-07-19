#include "thread_pool.h"

namespace WebServer {
    thread_pool::thread_pool(const std::size_t thread_count) {
        for (size_t _ = 0; _ < thread_count; ++_) {
            thread_list_.emplace_back([&]() { thread_loop(); });
        }
    }

    thread_pool::~thread_pool() {
        stop_source_.request_stop();
        condition_variable_.notify_all();
    }

    thread_pool::schedule_awaiter::schedule_awaiter(thread_pool &thread_pool)
            : thread_pool_{thread_pool} {}

    void thread_pool::schedule_awaiter::await_suspend(std::coroutine_handle<> handle) const noexcept {
        thread_pool_.enqueue(handle);
    }

    bool thread_pool::schedule_awaiter::await_ready() const noexcept { return false; }

    void thread_pool::schedule_awaiter::await_resume() const noexcept {}

    thread_pool::schedule_awaiter thread_pool::schedule() { return schedule_awaiter{*this}; }

    size_t thread_pool::size() const noexcept { return thread_list_.size(); }

    // 生产者消费者模式，
    void thread_pool::thread_loop() {

        // 首先检查stop_source_以确定是否被请求停止
        // 如果没有被请求停止，线程将进入等待状态
        // 等待新的协程被添加到队列中或者被请求停止，就唤醒线程
        while (!stop_source_.stop_requested()) {
            std::unique_lock lock(mutex_);
            condition_variable_.wait(lock, [this]() {
                return stop_source_.stop_requested() || !coroutine_queue_.empty();
            });
            if (stop_source_.stop_requested()) {
                break;
            }

            // 线程将从队列中取出一个协程，解锁并执行
            const std::coroutine_handle<> coroutine = coroutine_queue_.front();
            coroutine_queue_.pop();
            lock.unlock();

            coroutine.resume();
        }
    }

    void thread_pool::enqueue(std::coroutine_handle<> coroutine) {
        std::unique_lock lock(mutex_);
        coroutine_queue_.emplace(coroutine);
        condition_variable_.notify_one();
    }
}
