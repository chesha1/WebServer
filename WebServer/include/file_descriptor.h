#include <compare>
#include <coroutine>
#include <filesystem>
#include <optional>
#include <tuple>
#include <unistd.h>
#include "io_uring.h"
#include "task.h"

// 封装了一些 fd 相关的系统调用
// open(), pipe(), splice()
namespace WebServer {
    class file_descriptor {
    public:
        file_descriptor();

        explicit file_descriptor(int raw_file_descriptor);

        ~file_descriptor();

        // 删除拷贝构造和拷贝复制，防止 fd 重复
        // 保留移动语义
        file_descriptor(file_descriptor &&other) noexcept;

        file_descriptor &operator=(file_descriptor &&other) noexcept;

        file_descriptor(const file_descriptor &other) = delete;

        file_descriptor &operator=(const file_descriptor &other) = delete;

        // 重载大于、小于和等于三种比较
        std::strong_ordering operator<=>(const file_descriptor &other) const;

        [[nodiscard]] int get_raw_file_descriptor() const;

    protected:
        std::optional<int> raw_file_descriptor_;
    };

    // 在 fd 之间移动数据
    class splice_awaiter {
    public:
        splice_awaiter(int raw_file_descriptor_in, int raw_file_descriptor_out, size_t length);

        [[nodiscard]] bool await_ready() const;

        // co_await 时，向 io_uring 提交一个 splice 请求
        void await_suspend(std::coroutine_handle<> coroutine);

        [[nodiscard]] ssize_t await_resume() const;

    private:
        const int raw_file_descriptor_in_;
        const int raw_file_descriptor_out_;
        const size_t length_;
        sqe_data sqe_data_;
    };

    // 在 fd 之间移动长度为 length 的数据
    task<ssize_t> splice(
            const file_descriptor &file_descriptor_in, const file_descriptor &file_descriptor_out,
            const size_t length
    );

    // 创建一个管道，并返回两个文件描述符，一个用于读取，一个用于写入
    std::tuple<file_descriptor, file_descriptor> pipe();

    file_descriptor open(const std::filesystem::path &path);
}

