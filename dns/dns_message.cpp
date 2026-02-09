#include "dns_message.hpp"
#include <iostream>

namespace dns {

static constexpr int    kMaxNameDepth  = 16;
static constexpr size_t kMaxNameLength = 255;

uint16_t DNSParser::read16(const uint8_t* p) const {
    return (static_cast<uint16_t>(p[0]) << 8) | p[1];
}

uint32_t DNSParser::read32(const uint8_t* p) const {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) <<  8) |
           p[3];
}

NameParseResult DNSParser::parseName(
    const uint8_t* data,
    size_t length,
    size_t offset,
    int depth
) {
    if (depth > kMaxNameDepth || offset >= length) return {false,"",offset};

    std::string name;
    size_t pos = offset;
    size_t final_next = offset;
    bool jumped = false;

    while (true) {
        if (pos >= length) return {false,"",offset};
        uint8_t len = data[pos];

        // compression pointer
        if ((len & 0xC0) == 0xC0) {
            if (pos + 1 >= length) return {false,"",offset};
            uint16_t ptr = ((len & 0x3F) << 8) | data[pos + 1];
            if (ptr >= length) return {false,"",offset};
            if (!jumped) { final_next = pos + 2; jumped = true; }
            auto r = parseName(data, length, ptr, depth + 1);
            if (!r.success) return r;
            if (!name.empty() && !r.name.empty()) name.push_back('.');
            name += r.name;
            // compression pointer ends the name
            break;
        }

        // root label
        if (len == 0) {
            if (!jumped) final_next = pos + 1;
            break;
        }

        if (len > 63 || pos + 1 + len > length) return {false,"",offset};
        if (name.size() + len + (name.empty() ? 0 : 1) > kMaxNameLength) return {false,"",offset};

        if (!name.empty()) name.push_back('.');
        name.append(reinterpret_cast<const char*>(data + pos + 1), len);

        pos += 1 + len;
        if (!jumped) final_next = pos;
    }

    return {true, name, final_next};
}


bool DNSParser::parseQuestion(const uint8_t* data, size_t length, size_t& offset, Question& q) {
    auto r = parseName(data,length,offset,0);
    if(!r.success) return false;
    q.name = r.name; offset = r.next_offset;
    if(offset+4>length) return false;
    q.type = read16(data+offset);
    q.qclass = read16(data+offset+2);
    offset+=4;
    return true;
}

bool DNSParser::parseRecord(const uint8_t* data, size_t length, size_t& offset, DNSRecord& rr) {
    auto r = parseName(data, length, offset, 0);
    if (!r.success) return false;
    rr.name = r.name;
    offset = r.next_offset;

    if (offset + 10 > length) return false;

    rr.type     = read16(data + offset);
    rr.klass    = read16(data + offset + 2);
    rr.ttl      = read32(data + offset + 4);
    rr.rdlength = read16(data + offset + 8);
    offset += 10;

    if (offset + rr.rdlength > length) return false;

    const uint8_t* rdata = data + offset;
    size_t rdend = offset + rr.rdlength;
    rr.raw_rdata.assign(rdata, rdata + rr.rdlength);

    size_t rdata_offset = offset;

    switch (static_cast<RecordType>(rr.type)) {
        case RecordType::CNAME:
        case RecordType::NS:
        case RecordType::PTR: {
            auto nr = parseName(data, length, rdata_offset, 0);
            if (!nr.success || nr.next_offset > rdend) return false;
            rr.domain = nr.name;
            break;
        }
        case RecordType::MX: {
            if (rr.rdlength < 2) return false;
            MXRecordData mx;
            mx.preference = read16(rdata);
            auto nr = parseName(data, length, rdata_offset + 2, 0);
            if (!nr.success || nr.next_offset > rdend) return false;
            mx.exchange = nr.name;
            rr.mx = mx;
            break;
        }
        case RecordType::SRV: {
            if (rr.rdlength < 6) return false;
            SRVRecordData srv;
            srv.priority = read16(rdata);
            srv.weight   = read16(rdata + 2);
            srv.port     = read16(rdata + 4);
            auto nr = parseName(data, length, rdata_offset + 6, 0);
            if (!nr.success || nr.next_offset > rdend) return false;
            srv.target = nr.name;
            rr.srv = srv;
            break;
        }
        case RecordType::SOA: {
            size_t pos = rdata_offset;
            auto m = parseName(data, length, pos, 0);
            if (!m.success) return false;
            pos = m.next_offset;
            auto rname = parseName(data, length, pos, 0);
            if (!rname.success) return false;
            pos = rname.next_offset;
            if (pos + 20 > rdend) return false;
            SOARecordData soa;
            soa.mname   = m.name;
            soa.rname   = rname.name;
            soa.serial  = read32(data + pos);
            soa.refresh = read32(data + pos + 4);
            soa.retry   = read32(data + pos + 8);
            soa.expire  = read32(data + pos + 12);
            soa.minimum = read32(data + pos + 16);
            rr.soa = soa;
            break;
        }
        default:
            break;
    }

    offset = rdend;
    return true;
}


bool DNSParser::parse(const uint8_t* data, size_t length, DNSMessage& out){
    if(length<12) return false;
    out.header.id = read16(data);
    out.header.flags = read16(data+2);
    out.header.qdcount = read16(data+4);
    out.header.ancount = read16(data+6);
    out.header.nscount = read16(data+8);
    out.header.arcount = read16(data+10);

    if(out.header.qdcount>100||out.header.ancount>100||out.header.nscount>100||out.header.arcount>100) {
        
        printf("Raw DNS header bytes: %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
              data[0], data[1], data[2], data[3], data[4], data[5],
              data[6], data[7], data[8], data[9], data[10], data[11]);
        
        printf("DNS header exceeds limits! id=%u, flags=0x%04x, qdcount=%u, ancount=%u, nscount=%u, arcount=%u\n",
              out.header.id,
              out.header.flags,
              out.header.qdcount,
              out.header.ancount,
              out.header.nscount,
              out.header.arcount);
        return false;
    }

    size_t offset = 12;
    for(uint16_t i=0;i<out.header.qdcount;++i){Question q; if(!parseQuestion(data,length,offset,q)) return false; out.questions.push_back(q);}
    for(uint16_t i=0;i<out.header.ancount;++i){DNSRecord rr; if(!parseRecord(data,length,offset,rr)) return false; out.answers.push_back(rr);}
    for(uint16_t i=0;i<out.header.nscount;++i){DNSRecord rr; if(!parseRecord(data,length,offset,rr)) return false; out.authorities.push_back(rr);}
    for(uint16_t i=0;i<out.header.arcount;++i){DNSRecord rr; if(!parseRecord(data,length,offset,rr)) return false; out.additionals.push_back(rr);}
    return true;
}

bool DNSParser::matchesStandardPort(uint16_t port) {
    // DNS 标准端口：TCP/UDP 53
    // TCP DNS 用于大于 512 字节的响应，需要 2 字节长度前缀
    // UDP DNS 用于常规查询，响应最大 512 字节
    return port == 53;
}

bool DNSTTLWirePatcher::skipName(const uint8_t* data, size_t len, size_t& offset){
    while(true){
        if(offset>=len) return false;
        uint8_t c = data[offset];
        if((c&0xC0)==0xC0){ if(offset+2>len) return false; offset+=2; return true; }
        if(c==0){ offset+=1; return true; }
        if(c>63||offset+1+c>len) return false;
        offset+=1+c;
    }
    return false;
}

void DNSTTLWirePatcher::write32(uint8_t* p, uint32_t host){
    uint32_t net = htonl(host);
    std::memcpy(p,&net,sizeof(net));
}

bool DNSTTLWirePatcher::patchTTL(uint8_t* data, size_t len, const DNSMessage& msg, uint32_t newTTL_host, int* modified_count){
    if(len<sizeof(DNSHeader)) return false;
    size_t offset = sizeof(DNSHeader);
    int modified=0;

    for(const auto& q: msg.questions){
        if(!skipName(data,len,offset)) return false;
        if(offset+4>len) return false;
        offset+=4;
    }

    auto patchRRSet = [&](const std::vector<DNSRecord>& recs)->bool{
        for(const auto& rr: recs){
            if(!skipName(data,len,offset)) return false;
            if(offset+10>len) return false;
            uint16_t type = ntohs(*reinterpret_cast<const uint16_t*>(data+offset));
            size_t ttl_offset = offset+4;
            if(type!=static_cast<uint16_t>(RecordType::OPT)){
                if(ttl_offset+4>len) return false;
                write32(data+ttl_offset,newTTL_host);
                ++modified;
            }
            uint16_t rdlength = ntohs(*reinterpret_cast<const uint16_t*>(data+offset+8));
            offset+=10;
            if(offset+rdlength>len) return false;
            offset+=rdlength;
        }
        return true;
    };

    if(!patchRRSet(msg.answers)) return false;
    if(!patchRRSet(msg.authorities)) return false;
    if(!patchRRSet(msg.additionals)) return false;

    if(modified_count) *modified_count=modified;
    return true;
}

} // namespace dns

