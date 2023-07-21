#include <algorithm>
#include <atomic>
#include <optional>
#include <vector>
#include "task.h"

// 实现同步等待，内部异步
namespace WebServer {
    template<typename T>
    class sync_wait_task_promise;

    template<typename T>
    class sync_wait_task {
    public:
        using promise_type = sync_wait_task_promise<T>;

        explicit sync_wait_task(std::coroutine_handle<sync_wait_task_promise<T>> coroutine_handle
        ) noexcept: coroutine_(coroutine_handle) {}

        ~sync_wait_task() {
            if (coroutine_) {
                coroutine_.destroy();
            }
        }

        T get_return_value() const noexcept {
            return coroutine_.promise().get_return_value();
        }

        // 在任务完成之前阻塞当前线程
        void wait() const noexcept {
            coroutine_.promise().get_atomic_flag().wait(false, std::memory_order_acquire);
        }

    private:
        std::coroutine_handle<sync_wait_task_promise<T>> coroutine_;
    };

    template<typename T>
    class sync_wait_task_promise_base {
    public:
        [[nodiscard]] std::suspend_never initial_suspend() const noexcept { return {}; }

        class final_awaiter {
        public:
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            void await_resume() const noexcept {}

            void await_suspend(std::coroutine_handle<sync_wait_task_promise<T>> coroutine) const noexcept {
                std::atomic_flag &atomic_flag = coroutine.promise().get_atomic_flag();

                // 设置协程完成的标志，并唤醒所有正在等待这个协程（ atomic_flag ）完成的线程
                atomic_flag.test_and_set(std::memory_order_release);
                atomic_flag.notify_all();
            }
        };

        [[nodiscard]] final_awaiter final_suspend() const noexcept { return {}; }

        void unhandled_exception() const noexcept { std::terminate(); }

        std::atomic_flag &get_atomic_flag() noexcept { return atomic_flag_; }

    private:
        // 原子标记，用来表示协程是否已经完成
        std::atomic_flag atomic_flag_;
    };

    template<typename T>
    class sync_wait_task_promise final : public sync_wait_task_promise_base<T> {
    public:
        sync_wait_task<T> get_return_object() noexcept {
            return sync_wait_task<T>{std::coroutine_handle<sync_wait_task_promise<T>>::from_promise(*this)};
        }

        template<typename U>
        requires std::convertible_to<U &&, T>
        void return_value(U &&return_value) noexcept(std::is_nothrow_constructible_v<T, U &&>) {
            return_value_.emplace(std::forward<U>(return_value));
        }

        // 所在对象是左值时，才能调用这个函数
        T &get_return_value() & noexcept { return *return_value_; }

        // 所在对象是右边值时，才能调用这个函数
        T &&get_return_value() && noexcept { return std::move(*return_value_); }

    private:
        std::optional<T> return_value_;
    };

    // 启动任务，不关心返回值的版本
    template<>
    class sync_wait_task_promise<void> final : public sync_wait_task_promise_base<void> {
    public:
        sync_wait_task<void> get_return_object() noexcept {
            return sync_wait_task<void>{std::coroutine_handle<sync_wait_task_promise>::from_promise(*this)};
        }

        void return_void() noexcept {}
    };

    // 在同步环境中等待一个异步任务的完成，并返回其结果
    template<typename T>
    T sync_wait(task<T> &task) {
        // 创建一个协程，co_await 等待 task 完成，co_return 返回
        auto sync_wait_task_handle = ([&]() -> sync_wait_task<T> { co_return co_await task; })();
        sync_wait_task_handle.wait();

        // 如果 T 不是 void，那么就获取协程的返回值，并作为 sync_wait 函数的结果返回
        if constexpr (!std::is_same_v<T, void>) {
            return sync_wait_task_handle.get_return_value();
        }
    }

    template<typename T>
    T sync_wait(task<T> &&task) {
        auto sync_wait_task_handle = ([&]() -> sync_wait_task<T> { co_return co_await task; })();
        sync_wait_task_handle.wait();

        if constexpr (!std::is_same_v<T, void>) {
            return sync_wait_task_handle.get_return_value();
        }
    }

    // 同时等待一组任务完成，其中每个任务都是异步运行的，对所有的任务同步等待
    template<typename T>
    // 如果 T 是 void, 返回类型是 void，否则返回类型是 std::vector<T>
    std::conditional_t<std::is_same_v<T, void>, void, std::vector<T>>
    sync_wait_all(std::vector<task<T>> &task_list) {
        // 遍历任务列表，并对每个任务调用sync_wait函数，等待其完成
        if constexpr (std::is_same_v<T, void>) {
            for (auto &task: task_list) {
                sync_wait(task);
            }
        } else {
            std::vector<T> return_value_list;
            return_value_list.reserve(task_list.size());

            // 对 task_list 中的每个任务调用 sync_wait 函数，并将结果保存到 return_value_list 中
            std::transform(
                    task_list.begin(), task_list.end(), std::back_inserter(return_value_list),
                    [](task<T> &task) -> T { return sync_wait(task); }
            );
            return return_value_list;
        }
    }
}

