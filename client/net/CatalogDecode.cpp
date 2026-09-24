#include "CatalogDecode.h"

#include <cstdio>
#include <cstdint>

namespace KnC::Client {

using ::knc::Packet;

namespace {

// reads from a copy and guards every read so a short or custom frame never throws
struct Reader {
    Packet p;
    explicit Reader(const Packet& src) : p(src) {}
    uint32_t u32() { return p.remaining() >= 4 ? p.readUInt32() : 0; }
    int32_t  i32() { return p.remaining() >= 4 ? p.readInt32() : 0; }
    uint8_t  u8()  { return p.remaining() >= 1 ? p.readUInt8() : 0; }
    float    f32() { return p.remaining() >= 4 ? p.readFloat() : 0.0f; }
    std::string str() { return p.remaining() ? p.readString(64) : std::string(); }
    size_t left() const { return p.remaining(); }
};

std::string options(Reader& r) {
    uint32_t n = r.u32();
    std::string s = " opts=" + std::to_string(n) + "[";
    for (uint32_t i = 0; i < n && r.left() >= 16; ++i) {
        char buf[64];
        uint32_t pk = r.u32(), mode = r.u32(), per = r.u32(), act = r.u32();
        snprintf(buf, sizeof(buf), "%s%u/%u/%u/%u", i ? "," : "", pk, mode, per, act);
        s += buf;
    }
    return s + "]";
}

// 0xBF driver row
std::string decDriver(Reader& r) {
    char h[96];
    uint32_t id1 = r.u32(), id2 = r.u32(), id3 = r.u32(), key = r.u32(), id5 = r.u32();
    (void)id1; (void)id2; (void)id3; (void)id5;
    std::string asset = r.str();
    uint32_t s0 = r.u32(), s1 = r.u32(), s2 = r.u32(), s3 = r.u32(), s4 = r.u32();
    std::string label = r.str(), info = r.str();
    snprintf(h, sizeof(h), "DRIVER key=%u asset=%s slots=[%d,%d,%d,%d,%d] label=%s info=%s",
             key, asset.c_str(), (int)s0, (int)s1, (int)s2, (int)s3, (int)s4,
             label.c_str(), info.c_str());
    return h + options(r);
}

// 0xC0 kart row
std::string decKart(Reader& r) {
    char h[160];
    uint32_t vis = r.u32(); (void)vis;
    r.u32();
    uint32_t key = r.u32();
    r.u8();
    r.u32();
    uint32_t scheme = r.u32(); (void)scheme;
    r.u32(); r.u32();
    std::string model = r.str(), str2 = r.str(), str3 = r.str(); (void)str2; (void)str3;
    for (int i = 0; i < 8; ++i) r.u32();
    float st[17];
    for (int i = 0; i < 17; ++i) st[i] = r.f32();
    r.u32(); r.u32(); r.u32(); r.u32();
    snprintf(h, sizeof(h),
             "KART   key=%u model=%s stats=[%.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f]",
             key, model.c_str(), st[0], st[1], st[2], st[3], st[4], st[5], st[6], st[7],
             st[8], st[9], st[10], st[11], st[12], st[13], st[14], st[15], st[16]);
    return h + options(r);
}

// 0xC1 item row
std::string decItem(Reader& r) {
    char h[96];
    r.u32(); r.u32();
    uint32_t key = r.u32(), useType = r.u32();
    r.u32();
    std::string s1 = r.str(), s2 = r.str(), s3 = r.str(); (void)s2; (void)s3;
    snprintf(h, sizeof(h), "ITEM   key=%u use=%u name=%s", key, useType, s1.c_str());
    return h + options(r);
}

// 0xC2 part row
std::string decPart(Reader& r) {
    char h[128];
    r.u32(); r.u32();
    uint32_t key = r.u32();
    r.u32();
    std::string model = r.str();
    r.u32(); r.u32();
    int32_t restrict = r.i32();
    std::string s2 = r.str(), s3 = r.str(); (void)s3;
    r.u32(); r.u32(); r.u32(); r.u32();
    snprintf(h, sizeof(h), "PART   key=%u model=%s restrict=%d name=%s",
             key, model.c_str(), restrict, s2.c_str());
    return h + options(r);
}

}

std::string decodeCatalog(uint16_t opcode, const Packet& pkt) {
    Reader r(pkt);
    switch (opcode) {
    case 0xBF: return decDriver(r);
    case 0xC0: return decKart(r);
    case 0xC1: return decItem(r);
    case 0xC2: return decPart(r);
    default:   return std::string();
    }
}

}
