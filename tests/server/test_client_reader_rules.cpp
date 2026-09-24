/// the client reader rules of every catalogue car craft room craft and pendant record on recorded bursts

#include <gtest/gtest.h>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "net/Packet.h"
#include "packets/PacketBuilder.h"
#include "packets/gen/CustomCarPackets.h"
#include "packets/gen/RoomCraftPackets.h"

using namespace knc;

namespace {

struct Frame {
    uint16_t opcode = 0;
    std::vector<uint8_t> payload;
};

// KNCB then the frame count then op size payload records the server data and the test data files
std::vector<Frame> loadKncb(const std::string& relative) {
    std::vector<Frame> out;
    std::ifstream file(std::string(KNC_REPO_ROOT) + "/" + relative, std::ios::binary);
    if (!file) return out;
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (raw.size() < 8 || std::memcmp(raw.data(), "KNCB", 4) != 0) return out;
    size_t off = 8;
    while (off + 4 <= raw.size()) {
        Frame f;
        f.opcode = static_cast<uint16_t>(raw[off] | (raw[off + 1] << 8));
        const uint16_t size = static_cast<uint16_t>(raw[off + 2] | (raw[off + 3] << 8));
        if (off + 4 + size > raw.size()) break;
        f.payload.assign(raw.begin() + off + 4, raw.begin() + off + 4 + size);
        out.push_back(std::move(f));
        off += 4 + size;
    }
    return out;
}

// a bounded cursor like the client readers a short frame sets bad and reads zero
struct Cursor {
    const std::vector<uint8_t>& p;
    size_t at = 0;
    bool bad = false;
    explicit Cursor(const std::vector<uint8_t>& b) : p(b) {}
    uint32_t u32() {
        if (at + 4 > p.size()) { bad = true; at = p.size(); return 0; }
        uint32_t v = 0;
        std::memcpy(&v, p.data() + at, 4);
        at += 4;
        return v;
    }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    uint8_t u8() {
        if (at + 1 > p.size()) { bad = true; return 0; }
        return p[at++];
    }
    std::string cstr() {
        size_t e = at;
        while (e < p.size() && p[e] != 0) ++e;
        if (e >= p.size()) { bad = true; std::string s(p.begin() + at, p.end()); at = p.size(); return s; }
        std::string s(p.begin() + at, p.begin() + e);
        at = e + 1;
        return s;
    }
    void skip(size_t n) {
        if (at + n > p.size()) { bad = true; at = p.size(); return; }
        at += n;
    }
    size_t left() const { return p.size() - at; }
};

struct Option {
    uint32_t key = 0;
    uint32_t unit = 0;
    uint32_t amount = 0;
};

std::vector<Option> options(Cursor& c) {
    std::vector<Option> out;
    const uint32_t n = c.u32();
    for (uint32_t i = 0; i < n && !c.bad; ++i) {
        Option o;
        o.key = c.u32();
        o.unit = c.u32();
        o.amount = c.u32();
        c.skip(4);
        out.push_back(o);
    }
    return out;
}

// one catalogue row the fields the client dereferences
struct Row {
    uint16_t op = 0;
    uint32_t visible = 0;
    uint32_t key = 0;
    uint32_t category = 0;
    uint32_t scheme = 0;
    uint32_t maxPlaceable = 1;
    std::vector<std::string> strings;
    std::vector<int32_t> keysOut;
    std::vector<int32_t> abilityIds;
    std::vector<Option> opts;
    size_t left = 0;
    bool bad = false;
};

// the string slots of every record the client copies into with no bound
const std::map<uint16_t, std::vector<size_t>>& stringSlots() {
    static const std::map<uint16_t, std::vector<size_t>> slots = {
        {0x00BF, {36, 33, 35}}, {0x00C0, {33, 33, 34}}, {0x00C1, {33, 33, 34}},
        {0x00C2, {36, 33, 35}}, {0x0103, {33, 33, 34}}, {0x0108, {33, 33, 34}},
        {0x010C, {33, 33, 34}}, {0x0119, {33, 33, 34}},
    };
    return slots;
}

bool decodeRow(const Frame& f, Row& r) {
    Cursor c(f.payload);
    r.op = f.opcode;
    switch (f.opcode) {
        case 0x00BF:
            r.visible = c.u32(); c.u32(); c.u32(); r.key = c.u32(); c.u32();
            r.strings.push_back(c.cstr());
            for (int i = 0; i < 5; ++i) r.keysOut.push_back(c.i32());
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            r.opts = options(c);
            break;
        case 0x00C0:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); c.u8(); c.u32(); r.scheme = c.u32(); c.u32(); c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            for (int i = 0; i < 8; ++i) r.keysOut.push_back(c.i32());
            c.skip(0x44);
            for (int i = 0; i < 2; ++i) { r.abilityIds.push_back(c.i32()); c.u32(); }
            r.opts = options(c);
            break;
        case 0x00C1:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); c.u32(); c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            r.opts = options(c);
            break;
        case 0x00C2:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); c.u32();
            r.strings.push_back(c.cstr());
            c.u32(); r.category = c.u32(); c.i32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            for (int i = 0; i < 2; ++i) { r.abilityIds.push_back(c.i32()); c.u32(); }
            r.opts = options(c);
            break;
        case 0x0103:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            r.opts = options(c);
            break;
        case 0x0108:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); r.category = c.u32(); c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            c.skip(0x44);
            for (int i = 0; i < 2; ++i) { r.abilityIds.push_back(c.i32()); c.u32(); }
            c.skip(0x0C);
            r.opts = options(c);
            break;
        case 0x010C:
            r.visible = c.u32(); c.u32(); r.key = c.u32(); r.category = c.u32(); r.maxPlaceable = c.u32(); c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            r.opts = options(c);
            break;
        case 0x0119:
            r.visible = c.u32(); r.key = c.u32();
            r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr()); r.strings.push_back(c.cstr());
            break;
        default:
            return false;
    }
    r.left = c.left();
    r.bad = c.bad;
    return true;
}

// the five chassis folders under Car FactoryCar the only model names a factory part or kart can carry
bool isChassis(std::string s) {
    for (auto& ch : s) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s == "circler" || s == "firedragon" || s == "quatzalcuatl" || s == "squarer" || s == "striper";
}

// sub 432B20 widens a built slot name into WCHAR 10 with cch 20 and sub 455C20 wcscpy into 10 WCHAR
constexpr size_t kPresetNameMax = 9;

// the chars before the first NUL of a preset name cell or of a whole cstr
size_t presetNameLength(const uint8_t* p, size_t cell) {
    size_t n = 0;
    while (n < cell && p[n] != 0) ++n;
    return n;
}

// the problems of one recorded stream split by what the client does with them
struct Report {
    // a null deref a frame smash or a desync of the reader
    std::vector<std::string> crash;
    // a row the client drops or never draws
    std::vector<std::string> dropped;
    // rule name to count
    std::map<std::string, size_t> cosmetic;
};

void addCosmetic(Report& r, const std::string& rule) { ++r.cosmetic[rule]; }

Report check(const std::vector<Frame>& frames) {
    Report rep;
    std::map<uint16_t, std::vector<Row>> cat;
    std::set<uint32_t> prices;
    size_t priceRows = 0;
    std::map<uint32_t, std::array<uint32_t, 14>> ownedKarts;
    std::map<uint32_t, std::array<uint32_t, 5>> partInstances;
    std::vector<std::vector<std::array<uint32_t, 13>>> presetLists;
    std::vector<std::array<uint32_t, 12>> rooms;
    std::set<uint32_t> roomIds;
    std::vector<std::pair<uint32_t, uint32_t>> pendantsOwned;
    std::vector<std::string> sizeIssues;

    for (const Frame& f : frames) {
        const std::vector<uint8_t>& p = f.payload;
        if (f.opcode == 0x00C6) {
            if (p.size() != 28) rep.crash.push_back("0x00C6 row is not 28 bytes");
            Cursor c(p);
            prices.insert(c.u32());
            ++priceRows;
            continue;
        }
        if (stringSlots().count(f.opcode)) {
            Row r;
            decodeRow(f, r);
            if (r.bad || r.left != 0) rep.crash.push_back("opcode " + std::to_string(f.opcode) + " key " +
                                                      std::to_string(r.key) + " frame length does not match its reader");
            const std::vector<size_t>& slots = stringSlots().at(f.opcode);
            for (size_t i = 0; i < r.strings.size() && i < slots.size(); ++i) {
                if (r.strings[i].size() + 1 > slots[i]) {
                    rep.crash.push_back("opcode " + std::to_string(f.opcode) + " key " + std::to_string(r.key) +
                                        " string " + std::to_string(i) + " over its " + std::to_string(slots[i]) + " byte slot");
                }
            }
            cat[f.opcode].push_back(r);
            continue;
        }
        Cursor c(p);
        switch (f.opcode) {
            case 0x001C: {
                const int32_t n = c.i32();
                if (n > 64) rep.dropped.push_back("0x001C over its cap of 64");
                for (int32_t i = 0; i < n && !c.bad; ++i) {
                    std::array<uint32_t, 14> k{};
                    for (auto& v : k) v = c.u32();
                    ownedKarts[k[0]] = k;
                }
                break;
            }
            case 0x0109: {
                if (p.size() != 0x84) { rep.crash.push_back("0x0109 is not 132 bytes"); break; }
                std::array<uint32_t, 5> inst{};
                for (int i = 0; i < 4; ++i) inst[static_cast<size_t>(i)] = c.u32();
                std::memcpy(&inst[4], p.data() + 0x80, 4);
                if (partInstances.count(inst[0])) addCosmetic(rep, "0x0109 duplicate instance");
                partInstances[inst[0]] = inst;
                break;
            }
            case 0x0107: {
                const int32_t n = c.i32();
                std::vector<std::array<uint32_t, 13>> rows;
                for (int32_t i = 0; i < n && !c.bad; ++i) {
                    std::array<uint32_t, 13> row{};
                    for (auto& v : row) v = c.u32();
                    rows.push_back(row);
                }
                if (c.left() != 0 || c.bad) rep.crash.push_back("0x0107 length does not match its count");
                presetLists.push_back(rows);
                break;
            }
            case 0x0124: {
                if (p.size() != 0x34) { rep.crash.push_back("0x0124 is not 52 bytes"); break; }
                if (presetNameLength(p.data() + 8, 12) > kPresetNameMax) {
                    rep.crash.push_back("0x0124 name over 9 chars sub 432B20 overruns its stack cell");
                }
                break;
            }
            case 0x0114: {
                c.u32();
                const std::string name = c.cstr();
                // sub 47E620 copies with no cap into the 12 byte cell then the rename click widens it
                if (c.bad || name.size() > kPresetNameMax) {
                    rep.crash.push_back("0x0114 name over 9 chars sub 432B20 overruns its stack cell");
                }
                break;
            }
            case 0x010D: {
                if (p.size() != 0x30) { rep.crash.push_back("0x010D is not 48 bytes"); break; }
                std::array<uint32_t, 12> row{};
                for (auto& v : row) v = c.u32();
                rooms.push_back(row);
                roomIds.insert(row[0]);
                break;
            }
            case 0x010F: {
                const int32_t n = c.i32();
                if (p.size() != 4 + 0x30 * static_cast<size_t>(n < 0 ? 0 : n)) rep.crash.push_back("0x010F length");
                for (int32_t i = 0; i < n && !c.bad; ++i) {
                    const uint32_t id = c.u32();
                    c.skip(0x2C);
                    // sub 47DA00 stores through the lookup result with no null test
                    if (!roomIds.count(id)) rep.crash.push_back("0x010F echoes instance " + std::to_string(id) + " the client does not hold");
                }
                break;
            }
            case 0x010E:
                if (!p.empty()) rep.crash.push_back("0x010E carries a payload the reader never reads");
                break;
            case 0x011A:
            case 0x011B:
                if (p.size() != 8) { rep.crash.push_back("0x011A is not 8 bytes"); break; }
                pendantsOwned.push_back({c.u32(), c.u32()});
                break;
            default:
                break;
        }
    }

    // caps the client drops every row past these
    const std::map<uint16_t, size_t> caps = {
        {0x00BF, 32}, {0x00C0, 64}, {0x00C1, 64}, {0x00C2, 512}, {0x0103, 32}, {0x0108, 256},
        {0x010C, 256}, {0x0119, 64}};
    if (priceRows > 1536) rep.dropped.push_back("0x00C6 over its cap of 1536");
    for (const auto& kv : caps) {
        if (cat[kv.first].size() > kv.second) rep.dropped.push_back("opcode " + std::to_string(kv.first) + " over its cap");
        std::set<uint32_t> seen;
        for (const Row& r : cat[kv.first]) {
            if (!seen.insert(r.key).second) addCosmetic(rep, "duplicate key opcode " + std::to_string(kv.first));
        }
    }

    // price options every tile draw derefs option zero and a sold row needs its 0x00C6 row
    for (uint16_t op : {uint16_t(0x00BF), uint16_t(0x00C0), uint16_t(0x00C1), uint16_t(0x00C2), uint16_t(0x0103),
                        uint16_t(0x0108), uint16_t(0x010C)}) {
        for (const Row& r : cat[op]) {
            if (r.opts.empty()) { rep.crash.push_back("opcode " + std::to_string(op) + " key " + std::to_string(r.key) + " has no price option"); continue; }
            if (r.opts.size() > 4) addCosmetic(rep, "more than four options");
            if (r.visible != 0 && !prices.count(r.opts[0].key)) addCosmetic(rep, "sold row with no 0x00C6 row");
        }
    }

    std::map<uint32_t, const Row*> karts, parts, items, pets, craft, roomDefs, pendantDefs;
    std::set<uint32_t> drivers;
    for (const Row& r : cat[0x00C0]) karts[r.key] = &r;
    for (const Row& r : cat[0x00C2]) parts[r.key] = &r;
    for (const Row& r : cat[0x00C1]) items[r.key] = &r;
    for (const Row& r : cat[0x0103]) pets[r.key] = &r;
    for (const Row& r : cat[0x0108]) craft[r.key] = &r;
    for (const Row& r : cat[0x010C]) roomDefs[r.key] = &r;
    for (const Row& r : cat[0x0119]) pendantDefs[r.key] = &r;
    for (const Row& r : cat[0x00BF]) drivers.insert(r.key);

    for (const Row& r : cat[0x00BF]) {
        for (int32_t k : r.keysOut) {
            if (k > 0 && !parts.count(static_cast<uint32_t>(k))) rep.dropped.push_back("0xBF costume key not in 0xC2");
        }
    }
    for (const Row& r : cat[0x00C0]) {
        if (r.scheme == 1 && !isChassis(r.strings[0])) rep.crash.push_back("0xC0 factory kart model is no chassis folder");
        // 0x4A5ED0 returns at 0x4A5FD1 when a catalogue kart paint misses 0xC2 and the car never builds
        if (r.scheme == 0 && !parts.count(static_cast<uint32_t>(r.keysOut[0]))) rep.dropped.push_back("0xC0 kart paint not in 0xC2");
        for (int32_t id : r.abilityIds) if (id >= 0 && id < 26) addCosmetic(rep, "0xC0 ability pair drawn");
    }
    for (const Row& r : cat[0x00C2]) {
        for (int32_t id : r.abilityIds) if (id >= 0 && id < 26) addCosmetic(rep, "0xC2 ability pair drawn");
    }
    for (const Row& r : cat[0x0108]) {
        if (r.category > 6) rep.crash.push_back("0x0108 category out of 0 to 6");
        if (!isChassis(r.strings[0])) rep.crash.push_back("0x0108 model is no chassis folder");
        // carcraft part ability pairs draw 0x42B7A0 prints id 0 to 25 as a percent icon
        for (int32_t id : r.abilityIds) if (id >= 0 && id < 26) addCosmetic(rep, "0x0108 ability pair drawn");
    }
    for (const Row& r : cat[0x010C]) {
        if (r.category > 4) rep.crash.push_back("0x010C category out of 0 to 4");
        if (r.maxPlaceable == 0) rep.crash.push_back("0x010C max placeable 0 breaks every placement");
        if (r.category == 3 && r.visible != 0 && (r.key < 4001 || r.key > 4032)) addCosmetic(rep, "prop key sub 488300 cannot render");
    }

    for (const auto& kv : ownedKarts) {
        auto k = karts.find(kv.second[1]);
        if (k == karts.end()) { rep.crash.push_back("0x001C kart base key not in 0xC0"); continue; }
        if (k->second->scheme == 0 && !parts.count(kv.second[2])) rep.dropped.push_back("0x001C paint not in 0xC2");
    }
    for (const auto& kv : partInstances) {
        auto d = craft.find(kv.second[1]);
        // 0x430420 hands the 0x0108 lookup result to sprintf with no null test
        if (d == craft.end()) { rep.crash.push_back("0x0109 part key not in 0x0108"); continue; }
        if (d->second->category != kv.second[2]) rep.dropped.push_back("0x0109 category differs from its 0x0108 row");
    }
    // preset slot order cover tires booster bumper front fender rear fender wing
    static const uint32_t kSlotCategory[7] = {0, 2, 1, 5, 3, 4, 6};
    for (const auto& rows : presetLists) {
        // stage 18 derefs the first preset row in two draw paths
        if (rows.empty()) rep.crash.push_back("0x0107 count 0");
        if (rows.size() > 5) rep.dropped.push_back("0x0107 over its cap of 5");
        for (const auto& row : rows) {
            char name[12];
            std::memcpy(name, &row[2], 12);
            if (std::memchr(name, 0, 12) == nullptr) rep.crash.push_back("0x0107 name has no NUL in its 12 bytes");
            // the name plate click 0x432B20 MultiByteToWideChar cch 20 into WCHAR 10 the cookie sits right after
            if (presetNameLength(reinterpret_cast<const uint8_t*>(name), 12) > kPresetNameMax) {
                rep.crash.push_back("0x0107 name over 9 chars sub 432B20 overruns its stack cell");
            }
            const bool built = row[1] == 1;
            if (!built) {
                // the real server opens the factory on an empty slot with no kart and no part
                bool any = row[5] != 0;
                for (int s = 0; s < 7; ++s) any = any || row[static_cast<size_t>(6 + s)] != 0;
                if (any) addCosmetic(rep, "0x0107 empty slot carries a kart or a part");
                continue;
            }
            auto k = ownedKarts.find(row[5]);
            if (k == ownedKarts.end()) rep.dropped.push_back("0x0107 kart not in 0x001C");
            else if (!karts.count(k->second[1])) rep.crash.push_back("0x0107 kart base not in 0xC0 0x430420 derefs it");
            // 0x430420 builds Car FactoryCar CHASSIS model body a catalogue kart has no such nif so no car shows
            else if (karts[k->second[1]]->scheme != 1) rep.dropped.push_back("0x0107 built slot kart is no factory chassis");
            for (int s = 0; s < 7; ++s) {
                const uint32_t inst = row[static_cast<size_t>(6 + s)];
                if (inst == 0) continue;
                auto pi = partInstances.find(inst);
                if (pi == partInstances.end()) { rep.crash.push_back("0x0107 slot names a part instance not in 0x0109"); continue; }
                if (pi->second[2] != kSlotCategory[s]) rep.dropped.push_back("0x0107 slot holds a part of another category");
            }
            if (row[7] == 0) addCosmetic(rep, "0x0107 preset with no tire cannot be saved");
        }
    }
    // 0x416BB0 and 0x472290 list only built slots so a chassis with no slot never shows in the garage
    if (!presetLists.empty()) {
        const auto& last = presetLists.back();
        std::map<uint32_t, uint32_t> held;
        for (const auto& row : last) {
            if (row[1] != 1) continue;
            for (int s = 0; s < 7; ++s) {
                if (row[static_cast<size_t>(6 + s)] != 0) ++held[row[static_cast<size_t>(6 + s)]];
            }
        }
        for (const auto& kv : ownedKarts) {
            auto k = karts.find(kv.second[1]);
            if (k == karts.end() || k->second->scheme != 1) continue;
            bool built = false;
            for (const auto& row : last) built = built || (row[1] == 1 && row[5] == kv.first);
            if (!built) rep.dropped.push_back("0x001C chassis has no built 0x0107 slot the garage never lists it");
            // 0x42AD20 draws the wrench bar only on period mode 3
            if (kv.second[11] != 3) addCosmetic(rep, "0x001C chassis not on the durability bar");
        }
        for (const auto& kv : partInstances) {
            if (kv.second[3] != held[kv.first]) addCosmetic(rep, "0x0109 equip count is not the built slots holding it");
        }
    }
    for (const auto& kv : ownedKarts) {
        auto k = karts.find(kv.second[1]);
        if (k == karts.end() || kv.second[11] != 3) continue;
        // 0x42AD20 divides the durability by the amount of option zero and draws 93 columns per unit ratio
        const Row& def = *k->second;
        if (def.opts.empty() || def.opts[0].amount < kv.second[12]) {
            addCosmetic(rep, "0x001C mode 3 kart over the 0xC0 option zero amount the wrench bar runs past its box");
        }
    }
    for (const auto& row : rooms) {
        const uint32_t key = row[1], category = row[2], active = row[11];
        if (active == 1 && category != 3) {
            auto d = roomDefs.find(key);
            // sub 488300 hands the 0x010C lookup result plus 0x18 to sprintf with no null test
            if (d == roomDefs.end()) { rep.crash.push_back("0x010D object key not in 0x010C"); continue; }
            if (d->second->category != category) rep.dropped.push_back("0x010D category differs from its 0x010C row");
        }
        if (category == 3 && (key < 4001 || key > 4032)) addCosmetic(rep, "owned prop sub 488300 cannot render");
    }
    bool hiddenSeen = false;
    for (const Row& r : cat[0x0119]) {
        if (r.visible == 0) hiddenSeen = true;
        // sub 46EDB0 counts hidden rows in its hit test so a hidden row must come last
        else if (hiddenSeen) addCosmetic(rep, "visible pendant after a hidden one");
    }
    std::set<uint32_t> pendantInstances;
    for (const auto& pr : pendantsOwned) {
        if (!pendantDefs.count(pr.second)) rep.dropped.push_back("0x011A pendant key not in 0x0119");
        if (!pendantInstances.insert(pr.first).second) addCosmetic(rep, "0x011A instance shared by two rows");
    }
    return rep;
}

std::string joined(const std::vector<std::string>& v) {
    std::string s;
    for (const auto& x : v) s += x + "\n";
    return s;
}

}  // namespace

// the real server passes every rule so the rules read the client right

TEST(ClientReaderRules, TheReferenceBurstIsClean) {
    const std::vector<Frame> frames = loadKncb("server/data/reference_login_burst.bin");
    ASSERT_EQ(frames.size(), 733u);
    const Report rep = check(frames);
    EXPECT_TRUE(rep.crash.empty()) << joined(rep.crash);
    EXPECT_TRUE(rep.dropped.empty()) << joined(rep.dropped);
    EXPECT_TRUE(rep.cosmetic.empty()) << rep.cosmetic.size() << " cosmetic rules hit";
}

// the package image of 2026-09-23 recorded by the car craft and room craft gate runs

// both 0x0107 of the run carry the 11 char Factory Car on a catalogue kart the name plate overruns
TEST(ClientReaderRules, ThePackageCarCraftRunCarriesTheNamePlateOverrun) {
    const std::vector<Frame> frames = loadKncb("tests/server/data/package_carcraft_rx.bin");
    ASSERT_EQ(frames.size(), 867u);
    const Report rep = check(frames);
    ASSERT_EQ(rep.crash.size(), 2u) << joined(rep.crash);
    for (const auto& c : rep.crash) EXPECT_EQ(c, "0x0107 name over 9 chars sub 432B20 overruns its stack cell");
    ASSERT_EQ(rep.dropped.size(), 2u) << joined(rep.dropped);
    for (const auto& d : rep.dropped) EXPECT_EQ(d, "0x0107 built slot kart is no factory chassis");
    // the image ships car craft twin rows in 0xC2 whose keys 2000 to 2004 shadow five head parts
    EXPECT_EQ(rep.cosmetic.at("duplicate key opcode 194"), 5u);
    // the 35 twins and the slot changer 1000 claim to sell on option key 0
    EXPECT_EQ(rep.cosmetic.at("sold row with no 0x00C6 row"), 36u);
    // the image writes 0 0 0 0 in the 0x0108 ability block two 0% icons on 35 parts
    EXPECT_EQ(rep.cosmetic.at("0x0108 ability pair drawn"), 70u);
    EXPECT_EQ(rep.cosmetic.size(), 3u);
}

TEST(ClientReaderRules, ThePackageRoomCraftRunHasNoRoomCrashClassRecord) {
    const std::vector<Frame> frames = loadKncb("tests/server/data/package_roomcraft_rx.bin");
    ASSERT_EQ(frames.size(), 867u);
    const Report rep = check(frames);
    // the one car craft snapshot of the run carries the same name plate overrun
    ASSERT_EQ(rep.crash.size(), 1u) << joined(rep.crash);
    EXPECT_EQ(rep.crash[0], "0x0107 name over 9 chars sub 432B20 overruns its stack cell");
    ASSERT_EQ(rep.dropped.size(), 1u) << joined(rep.dropped);
    EXPECT_EQ(rep.dropped[0], "0x0107 built slot kart is no factory chassis");
    // the room frames of the run the buy the stage push and the save ack all pass
    size_t acks = 0;
    for (const Frame& f : frames) if (f.opcode == 0x010F) ++acks;
    EXPECT_EQ(acks, 1u);
}

// the builders of this change

// 0x0108 ships minus one ability ids so 0x42B7A0 draws no 0% icon and the tile has its three options
TEST(ClientReaderRules, CarCraftPartDefinitionOfTheBuilder) {
    CarPartDef def;
    def.partKey = 3000;
    def.category = 2;
    def.modelDirName = "Firedragon";
    def.displayNameKey = "TIRES_3000_TITLE";
    def.descriptionKey = "TIRES_3000_INFO";
    def.priceRows.push_back({2007, 1, 1, 0});
    def.priceRows.push_back({3007, 1, 7, 0});
    def.priceRows.push_back({1007, 0, 0, 0});
    Frame f;
    f.opcode = 0x0108;
    f.payload = CustomCarPackets::partDef(def).payload();
    Row r;
    ASSERT_TRUE(decodeRow(f, r));
    EXPECT_FALSE(r.bad);
    EXPECT_EQ(r.left, 0u);
    ASSERT_EQ(r.abilityIds.size(), 2u);
    EXPECT_EQ(r.abilityIds[0], -1);
    EXPECT_EQ(r.abilityIds[1], -1);
    ASSERT_EQ(r.opts.size(), 3u);
    EXPECT_EQ(r.opts[0].key, 2007u);

    // a whole stage 18 stream of the builders passes every rule
    std::vector<Frame> stream;
    auto add = [&stream](const Packet& p) { stream.push_back({p.opcode(), p.payload()}); };
    for (uint32_t key : {1007u, 2007u, 3007u}) {
        Packet price(0x00C6);
        price.writeUInt32(key);
        for (int i = 0; i < 6; ++i) price.writeUInt32(0);
        add(price);
    }
    add(PacketBuilder::partCatalog(9007, 0, "RED", "Red Paint", "Red Paint", 0, {2007}, -1, true, true));
    add(PacketBuilder::partCatalog(9100, 1, "NAMEBOX_NORMAL", "Standard Plate", "Standard Plate", 0, {}, -1, true, false));
    std::array<int32_t, 8> skins{9007, 9100, 0, 0, 0, 0, 0, 0};
    std::array<float, 17> stats{};
    add(PacketBuilder::vehicleCatalog(12005, "Firedragon", skins, {1007}, true,
                                      "CAR_CHASSIS_01_TITLE", "CAR_CHASSIS_01_INFO", stats, {},
                                      CustomCarPackets::FACTORY_START_DURABILITY));
    add(CustomCarPackets::partDef(def));
    CarPartInstance inst;
    inst.instanceId = 41;
    inst.partKey = 3000;
    inst.category = 2;
    inst.equipRefcount = 1;
    inst.grade = 7;
    add(CustomCarPackets::partInstance(inst));
    Packet owned = Packet(0x001C);
    owned.writeInt32(1);
    const uint32_t kartRow[14] = {9, 12005, 9007, 9100, 0, 0, 0, 0, 0, 0, 0, 3, 500, 1};
    for (uint32_t v : kartRow) owned.writeUInt32(v);
    add(owned);
    // a built car with the old default name the builder cuts it to the 9 char cell
    CarPreset built;
    built.presetId = 3;
    built.slotState = CustomCarPackets::SLOT_BUILT;
    built.name = "Factory Car";
    built.kartInstanceId = 9;
    built.partTire = 41;
    add(CustomCarPackets::presetList({}));
    add(CustomCarPackets::presetList({built}));
    add(CustomCarPackets::presetRowUpdate(built));
    add(CustomCarPackets::presetRenameAck(3, "Factory Car"));
    const Report rep = check(stream);
    EXPECT_TRUE(rep.crash.empty()) << joined(rep.crash);
    EXPECT_TRUE(rep.dropped.empty()) << joined(rep.dropped);
    EXPECT_TRUE(rep.cosmetic.empty()) << rep.cosmetic.size() << " cosmetic rules hit";
}

// an empty preset set ships one empty slot row since a count 0 crashes stage 18
TEST(ClientReaderRules, PresetListNeverShipsCountZero) {
    const Packet p = CustomCarPackets::presetList({});
    Cursor c(p.payload());
    EXPECT_EQ(c.i32(), 1);
    EXPECT_EQ(p.payload().size(), 4u + 0x34u);
    c.u32();
    EXPECT_EQ(c.u32(), CustomCarPackets::SLOT_EMPTY);
    for (int i = 0; i < 3; ++i) EXPECT_EQ(c.u32(), 0u);
    for (int i = 0; i < 8; ++i) EXPECT_EQ(c.u32(), 0u);
}

// the 0x0107 the package server sent dock2 on 0x010A 2026-09-23 the stock client died on the name plate
const uint8_t kPackageFactoryOpen[56] = {
    0x01, 0x00, 0x00, 0x00, 0x1B, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x46, 0x61, 0x63, 0x74,
    0x6F, 0x72, 0x79, 0x20, 0x43, 0x61, 0x72, 0x00, 0x22, 0x00, 0x00, 0x00, 0x12, 0x00, 0x00, 0x00,
    0x14, 0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x17, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
    0x16, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00};

bool hasNameOverrun(const Report& rep) {
    for (const auto& c : rep.crash) {
        if (c.find("name over 9 chars") != std::string::npos) return true;
    }
    return false;
}

// the exact failing bytes hit the name plate rule and the same slot through the fixed builder does not
TEST(ClientReaderRules, TheFactoryCarNameOverrunsTheNamePlateClick) {
    Frame bad{0x0107, std::vector<uint8_t>(kPackageFactoryOpen, kPackageFactoryOpen + 56)};
    const Report before = check({bad});
    EXPECT_TRUE(hasNameOverrun(before)) << joined(before.crash);

    CarPreset slot;
    Cursor c(bad.payload);
    c.u32();
    slot.presetId = c.u32();
    slot.slotState = c.u32();
    slot.name = std::string(reinterpret_cast<const char*>(kPackageFactoryOpen + 12));
    c.skip(12);
    slot.kartInstanceId = c.u32();
    slot.partCover = c.u32();
    slot.partTire = c.u32();
    slot.partBooster = c.u32();
    slot.partBumper = c.u32();
    slot.partFFender = c.u32();
    slot.partRFender = c.u32();
    slot.partWing = c.u32();
    ASSERT_EQ(slot.name, "Factory Car");
    ASSERT_EQ(slot.kartInstanceId, 0x22u);

    Frame fixed{0x0107, CustomCarPackets::presetList({slot}).payload()};
    const Report after = check({fixed});
    EXPECT_FALSE(hasNameOverrun(after)) << joined(after.crash);
    // everything past the name is byte for byte what the package sent
    ASSERT_EQ(fixed.payload.size(), 56u);
    EXPECT_EQ(std::memcmp(fixed.payload.data() + 24, kPackageFactoryOpen + 24, 32), 0);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(fixed.payload.data() + 12)), "Factory C");
}

// the rule edge 9 chars and the NUL fit the WCHAR 10 cell 10 chars write the cookie
TEST(ClientReaderRules, TheNamePlateCellHoldsNineChars) {
    for (const auto& name : {std::string("ABCDEFGHI"), std::string("ABCDEFGHIJ")}) {
        CarPreset slot = CustomCarPackets::emptyPreset(4);
        std::vector<uint8_t> raw = CustomCarPackets::presetList({slot}).payload();
        std::memcpy(raw.data() + 12, name.c_str(), name.size() + 1);
        const Report rep = check({Frame{0x0107, raw}});
        EXPECT_EQ(hasNameOverrun(rep), name.size() > 9) << name;
    }
    EXPECT_EQ(CustomCarPackets::clampPresetName("Factory Car"), "Factory C");
    EXPECT_EQ(CustomCarPackets::clampPresetName("Racer"), "Racer");
}

// a room object with no price still ships one inert option and a pendant row fits its slots
TEST(ClientReaderRules, RoomObjectAndPendantRowsOfTheBuilders) {
    RoomObjectDef def;
    def.objectKey = 4001;
    def.category = 3;
    def.maxPlaceable = 10;
    def.assetFolder = "fortree01";
    def.nameLocKey = "OBJECT_01_TITLE";
    def.descLocKey = "OBJECT_01_INFO";
    Frame room{0x010C, RoomCraftPackets::objectDefinition(def).payload()};
    Row r;
    ASSERT_TRUE(decodeRow(room, r));
    EXPECT_FALSE(r.bad);
    EXPECT_EQ(r.left, 0u);
    ASSERT_EQ(r.opts.size(), 1u);
    EXPECT_EQ(r.opts[0].key, 0u);

    Frame pendant{0x0119, PacketBuilder::pendantDefinition(0, 13, "pendant_13", "PENDANT_13_TITLE",
                                                            "PENDANT_13_INFO").payload()};
    Row pr;
    ASSERT_TRUE(decodeRow(pendant, pr));
    EXPECT_FALSE(pr.bad);
    EXPECT_EQ(pr.left, 0u);
    EXPECT_EQ(pr.key, 13u);
    EXPECT_EQ(pr.visible, 0u);
}

// the factory of the official server one built slot per owned chassis with its BASIC set installed

namespace {

// the 35 emulator part rows key is category plus one thousands plus the chassis folder index
std::vector<CarPartDef> shippedPartDefs() {
    static const char* const kFolder[5] = {"Firedragon", "Striper", "Circler", "Quatzalcuatl", "Squarer"};
    static const char* const kFamily[7] = {"COVER", "BOOSTER", "TIRES", "F_FENDER", "R_FENDER", "BUMPER", "WING"};
    std::vector<CarPartDef> defs;
    for (uint32_t c = 0; c < 7; ++c) {
        for (uint32_t f = 0; f < 5; ++f) {
            CarPartDef d;
            d.partKey = (c + 1) * 1000 + f;
            d.category = c;
            d.modelDirName = kFolder[f];
            d.displayNameKey = std::string(kFamily[c]) + "_" + std::to_string(d.partKey) + "_TITLE";
            d.descriptionKey = std::string(kFamily[c]) + "_" + std::to_string(d.partKey) + "_INFO";
            d.priceRows = {{2007, 1, 1, 0}, {3007, 1, 7, 0}, {1007, 0, 0, 0}};
            defs.push_back(d);
        }
    }
    return defs;
}

// the stock stream of a player owning the given chassis and the given 0x0107 rows
std::vector<Frame> factoryStream(const std::vector<std::pair<uint32_t, uint32_t>>& karts,
                                 const std::vector<CarPartInstance>& parts,
                                 const std::vector<CarPreset>& presets,
                                 const std::vector<uint32_t>& chassisOptions = {1007},
                                 int32_t chassisAmount = CustomCarPackets::FACTORY_START_DURABILITY) {
    std::vector<Frame> stream;
    auto add = [&stream](const Packet& p) { stream.push_back({p.opcode(), p.payload()}); };
    for (uint32_t key : {1007u, 2007u, 3007u}) {
        Packet price(0x00C6);
        price.writeUInt32(key);
        for (int i = 0; i < 6; ++i) price.writeUInt32(0);
        add(price);
    }
    add(PacketBuilder::partCatalog(9007, 0, "RED", "Red Paint", "Red Paint", 0, {2007}, -1, true, true));
    add(PacketBuilder::partCatalog(9100, 1, "NAMEBOX_NORMAL", "Standard Plate", "Standard Plate", 0, {}, -1, true, false));
    std::array<int32_t, 8> skins{9007, 9100, 0, 0, 0, 0, 0, 0};
    std::array<float, 17> stats{};
    add(PacketBuilder::vehicleCatalog(12002, "Circler", skins, chassisOptions, true,
                                      "CAR_CHASSIS_03_TITLE", "CAR_CHASSIS_03_INFO", stats, {}, chassisAmount));
    add(PacketBuilder::vehicleCatalog(12004, "Striper", skins, chassisOptions, true,
                                      "CAR_CHASSIS_02_TITLE", "CAR_CHASSIS_02_INFO", stats, {}, chassisAmount));
    for (const auto& d : shippedPartDefs()) add(CustomCarPackets::partDef(d));
    Packet owned(0x001C);
    owned.writeInt32(static_cast<int32_t>(karts.size()));
    for (const auto& k : karts) {
        const uint32_t row[14] = {k.first, k.second, 9007, 9100, 0, 0, 0, 0, 0, 0, 0,
                                  CustomCarPackets::FACTORY_PERIOD_MODE,
                                  static_cast<uint32_t>(CustomCarPackets::FACTORY_START_DURABILITY), 1};
        for (uint32_t v : row) owned.writeUInt32(v);
    }
    add(owned);
    for (const auto& p : parts) add(CustomCarPackets::partInstance(p));
    add(CustomCarPackets::presetList(presets));
    return stream;
}

// the basic set of one chassis as the sync grants it from instance id first on
std::vector<CarPartInstance> basicInstances(const std::string& model, uint32_t first, int32_t count) {
    std::vector<CarPartInstance> out;
    const auto keys = CustomCarPackets::basicSetKeys(model, shippedPartDefs());
    for (uint32_t c = 0; c < 7; ++c) {
        CarPartInstance p;
        p.instanceId = first + c;
        p.partKey = keys[c];
        p.category = c;
        p.equipRefcount = count;
        out.push_back(p);
    }
    return out;
}

std::array<uint32_t, 7> idsOf(const std::vector<CarPartInstance>& parts) {
    std::array<uint32_t, 7> ids{};
    for (const auto& p : parts) ids[p.category] = p.instanceId;
    return ids;
}

}  // namespace

// the BASIC set is the seven parts of the chassis folder Whirlwind is Circler
TEST(CarFactorySlots, TheBasicSetIsTheChassisFolder) {
    const std::array<uint32_t, 7> circler = {1002, 2002, 3002, 4002, 5002, 6002, 7002};
    EXPECT_EQ(CustomCarPackets::basicSetKeys("Circler", shippedPartDefs()), circler);
    EXPECT_EQ(CustomCarPackets::basicSetKeys("circler", shippedPartDefs()), circler);
    EXPECT_EQ(CustomCarPackets::basicSetKeys("Mini", shippedPartDefs()), (std::array<uint32_t, 7>{}));
    const CarPreset slot = CustomCarPackets::chassisPreset(27, 34, {11, 12, 13, 14, 15, 16, 17});
    EXPECT_EQ(slot.slotState, CustomCarPackets::SLOT_BUILT);
    EXPECT_TRUE(slot.name.empty());
    EXPECT_EQ(slot.partCover, 11u);
    EXPECT_EQ(slot.partBooster, 12u);
    EXPECT_EQ(slot.partTire, 13u);
    EXPECT_EQ(slot.partFFender, 14u);
    EXPECT_EQ(slot.partRFender, 15u);
    EXPECT_EQ(slot.partBumper, 16u);
    EXPECT_EQ(slot.partWing, 17u);
}

// a fresh player on the empty slot gets two chassis the empty row turns into the first slot
TEST(CarFactorySlots, OneSlotPerOwnedChassisInKartOrder) {
    const std::vector<FactoryChassis> chassis = {{34, "Circler"}, {40, "Striper"}};
    const FactoryPresetPlan plan = CustomCarPackets::planFactoryPresets({CustomCarPackets::emptyPreset(27)}, chassis);
    ASSERT_EQ(plan.fit.size(), 2u);
    EXPECT_EQ(plan.fit[0].chassis.kartInstanceId, 34u);
    EXPECT_EQ(plan.fit[0].presetId, 27u);
    EXPECT_EQ(plan.fit[1].chassis.kartInstanceId, 40u);
    EXPECT_EQ(plan.fit[1].presetId, 0u);
    EXPECT_TRUE(plan.dropPresetIds.empty());
    EXPECT_TRUE(plan.resetPresetIds.empty());
    EXPECT_FALSE(plan.seedEmpty);

    const std::vector<CarPreset> after = {CustomCarPackets::chassisPreset(27, 34, {1, 2, 3, 4, 5, 6, 7}),
                                          CustomCarPackets::chassisPreset(28, 40, {8, 9, 10, 11, 12, 13, 14})};
    const FactoryPresetPlan again = CustomCarPackets::planFactoryPresets(after, chassis);
    EXPECT_TRUE(again.fit.empty());
    EXPECT_TRUE(again.dropPresetIds.empty());
    EXPECT_TRUE(again.resetPresetIds.empty());
    EXPECT_FALSE(again.seedEmpty);
}

// no chassis keeps one clean empty slot a sold chassis slot goes back to empty a spare row goes
TEST(CarFactorySlots, NoChassisKeepsOneCleanEmptySlot) {
    EXPECT_TRUE(CustomCarPackets::planFactoryPresets({}, {}).seedEmpty);
    const FactoryPresetPlan keep = CustomCarPackets::planFactoryPresets({CustomCarPackets::emptyPreset(6)}, {});
    EXPECT_FALSE(keep.seedEmpty);
    EXPECT_TRUE(keep.resetPresetIds.empty());
    EXPECT_TRUE(keep.dropPresetIds.empty());

    const CarPreset sold = CustomCarPackets::chassisPreset(5, 99, {1, 2, 3, 4, 5, 6, 7});
    const FactoryPresetPlan plan = CustomCarPackets::planFactoryPresets({sold, CustomCarPackets::emptyPreset(6)}, {});
    EXPECT_EQ(plan.resetPresetIds, std::vector<uint32_t>{5});
    EXPECT_EQ(plan.dropPresetIds, std::vector<uint32_t>{6});
    EXPECT_FALSE(plan.seedEmpty);
}

// sub 450140 resolves a kart to its first slot so a second slot of one chassis goes
TEST(CarFactorySlots, ASecondSlotOfOneChassisGoes) {
    const std::vector<CarPreset> rows = {CustomCarPackets::chassisPreset(3, 34, {1, 2, 3, 4, 5, 6, 7}),
                                         CustomCarPackets::chassisPreset(4, 34, {8, 9, 10, 11, 12, 13, 14}),
                                         CustomCarPackets::emptyPreset(5)};
    const FactoryPresetPlan plan = CustomCarPackets::planFactoryPresets(rows, {{34, "Circler"}});
    EXPECT_TRUE(plan.fit.empty());
    EXPECT_EQ(plan.dropPresetIds, (std::vector<uint32_t>{4, 5}));
}

// the equip count is how many built slots hold the part sub 42F6C0 adds one per install
TEST(CarFactorySlots, TheEquipCountIsTheBuiltSlotsHoldingThePart) {
    const CarPreset a = CustomCarPackets::chassisPreset(3, 34, {1, 2, 3, 4, 5, 6, 7});
    const CarPreset b = CustomCarPackets::chassisPreset(4, 40, {8, 9, 3, 11, 12, 13, 0});
    CarPreset empty = CustomCarPackets::emptyPreset(5);
    empty.partTire = 3;
    const auto counts = CustomCarPackets::refcountsFromPresets({a, b, empty});
    std::map<uint32_t, int32_t> m(counts.begin(), counts.end());
    EXPECT_EQ(m[3], 2);
    EXPECT_EQ(m[1], 1);
    EXPECT_EQ(m[8], 1);
    EXPECT_EQ(m.count(14), 0u);
    EXPECT_EQ(m.size(), 12u);
}

// a built slot that lost its tire takes the basic tire of its family before any other live tire
TEST(CarFactorySlots, APickedTireIsTheBasicTireFirst) {
    CarPartInstance other;
    other.instanceId = 5;
    other.partKey = 3000;
    other.category = 2;
    CarPartInstance basic = other;
    basic.instanceId = 9;
    basic.partKey = 3002;
    CarPartInstance expired = basic;
    expired.instanceId = 2;
    expired.periodActive = 0;
    EXPECT_EQ(CustomCarPackets::pickTire("Circler", shippedPartDefs(), {expired, other, basic}), 9u);
    EXPECT_EQ(CustomCarPackets::pickTire("Circler", shippedPartDefs(), {expired, other}), 5u);
    EXPECT_EQ(CustomCarPackets::pickTire("Circler", shippedPartDefs(), {expired}), 0u);
}

// a chassis starts on the full bar the wrench bar of 0x42AD20 clamps at 500
TEST(CarFactorySlots, AChassisStartsOnTheFullDurabilityBar) {
    EXPECT_EQ(CustomCarPackets::FACTORY_PERIOD_MODE, 3u);
    EXPECT_EQ(CustomCarPackets::FACTORY_START_DURABILITY, 500);
}

// the whole stream of a player with two chassis passes and the old empty slot hid both from the garage
TEST(ClientReaderRules, TheOfficialFactoryStreamIsClean) {
    const std::vector<std::pair<uint32_t, uint32_t>> karts = {{34, 12002}, {40, 12004}};
    const std::vector<CarPartInstance> circler = basicInstances("Circler", 100, 1);
    const std::vector<CarPartInstance> striper = basicInstances("Striper", 107, 1);
    std::vector<CarPartInstance> parts = circler;
    parts.insert(parts.end(), striper.begin(), striper.end());
    const std::vector<CarPreset> slots = {CustomCarPackets::chassisPreset(27, 34, idsOf(circler)),
                                          CustomCarPackets::chassisPreset(28, 40, idsOf(striper))};
    const Report rep = check(factoryStream(karts, parts, slots));
    EXPECT_TRUE(rep.crash.empty()) << joined(rep.crash);
    EXPECT_TRUE(rep.dropped.empty()) << joined(rep.dropped);
    EXPECT_TRUE(rep.cosmetic.empty()) << rep.cosmetic.size() << " cosmetic rules hit";

    // the 070 answer one empty slot and no part the two chassis never show in the garage
    const Report before = check(factoryStream(karts, {}, {CustomCarPackets::emptyPreset(27)}));
    EXPECT_TRUE(before.crash.empty()) << joined(before.crash);
    ASSERT_EQ(before.dropped.size(), 2u) << joined(before.dropped);
    for (const auto& d : before.dropped) EXPECT_EQ(d, "0x001C chassis has no built 0x0107 slot the garage never lists it");
}

// the day options the package image sent on a chassis row made the wrench bar divide 500 by 1
TEST(ClientReaderRules, AChassisRowSellsTheFullDurabilityBar) {
    const Packet row = PacketBuilder::vehicleCatalog(12002, "Circler", {9007, 9100, 0, 0, 0, 0, 0, 0}, {1007}, true,
                                                     "CAR_CHASSIS_03_TITLE", "CAR_CHASSIS_03_INFO", {}, {},
                                                     CustomCarPackets::FACTORY_START_DURABILITY);
    Row r;
    ASSERT_TRUE(decodeRow(Frame{0x00C0, row.payload()}, r));
    EXPECT_EQ(r.left, 0u);
    ASSERT_EQ(r.opts.size(), 1u);
    EXPECT_EQ(r.opts[0].key, 1007u);
    EXPECT_EQ(r.opts[0].unit, 3u);
    EXPECT_EQ(r.opts[0].amount, 500u);

    const std::vector<CarPartInstance> circler = basicInstances("Circler", 100, 1);
    const std::vector<CarPreset> slot = {CustomCarPackets::chassisPreset(27, 34, idsOf(circler))};
    const Report days = check(factoryStream({{34, 12002}}, circler, slot, {2007, 3007, 1007}, 0));
    EXPECT_TRUE(days.crash.empty()) << joined(days.crash);
    ASSERT_EQ(days.cosmetic.size(), 1u);
    EXPECT_EQ(days.cosmetic.count("0x001C mode 3 kart over the 0xC0 option zero amount the wrench bar runs past its box"), 1u);
    EXPECT_TRUE(check(factoryStream({{34, 12002}}, circler, slot)).cosmetic.empty());
}
