#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#include <optional>
#include <span>
#include <string>

namespace WebServer {
    class http_request;

    class http_parser {
    public:
        // 输入一个字符的 span 对象，表示从网络接收到的 HTTP 请求的原始数据
        // 如果能成功解析出一个 HTTP 请求，返回包含这个请求的 optional ；如果解析失败，返回一个空的 optional
        std::optional<http_request> parse_packet(std::span<char> packet);

    private:
        // 储存从网络接收到的原始 HTTP 请求数据
        std::string raw_http_request_;
    };
}

#endif