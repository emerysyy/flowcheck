#ifndef DNS_RESPONSE_CACHE_HPP
#define DNS_RESPONSE_CACHE_HPP

#include <unordered_map>
#include <list>
#include <vector>
#include <string>
#include <cstdint>
#include <mutex>
#include <chrono>
#include <atomic>

namespace dns {

/**
 * CacheKey:
 *  - qname 已经是 lowercase、无 trailing dot
 *  - 不包含 ECS，不支持 negative caching
 */
struct CacheKey {
    std::string qname;
    uint16_t    qtype;
    uint16_t    qclass;

    bool operator==(const CacheKey& o) const noexcept {
        return qname == o.qname &&
               qtype == o.qtype &&
               qclass == o.qclass;
    }
};

struct CacheKeyHash {
    size_t operator()(const CacheKey& k) const noexcept {
        size_t seed = std::hash<std::string>{}(k.qname);
        size_t h2   = (static_cast<size_t>(k.qtype) << 16) | k.qclass;
        seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

struct CachedDNSResponse {
    std::vector<uint8_t> wire;   // 原始 response wire
    uint32_t ttl;                // min TTL，仅用于过期判断
    std::chrono::steady_clock::time_point stored_at;
};

struct CacheStats {
    std::atomic<uint64_t> hits{0};
    std::atomic<uint64_t> misses{0};
    std::atomic<uint64_t> evictions{0};
};

class DNSResponseCache {
public:
    explicit DNSResponseCache(size_t max_entries = 1024);

    bool storeResponse(const uint8_t* data, size_t len);

    bool buildResponseFromCache(
        const uint8_t* query,
        size_t query_len,
        std::vector<uint8_t>& out_response
    );

    const CacheStats& stats() const { return stats_; }

private:
    bool extractKeyFromMessage(
        const uint8_t* data,
        size_t len,
        CacheKey& key,
        uint16_t* out_txid = nullptr
    );

    bool validateResponseSemantics(
        const uint8_t* data,
        size_t len,
        uint32_t& out_min_ttl
    );

    void rewriteTransactionID(uint8_t* data, uint16_t txid);

private:
    using LRUList  = std::list<CacheKey>;
    using MapValue = std::pair<CachedDNSResponse, LRUList::iterator>;

    std::unordered_map<CacheKey, MapValue, CacheKeyHash> cache_;
    LRUList lru_;

    size_t max_entries_;
    mutable std::mutex lock_;
    CacheStats stats_;
};

} // namespace dns

#endif // DNS_RESPONSE_CACHE_HPP

