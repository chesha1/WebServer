#include <sstream>
#include <string>

#include "http_message.h"

namespace WebServer {
    std::string http_response::serialize() const {
        std::stringstream raw_http_response;
        raw_http_response << version << ' ' << status << ' ' << status_text << "\r\n";

        // 遍历 header_list，每个元素是一个键值对，表示一个 HTTP 头部字段
        // 键和值之间用 ":" 分隔，每个头部字段以 "\r\n" 结束
        for (const auto &[k, v]: header_list) {
            raw_http_response << k << ':' << v << "\r\n";
        }

        // 在头部的最后添加一个额外的 "\r\n"，表示头部的结束
        // 根据 HTTP 协议，这个额外的 "\r\n" 后面会接着是 HTTP 响应的主体（body）
        raw_http_response << "\r\n";

        // 换为字符串并返回
        return raw_http_response.str();
    }
}