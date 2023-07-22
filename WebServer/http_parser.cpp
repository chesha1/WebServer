#include <algorithm>
#include <cctype>
#include <cstddef>
#include <iterator>
#include <optional>
#include <string_view>
#include <tuple>
#include <vector>
#include "http_parser.h"
#include "http_message.h"

namespace WebServer {

    // 输入一个 string_view，返回一个去除前后空白字符的 string_view
    std::string_view trim_whitespace(std::string_view string) {
        const auto first = std::find_if_not(string.cbegin(), string.cend(),
                                            [](unsigned char c) {
                                                return std::isspace(c);
                                            });
        const auto last = std::find_if_not(string.crbegin(), string.crend(), [](unsigned char c) {
            return std::isspace(c);
        }).base();
        return (last <= first) ? std::string_view()
                               : std::string_view(&*first, static_cast<std::size_t>(last - first));
    }

    // 将一个 string_view 按照给定的分隔符切分成多个 string_view
    std::vector<std::string_view> split(std::string_view string, const char delimiter) {
        std::vector<std::string_view> result;
        size_t segment_start = 0;
        size_t segment_end = 0;

        while ((segment_end = string.find(delimiter, segment_start)) != std::string::npos) {
            std::string_view token = string.substr(segment_start, segment_end - segment_start);
            result.emplace_back(token);
            segment_start = segment_end + 1;
        }

        result.emplace_back(string.substr(segment_start));
        return result;
    }

    std::vector<std::string_view> split(std::string_view string, std::string_view delimiter) {
        std::vector<std::string_view> result;
        size_t segment_start = 0;
        size_t segment_end = 0;
        const size_t delimiter_length = delimiter.length();

        while ((segment_end = string.find(delimiter, segment_start)) != std::string::npos) {
            std::string_view token = string.substr(segment_start, segment_end - segment_start);
            result.emplace_back(token);
            segment_start = segment_end + delimiter_length;
        }

        result.emplace_back(string.substr(segment_start));
        return result;
    }

    std::optional<http_request> http_parser::parse_packet(std::span<char> packet) {
        raw_http_request_.reserve(raw_http_request_.size() + packet.size());
        raw_http_request_.insert(raw_http_request_.end(), packet.begin(), packet.end());

        std::string_view raw_http_request(raw_http_request_.data());

        // 然后检查这个字符串是否以"\r\n\r\n"结束，这是一个 HTTP 请求的标准结束标志
        if (!raw_http_request.ends_with("\r\n\r\n")) {
            return {};
        }

        http_request http_request;

        // 将请求按照"\r\n"切分成多行
        const std::vector<std::string_view> request_line_list = split(raw_http_request, "\r\n");

        // 将第一行（请求行）按照空格切分，得到 HTTP 方法、URL 和版本
        const std::vector<std::string_view> status_line_list = split(request_line_list[0], ' ');
        http_request.method = status_line_list[0];
        http_request.url = status_line_list[1];
        http_request.version = status_line_list[2];

        // 遍历剩下的行（头部行），每行按照":"切分成键和值，构成一个头部字段
        for (size_t line_index = 1; line_index < request_line_list.size(); ++line_index) {
            const std::string_view header_line = request_line_list[line_index];
            const std::vector<std::string_view> header = split(header_line, ':');
            if (header.size() == 2) {
                http_request.header_list.emplace_back(header[0], trim_whitespace(header[1]));
            }
        }

        raw_http_request_.clear();
        return http_request;
    }
}