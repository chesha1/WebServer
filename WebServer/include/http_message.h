#include <string>
#include <tuple>
#include <vector>

// HTTP 请求和响应
namespace WebServer {

    class http_request {
    public:
        std::string method; // HTTP请 求的方法，比如"GET"、"POST"等
        std::string url; // 请求的 URL
        std::string version; // HTTP 的版本，比如"HTTP/1.1"
        std::vector<std::tuple<std::string, std::string>> header_list; // HTTP 请求头的键值对
    };

    class http_response {
    public:
        std::string version;
        std::string status; // HTTP 响应的状态码，比如"200"
        std::string status_text; // HTTP 状态码对应的文本，比如"OK"
        std::vector<std::tuple<std::string, std::string>> header_list; // HTTP 响应头的键值对

        [[nodiscard]] std::string serialize() const; // 将 http_response 对象序列化成一个字符串
    };

}