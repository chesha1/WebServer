#ifndef TASK_H
#define TASK_H

#include <atomic>
#include <coroutine>
#include <memory>
#include <optional>
#include <utility>

// 协程返回值 task 的实现，包括自定义 promise_type 类型的 task_promise
namespace WebServer {

    // 自定义 promise_type
    template<typename T>
    class task_promise;

    template<typename T = void>
    class task {
    public:
        using promise_type = task_promise<T>;
        explicit task(std::coroutine_handle<task_promise<T>> h) noexcept: m_handle(h) {}

        ~task() noexcept {
            if (m_handle) {
                if (m_handle.done()) {
                    m_handle.destroy();
                } else {
                    // 检查这个 task 是否已经被 detached，如果没有，则将其设置为已 detached。这个操作不需要同步
                    m_handle.promise().get_detached_flag().test_and_set(std::memory_order_relaxed);
                }
            }
        }

        // 删除复制构造函数，保留移动构造
        task(const task &other) = delete;

        task(task &&other) noexcept: m_handle{std::exchange(other.m_handle, nullptr)} {}

        // 删除拷贝赋值
        task &operator=(const task &other) = delete;

        task &operator=(task &&other) noexcept = delete;

        // 实现 Awaiter，co_await 时应该执行什么操作
        class task_awaiter {
        public:
            explicit task_awaiter(std::coroutine_handle<task_promise<T>> coroutine)
                    : m_handle{coroutine} {}

            // 检查被等待的值是否已经就绪，返回 true 协程就会立即继续执行
            [[nodiscard]] bool await_ready() const noexcept {
                return m_handle == nullptr || m_handle.done();
            }

            // 协程被恢复时调用，当类型 T 不是 void 时，返回协程 promise 的返回值
            auto await_resume() const noexcept -> decltype(auto) {
                if constexpr (!std::is_same_v<T, void>) {
                    return m_handle.promise().get_return_value();
                }
            }

            // 协程需要被挂起时被调用，保存当前协程句柄，然后返回一个要在当前协程暂停后恢复执行的协程句柄
            [[nodiscard]] std::coroutine_handle<>
            await_suspend(std::coroutine_handle<> calling_coroutine) const noexcept {
                m_handle.promise().get_calling_coroutine() = calling_coroutine;
                return m_handle;
            }

        private:
            std::coroutine_handle<task_promise<T>> m_handle;
        };

        task_awaiter operator
        co_await() noexcept { return task_awaiter(m_handle); }

        void resume() const noexcept {
            if (m_handle == nullptr || m_handle.done()) {
                return;
            }
            m_handle.resume();
        }

        auto detach() noexcept {
            m_handle.promise().get_detached_flag().test_and_set(std::memory_order_relaxed);
            m_handle = nullptr;
        }

    private:
        std::coroutine_handle<task_promise<T>> m_handle = nullptr;
    };

    // 自定义 promise_type 的基类
    template<typename T>
    class task_promise_base {
    public:
        class final_awaiter {
        public:
            [[nodiscard]] bool await_ready() const noexcept { return false; }

            void await_resume() const noexcept {}

            std::coroutine_handle<> await_suspend(std::coroutine_handle<task_promise<T>> coroutine
            ) const noexcept {
                if (coroutine.promise().get_detached_flag().test(std::memory_order_relaxed)) {
                    coroutine.destroy();
                }
                return coroutine.promise().get_calling_coroutine().value_or(std::noop_coroutine());
            }
        };

        [[nodiscard]] std::suspend_always initial_suspend() const noexcept { return {}; }

        [[nodiscard]] final_awaiter final_suspend() const noexcept { return final_awaiter{}; }

        void unhandled_exception() const noexcept { std::terminate(); }
//        void unhandled_exception() const noexcept {  }

        std::optional<std::coroutine_handle<>> &get_calling_coroutine() noexcept {
            return calling_coroutine_;
        }

        std::atomic_flag &get_detached_flag() noexcept { return detached_flag_; }

    private:
        std::optional<std::coroutine_handle<>> calling_coroutine_;
        std::atomic_flag detached_flag_;
    };

    template<typename T>
    class task_promise final : public task_promise_base<T> {
    public:
        task<T> get_return_object() noexcept {
            return task<T>{std::coroutine_handle<task_promise<T>>::from_promise(*this)};
        }

        // 需要可以转换成 T 的 U &&
        template<typename U>
        requires std::convertible_to<U &&, T>
        void return_value(U &&return_value) noexcept(std::is_nothrow_constructible_v<T, U &&>) {
            // 在 optional 对象内原地构造一个新值
            return_value_.emplace(std::forward<U>(return_value));
        }

        T & get_return_value() & noexcept { return *return_value_; }

        T && get_return_value() && noexcept { return std::move(*return_value_); }

    private:
        std::optional<T> return_value_;
    };

    template<>
    class task_promise<void> final : public task_promise_base<void> {
    public:
        task<void> get_return_object() noexcept {
            return task<void>{std::coroutine_handle<task_promise>::from_promise(*this)};
        };

        void return_void() const noexcept {}
    };
}

#endif