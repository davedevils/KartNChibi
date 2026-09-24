#include "SampleServer.h"

#include "net/Utf.h"

#include <ws2tcpip.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "ws2_32.lib")

namespace KnC::Client {

using ::knc::Packet;

namespace {

constexpr uint32_t kMyId = 1001;
constexpr uint32_t kBotA = 1002;
constexpr uint32_t kBotB = 1003;
constexpr uint32_t kRoomId = 7;
constexpr uint32_t kDefaultTrack = 90;

// KNC SAMPLE TRACK picks the sample race track a timing aid for Pinky Road 10 or Toy Circuit 90
uint32_t sampleTrack() {
    const char* forced = std::getenv("KNC_SAMPLE_TRACK");
    return forced != nullptr ? static_cast<uint32_t>(std::atoi(forced)) : kDefaultTrack;
}
constexpr size_t kBlobSize = 0x4C8;

// one sample driver of the 0x00BF rows the asset is a Driver Body High folder on disk
struct SampleDriver { uint32_t key; const char* asset; const char* name; const char* desc; bool pick; };
const SampleDriver kDrivers[] = {
    {1, "Cosmo", "DRIVER_COSMO_TITLE", "DRIVER_COSMO_INFO", true},
    {2, "Moriko", "DRIVER_MORIKO_TITLE", "DRIVER_MORIKO_INFO", true},
    {3, "Witch", "DRIVER_WITCH_TITLE", "DRIVER_WITCH_INFO", true},
    {4, "Wolf", "DRIVER_WOLF_TITLE", "DRIVER_WOLF_INFO", true},
    {5, "Pumpkin", "DRIVER_PUMPKIN_TITLE", "DRIVER_PUMPKIN_INFO", false},
    {6, "Mummy", "DRIVER_MUMMY_TITLE", "DRIVER_MUMMY_INFO", false},
};

// one sample kart of the 0x00C0 rows the model is a Car Body High folder and a Data Car file
struct SampleKart { uint32_t key; const char* model; const char* name; const char* desc; uint32_t level; uint32_t price; };
const SampleKart kKarts[] = {
    {10010, "Basic_1", "CAR_BASIC_1_TITLE", "CAR_BASIC_1_INFO", 1, 1000},
    {10011, "Basic_2", "CAR_BASIC_2_TITLE", "CAR_BASIC_2_INFO", 2, 2000},
    {10012, "Basic_3", "CAR_BASIC_3_TITLE", "CAR_BASIC_3_INFO", 3, 3000},
    {10013, "Basic_4", "CAR_BASIC_4_TITLE", "CAR_BASIC_4_INFO", 5, 5000},
    {10101, "Bike_01", "CAR_BIKE_01_TITLE", "CAR_BIKE_01_INFO", 4, 6000},
    {12001, "M500", "CAR_M500_TITLE", "CAR_M500_INFO", 15, 40000},
    {11401, "FR_01", "CAR_FR_01_TITLE", "CAR_FR_01_INFO", 5, 8000},
    {11101, "DS_01", "CAR_DS_01_TITLE", "CAR_DS_01_INFO", 10, 20000},
    // the factory chassis of the seed its model scheme is one so the factory builds it from parts
    {601, "Circler", "CAR_CHASSIS_02_TITLE", "CAR_CHASSIS_02_INFO", 1, 9000},
};

// the seven 0x0108 car craft parts of the Circler set one per category in the seed order
struct SampleCraftPart { uint32_t key; uint32_t category; const char* name; const char* desc; int32_t grade; };
const SampleCraftPart kCraftParts[] = {
    {1001, 0, "COVER_1001_TITLE", "COVER_1001_INFO", 3},       {2001, 1, "BOOSTER_2001_TITLE", "BOOSTER_2001_INFO", 12},
    {3001, 2, "TIRES_3001_TITLE", "TIRES_3001_INFO", 30},      {4001, 3, "F_FENDER_4001_TITLE", "F_FENDER_4001_INFO", 70},
    {5001, 4, "R_FENDER_5001_TITLE", "R_FENDER_5001_INFO", 3}, {6001, 5, "BUMPER_6001_TITLE", "BUMPER_6001_INFO", 12},
    {7001, 6, "WING_7001_TITLE", "WING_7001_INFO", 30},
};

struct SampleItem { uint32_t key; const char* icon; const char* name; const char* desc; uint32_t price; };
const SampleItem kItems[] = {
    {1000, "slotchanger", "ITEM_1000_TITLE", "ITEM_1000_INFO", 500},
    {3000, "repair_half", "ITEM_3000_TITLE", "ITEM_3000_INFO", 800},
    {3001, "repair_full", "ITEM_3001_TITLE", "ITEM_3001_INFO", 1500},
    {4001, "bonus_gold", "ITEM_4001_TITLE", "ITEM_4001_INFO", 2000},
    {4002, "bonus_exp", "ITEM_4002_TITLE", "ITEM_4002_INFO", 2000},
    {2000, "gacha_coin", "ITEM_2000_TITLE", "ITEM_2000_INFO", 300},
};

struct SamplePart { uint32_t key; const char* model; uint32_t slot; const char* name; const char* desc; uint32_t price; };
const SamplePart kParts[] = {
    {2217, "common_char_back_01", 6, "PART_2217_TITLE", "PART_2217_INFO", 1200},
    {2101, "common_char_head_01", 4, "PART_2101_TITLE", "PART_2101_INFO", 900},
    {2102, "common_char_head_02", 4, "PART_2102_TITLE", "PART_2102_INFO", 900},
    {2301, "common_char_glass_01", 3, "PART_2301_TITLE", "PART_2301_INFO", 700},
    {2401, "witch_char_body_01", 2, "PART_2401_TITLE", "PART_2401_INFO", 1500},
    {3101, "ANT_04", 8, "PART_3101_TITLE", "PART_3101_INFO", 600},
    {3102, "ANT_05", 8, "PART_3102_TITLE", "PART_3102_INFO", 600},
    {3201, "NAMEBOX_001", 1, "PART_3201_TITLE", "PART_3201_INFO", 400},
    {3202, "NAMEBOX_002", 1, "PART_3202_TITLE", "PART_3202_INFO", 400},
};

struct SampleTheme { uint32_t id; const char* folder; const char* name; };
const SampleTheme kThemes[] = {
    {10, "Forest", "THEME_FOREST_INFO"}, {20, "Cookie", "THEME_COOKIE_INFO"}, {30, "Desert", "THEME_DESERT_INFO"},
    {40, "Toy", "THEME_TOY_INFO"},       {60, "Snow", "THEME_SNOW_INFO"},     {90, "Race", "THEME_RACE_INFO"},
};

struct SampleTrack { uint32_t id; uint32_t theme; const char* folder; const char* name; };
const SampleTrack kTracks[] = {
    {10, 10, "Forest_01", "TRACK_FOREST_01_INFO"}, {11, 10, "Forest_02", "TRACK_FOREST_02_INFO"},
    {20, 20, "Cookie_01", "TRACK_COOKIE_01_INFO"}, {30, 30, "Desert_01", "TRACK_DESERT_01_INFO"},
    {40, 40, "Toy_01", "TRACK_TOY_01_INFO"},       {60, 60, "Snow_01", "TRACK_SNOW_01_INFO"},
    {90, 90, "Race_01", "TRACK_RACE_01_INFO"},     {91, 90, "Race_02", "TRACK_RACE_02_INFO"},
};

void putU32(std::vector<uint8_t>& b, size_t at, uint32_t v) {
    if (at + 4 > b.size()) return;
    b[at] = static_cast<uint8_t>(v); b[at + 1] = static_cast<uint8_t>(v >> 8);
    b[at + 2] = static_cast<uint8_t>(v >> 16); b[at + 3] = static_cast<uint8_t>(v >> 24);
}

void putWide(std::vector<uint8_t>& b, size_t at, const std::u16string& s, size_t cap) {
    for (size_t i = 0; i < cap && i < s.size() && at + 2 * i + 1 < b.size(); ++i) {
        b[at + 2 * i] = static_cast<uint8_t>(s[i]);
        b[at + 2 * i + 1] = static_cast<uint8_t>(s[i] >> 8);
    }
}

void putAscii(std::vector<uint8_t>& b, size_t at, const std::string& s, size_t cap) {
    for (size_t i = 0; i < cap && i < s.size() && at + i < b.size(); ++i) b[at + i] = static_cast<uint8_t>(s[i]);
}

// the price option list of a def one permanent option on the price row of the same key
void priceList(Packet& p, uint32_t key) {
    p.writeUInt32(1);
    p.writeUInt32(key);
    p.writeUInt32(0);
    p.writeUInt32(0);
    p.writeUInt32(1);
}

// an owned character record of 0x2C bytes
void ownedCharacter(Packet& p, uint32_t instance, uint32_t driverKey, uint32_t back) {
    p.writeUInt32(instance);
    p.writeUInt32(driverKey);
    p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(back);
    p.writeUInt32(driverKey); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(1);
}

// an owned kart record of 0x38 bytes a zero durability makes a permanent row
void ownedKart(Packet& p, uint32_t instance, uint32_t kartKey, uint32_t plate, int32_t durability) {
    p.writeUInt32(instance);
    p.writeUInt32(kartKey);
    p.writeUInt32(0); p.writeUInt32(plate); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0);
    p.writeUInt32(0); p.writeUInt32(0);
    p.writeUInt32(kartKey); p.writeUInt32(durability > 0 ? 3u : 0u); p.writeInt32(durability); p.writeUInt32(1);
}

// an owned consumable record of 0x1C bytes an active flag of zero is a dead row the garage can delete
void ownedItem(Packet& p, uint32_t instance, uint32_t itemKey, uint32_t uses, uint32_t active = 1) {
    p.writeUInt32(instance); p.writeUInt32(itemKey); p.writeUInt32(itemKey); p.writeUInt32(2); p.writeUInt32(uses);
    p.writeUInt32(active); p.writeUInt32(0);
}

// an owned pet record of 0x1C bytes the equipped flag is tested against one
void ownedPet(Packet& p, uint32_t instance, uint32_t petKey, uint32_t equipped) {
    p.writeUInt32(instance); p.writeUInt32(petKey); p.writeUInt32(equipped); p.writeUInt32(900 + petKey);
    p.writeUInt32(1); p.writeUInt32(30); p.writeUInt32(1);
}

void ownedPart(Packet& p, uint32_t instance, uint32_t partKey) {
    p.writeUInt32(instance); p.writeUInt32(partKey); p.writeUInt32(0); p.writeUInt32(partKey); p.writeUInt32(0);
    p.writeUInt32(0); p.writeUInt32(1);
}

// the room member and the grid spawn share the two blobs the keys sit at their offset 4
void memberBlobs(Packet& p, uint32_t driverKey, uint32_t kartKey, uint32_t ready) {
    std::vector<uint8_t> a(0x2C, 0), b(0x38, 0), tail(0x3C, 0);
    putU32(a, 0, 5); putU32(a, 4, driverKey);
    putU32(b, 0, 6); putU32(b, 4, kartKey);
    p.writeBytes(a.data(), a.size());
    p.writeBytes(b.data(), b.size());
    p.writeUInt32(ready);
    p.writeBytes(tail.data(), tail.size());
}

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

}

SampleServer::~SampleServer() { stop(); }

bool SampleServer::start(const std::string& state) {
    m_state = state;
    // KNC SAMPLE CLEARED clears that many first mission rows so a later kind can run
    if (const char* cleared = std::getenv("KNC_SAMPLE_CLEARED")) {
        const int n = std::atoi(cleared);
        for (int i = 0; i < 5; ++i) m_missionCleared[i] = i < n;
    }
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
    m_listen = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listen == INVALID_SOCKET) return false;
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;
    if (::bind(m_listen, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) return false;
    int len = sizeof(addr);
    if (::getsockname(m_listen, reinterpret_cast<sockaddr*>(&addr), &len) != 0) return false;
    m_port = ntohs(addr.sin_port);
    if (::listen(m_listen, 2) != 0) return false;
    m_running = true;
    m_thread = std::thread([this]() { serve(); });
    std::printf("[sample] server on 127.0.0.1:%u for state %s\n", m_port, m_state.c_str());
    return true;
}

void SampleServer::stop() {
    m_quit = true;
    if (m_listen != INVALID_SOCKET) { ::closesocket(m_listen); m_listen = INVALID_SOCKET; }
    if (m_client != INVALID_SOCKET) { ::closesocket(m_client); m_client = INVALID_SOCKET; }
    if (m_thread.joinable()) m_thread.join();
    m_running = false;
}

void SampleServer::send(const Packet& pkt) {
    if (m_client == INVALID_SOCKET) return;
    const std::vector<uint8_t> bytes = pkt.serialize();
    size_t off = 0;
    while (off < bytes.size()) {
        const int n = ::send(m_client, reinterpret_cast<const char*>(bytes.data() + off), static_cast<int>(bytes.size() - off), 0);
        if (n <= 0) return;
        off += static_cast<size_t>(n);
    }
}

// one connection at a time the client comes back once after the redirect
void SampleServer::serve() {
    while (!m_quit) {
        fd_set rs; FD_ZERO(&rs); FD_SET(m_listen, &rs);
        timeval tv{0, 200000};
        if (::select(0, &rs, nullptr, nullptr, &tv) <= 0) continue;
        m_client = ::accept(m_listen, nullptr, nullptr);
        if (m_client == INVALID_SOCKET) continue;
        ++m_connections;
        m_rx.clear();
        std::printf("[sample] connection %d\n", m_connections);
        while (!m_quit && m_client != INVALID_SOCKET) {
            fd_set cs; FD_ZERO(&cs); FD_SET(m_client, &cs);
            timeval ct{0, 50000};
            const int sel = ::select(0, &cs, nullptr, nullptr, &ct);
            if (sel > 0) {
                char buf[8192];
                const int n = ::recv(m_client, buf, sizeof(buf), 0);
                if (n <= 0) { ::closesocket(m_client); m_client = INVALID_SOCKET; break; }
                m_rx.insert(m_rx.end(), buf, buf + n);
                for (;;) {
                    if (m_rx.size() < ::knc::PACKET_HEADER_SIZE) break;
                    const size_t need = Packet::peekSize(m_rx.data(), m_rx.size());
                    if (need == 0 || m_rx.size() < need) break;
                    auto parsed = Packet::parse(m_rx.data(), need);
                    m_rx.erase(m_rx.begin(), m_rx.begin() + static_cast<long long>(need));
                    if (!parsed) continue;
                    Packet pkt = std::move(*parsed);
                    handle(pkt.cmdFull(), pkt);
                    if (m_client == INVALID_SOCKET) break;
                }
            }
            tick();
        }
    }
}

// the profile blob of 0x0007 and 0x00A7 the fields at the offsets of the opcode page
void SampleServer::sendProfile(uint16_t opcode) {
    std::vector<uint8_t> body(4 + kBlobSize, 0);
    putU32(body, 0, kMyId);
    const size_t b = 4;
    putU32(body, b + 0x000, 4242);
    putWide(body, b + 0x004, u"sampleticket", 64);
    putWide(body, b + 0x486, m_state == "creation" ? u"" : u"Sample", 13);
    body[b + 0x4A0] = 1;
    // the licence pair carries the level of the stock capture account the shop rows want the higher one
    body[b + 0x4A1] = m_state == "licence" ? 2 : 12;
    putU32(body, b + 0x4A4, 3450);
    putU32(body, b + 0x4A8, m_astro);
    putU32(body, b + 0x4AC, m_gold);
    putU32(body, b + 0x4B0, 5);
    putU32(body, b + 0x4B4, 6);
    body[b + 0x4B9] = 0;
    putU32(body, b + 0x4BC, 3000);
    putU32(body, b + 0x4C0, 5000);
    // the worn pendant the char panel draws over its button
    putU32(body, b + 0x4C4, m_pendant);
    Packet p = Packet::fromCmdFull(opcode);
    p.writeBytes(body.data(), body.size());
    send(p);
}

// the catalogue rows of the login burst then the owned lists
void SampleServer::sendBurst() {
    for (const SampleTheme& t : kThemes) {
        Packet p = Packet::fromCmdFull(0x00C4);
        p.writeUInt32(1); p.writeUInt32(t.id); p.writeString(t.folder); p.writeString(t.name);
        send(p);
    }
    for (const SampleTrack& t : kTracks) {
        Packet p = Packet::fromCmdFull(0x00C3);
        p.writeUInt32(1); p.writeUInt32(t.id); p.writeUInt32(t.theme); p.writeString(t.folder);
        p.writeFloat(0.4f); p.writeFloat(0.6f); p.writeFloat(90.f);
        p.writeUInt32(0); p.writeUInt32(500); p.writeUInt32(1); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(3);
        p.writeUInt32(0); p.writeFloat(3000.f); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0);
        p.writeString(t.name);
        send(p);
    }
    for (const SampleDriver& d : kDrivers) {
        Packet p = Packet::fromCmdFull(0x00BF);
        p.writeUInt32(1); p.writeUInt32(d.pick ? 1 : 0); p.writeUInt32(d.key == 3 ? 1 : 0); p.writeUInt32(d.key); p.writeUInt32(1);
        p.writeString(d.asset);
        for (int i = 0; i < 5; ++i) p.writeUInt32(0);
        p.writeString(d.name); p.writeString(d.desc);
        priceList(p, 100 + d.key);
        send(p);
    }
    for (const SampleKart& k : kKarts) {
        Packet p = Packet::fromCmdFull(0x00C0);
        p.writeUInt32(1); p.writeUInt32(k.key == 12001 ? 2 : 0); p.writeUInt32(k.key); p.writeUInt8(0);
        p.writeUInt32(k.model[0] == 'B' && k.model[1] == 'i' ? 1 : 0); p.writeUInt32(k.key == 601 ? 1 : 0); p.writeUInt32(0); p.writeUInt32(k.level);
        p.writeString(k.model); p.writeString(k.name); p.writeString(k.desc);
        for (int i = 0; i < 8; ++i) p.writeUInt32(0);
        const float base = 0.5f + 0.03f * static_cast<float>(k.level);
        for (int i = 0; i < 17; ++i) p.writeFloat(base + 0.02f * static_cast<float>(i % 4));
        for (int i = 0; i < 4; ++i) p.writeUInt32(0);
        priceList(p, k.key);
        send(p);
    }
    for (const SampleItem& it : kItems) {
        Packet p = Packet::fromCmdFull(0x00C1);
        // use type 4 and 5 are the repair scroll family the garage Use button asks before it sends
        const uint32_t useType = it.key == 3000 ? 4u : it.key == 3001 ? 5u : 0u;
        p.writeUInt32(1); p.writeUInt32(it.key == 4001 ? 1 : 0); p.writeUInt32(it.key); p.writeUInt32(useType); p.writeUInt32(1);
        p.writeString(it.icon); p.writeString(it.name); p.writeString(it.desc);
        priceList(p, it.key);
        send(p);
    }
    for (const SamplePart& pt : kParts) {
        Packet p = Packet::fromCmdFull(0x00C2);
        p.writeUInt32(1); p.writeUInt32(0); p.writeUInt32(pt.key); p.writeUInt32(1);
        p.writeString(pt.model); p.writeUInt32(0); p.writeUInt32(pt.slot); p.writeUInt32(0xFFFFFFFFu);
        p.writeString(pt.name); p.writeString(pt.desc);
        for (int i = 0; i < 4; ++i) p.writeUInt32(0);
        priceList(p, pt.key);
        send(p);
    }
    // the 0x010C room craft objects the room decor tail names them by key
    struct SampleRoomObject { uint32_t key; uint32_t category; const char* folder; };
    static const SampleRoomObject kRoomObjects[] = {
        {1001, 0, "SKY01"}, {1002, 0, "SKY02"}, {1011, 0, "sky11"}, {2001, 1, "Floor01"}, {2003, 1, "Floor03"},
        {3001, 2, "mountain01"}, {3002, 2, "waterfall01"},
        {4008, 3, "fence01"}, {4009, 3, "flower01"}, {4011, 3, "grass01"}, {4027, 3, "fortree01"},
        {5005, 4, "Skull01"},
    };
    for (const SampleRoomObject& o : kRoomObjects) {
        Packet p = Packet::fromCmdFull(0x010C);
        p.writeUInt32(1); p.writeUInt32(0); p.writeUInt32(o.key); p.writeUInt32(o.category); p.writeUInt32(10); p.writeUInt32(0);
        p.writeString(o.folder); p.writeString(std::string("ROOM_") + o.folder); p.writeString("ROOM_INFO");
        p.writeUInt32(0);
        send(p);
    }
    for (int i = 1; i <= 2; ++i) {
        Packet p = Packet::fromCmdFull(0x0103);
        p.writeUInt32(1); p.writeUInt32(0); p.writeUInt32(static_cast<uint32_t>(i)); p.writeUInt32(0);
        p.writeString(i == 1 ? "pet_01" : "pet_02"); p.writeString(i == 1 ? "PET_01_TITLE" : "PET_02_TITLE"); p.writeString("PET_INFO");
        priceList(p, 900 + static_cast<uint32_t>(i));
        send(p);
    }
    auto price = [&](uint32_t key, uint32_t unit, uint32_t amount, uint32_t base, uint32_t sale) {
        Packet p = Packet::fromCmdFull(0x00C6);
        p.writeUInt32(key); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(unit); p.writeUInt32(amount);
        p.writeUInt32(base); p.writeUInt32(sale);
        send(p);
    };
    for (const SampleDriver& d : kDrivers) price(100 + d.key, 0, 0, 5000, 0);
    for (const SampleKart& k : kKarts) price(k.key, 3, 0, k.price, k.key == 10013 ? 4000 : 0);
    for (const SampleItem& it : kItems) price(it.key, 2, 5, it.price, 0);
    for (const SamplePart& pt : kParts) price(pt.key, 1, 30, pt.price, 0);
    for (const SampleCraftPart& part : kCraftParts) price(part.key, 1, 1, 1000, 0);
    price(901, 0, 0, 3000, 0);
    price(902, 0, 0, 4500, 0);

    // twelve visible pendants then the hidden one last and three owned rows the key is the instance
    for (uint32_t key = 1; key <= 13; ++key) {
        Packet p = Packet::fromCmdFull(0x0119);
        char base[16];
        char title[32];
        char info[32];
        std::snprintf(base, sizeof(base), "pendant_%02u", key);
        std::snprintf(title, sizeof(title), "PENDANT_%02u_TITLE", key);
        std::snprintf(info, sizeof(info), "PENDANT_%02u_INFO", key);
        p.writeUInt32(key == 13 ? 0u : 1u); p.writeUInt32(key);
        p.writeString(base); p.writeString(title); p.writeString(info);
        send(p);
    }
    for (uint32_t key : {1u, 6u, 8u}) {
        Packet p = Packet::fromCmdFull(0x011A);
        p.writeUInt32(key); p.writeUInt32(key);
        send(p);
    }
    Packet worn = Packet::fromCmdFull(0x0123);
    worn.writeUInt32(m_pendant);
    send(worn);
    Packet chars = Packet::fromCmdFull(0x001B);
    chars.writeInt32(2);
    ownedCharacter(chars, 5, 1, 2217);
    ownedCharacter(chars, 8, 3, 0);
    send(chars);
    Packet karts = Packet::fromCmdFull(0x001C);
    karts.writeInt32(4);
    ownedKart(karts, 6, 10010, 3201, m_kartDurability);
    ownedKart(karts, 9, 10101, 0, 500);
    ownedKart(karts, 12, 12001, 0, 260);
    ownedKart(karts, 15, 601, 0, 0);
    send(karts);
    // the 0x0108 defs the 0x0109 owned instances and the 0x0107 preset of the factory car
    for (const SampleCraftPart& part : kCraftParts) {
        Packet p = Packet::fromCmdFull(0x0108);
        p.writeUInt32(1); p.writeUInt32(0); p.writeUInt32(part.key); p.writeUInt32(part.category); p.writeUInt32(0);
        p.writeString("Circler"); p.writeString(part.name); p.writeString(part.desc);
        for (int i = 0; i < 0x60; ++i) p.writeUInt8(0);
        priceList(p, part.key);
        send(p);
    }
    for (size_t i = 0; i < 7; ++i) {
        Packet p = Packet::fromCmdFull(0x0109);
        std::array<uint8_t, 0x84> blob{};
        auto put = [&](size_t off, uint32_t v) { for (int b = 0; b < 4; ++b) blob[off + static_cast<size_t>(b)] = static_cast<uint8_t>(v >> (8 * b)); };
        put(0x00, 7101 + static_cast<uint32_t>(i));
        put(0x04, kCraftParts[i].key);
        put(0x08, kCraftParts[i].category);
        put(0x0C, 1);
        put(0x80, static_cast<uint32_t>(kCraftParts[i].grade));
        for (uint8_t b : blob) p.writeUInt8(b);
        send(p);
    }
    {
        // the 0x20 config order is kart cover tires booster bumper front fender rear fender wing
        Packet p = Packet::fromCmdFull(0x0107);
        p.writeInt32(1);
        p.writeUInt32(1); p.writeUInt32(0);
        const char name[12] = {'F', 'a', 'c', 't', 'o', 'r', 'y', ' ', 'C', 'a', 'r', 0};
        for (char c : name) p.writeUInt8(static_cast<uint8_t>(c));
        p.writeUInt32(15);
        const uint32_t slots[7] = {7101, 7103, 7102, 7106, 7104, 7105, 7107};
        for (uint32_t s : slots) p.writeUInt32(s);
        send(p);
    }
    Packet items = Packet::fromCmdFull(0x001D);
    items.writeInt32(3);
    ownedItem(items, 20, 1000, 3);
    ownedItem(items, 21, 3000, 4);
    // a spent bonus gold row the garage shows Delete on it
    ownedItem(items, 22, 4001, 0, 0);
    send(items);
    uint32_t instance = 30;
    for (uint32_t key : {2217u, 2101u, 3101u, 3201u}) {
        Packet p = Packet::fromCmdFull(0x001E);
        ownedPart(p, instance++, key);
        send(p);
    }
    // the 0x0104 owned pets one row not worn so the garage Pet tab can equip it
    Packet pets = Packet::fromCmdFull(0x0104);
    pets.writeInt32(1);
    ownedPet(pets, 50, 1, 0);
    send(pets);
    // the 0x010D owned room craft rows a placed sky and floor and two free objects
    struct SampleOwned { uint32_t instance; uint32_t key; uint32_t category; uint32_t placed; };
    static const SampleOwned kOwnedRooms[] = {
        {701, 1011, 0, 1}, {702, 2001, 1, 1}, {703, 4027, 3, 0}, {704, 4009, 3, 0}, {705, 5005, 4, 0},
    };
    for (const SampleOwned& o : kOwnedRooms) {
        Packet p = Packet::fromCmdFull(0x010D);
        p.writeUInt32(o.instance); p.writeUInt32(o.key); p.writeUInt32(o.category);
        p.writeFloat(0.f); p.writeFloat(0.f); p.writeFloat(0.f); p.writeFloat(0.f);
        p.writeUInt32(o.placed); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(1);
        send(p);
    }
    // the 0x00C5 licence test defs of the rookie tier the boards print their rewards
    for (uint32_t key = 0; key < 4; ++key) {
        Packet p = Packet::fromCmdFull(0x00C5);
        p.writeUInt32(0); p.writeUInt32(key); p.writeString("License_01");
        p.writeUInt32(0); p.writeUInt32(50 + key * 10); p.writeUInt32(100 + key * 50); p.writeUInt32(0);
        p.writeUInt32(7); p.writeUInt32(0); p.writeUInt32(0); p.writeUInt32(0);
        p.writeFloat(10.f); p.writeFloat(3000.f); p.writeFloat(0.f); p.writeFloat(0.f); p.writeFloat(-1.f);
        p.writeString(""); p.writeString("");
        send(p);
    }
    // the 0x00A2 licence rows the stock capture account passed the four rookie tests
    Packet licence = Packet::fromCmdFull(0x00A2);
    licence.writeInt32(4);
    for (uint32_t key = 0; key < 4; ++key) { licence.writeUInt32(key); licence.writeUInt32(1); licence.writeUInt32(0); }
    send(licence);
}

void SampleServer::sendLobby() {
    send(Packet::fromCmdFull(0x0012));
    struct Row { uint32_t id; const char16_t* name; uint32_t count; uint32_t mode; uint32_t locked; };
    const Row rows[] = {
        {3, u"Toy Circuit fun", 3, 0, 0}, {4, u"speed only pros", 6, 2, 0}, {5, u"team item 4v4", 8, 1, 1},
        {6, u"beginners welcome", 1, 0, 0}, {8, u"Snow night race", 2, 2, 0},
    };
    for (const Row& r : rows) {
        Packet p = Packet::fromCmdFull(0x002D);
        p.writeUInt32(r.id); p.writeWString(r.name);
        // channel zero then the playing flag then the time left the 0x002D page order
        p.writeUInt32(r.count); p.writeUInt32(8); p.writeUInt32(r.mode); p.writeUInt32(r.locked);
        p.writeUInt32(0); p.writeUInt32(r.id == 4 ? 1u : 0u); p.writeUInt32(0);
        send(p);
    }
    const char16_t* const chat[][2] = {{u"HlTester", u"hi all, anyone up for a race"}, {u"HlTestTwo", u"sure, Toy Circuit"}};
    for (const auto& line : chat) {
        Packet p = Packet::fromCmdFull(0x00B4);
        p.writeUInt32(kBotA); p.writeWString(line[0]); p.writeWString(line[1]); p.writeUInt32(0);
        send(p);
    }
    sendSocial();
    if (m_state == "creation") send(Packet::fromCmdFull(0x0003));
}

void SampleServer::sendSocial() {
    Packet friends = Packet::fromCmdFull(0x0076);
    friends.writeInt32(2);
    for (int i = 0; i < 2; ++i) {
        std::vector<uint8_t> rec(0x2C, 0);
        putU32(rec, 0, i == 0 ? kBotA : kBotB);
        putWide(rec, 4, i == 0 ? u"HlTester" : u"HlTestTwo", 14);
        putU32(rec, 0x20, i == 0 ? 9 : 4);
        putU32(rec, 0x24, i == 0 ? 0 : static_cast<uint32_t>(-2));
        putU32(rec, 0x28, i == 0 ? 8 : static_cast<uint32_t>(-2));
        friends.writeBytes(rec.data(), rec.size());
    }
    send(friends);
    Packet requests = Packet::fromCmdFull(0x0078);
    requests.writeInt32(1);
    {
        std::vector<uint8_t> rec(0x24, 0);
        putU32(rec, 0, 1004);
        putWide(rec, 4, u"Pumpkin77", 14);
        putU32(rec, 0x20, 6);
        requests.writeBytes(rec.data(), rec.size());
    }
    send(requests);
    for (int i = 0; i < 2; ++i) {
        Packet note = Packet::fromCmdFull(0x0083);
        std::vector<uint8_t> rec(0x18C, 0);
        putU32(rec, 0, 500 + static_cast<uint32_t>(i));
        putWide(rec, 0x04, i == 0 ? u"HlTester" : u"HlTestTwo", 13);
        putWide(rec, 0x1E, u"2026-09-15", 11);
        putWide(rec, 0x34, i == 0 ? u"14:02:11" : u"09:40:03", 9);
        rec[0x46] = i == 1 ? 1 : 0;
        putWide(rec, 0x48, i == 0 ? u"good race yesterday, same time tonight?" : u"welcome to the club", 150);
        note.writeBytes(rec.data(), rec.size());
        send(note);
    }
}

void SampleServer::sendRoom() {
    m_inRoom = true;
    Packet ctx = Packet::fromCmdFull(0x0013);
    ctx.writeUInt32(kRoomId); ctx.writeWString(u"Sample room"); ctx.writeUInt32(8); ctx.writeUInt32(0); ctx.writeUInt32(0);
    ctx.writeUInt32(0); ctx.writeUInt32(0); ctx.writeUInt32(0); ctx.writeUInt32(kMyId); ctx.writeUInt32(0);
    // the decor tail as the package server sends it for the stock capture master placed rows in instance order
    struct SampleDecor { uint32_t instance; uint32_t key; uint32_t category; float x, y, z, yaw; };
    // Floor01 the default floor of every fresh master then the sky and effect that master picked in the editor
    std::vector<SampleDecor> decor = {{702, 2001, 1, 0.f, 0.f, 0.f, 0.f}, {803, 1011, 0, 0.f, 0.f, 0.f, 0.f}, {804, 5005, 4, 0.f, 0.f, 0.f, 0.f}};
    ctx.writeUInt32(static_cast<uint32_t>(decor.size()));
    for (const SampleDecor& d : decor) {
        ctx.writeUInt32(d.instance); ctx.writeUInt32(d.key); ctx.writeUInt32(d.category);
        ctx.writeFloat(d.x); ctx.writeFloat(d.y); ctx.writeFloat(d.z); ctx.writeFloat(d.yaw);
        ctx.writeUInt32(1); ctx.writeUInt32(0); ctx.writeUInt32(0); ctx.writeUInt32(0); ctx.writeUInt32(1);
    }
    send(ctx);
    for (uint32_t slot = 0; slot < 8; ++slot) {
        Packet p = Packet::fromCmdFull(0x0032);
        p.writeUInt32(slot); p.writeUInt32(1);
        send(p);
    }
    struct Member { uint32_t slot; uint32_t id; const char16_t* name; uint8_t level; uint32_t driver; uint32_t kart; uint32_t ready; };
    const Member members[] = {{0, kMyId, u"Sample", 12, 1, 10010, 0}, {1, kBotA, u"HlTester", 9, 2, 10011, 1}, {2, kBotB, u"HlTestTwo", 4, 3, 10101, 0}};
    for (const Member& m : members) {
        Packet p = Packet::fromCmdFull(0x0021);
        p.writeUInt32(m.slot); p.writeUInt32(0); p.writeUInt32(m.id); p.writeWString(m.name);
        // the u32 after the pccafe byte is the worn pendant the room plate draws it
        p.writeUInt8(m.level); p.writeUInt8(0); p.writeUInt8(0); p.writeUInt32(m.id == kMyId ? m_pendant : m.id == kBotA ? 8u : 0u);
        memberBlobs(p, m.driver, m.kart, m.ready);
        send(p);
    }
    Packet track = Packet::fromCmdFull(0x0035);
    track.writeUInt32(sampleTrack()); track.writeUInt32(0);
    send(track);
}

// the launch then one 0x003E per racer then the rank board rebuild
void SampleServer::sendGrid() {
    m_racing = true;
    m_resultSent = false;
    m_goAt = -1.0;
    send(Packet::fromCmdFull(0x0034));
    Packet launch = Packet::fromCmdFull(0x0014);
    launch.writeUInt32(3); launch.writeUInt32(0); launch.writeUInt32(sampleTrack()); launch.writeUInt32(0); launch.writeUInt32(3); launch.writeUInt32(0);
    send(launch);
    // the rows follow a second later the race screen must be up to read them like on the real server
    m_gridAt = nowSeconds() + 1.0;
}

void SampleServer::sendGridRows() {
    struct Racer { uint32_t id; const char16_t* name; uint32_t grid; uint32_t driver; uint32_t kart; };
    const Racer racers[] = {{kMyId, u"Sample", 0, 1, 10010}, {kBotA, u"HlTester", 1, 2, 10011}, {kBotB, u"HlTestTwo", 2, 3, 10101}};
    for (const Racer& r : racers) {
        Packet p = Packet::fromCmdFull(0x003E);
        p.writeUInt32(r.id); p.writeWString(r.name); p.writeUInt32(r.grid); p.writeUInt32(0);
        memberBlobs(p, r.driver, r.kart, 0);
        send(p);
    }
    send(Packet::fromCmdFull(0x000D));
}

void SampleServer::sendResult() {
    m_resultSent = true;
    struct Row { uint32_t id; const char16_t* name; int32_t timeMs; uint32_t gold; uint32_t exp; };
    const Row rows[] = {{kBotA, u"HlTester", 142747, 320, 180}, {kMyId, u"Sample", 143102, 280, 150}, {kBotB, u"HlTestTwo", 151990, 240, 120}};
    Packet finish = Packet::fromCmdFull(0x003C);
    finish.writeUInt32(kMyId); finish.writeUInt32(m_gold + 280); finish.writeUInt8(12); finish.writeUInt32(3600); finish.writeInt32(1);
    send(finish);
    for (int i = 0; i < 3; ++i) {
        Packet rank = Packet::fromCmdFull(0x003D);
        rank.writeUInt32(rows[i].id); rank.writeInt32(i);
        send(rank);
    }
    Packet board = Packet::fromCmdFull(0x0046);
    board.writeUInt32(0); board.writeInt32(3);
    for (int i = 0; i < 3; ++i) {
        board.writeUInt32(static_cast<uint32_t>(i)); board.writeInt32(rows[i].timeMs); board.writeUInt32(rows[i].id);
        board.writeWString(rows[i].name); board.writeUInt8(10); board.writeUInt32(0);
        board.writeUInt32(rows[i].gold); board.writeUInt32(rows[i].exp); board.writeUInt32(i == 0 ? 40 : 0); board.writeUInt32(i == 0 ? 20 : 0);
        board.writeUInt8(0); board.writeUInt32(0); board.writeUInt32(0); board.writeUInt32(0);
    }
    send(board);
    Packet open = Packet::fromCmdFull(0x0042);
    open.writeUInt32(5);
    send(open);
}

// the five stock worlds on the ids 0 to 4 the stage 24 art goes by id
const SampleServer::MissionRow SampleServer::kMissions[5] = {
    {0, 0, 0, 120000, 0, 500, 200, 0, 0, "Mission_01"},
    {1, 1, 5, 240000, 0, 800, 300, 0, 0, "Mission_02"},
    {2, 0, 0, 150000, 10, 1000, 400, 7, 1, "Mission_03"},
    {3, 1, 5, 240000, 0, 1500, 600, 0, 5, "Mission_04"},
    {4, 2, 0, 240000, 20, 2500, 1000, 1, 10015, "Mission_05"},
};

void SampleServer::sendMissionMenu() {
    if (!m_missionDefsSent) {
        m_missionDefsSent = true;
        for (const MissionRow& d : kMissions) {
            std::vector<uint8_t> rec(0xBC, 0);
            char key[32];
            putU32(rec, 0x04, d.id); putU32(rec, 0x08, d.kind); putU32(rec, 0x10, static_cast<uint32_t>(d.goal));
            putU32(rec, 0x14, static_cast<uint32_t>(d.limitMs)); putU32(rec, 0x18, d.fee); putU32(rec, 0x1C, d.gold);
            putU32(rec, 0x20, d.exp); putU32(rec, 0x28, d.itemType); putU32(rec, 0x2C, d.itemKey);
            putAscii(rec, 0x38, d.world, 32);
            std::snprintf(key, sizeof(key), "MISSION_%02u_TITLE", d.id + 1); putAscii(rec, 0x59, key, 32);
            std::snprintf(key, sizeof(key), "MISSION_%02u_INFO", d.id + 1); putAscii(rec, 0x7A, key, 32);
            std::snprintf(key, sizeof(key), "MISSION_%02u_STORY", d.id + 1); putAscii(rec, 0x9B, key, 32);
            Packet p = Packet::fromCmdFull(0x0087);
            p.writeBytes(rec.data(), rec.size());
            send(p);
        }
    }
    // the first row cleared so the second is new and the three after it locked
    Packet progress = Packet::fromCmdFull(0x0088);
    progress.writeInt32(5);
    for (const MissionRow& d : kMissions) { progress.writeUInt32(d.id); progress.writeUInt32(m_missionCleared[d.id] ? 1 : 0); }
    send(progress);
    send(Packet::fromCmdFull(0x008F));
}

void SampleServer::handle(uint16_t opcode, Packet& pkt) {
    switch (opcode) {
    case 0x0007:
        sendProfile(0x0007);
        sendBurst();
        {
            Packet channels = Packet::fromCmdFull(0x000E);
            channels.writeInt32(4);
            const char16_t* const names[4] = {u"Newbie 1", u"Newbie 2", u"Advanced 1", u"Master 1"};
            const uint32_t tiers[4] = {0, 0, 1, 2};
            for (int i = 0; i < 4; ++i) {
                channels.writeUInt32(static_cast<uint32_t>(i + 1)); channels.writeWString(names[i]);
                channels.writeUInt32(12 + static_cast<uint32_t>(i) * 7); channels.writeUInt32(100); channels.writeUInt32(tiers[i]);
            }
            channels.writeWString(u"Sample server, every row here is made up");
            send(channels);
        }
        break;
    case 0x0018: {
        Packet redirect = Packet::fromCmdFull(0x0054);
        redirect.writeUInt32(0); redirect.writeString("127.0.0.1"); redirect.writeUInt32(m_port);
        send(redirect);
        break;
    }
    case 0x00A7:
        sendProfile(0x00A7);
        sendBurst();
        {
            std::vector<uint8_t> refresh(38, 0);
            refresh[0] = 1; refresh[1] = m_state == "licence" ? 2 : 12;
            putU32(refresh, 0x02, 3450); putU32(refresh, 0x06, m_astro); putU32(refresh, 0x0A, m_gold);
            putU32(refresh, 0x0E, 5); putU32(refresh, 0x12, 6); putU32(refresh, 0x1A, 3000); putU32(refresh, 0x1E, 5000);
            Packet p = Packet::fromCmdFull(0x000A);
            p.writeBytes(refresh.data(), refresh.size());
            send(p);
        }
        sendLobby();
        break;
    case 0x0012:
        m_inRoom = false;
        m_racing = false;
        sendLobby();
        break;
    case 0x00B4: {
        const std::u16string text = pkt.readWString(8191);
        Packet echo = Packet::fromCmdFull(0x00B4);
        echo.writeUInt32(kMyId); echo.writeWString(u"Sample"); echo.writeWString(text); echo.writeUInt32(0);
        send(echo);
        break;
    }
    case 0x0016:
        send(Packet::fromCmdFull(0x0016));
        break;
    case 0x000F:
        send(Packet::fromCmdFull(0x000F));
        break;
    case 0x0010:
        send(Packet::fromCmdFull(0x0010));
        break;
    case 0x00B9: {
        const uint32_t category = pkt.readUInt32();
        const uint32_t key = pkt.readUInt32();
        Packet ack = Packet::fromCmdFull(0x00B9);
        ack.writeUInt32(category);
        if (category == 0) ownedCharacter(ack, key == 3 ? 8 : 5, key, key == 1 ? 2217 : 0);
        else if (category == 1) ownedKart(ack, key == 10101 ? 9 : key == 12001 ? 12 : 6, key, 0, 400);
        else if (category == 2) {
            // a repair scroll spends one use and the 0x10 kart period tail lands on the selected kart
            const bool scroll = key == 3000 || key == 3001;
            if (scroll && m_scrollLeft > 0) --m_scrollLeft;
            ownedItem(ack, 21, key, scroll ? m_scrollLeft : 3u);
            if (scroll) {
                m_kartDurability += key == 3000 ? 250 : 500;
                if (m_kartDurability > 500) m_kartDurability = 500;
                ack.writeUInt32(key); ack.writeUInt32(3); ack.writeUInt32(m_kartDurability); ack.writeUInt32(1);
            }
        }
        else if (category == 4) { ack.writeUInt32(1); ownedPet(ack, 50, key, 1); }
        else ownedPart(ack, 30, key);
        send(ack);
        // sub 484D90 the select of a driver or a kart is followed by the equipment set
        if (category == 0 || category == 1) {
            Packet set = Packet::fromCmdFull(0x00BC);
            set.writeUInt32(category == 0 ? (key == 3 ? 8u : 5u) : 5u);
            set.writeUInt32(category == 1 ? (key == 10101 ? 9u : key == 12001 ? 12u : 6u) : 6u);
            set.writeInt32(0);
            ownedCharacter(set, 5, 1, 2217);
            ownedKart(set, 6, 10010, 3201, m_kartDurability);
            send(set);
        }
        break;
    }
    case 0x00BA: {
        const uint32_t category = pkt.readUInt32();
        const uint32_t key = pkt.readUInt32();
        Packet ack = Packet::fromCmdFull(0x00BA);
        ack.writeUInt32(category);
        if (category == 2) ownedItem(ack, 21, key, 3);
        else if (category == 3) ownedPart(ack, 30, key);
        else if (category == 4) ownedPet(ack, 50, key, 0);
        send(ack);
        break;
    }
    case 0x00B8: {
        // the delete ack echoes the category and the base key the client drops the row on it
        const uint32_t category = pkt.readUInt32();
        const uint32_t key = pkt.readUInt32();
        Packet ack = Packet::fromCmdFull(0x00B8);
        ack.writeUInt32(category); ack.writeUInt32(key);
        send(ack);
        break;
    }
    case 0x00B7: {
        const uint32_t category = pkt.readUInt32();
        const uint32_t key = pkt.readUInt32();
        if (m_gold > 500) m_gold -= 500;
        Packet ack = Packet::fromCmdFull(0x00B7);
        ack.writeUInt32(category); ack.writeUInt32(m_gold); ack.writeUInt32(m_astro);
        if (category == 0) ownedCharacter(ack, 40, key, 0);
        else if (category == 1) ownedKart(ack, 41, key, 0, 500);
        else if (category == 2) ownedItem(ack, 42, key, 5);
        else ownedPart(ack, 43, key);
        send(ack);
        break;
    }
    case 0x002D:
    case 0x002F:
        sendRoom();
        break;
    case 0x0035: {
        Packet track = Packet::fromCmdFull(0x0035);
        track.writeUInt32(pkt.readUInt32()); track.writeUInt32(0);
        send(track);
        break;
    }
    case 0x0033: {
        const uint32_t pressed = pkt.readUInt32();
        if (pressed && m_inRoom && !m_racing) sendGrid();
        break;
    }
    case 0x000D:
        if (m_racing) {
            Packet rolls = Packet::fromCmdFull(0x0131);
            for (int i = 0; i < 8; ++i) rolls.writeUInt32(static_cast<uint32_t>(i * 3 % 7));
            send(rolls);
            Packet go = Packet::fromCmdFull(0x003A);
            go.writeUInt32(0);
            send(go);
            m_goAt = nowSeconds();
        }
        break;
    case 0x003B:
        m_racing = false;
        sendRoom();
        break;
    case 0x0004: {
        pkt.readUInt32();
        const std::u16string nick = pkt.readWString(12);
        Packet ack = Packet::fromCmdFull(0x0004);
        ack.writeInt32(0); ack.writeWString(nick);
        ownedCharacter(ack, 5, 1, 0);
        ownedKart(ack, 6, 10010, 0, 500);
        send(ack);
        break;
    }
    case 0x008F:
        sendMissionMenu();
        break;
    case 0x0090: {
        // the fee of a row not cleared yet then the ack no rally path goes with a mission
        const uint32_t id = pkt.readUInt32();
        m_missionRun = id;
        if (id < 5 && !m_missionCleared[id] && m_gold >= kMissions[id].fee) m_gold -= kMissions[id].fee;
        Packet ack = Packet::fromCmdFull(0x0090);
        ack.writeUInt32(id); ack.writeUInt32(m_gold);
        send(ack);
        break;
    }
    case 0x008C: {
        // one answer per run the repeats of state 3000 get nothing the reward on a first clear
        const uint32_t id = pkt.readUInt32();
        if (id != m_missionRun || id >= 5) break;
        m_missionRun = 99;
        const MissionRow& d = kMissions[id];
        const bool first = !m_missionCleared[id];
        m_missionCleared[id] = true;
        if (first) m_gold += d.gold;
        const bool item = first && d.itemKey != 0 && (d.itemType == 1 || d.itemType == 7);
        Packet done = Packet::fromCmdFull(0x008C);
        done.writeUInt32(item ? 1 : 0); done.writeUInt32(id); done.writeUInt32(0);
        done.writeUInt32(m_gold); done.writeUInt32(3450 + (first ? d.exp : 0));
        if (item && d.itemType == 1) ownedKart(done, 40, d.itemKey, 0, 500);
        if (item && d.itemType == 7) { done.writeUInt32(d.itemKey); done.writeUInt32(d.itemKey); }
        send(done);
        break;
    }
    case 0x0123: {
        // the worn key back an unknown key keeps the old one minus one takes it off
        const int32_t key = pkt.readInt32();
        if (key <= 0) m_pendant = 0;
        else if (key == 1 || key == 6 || key == 8) m_pendant = static_cast<uint32_t>(key);
        Packet ack = Packet::fromCmdFull(0x0123);
        ack.writeUInt32(m_pendant);
        send(ack);
        break;
    }
    case 0x0132: {
        // one page of the channel user list the own name first then the two bots
        Packet page = Packet::fromCmdFull(0x0132);
        page.writeUInt32(0); page.writeUInt32(1); page.writeUInt32(0);
        const uint32_t ids[3] = {kMyId, kBotA, kBotB};
        const char16_t* const names[3] = {u"Sample", u"HlTester", u"HlTestTwo"};
        const uint32_t levels[3] = {13, 9, 4};
        for (int i = 0; i < 7; ++i) {
            std::vector<uint8_t> rec(0x28, 0);
            if (i < 3) {
                putU32(rec, 0, ids[i]);
                putWide(rec, 4, names[i], 13);
                putU32(rec, 0x20, levels[i]);
                putU32(rec, 0x24, i == 1 ? 3u : 0u);
            }
            page.writeBytes(rec.data(), rec.size());
        }
        page.writeUInt32(3);
        send(page);
        break;
    }
    case 0x0073: {
        Packet status = Packet::fromCmdFull(0x0073);
        status.writeInt32(1); status.writeUInt32(kBotA); status.writeInt32(0); status.writeInt32(8);
        send(status);
        break;
    }
    case 0x0070:
    case 0x0071: {
        const uint32_t id = pkt.readUInt32();
        Packet ack = Packet::fromCmdFull(opcode);
        ack.writeUInt32(0); ack.writeUInt32(id);
        send(ack);
        break;
    }
    // an invite out of a room gets the refusal line our server answers on S2C 0x0126
    case 0x006C: {
        const std::u16string name = pkt.readWString(13);
        if (m_inRoom) break;
        Packet line = Packet::fromCmdFull(0x0126);
        line.writeUInt32(0); line.writeWString(name); line.writeString("MSG_UNSUPPORT"); line.writeUInt32(5);
        send(line);
        break;
    }
    case 0x0064: {
        const uint32_t value = pkt.readUInt32();
        if (m_inRoom) {
            Packet team = Packet::fromCmdFull(0x0064);
            team.writeUInt32(kMyId); team.writeUInt32(value);
            send(team);
        }
        break;
    }
    default:
        break;
    }
}

// the standings every half second after the GO and the board ten seconds later in the result state
void SampleServer::tick() {
    if (!m_racing) return;
    const double now = nowSeconds();
    if (m_gridAt >= 0.0 && now >= m_gridAt) {
        m_gridAt = -1.0;
        sendGridRows();
    }
    if (m_goAt < 0.0) return;
    if (now - m_clock < 0.5) return;
    m_clock = now;
    const uint32_t ids[3] = {kBotA, kMyId, kBotB};
    const int pings[3] = {48, 12, 230};
    for (int i = 0; i < 3; ++i) {
        Packet p = Packet::fromCmdFull(0x0045);
        p.writeUInt32(ids[i]); p.writeInt32(i); p.writeInt32(pings[i]);
        send(p);
    }
    // a bump hit on the own car after the green light the damage clip and the crash sprite
    const double sinceGo = now - m_goAt;
    if (sinceGo >= 13.5 && sinceGo < 14.0) {
        Packet hit = Packet::fromCmdFull(0x0069);
        hit.writeUInt32(kMyId); hit.writeInt16(1000);
        send(hit);
    }
    if (m_state == "result" && !m_resultSent && now - m_goAt > 13.0) sendResult();
}

}
