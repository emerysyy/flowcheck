#ifndef dns_message_hpp
#define dns_message_hpp

#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <arpa/inet.h>
#include <cstring>

namespace dns {

enum class RecordType : uint16_t {
    A     = 1,
    NS    = 2,
    CNAME = 5,
    SOA   = 6,
    PTR   = 12,
    MX    = 15,
    TXT   = 16,
    AAAA  = 28,
    SRV   = 33,
    OPT   = 41,
};

struct DNSHeader {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;

    inline uint8_t dns_rcode() const {
        return flags & 0x0F; // 直接取低4位
    }
};

struct Question {
    std::string name;
    uint16_t type;
    uint16_t qclass;
};

struct MXRecordData {
    uint16_t preference;
    std::string exchange;
};

struct SRVRecordData {
    uint16_t priority;
    uint16_t weight;
    uint16_t port;
    std::string target;
};

struct SOARecordData {
    std::string mname;
    std::string rname;
    uint32_t serial;
    uint32_t refresh;
    uint32_t retry;
    uint32_t expire;
    uint32_t minimum;
};

struct DNSRecord {
    std::string name;
    uint16_t type;
    uint16_t klass;
    uint32_t ttl;
    uint16_t rdlength;
    std::vector<uint8_t> raw_rdata;

    std::optional<std::string> domain;
    std::optional<MXRecordData> mx;
    std::optional<SRVRecordData> srv;
    std::optional<SOARecordData> soa;

    bool isOPT() const { return type == static_cast<uint16_t>(RecordType::OPT); }

    // ---- IPv4 ----
    inline std::optional<std::string> ipv4() const {
        if (type != static_cast<uint16_t>(RecordType::A)) return std::nullopt;
        if (raw_rdata.size() != 4) return std::nullopt;
        char buf[INET_ADDRSTRLEN] = {};
        if (!inet_ntop(AF_INET, raw_rdata.data(), buf, sizeof(buf))) return std::nullopt;
        return std::string(buf);
    }

    // ---- IPv6 ----
    inline std::optional<std::string> ipv6() const {
        if (type != static_cast<uint16_t>(RecordType::AAAA)) return std::nullopt;
        if (raw_rdata.size() != 16) return std::nullopt;
        char buf[INET6_ADDRSTRLEN] = {};
        if (!inet_ntop(AF_INET6, raw_rdata.data(), buf, sizeof(buf))) return std::nullopt;
        return std::string(buf);
    }
};

struct DNSMessage {
    DNSHeader header;
    std::vector<Question> questions;
    std::vector<DNSRecord> answers;
    std::vector<DNSRecord> authorities;
    std::vector<DNSRecord> additionals;
};

struct NameParseResult {
    bool success;
    std::string name;
    size_t next_offset;
};

class DNSParser {
public:
    bool parse(const uint8_t* data, size_t length, DNSMessage& out);
    
    static bool matchesStandardPort(uint16_t port);

private:
    NameParseResult parseName(const uint8_t* data, size_t length, size_t offset, int depth);
    bool parseQuestion(const uint8_t* data, size_t length, size_t& offset, Question& q);
    bool parseRecord(const uint8_t* data, size_t length, size_t& offset, DNSRecord& rr);

    uint16_t read16(const uint8_t* p) const;
    uint32_t read32(const uint8_t* p) const;
};

class DNSTTLWirePatcher {
public:
    static bool patchTTL(uint8_t* data, size_t len, const DNSMessage& msg, uint32_t newTTL_host, int* modified_count=nullptr);

private:
    static bool skipName(const uint8_t* data, size_t len, size_t& offset);
    static void write32(uint8_t* p, uint32_t host);
};

} // namespace dns

#endif // dns_message_hpp

