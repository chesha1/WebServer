#include <liburing.h>
#include <sys/socket.h>
#include <span>
#include <vector>

struct io_uring_buf_ring;
struct io_uring_cqe;

// io_uring 的简单封装
namespace WebServer {
    // submission queue entry
    struct sqe_data {
        void *coroutine = nullptr;
        int cqe_res = 0;
        unsigned int cqe_flags = 0;
    };

    class io_uring {
    public:
        static io_uring &get_instance() noexcept;

        io_uring();

        ~io_uring();

        io_uring(io_uring &&other) = delete;

        io_uring &operator=(io_uring &&other) = delete;

        io_uring(const io_uring &other) = delete;

        io_uring &operator=(const io_uring &other) = delete;

        // 用来遍历已经完成的 io_uring 事件，cq 的迭代器
        class cqe_iterator {
        public:
            explicit cqe_iterator(const ::io_uring *io_uring, unsigned int head);

            cqe_iterator(const cqe_iterator &) = default;

            cqe_iterator &operator++() noexcept;

            bool operator!=(const cqe_iterator &right) const noexcept;

            io_uring_cqe *operator*() const noexcept;

        private:
            const ::io_uring *io_uring_;

            // 迭代器正在处理的 CQE 在队列中的位置
            unsigned int head_;
        };

        cqe_iterator begin();

        cqe_iterator end();

        // liburing 库中的 io_uring_cqe_seen 函数
        // 处理完一个 cqe 后，应用程序需要告诉 io_uring，它已经“看到了”这个完成的 IO 操作
        void cqe_seen(io_uring_cqe *cqe);

        // wait_nr 是最小 ceq 的数量
        // 不到这个数量会一直阻塞，达到才返回
        int submit_and_wait(int wait_nr);

        // 创建并提交一个可以接受多个连接的 accept 请求到 io_uring 的 sq
        void submit_multishot_accept_request(
                sqe_data *sqe_data, int raw_file_descriptor, sockaddr *client_addr, socklen_t *client_len
        );

        // 创建并提交一个接收数据的 recv 请求到 io_uring 的 sq
        void submit_recv_request(sqe_data *sqe_data, int raw_file_descriptor, size_t length);

        void submit_send_request(
                sqe_data *sqe_data, int raw_file_descriptor, const std::span<char> &buffer, size_t length
        );

        // splice 是零拷贝的移动数据
        // 提交一个 splice 请求
        void submit_splice_request(
                sqe_data *sqe_data, int raw_file_descriptor_in, int raw_file_descriptor_out, size_t length
        );

        // 提交一个 cancel 请求
        // 取消已经提交到 io_uring 的操作请求
        void submit_cancel_request(sqe_data *sqe_data);

        // 初始化和设置 io_uring 的缓冲区环，用于存储和传输数据
        void setup_buffer_ring(
                io_uring_buf_ring *buffer_ring, std::span<std::vector<char>> buffer_list,
                unsigned int buffer_ring_size
        );

        // 向 io_uring 的缓冲区环添加一个新的缓冲区，动态地扩展缓冲区环
        void add_buffer(
                io_uring_buf_ring *buffer_ring, std::span<char> buffer, unsigned int buffer_id,
                unsigned int buffer_ring_size
        );

    private:
        // io_uring in liburing
        ::io_uring io_uring_;
    };
}