#include "dns_cache.hpp"
#include "dns_message.hpp"

#include <cstring>
#include <arpa/inet.h>
#include <algorithm>

namespace dns {

/**
 * normalizeName
 * 前提条件：
 *  - DNSParser 已经正确解压 compression pointer
 *  - 不处理 IDN（假定上游已 punycode）
 */
static inline std::string normalizeName(std::string s) {
    for (char& c : s) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    if (!s.empty() && s.back() == '.') {
        s.pop_back();
    }
    return s;
}

DNSResponseCache::DNSResponseCache(size_t max_entries)
: max_entries_(max_entries) {}

bool DNSResponseCache::extractKeyFromMessage(
    const uint8_t* data,
    size_t len,
    CacheKey& key,
    uint16_t* out_txid
) {
    DNSParser parser;
    DNSMessage msg;
    if (!parser.parse(data, len, msg)) return false;

    if (msg.questions.size() != 1) return false;

    const auto& q = msg.questions[0];
    key.qname  = normalizeName(q.name);
    key.qtype  = q.type;
    key.qclass = q.qclass;

    if (out_txid) {
        *out_txid = ntohs(*reinterpret_cast<const uint16_t*>(data));
    }
    return true;
}

bool DNSResponseCache::validateResponseSemantics(
    const uint8_t* data,
    size_t len,
    uint32_t& out_min_ttl
) {
    if (len < 12) return false;

    uint16_t flags = ntohs(*reinterpret_cast<const uint16_t*>(data + 2));

    // QR=1
    if (!(flags & 0x8000)) return false;
    // TC=0
    if (flags & 0x0200) return false;
    // RCODE=NOERROR
    if ((flags & 0x000F) != 0) return false;
    // AA=1（严格模式）AA = Authoritative Answer 位
    // 表示：这个 DNS 响应是否由“该域名的权威服务器”直接生成。
//    if (!(flags & 0x0400)) return false;

    DNSParser parser;
    DNSMessage msg;
    if (!parser.parse(data, len, msg)) return false;

    bool found_rr = false;
    uint32_t min_ttl = UINT32_MAX;

    auto scan = [&](const std::vector<DNSRecord>& v) {
        for (const auto& rr : v) {
            if (rr.isOPT()) continue;
            found_rr = true;
            min_ttl = std::min(min_ttl, rr.ttl);
        }
    };

    scan(msg.answers);
    scan(msg.authorities);

    if (!found_rr) return false;
    if (min_ttl == 0) return false;

    out_min_ttl = min_ttl;
    return true;
}

void DNSResponseCache::rewriteTransactionID(uint8_t* data, uint16_t txid) {
    uint16_t net = htons(txid);
    std::memcpy(data, &net, sizeof(net));
}

bool DNSResponseCache::storeResponse(const uint8_t* data, size_t len) {
    CacheKey key;
    if (!extractKeyFromMessage(data, len, key)) return false;

    uint32_t min_ttl = 0;
    if (!validateResponseSemantics(data, len, min_ttl)) return false;

    CachedDNSResponse entry;
    entry.wire.assign(data, data + len);
    entry.ttl = min_ttl;
    entry.stored_at = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> g(lock_);

    auto it = cache_.find(key);
    if (it != cache_.end()) {
        // 更新已有条目（不触发 eviction）
        lru_.erase(it->second.second);
        lru_.push_front(key);
        it->second.first  = std::move(entry);
        it->second.second = lru_.begin();
        return true;
    }

    // 新条目：先驱逐，再插入（避免误删刚插入的 key）
    if (cache_.size() >= max_entries_) {
        const CacheKey& victim = lru_.back();
        cache_.erase(victim);
        lru_.pop_back();
        stats_.evictions++;
    }

    lru_.push_front(key);
    try {
        cache_.emplace(
            key,
            std::make_pair(std::move(entry), lru_.begin())
        );
    } catch (const std::bad_alloc&) {
        lru_.pop_front();
        return false;
    }

    return true;
}

bool DNSResponseCache::buildResponseFromCache(
    const uint8_t* query,
    size_t query_len,
    std::vector<uint8_t>& out_response
) {
    CacheKey key;
    uint16_t txid = 0;

    if (!extractKeyFromMessage(query, query_len, key, &txid)) {
        stats_.misses++;
        return false;
    }

    std::lock_guard<std::mutex> g(lock_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        stats_.misses++;
        return false;
    }

    auto now = std::chrono::steady_clock::now();
    uint32_t elapsed = static_cast<uint32_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            now - it->second.first.stored_at
        ).count()
    );

    if (elapsed >= it->second.first.ttl) {
        lru_.erase(it->second.second);
        cache_.erase(it);
        stats_.evictions++;
        stats_.misses++;
        return false;
    }

    // LRU 提升
    auto& entry_pair = it->second;
    lru_.erase(entry_pair.second);   // 仅 list iterator 失效，map iterator 仍有效
    lru_.push_front(key);
    entry_pair.second = lru_.begin();

    out_response = entry_pair.first.wire;
    rewriteTransactionID(out_response.data(), txid);

    stats_.hits++;
    return true;
}

} // namespace dns

