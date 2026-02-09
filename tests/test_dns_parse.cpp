#include "dns/dns_message.hpp"
#include <iostream>
#include <fstream>
#include <vector>

using namespace dns;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "用法: " << argv[0] << " <DNS响应文件>" << std::endl;
        return 1;
    }

    std::string filename = argv[1];

    // 读取 DNS 响应文件
    std::ifstream file(filename, std::ios::binary);
    if (!file) {
        std::cerr << "无法打开文件: " << filename << std::endl;
        return 1;
    }

    std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    file.close();

    std::cout << "DNS 响应解析测试" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "文件: " << filename << std::endl;
    std::cout << "大小: " << data.size() << " 字节\n" << std::endl;

    // 解析 DNS 消息
    DNSParser parser;
    DNSMessage msg;

    if (!parser.parse(data.data(), data.size(), msg)) {
        std::cerr << "DNS 解析失败" << std::endl;
        return 1;
    }

    std::cout << "DNS 解析成功！\n" << std::endl;

    // 显示头部信息
    std::cout << "头部信息:" << std::endl;
    std::cout << "  Transaction ID: 0x" << std::hex << msg.header.id << std::dec << std::endl;
    std::cout << "  Flags: 0x" << std::hex << msg.header.flags << std::dec << std::endl;
    std::cout << "  Questions: " << msg.header.qdcount << std::endl;
    std::cout << "  Answers: " << msg.header.ancount << std::endl;
    std::cout << "  Authority: " << msg.header.nscount << std::endl;
    std::cout << "  Additional: " << msg.header.arcount << std::endl;

    // 显示问题部分
    if (!msg.questions.empty()) {
        std::cout << "\n;; QUESTION SECTION:" << std::endl;
        for (const auto& q : msg.questions) {
            std::cout << ";" << q.name << ".";
            std::cout << "            IN    ";
            if (q.type == static_cast<uint16_t>(RecordType::A)) {
                std::cout << "A";
            } else if (q.type == static_cast<uint16_t>(RecordType::AAAA)) {
                std::cout << "AAAA";
            } else if (q.type == static_cast<uint16_t>(RecordType::CNAME)) {
                std::cout << "CNAME";
            } else {
                std::cout << "TYPE" << q.type;
            }
            std::cout << std::endl;
        }
    }

    // 显示答案部分
    if (!msg.answers.empty()) {
        std::cout << "\n;; ANSWER SECTION:" << std::endl;
        for (const auto& ans : msg.answers) {
            std::cout << ans.name << ".";
            std::cout << "        " << ans.ttl << "    IN    ";

            if (ans.type == static_cast<uint16_t>(RecordType::A)) {
                std::cout << "A    ";
                auto ip = ans.ipv4();
                if (ip.has_value()) {
                    std::cout << ip.value();
                }
            } else if (ans.type == static_cast<uint16_t>(RecordType::AAAA)) {
                std::cout << "AAAA    ";
                auto ip = ans.ipv6();
                if (ip.has_value()) {
                    std::cout << ip.value();
                }
            } else if (ans.type == static_cast<uint16_t>(RecordType::CNAME)) {
                std::cout << "CNAME    ";
                if (ans.domain.has_value()) {
                    std::cout << ans.domain.value();
                }
            } else if (ans.type == static_cast<uint16_t>(RecordType::PTR)) {
                std::cout << "PTR    ";
                if (ans.domain.has_value()) {
                    std::cout << ans.domain.value();
                }
            } else {
                std::cout << "TYPE" << ans.type;
            }
            std::cout << std::endl;
        }
    }

    // 统计信息
    std::cout << "\n========================================" << std::endl;
    std::cout << "统计信息:" << std::endl;
    std::cout << "  问题数: " << msg.questions.size() << std::endl;
    std::cout << "  答案数: " << msg.answers.size() << std::endl;

    // 统计记录类型
    int a_count = 0, aaaa_count = 0, cname_count = 0, other_count = 0;
    for (const auto& ans : msg.answers) {
        if (ans.type == static_cast<uint16_t>(RecordType::A)) {
            a_count++;
        } else if (ans.type == static_cast<uint16_t>(RecordType::AAAA)) {
            aaaa_count++;
        } else if (ans.type == static_cast<uint16_t>(RecordType::CNAME)) {
            cname_count++;
        } else {
            other_count++;
        }
    }

    std::cout << "  A 记录: " << a_count << std::endl;
    std::cout << "  AAAA 记录: " << aaaa_count << std::endl;
    std::cout << "  CNAME 记录: " << cname_count << std::endl;
    std::cout << "  其他记录: " << other_count << std::endl;

    if (cname_count > 0) {
        std::cout << "\n✓ 发现 CNAME 记录！" << std::endl;
    }

    return 0;
}
