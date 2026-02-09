#include "http_parser.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace proto {

static constexpr size_t kMaxHeaderSize = 8192;
static constexpr size_t kMaxHostName = 256;

bool HTTP::isRequest(const uint8_t* data, size_t length) {
    if (length < 16) return false; // Minimum: "G / HTTP/1.0\r\n"

    const char* str = reinterpret_cast<const char*>(data);

    // 方法检测：使用查找表优化
    static const struct {
        const char* method;
        size_t len;
    } methods[] = {
        {"GET ", 4},
        {"POST ", 5},
        {"PUT ", 4},
        {"HEAD ", 5},
        {"DELETE ", 7},
        {"OPTIONS ", 8},
        {"PATCH ", 6},
        {"CONNECT ", 8},
        {"TRACE ", 6},
    };

    bool method_found = false;
    for (const auto& m : methods) {
        if (length >= m.len && memcmp(str, m.method, m.len) == 0) {
            method_found = true;
            break;
        }
    }

    if (!method_found) return false;

    // 查找第一行结束
    const char* line_end = static_cast<const char*>(
        memchr(str, '\r', std::min(length, size_t(2048)))
    );

    if (!line_end || line_end + 1 >= str + length || line_end[1] != '\n') {
        return false;
    }

    // 检查 HTTP 版本（从后往前找）
    size_t line_len = line_end - str;
    if (line_len >= 8) { // 至少 "HTTP/1.0"
        const char* version_pos = line_end - 8;
        if (version_pos >= str &&
            (memcmp(version_pos, "HTTP/1.0", 8) == 0 ||
             memcmp(version_pos, "HTTP/1.1", 8) == 0 ||
             memcmp(version_pos, "HTTP/2.0", 8) == 0)) {
            return true;
        }
    }

    return false;
}

bool HTTP::isResponse(const uint8_t* data, size_t length) {
    if (length < 12) return false; // "HTTP/1.0 200"

    const char* str = reinterpret_cast<const char*>(data);

    // HTTP 响应必须以 "HTTP/1." 开头
    if (memcmp(str, "HTTP/1.", 7) != 0) {
        return false;
    }

    // 检查版本号
    if (str[7] != '0' && str[7] != '1') {
        return false;
    }

    // 空格 + 状态码
    if (str[8] != ' ') return false;

    // 状态码必须是 3 位数字
    if (!std::isdigit(static_cast<unsigned char>(str[9])) ||
        !std::isdigit(static_cast<unsigned char>(str[10])) ||
        !std::isdigit(static_cast<unsigned char>(str[11]))) {
        return false;
    }

    return true;
}

bool HTTP::matchesStandardPort(uint16_t port) {
    // HTTP 标准端口
    return port == 80 || port == 8080 || port == 8000 || port == 3000 || port == 8888;
}

std::string HTTP::normalizeHost(std::string host) {
    // Trim leading whitespace
    size_t start = 0;
    while (start < host.length() &&
           std::isspace(static_cast<unsigned char>(host[start]))) {
        start++;
    }

    // Trim trailing whitespace
    size_t end = host.length();
    while (end > start &&
           std::isspace(static_cast<unsigned char>(host[end - 1]))) {
        end--;
    }

    host = host.substr(start, end - start);

    // Remove port number if present
    size_t colonPos = host.rfind(':');
    if (colonPos != std::string::npos) {
        bool isPort = true;
        for (size_t i = colonPos + 1; i < host.length(); ++i) {
            if (!std::isdigit(static_cast<unsigned char>(host[i]))) {
                isPort = false;
                break;
            }
        }
        if (isPort) {
            host = host.substr(0, colonPos);
        }
    }

    // Convert to lowercase
    std::transform(host.begin(), host.end(), host.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    return host;
}

std::optional<std::string> HTTP::extractHostHeader(const char* data, size_t length) {
    // Look for Host: header (case-insensitive)
    for (size_t i = 0; i + 6 < length; ++i) {
        if ((data[i] == '\r' && i + 1 < length && data[i + 1] == '\n' &&
             i + 6 < length && std::tolower(data[i + 2]) == 'h') ||
            (data[i] == '\n' && i + 5 < length && std::tolower(data[i + 1]) == 'h')) {

            size_t nameStart = (data[i] == '\r') ? i + 2 : i + 1;

            // Check if it's "Host:"
            if (nameStart + 5 > length) continue;

            char h = std::tolower(data[nameStart]);
            char o1 = std::tolower(data[nameStart + 1]);
            char s = std::tolower(data[nameStart + 2]);
            char t = std::tolower(data[nameStart + 3]);
            char colon = data[nameStart + 4];

            if (h == 'h' && o1 == 'o' && s == 's' && t == 't' && colon == ':') {
                size_t valueStart = nameStart + 5;
                while (valueStart < length &&
                       std::isspace(static_cast<unsigned char>(data[valueStart]))) {
                    valueStart++;
                }

                size_t valueEnd = valueStart;
                while (valueEnd < length && data[valueEnd] != '\r' && data[valueEnd] != '\n') {
                    valueEnd++;
                }

                if (valueEnd > valueStart && (valueEnd - valueStart) <= kMaxHostName) {
                    std::string host(data + valueStart, valueEnd - valueStart);
                    return normalizeHost(host);
                }
            }
        }
    }

    return std::nullopt;
}

HTTPResult HTTP::parseHost(const uint8_t* data, size_t length) {
    HTTPResult result{false, ""};

    if (length > kMaxHeaderSize) {
        return result;
    }

    if (!isRequest(data, length)) {
        return result;
    }

    const char* str = reinterpret_cast<const char*>(data);

    // Find end of headers (double CRLF)
    const char* headersEnd = nullptr;
    for (size_t i = 0; i + 3 < length; ++i) {
        if (str[i] == '\r' && str[i + 1] == '\n' &&
            str[i + 2] == '\r' && str[i + 3] == '\n') {
            headersEnd = str + i;
            break;
        } else if (str[i] == '\n' && str[i + 1] == '\n') {
            headersEnd = str + i;
            break;
        }
    }

    if (headersEnd == nullptr) {
        return result;
    }

    std::optional<std::string> host = extractHostHeader(str, headersEnd - str);
    if (host.has_value() && !host->empty()) {
        result.success = true;
        result.host = *host;
    }

    return result;
}

} // namespace proto
