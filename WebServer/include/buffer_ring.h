#ifndef BUFFER_RING_H
#define BUFFER_RING_H

#include <bitset>
#include <cstddef>
#include <memory>
#include <span>
#include <vector>
#include <liburing/io_uring.h>
#include "constant.h"

namespace WebServer {

    // 类buffer_ring是一个使用了 thread_local 单例模式的环形缓冲区管理器
    // 因为是 thread_local ，每个线程都会有一个独立的 buffer_ring 实例
    // 这样可以避免在不同线程之间共享数据时需要使用锁，从而提高性能
    class buffer_ring {
    public:
        // 返回当前线程的 buffer_ring 单例实例
        static buffer_ring &get_instance() noexcept;

        // 初始化环形缓冲区
        void register_buffer_ring(const unsigned int buffer_ring_size, const size_t buffer_size);

        // 允许 io_uring 借用一个指定 ID 的缓冲区
        std::span<char> borrow_buffer(const unsigned int buffer_id, const size_t size);

        // 允许 io_uring 归还一个指定 ID 的缓冲区
        void return_buffer(const unsigned int buffer_id);

    private:
        // 它是一个 io_uring_buf_ring 类型的智能指针
        // 用于管理 io_uring 中的环形缓冲区，每次分配一个新的缓冲区时，它都会更新
        std::unique_ptr<io_uring_buf_ring> buffer_ring_;

        // 二维的 std::vector，包含了所有的缓冲区
        // 每个缓冲区是一个 std::vector<char>，代表一块内存区域
        std::vector<std::vector<char>> buffer_list_;

        // 它是一个 bitset，用于跟踪哪些缓冲区正在被借用
        // 当一个缓冲区被借用时，相应的位会被设置为true
        std::bitset<MAX_BUFFER_RING_SIZE> borrowed_buffer_set_;
    };
}

#endif