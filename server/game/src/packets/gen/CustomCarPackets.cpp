#include "packets/gen/CustomCarPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <utility>

namespace knc {

namespace {

// opcodes above 0xFF need fromCmdFull else the byte ctor truncates
constexpr uint16_t OP_PRESET_LIST   = 0x0107;
constexpr uint16_t OP_PART_DEF      = 0x0108;
constexpr uint16_t OP_PART_INSTANCE = 0x0109;
constexpr uint16_t OP_OPEN_ACK      = 0x010A;
constexpr uint16_t OP_SAVE_RESULT   = 0x010B;
// CMD S ENTITY DATA 276
constexpr uint16_t OP_RENAME_ACK    = 0x0114;
// CMD S ENTITY DATA 292
constexpr uint16_t OP_ROW_UPDATE    = 0x0124;

constexpr size_t PRESET_REC_SIZE   = 0x34;
constexpr size_t PART_REC_SIZE     = 0x84;
constexpr size_t CAR_CONFIG_SIZE   = 0x20;
constexpr size_t PART_DEF_HEAD     = 0x74;
constexpr size_t PRICE_ROW_SIZE    = 0x10;

void putU32(uint8_t* p, size_t off, uint32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

void putI32(uint8_t* p, size_t off, int32_t v) {
    putU32(p, off, static_cast<uint32_t>(v));
}

// sub 44EB30 bounds against the frame not the destination so cap here
std::string clampAscii(const std::string& in, size_t maxChars, const char* what) {
    size_t nulProcessedLen = 0;
    std::string out = clampAsciiCore(in, maxChars, AsciiNulMode::kStopAtNul, nulProcessedLen);
    if (out.size() < in.size()) {
        LOG_WARN("PACKET", std::string("customcar ") + what + " truncated to " +
                           std::to_string(out.size()) + " chars");
    }
    return out;
}

int32_t toI32(const std::map<std::string, std::string>& row, const char* key,
              int32_t fallback = 0) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, fallback));
}

uint32_t toU32(const std::map<std::string, std::string>& row, const char* key,
               uint32_t fallback = 0) {
    return static_cast<uint32_t>(toI32(row, key, static_cast<int32_t>(fallback)));
}

std::string toStr(const std::map<std::string, std::string>& row, const char* key) {
    return rowStrCore(row, key);
}

int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// blobs cannot round trip through the row map because it stops at the first NUL
void decodeStatHex(const std::string& hex, float* out17) {
    for (int i = 0; i < 17; ++i) out17[i] = 0.0f;
    if (hex.size() < 136) return;
    uint8_t raw[68] = {0};
    for (size_t i = 0; i < 68; ++i) {
        int hi = hexNibble(hex[i * 2]);
        int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return;
        raw[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    std::memcpy(out17, raw, sizeof(raw));
}

const CarPartInstance* findOwned(const std::vector<CarPartInstance>& owned, uint32_t instanceId) {
    if (instanceId == 0) return nullptr;
    for (const auto& o : owned) {
        if (o.instanceId == instanceId) return &o;
    }
    return nullptr;
}

std::string lowerAscii(std::string s) {
    for (auto& c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

// preset field of a 0x0108 category cover booster tire front fender rear fender bumper wing
uint32_t* slotOfCategory(CarPreset& p, uint32_t category) {
    switch (category) {
        case 0: return &p.partCover;
        case 1: return &p.partBooster;
        case 2: return &p.partTire;
        case 3: return &p.partFFender;
        case 4: return &p.partRFender;
        case 5: return &p.partBumper;
        case 6: return &p.partWing;
        default: return nullptr;
    }
}

bool cleanEmpty(const CarPreset& p) {
    return p.slotState == CustomCarPackets::SLOT_EMPTY && p.name.empty() && p.kartInstanceId == 0 &&
           p.partCover == 0 && p.partTire == 0 && p.partBooster == 0 && p.partBumper == 0 &&
           p.partFFender == 0 && p.partRFender == 0 && p.partWing == 0;
}

CarPreset presetFromRow(const std::map<std::string, std::string>& r) {
    CarPreset p;
    p.presetId       = toU32(r, "preset_id");
    p.slotState      = toU32(r, "slot_state", 1);
    p.name           = toStr(r, "name");
    p.kartInstanceId = toU32(r, "kart_instance_id");
    p.partCover      = toU32(r, "part_inst_cover");
    p.partTire       = toU32(r, "part_inst_tire");
    p.partBooster    = toU32(r, "part_inst_booster");
    p.partBumper     = toU32(r, "part_inst_bumper");
    p.partFFender    = toU32(r, "part_inst_ffender");
    p.partRFender    = toU32(r, "part_inst_rfender");
    p.partWing       = toU32(r, "part_inst_wing");
    return p;
}

CarPartInstance partFromRow(const std::map<std::string, std::string>& r) {
    CarPartInstance p;
    p.instanceId    = toU32(r, "instance_id");
    p.partKey       = toU32(r, "part_key");
    p.category      = toU32(r, "category");
    p.equipRefcount = toI32(r, "equip_refcount");
    p.priceTableKey = toU32(r, "price_table_key");
    p.periodType    = toU32(r, "period_type");
    p.periodValue   = toI32(r, "period_value");
    p.periodActive  = toU32(r, "period_active", 1);
    p.grade         = toI32(r, "grade");
    return p;
}

const char* const kPresetColumns =
    "preset_id, slot_state, name, kart_instance_id, part_inst_cover, part_inst_tire, "
    "part_inst_booster, part_inst_bumper, part_inst_ffender, part_inst_rfender, part_inst_wing";

const char* const kPartColumns =
    "instance_id, part_key, category, equip_refcount, price_table_key, period_type, "
    "period_value, period_active, grade";

// the permanent price row of a def the row draw prints UNIT PERMANENT for period type 0
uint32_t permanentPriceKey(const std::vector<CarPartDef>& defs, uint32_t partKey) {
    for (const auto& d : defs) {
        if (d.partKey != partKey) continue;
        for (const auto& pr : d.priceRows) {
            if (pr.periodType == 0) return pr.priceTableKey;
        }
    }
    return 0;
}

// one permanent BASIC part owned by the character zero on a failed insert
uint32_t grantBasicPart(Transaction& tx, int32_t characterId, uint32_t partKey, uint32_t category,
                        const std::vector<CarPartDef>& defs) {
    if (!tx.execute("INSERT INTO custom_car_part_instance (character_id, part_key, category, "
                    "equip_refcount, price_table_key, period_type, period_value, period_active, grade) "
                    "VALUES (?, ?, ?, 0, ?, 0, 0, 1, 0)",
                    {characterId, partKey, category, permanentPriceKey(defs, partKey)})) {
        return 0;
    }
    return static_cast<uint32_t>(tx.lastInsertId());
}

bool writeBuiltPreset(Transaction& tx, int32_t characterId, const CarPreset& p) {
    const DbParams slots = {p.kartInstanceId, p.partCover, p.partTire, p.partBooster, p.partBumper,
                            p.partFFender, p.partRFender, p.partWing};
    if (p.presetId != 0) {
        DbParams params = slots;
        params.push_back(p.presetId);
        params.push_back(characterId);
        return tx.execute("UPDATE custom_car_preset SET slot_state = 1, name = '', kart_instance_id = ?, "
                          "part_inst_cover = ?, part_inst_tire = ?, part_inst_booster = ?, "
                          "part_inst_bumper = ?, part_inst_ffender = ?, part_inst_rfender = ?, "
                          "part_inst_wing = ? WHERE preset_id = ? AND character_id = ?",
                          params);
    }
    DbParams params = {characterId};
    params.insert(params.end(), slots.begin(), slots.end());
    return tx.execute("INSERT INTO custom_car_preset (character_id, slot_state, name, kart_instance_id, "
                      "part_inst_cover, part_inst_tire, part_inst_booster, part_inst_bumper, "
                      "part_inst_ffender, part_inst_rfender, part_inst_wing) "
                      "VALUES (?, 1, '', ?, ?, ?, ?, ?, ?, ?, ?)",
                      params);
}

} // namespace

std::array<uint8_t, 0x34> CustomCarPackets::presetRecord(const CarPreset& preset) {
    std::array<uint8_t, 0x34> r{};
    putU32(r.data(), 0x00, preset.presetId);
    putU32(r.data(), 0x04, preset.slotState);

    // nine chars max the rename click of sub 432B20 overruns its stack cell past that
    const size_t n = std::min(preset.name.size(), CustomCarPackets::PRESET_NAME_MAX);
    for (size_t i = 0; i < n; ++i) {
        char c = preset.name[i];
        if (c == '\0') break;
        r[0x08 + i] = static_cast<uint8_t>(c);
    }

    putU32(r.data(), 0x14, preset.kartInstanceId);
    putU32(r.data(), 0x18, preset.partCover);
    putU32(r.data(), 0x1C, preset.partTire);
    putU32(r.data(), 0x20, preset.partBooster);
    putU32(r.data(), 0x24, preset.partBumper);
    putU32(r.data(), 0x28, preset.partFFender);
    putU32(r.data(), 0x2C, preset.partRFender);
    putU32(r.data(), 0x30, preset.partWing);
    return r;
}

std::array<uint8_t, 0x20> CustomCarPackets::carConfigBlob(const CarPreset& preset) {
    // same bytes the preset row carries from offset 0x14 so the two can never disagree
    const auto rec = presetRecord(preset);
    std::array<uint8_t, 0x20> cfg{};
    std::memcpy(cfg.data(), rec.data() + 0x14, CAR_CONFIG_SIZE);
    return cfg;
}

std::array<uint8_t, 0x84> CustomCarPackets::partInstanceRecord(const CarPartInstance& inst) {
    std::array<uint8_t, 0x84> r{};
    putU32(r.data(), 0x00, inst.instanceId);
    putU32(r.data(), 0x04, inst.partKey);
    putU32(r.data(), 0x08, inst.category);
    putI32(r.data(), 0x0C, inst.equipRefcount);
    putU32(r.data(), 0x10, inst.priceTableKey);
    putU32(r.data(), 0x14, inst.periodType);
    putI32(r.data(), 0x18, inst.periodValue);
    putU32(r.data(), 0x1C, inst.periodActive);
    // 0x20 to 0x7F stays zero no client read found anywhere
    putI32(r.data(), 0x80, inst.grade);
    return r;
}

std::array<uint8_t, 0x3C> CustomCarPackets::customCarBlockEmpty() {
    return std::array<uint8_t, 0x3C>{};
}

std::array<uint8_t, 0x3C> CustomCarPackets::customCarBlock(
        uint32_t chassisKartKeyValue,
        const CarPreset& preset,
        const std::vector<CarPartInstance>& owned) {

    std::array<uint8_t, 0x3C> r{};
    putU32(r.data(), 0x00, chassisKartKeyValue);

    // pair order is fixed cover tire booster bumper ffender rfender wing
    const std::pair<size_t, uint32_t> slots[7] = {
        {0x04, preset.partCover},
        {0x0C, preset.partTire},
        {0x14, preset.partBooster},
        {0x1C, preset.partBumper},
        {0x24, preset.partFFender},
        {0x2C, preset.partRFender},
        {0x34, preset.partWing}
    };

    for (const auto& s : slots) {
        const CarPartInstance* inst = findOwned(owned, s.second);
        // zero refcount means not equipped so the slot must read empty
        if (!inst || inst->equipRefcount == 0) continue;
        putU32(r.data(), s.first, inst->partKey);
        putI32(r.data(), s.first + 4, inst->grade);
    }
    return r;
}

CarPreset CustomCarPackets::emptyPreset(uint32_t presetId) {
    CarPreset p;
    p.presetId  = presetId ? presetId : 1u;
    p.slotState = SLOT_EMPTY;
    return p;
}

Packet CustomCarPackets::presetList(const std::vector<CarPreset>& presets) {
    Packet pkt = Packet::fromCmdFull(OP_PRESET_LIST);

    std::vector<CarPreset> rows = presets;
    if (rows.empty()) {
        // count zero is a hard crash in stage 18 init and on mouse move
        LOG_WARN("PACKET", "customcar preset list empty sending one empty slot");
        rows.push_back(emptyPreset(1));
    }
    if (rows.size() > MAX_PRESETS_PER_CHAR) {
        LOG_WARN("PACKET", "customcar preset list " + std::to_string(rows.size()) +
                           " rows client keeps five");
        rows.resize(MAX_PRESETS_PER_CHAR);
    }

    for (auto& p : rows) {
        if (p.slotState == SLOT_BUILT && (p.kartInstanceId == 0 || p.partTire == 0)) {
            // a tire of zero on the selected kart makes sub 47E400 reselect the kart of catalog key 10
            LOG_ERROR("PACKET", "customcar preset " + std::to_string(p.presetId) +
                                " is a built car with no chassis or no tire");
        }
        if (p.name.size() > PRESET_NAME_MAX) {
            LOG_WARN("PACKET", "customcar preset " + std::to_string(p.presetId) +
                               " name cut to " + std::to_string(PRESET_NAME_MAX) + " chars");
        }
    }

    pkt.writeUInt32(static_cast<uint32_t>(rows.size()));
    for (const auto& p : rows) {
        const auto rec = presetRecord(p);
        pkt.writeBytes(rec.data(), rec.size());
    }

    const size_t expected = 4 + PRESET_REC_SIZE * rows.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "presetList size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet CustomCarPackets::partDef(const CarPartDef& def) {
    Packet pkt = Packet::fromCmdFull(OP_PART_DEF);

    const std::string model = clampAscii(def.modelDirName, 32, "modelDirName");
    const std::string disp  = clampAscii(def.displayNameKey, 32, "displayNameKey");
    const std::string desc  = clampAscii(def.descriptionKey, 33, "descriptionKey");

    if (def.category > 6) {
        LOG_ERROR("PACKET", "customcar part def " + std::to_string(def.partKey) +
                            " category " + std::to_string(def.category) + " out of range");
    }

    pkt.writeUInt32(def.enabled);
    pkt.writeUInt32(0);              // record offset 0x04 no client read found
    pkt.writeUInt32(def.partKey);
    pkt.writeUInt32(def.category);
    pkt.writeUInt32(0);              // record offset 0x10 no client read found

    pkt.writeString(model);
    pkt.writeString(disp);
    pkt.writeString(desc);

    for (int i = 0; i < 17; ++i) pkt.writeFloat(def.stats[i]);

    // carcraft part ability pairs draw 0x42B7A0 prints id 0 as 0% so the burst sends -1
    for (int i = 0; i < 2; ++i) {
        pkt.writeInt32(def.abilityId[i]);
        pkt.writeUInt32(def.abilityPercent[i]);
    }

    for (int i = 0; i < 3; ++i) pkt.writeUInt32(def.wheelAttach[i]);

    size_t rows = def.priceRows.size();
    if (rows > MAX_PRICE_ROWS) {
        LOG_WARN("PACKET", "customcar part def " + std::to_string(def.partKey) +
                           " has " + std::to_string(rows) + " price rows client keeps four");
        rows = MAX_PRICE_ROWS;
    }
    // never zero the tile draw derefs the first option with no null test
    if (rows == 0) {
        pkt.writeUInt32(1);
        for (int i = 0; i < 4; ++i) pkt.writeUInt32(0);
        return pkt;
    }
    pkt.writeUInt32(static_cast<uint32_t>(rows));
    for (size_t i = 0; i < rows; ++i) {
        const auto& pr = def.priceRows[i];
        // dword zero is a key into the 0x00C6 table not a coin amount
        pkt.writeUInt32(pr.priceTableKey);
        pkt.writeUInt32(pr.periodType);
        pkt.writeUInt32(pr.periodValue);
        pkt.writeUInt32(pr.active);
    }

    const size_t expected = PART_DEF_HEAD + model.size() + 1 + disp.size() + 1 +
                            desc.size() + 1 + 4 + PRICE_ROW_SIZE * rows;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "partDef size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet CustomCarPackets::partInstance(const CarPartInstance& inst) {
    Packet pkt = Packet::fromCmdFull(OP_PART_INSTANCE);

    if (inst.periodActive == 0) {
        LOG_WARN("PACKET", "customcar part instance " + std::to_string(inst.instanceId) +
                           " inactive client blocks equip and draws UNIT EXPIRED");
    }

    const auto rec = partInstanceRecord(inst);
    pkt.writeBytes(rec.data(), rec.size());

    if (pkt.payload().size() != PART_REC_SIZE) {
        LOG_ERROR("PACKET", "partInstance size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(PART_REC_SIZE));
    }
    return pkt;
}

Packet CustomCarPackets::openCarCraftAck() {
    // handler calls the reader zero times any payload byte desyncs the stream
    Packet pkt = Packet::fromCmdFull(OP_OPEN_ACK);
    if (!pkt.payload().empty()) {
        LOG_ERROR("PACKET", "openCarCraftAck size " + std::to_string(pkt.payload().size()) +
                            " expected 0");
    }
    return pkt;
}

Packet CustomCarPackets::saveResult(const CarPreset& preset,
                                    uint32_t selectedKartInstanceId,
                                    const std::vector<CarPartInstance>& parts) {
    Packet pkt = Packet::fromCmdFull(OP_SAVE_RESULT);

    pkt.writeUInt32(preset.presetId);
    pkt.writeUInt32(preset.slotState);
    pkt.writeUInt32(selectedKartInstanceId);

    const auto cfg = carConfigBlob(preset);
    pkt.writeBytes(cfg.data(), cfg.size());

    pkt.writeUInt32(static_cast<uint32_t>(parts.size()));
    for (const auto& p : parts) {
        const auto rec = partInstanceRecord(p);
        pkt.writeBytes(rec.data(), rec.size());
    }

    const size_t expected = 0x30 + PART_REC_SIZE * parts.size();
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "saveResult size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet CustomCarPackets::presetRenameAck(uint32_t presetId, const std::string& name) {
    Packet pkt = Packet::fromCmdFull(OP_RENAME_ACK);

    // sub 47E620 strcpy's this straight into a 12 byte stack cell with no bound
    const std::string clamped = clampPresetName(name);

    pkt.writeUInt32(presetId);
    pkt.writeString(clamped);

    const size_t expected = 4 + clamped.size() + 1;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "presetRenameAck size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet CustomCarPackets::presetRowUpdate(const CarPreset& preset) {
    Packet pkt = Packet::fromCmdFull(OP_ROW_UPDATE);

    // same 0x34 byte shape as one 0x0107 row first dword is the lookup key
    const auto rec = presetRecord(preset);
    pkt.writeBytes(rec.data(), rec.size());

    if (pkt.payload().size() != PRESET_REC_SIZE) {
        LOG_ERROR("PACKET", "presetRowUpdate size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(PRESET_REC_SIZE));
    }
    return pkt;
}

CarSaveRequest CustomCarPackets::parseSaveRequest(Packet& pkt) {
    CarSaveRequest req;
    pkt.resetReadPos();

    if (pkt.remaining() < 0x28) {
        LOG_WARN("PACKET", "customcar save request short " +
                           std::to_string(pkt.payload().size()) + " bytes");
        return req;
    }

    req.presetId       = pkt.readUInt32();
    req.kartInstanceId = pkt.readUInt32();
    req.partCover      = pkt.readUInt32();
    req.partTire       = pkt.readUInt32();
    req.partBooster    = pkt.readUInt32();
    req.partBumper     = pkt.readUInt32();
    req.partFFender    = pkt.readUInt32();
    req.partRFender    = pkt.readUInt32();
    req.partWing       = pkt.readUInt32();

    uint32_t count = pkt.readUInt32();
    if (count > MAX_PARTS_PER_CHAR) {
        // a lying count would otherwise spin the loop on an empty tail
        LOG_WARN("PACKET", "customcar save request claims " + std::to_string(count) +
                           " parts capping");
        count = static_cast<uint32_t>(MAX_PARTS_PER_CHAR);
    }

    for (uint32_t i = 0; i < count; ++i) {
        if (pkt.remaining() < PART_REC_SIZE) {
            LOG_WARN("PACKET", "customcar save request truncated at part " +
                               std::to_string(i));
            break;
        }
        const uint32_t instanceId = pkt.readUInt32();
        // the key and category echo and the rest of the client copy are never trusted
        pkt.readUInt32();
        pkt.readUInt32();
        const int32_t refcount = pkt.readInt32();
        pkt.readBytes(PART_REC_SIZE - 16);

        req.clientInstanceIds.push_back(instanceId);
        req.clientRefcounts.push_back(refcount);
    }

    req.valid = true;
    return req;
}

CarPreset CustomCarPackets::validateSave(const CarSaveRequest& req,
                                         const CarPreset& current,
                                         const std::vector<CarPartInstance>& owned) {
    CarPreset out = current;
    if (!req.valid) return out;

    // id and name stay server side the client never gets to rename a slot
    out.presetId  = current.presetId;
    out.slotState = 1;
    out.name      = current.name;

    // caller still has to prove the kart belongs to this character
    out.kartInstanceId = req.kartInstanceId;

    struct SlotWire {
        uint32_t requested;
        CarPartCategory category;
        uint32_t* dst;
    };
    const SlotWire slots[7] = {
        {req.partCover,   CarPartCategory::Cover,   &out.partCover},
        {req.partTire,    CarPartCategory::Tire,    &out.partTire},
        {req.partBooster, CarPartCategory::Booster, &out.partBooster},
        {req.partBumper,  CarPartCategory::Bumper,  &out.partBumper},
        {req.partFFender, CarPartCategory::FFender, &out.partFFender},
        {req.partRFender, CarPartCategory::RFender, &out.partRFender},
        {req.partWing,    CarPartCategory::Wing,    &out.partWing}
    };

    for (const auto& s : slots) {
        if (s.requested == 0) { *s.dst = 0; continue; }
        const CarPartInstance* inst = findOwned(owned, s.requested);
        if (!inst) {
            LOG_WARN("PACKET", "customcar save wants unowned part instance " +
                               std::to_string(s.requested));
            *s.dst = 0;
            continue;
        }
        if (inst->category != static_cast<uint32_t>(s.category)) {
            LOG_WARN("PACKET", "customcar save part " + std::to_string(s.requested) +
                               " category mismatch");
            *s.dst = 0;
            continue;
        }
        if (inst->periodActive == 0) {
            LOG_WARN("PACKET", "customcar save part " + std::to_string(s.requested) +
                               " expired");
            *s.dst = 0;
            continue;
        }
        *s.dst = s.requested;
    }

    if (out.partTire == 0) {
        // client never sends a save with an empty tire so keep the old one
        LOG_WARN("PACKET", "customcar save had no valid tire keeping previous");
        out.partTire = current.partTire;
    }
    return out;
}

bool CustomCarPackets::presetIsBuildable(const CarPreset& preset,
                                         const std::vector<uint32_t>& factoryKarts,
                                         const std::vector<CarPartInstance>& owned) {
    if (preset.kartInstanceId == 0) return false;
    if (std::find(factoryKarts.begin(), factoryKarts.end(), preset.kartInstanceId) == factoryKarts.end()) {
        return false;
    }
    // sub 42F3E0 refuses a save with no tire so a built car always holds one
    const CarPartInstance* tire = findOwned(owned, preset.partTire);
    return tire != nullptr && tire->category == static_cast<uint32_t>(CarPartCategory::Tire) &&
           tire->periodActive != 0;
}

CarRenameRequest CustomCarPackets::parseRenameRequest(Packet& pkt) {
    CarRenameRequest req;
    pkt.resetReadPos();

    if (pkt.remaining() < 4) {
        LOG_WARN("PACKET", "customcar rename request short " +
                           std::to_string(pkt.payload().size()) + " bytes");
        return req;
    }

    req.presetId = pkt.readUInt32();
    // client writes an unbounded cstr sub 0044ead0 has no cap either so clamp on the way to the wire
    req.name     = pkt.readString(32);
    req.valid    = true;
    return req;
}

std::string CustomCarPackets::clampPresetName(const std::string& name) {
    return clampAscii(name, PRESET_NAME_MAX, "presetName");
}

std::vector<CarPreset> CustomCarPackets::loadPresets(int32_t characterId) {
    std::vector<CarPreset> out;
    auto rows = Database::instance().queryPrepared(
        std::string("SELECT ") + kPresetColumns + " FROM custom_car_preset "
        "WHERE character_id = ? ORDER BY preset_id LIMIT 5",
        {characterId});
    for (const auto& r : rows) out.push_back(presetFromRow(r));
    return out;
}

std::vector<CarPartInstance> CustomCarPackets::loadPartInstances(int32_t characterId) {
    std::vector<CarPartInstance> out;
    auto rows = Database::instance().queryPrepared(
        std::string("SELECT ") + kPartColumns + " FROM custom_car_part_instance "
        "WHERE character_id = ? ORDER BY instance_id LIMIT 256",
        {characterId});
    for (const auto& r : rows) out.push_back(partFromRow(r));
    return out;
}

std::vector<CarPartDef> CustomCarPackets::loadPartDefs() {
    std::vector<CarPartDef> out;
    auto rows = Database::instance().queryPrepared(
        "SELECT part_key, enabled, category, model_dir_name, display_name_key, "
        "description_key, stat_block_hex, wheel_attach_0, wheel_attach_1, wheel_attach_2 "
        "FROM carcraft_part_def ORDER BY part_key LIMIT 256", {});

    for (const auto& r : rows) {
        CarPartDef d;
        d.partKey        = toU32(r, "part_key");
        d.enabled        = toU32(r, "enabled", 1);
        d.category       = toU32(r, "category");
        d.modelDirName   = toStr(r, "model_dir_name");
        d.displayNameKey = toStr(r, "display_name_key");
        d.descriptionKey = toStr(r, "description_key");
        decodeStatHex(toStr(r, "stat_block_hex"), d.stats);
        d.wheelAttach[0] = toU32(r, "wheel_attach_0");
        d.wheelAttach[1] = toU32(r, "wheel_attach_1");
        d.wheelAttach[2] = toU32(r, "wheel_attach_2");
        out.push_back(d);
    }

    auto prices = Database::instance().queryPrepared(
        "SELECT part_key, row_index, price_table_key, period_type, period_value, active "
        "FROM carcraft_part_price ORDER BY part_key, row_index", {});

    for (const auto& r : prices) {
        const uint32_t key = toU32(r, "part_key");
        for (auto& d : out) {
            if (d.partKey != key) continue;
            if (d.priceRows.size() >= MAX_PRICE_ROWS) break;
            CarPartPriceRow pr;
            pr.priceTableKey = toU32(r, "price_table_key");
            pr.periodType    = toU32(r, "period_type");
            pr.periodValue   = toU32(r, "period_value");
            pr.active        = toU32(r, "active", 1);
            d.priceRows.push_back(pr);
            break;
        }
    }
    return out;
}

bool CustomCarPackets::savePreset(int32_t characterId, const CarPreset& preset) {
    if (preset.slotState == SLOT_EMPTY) {
        return Database::instance().executePrepared(
            "UPDATE custom_car_preset SET slot_state = 0, name = '', kart_instance_id = 0, "
            "part_inst_cover = 0, part_inst_tire = 0, part_inst_booster = 0, part_inst_bumper = 0, "
            "part_inst_ffender = 0, part_inst_rfender = 0, part_inst_wing = 0 "
            "WHERE preset_id = ? AND character_id = ?",
            {static_cast<int>(preset.presetId), characterId});
    }

    // rebinding a preset to a kart the player does not own would leak a chassis
    auto owns = Database::instance().queryPrepared(
        "SELECT id FROM owned_kart WHERE id = ? AND character_id = ?",
        {static_cast<int>(preset.kartInstanceId), characterId});
    if (owns.empty()) {
        LOG_WARN("DB", "customcar save rejected kart " +
                       std::to_string(preset.kartInstanceId) + " not owned by " +
                       std::to_string(characterId));
        return false;
    }

    return Database::instance().executePrepared(
        "UPDATE custom_car_preset SET slot_state = ?, kart_instance_id = ?, "
        "part_inst_cover = ?, part_inst_tire = ?, part_inst_booster = ?, "
        "part_inst_bumper = ?, part_inst_ffender = ?, part_inst_rfender = ?, "
        "part_inst_wing = ? WHERE preset_id = ? AND character_id = ?",
        {static_cast<int>(preset.slotState),
         static_cast<int>(preset.kartInstanceId),
         static_cast<int>(preset.partCover),
         static_cast<int>(preset.partTire),
         static_cast<int>(preset.partBooster),
         static_cast<int>(preset.partBumper),
         static_cast<int>(preset.partFFender),
         static_cast<int>(preset.partRFender),
         static_cast<int>(preset.partWing),
         static_cast<int>(preset.presetId),
         characterId});
}

bool CustomCarPackets::renamePreset(int32_t characterId, uint32_t presetId,
                                    const std::string& name) {
    // ownership check first gives a clean log line instead of a silent zero row update
    auto rows = Database::instance().queryPrepared(
        "SELECT preset_id FROM custom_car_preset WHERE preset_id = ? AND character_id = ?",
        {static_cast<int>(presetId), characterId});
    if (rows.empty()) {
        LOG_WARN("DB", "customcar rename rejected preset " + std::to_string(presetId) +
                       " not owned by " + std::to_string(characterId));
        return false;
    }

    const std::string clamped = clampPresetName(name);
    if (clamped.empty()) {
        LOG_WARN("DB", "customcar rename rejected empty name for preset " +
                       std::to_string(presetId));
        return false;
    }

    return Database::instance().executePrepared(
        "UPDATE custom_car_preset SET name = ? WHERE preset_id = ? AND character_id = ?",
        {clamped, static_cast<int>(presetId), characterId});
}

bool CustomCarPackets::syncRefcounts(int32_t characterId) {
    // the count is shared by every built slot so recount from the stored rows
    const auto counts = refcountsFromPresets(loadPresets(characterId));
    bool ok = Database::instance().executePrepared(
        "UPDATE custom_car_part_instance SET equip_refcount = 0 WHERE character_id = ?",
        {characterId});
    for (const auto& c : counts) {
        ok = Database::instance().executePrepared(
                 "UPDATE custom_car_part_instance SET equip_refcount = ? "
                 "WHERE character_id = ? AND instance_id = ?",
                 {c.second, characterId, c.first}) && ok;
    }
    return ok;
}

bool CustomCarPackets::ensureDefaultPreset(int32_t characterId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT COUNT(*) AS n FROM custom_car_preset WHERE character_id = ?",
        {characterId});
    if (!rows.empty() && toI32(rows[0], "n") > 0) return true;

    // the real server opens the factory on one empty slot the craft save fills it
    const bool ok = Database::instance().executePrepared(
        "INSERT INTO custom_car_preset (character_id, slot_state, name, kart_instance_id, "
        "part_inst_cover, part_inst_tire, part_inst_booster, part_inst_bumper, "
        "part_inst_ffender, part_inst_rfender, part_inst_wing) "
        "VALUES (?, 0, '', 0, 0, 0, 0, 0, 0, 0, 0)",
        {characterId});

    if (ok) {
        LOG_INFO("DB", "customcar empty preset slot created for character " +
                       std::to_string(characterId));
    }
    return ok;
}

uint32_t CustomCarPackets::chassisKartKey(uint32_t kartInstanceId) {
    if (kartInstanceId == 0) return 0;
    // owned kart holds the wire instance id and is the only kart truth
    auto rows = Database::instance().queryPrepared(
        "SELECT base_key AS k FROM owned_kart WHERE id = ?",
        {static_cast<int>(kartInstanceId)});
    if (rows.empty()) return 0;
    return toU32(rows[0], "k");
}

bool CustomCarPackets::isFactoryKart(uint32_t kartInstanceId) {
    if (kartInstanceId == 0) return false;
    auto rows = Database::instance().queryPrepared(
        "SELECT vt.is_factory_car AS f FROM owned_kart k "
        "JOIN vehicle_templates vt ON vt.id = k.base_key WHERE k.id = ?",
        {static_cast<int>(kartInstanceId)});
    if (rows.empty()) return false;
    return toI32(rows[0], "f") != 0;
}

std::array<uint8_t, 0x3C> CustomCarPackets::customCarBlockFor(int32_t characterId,
                                                              uint32_t kartInstanceId) {
    // client skips the whole block unless the kart catalog row is a factory car
    if (!isFactoryKart(kartInstanceId)) return customCarBlockEmpty();

    for (const auto& p : loadPresets(characterId)) {
        if (p.kartInstanceId != kartInstanceId) continue;
        return customCarBlock(chassisKartKey(kartInstanceId), p,
                              loadPartInstances(characterId));
    }
    return customCarBlockEmpty();
}

FactoryPresetPlan CustomCarPackets::planFactoryPresets(const std::vector<CarPreset>& presets,
                                                       const std::vector<FactoryChassis>& chassis) {
    FactoryPresetPlan plan;
    std::vector<CarPreset> rows = presets;
    std::sort(rows.begin(), rows.end(),
              [](const CarPreset& a, const CarPreset& b) { return a.presetId < b.presetId; });

    auto isChassis = [&chassis](uint32_t kart) {
        return kart != 0 && std::any_of(chassis.begin(), chassis.end(),
                                        [kart](const FactoryChassis& c) { return c.kartInstanceId == kart; });
    };

    // sub 450140 resolves a chassis to the first slot holding it so one slot per chassis
    std::vector<uint32_t> covered;
    std::vector<uint32_t> spare;
    for (const auto& p : rows) {
        const bool fresh = std::find(covered.begin(), covered.end(), p.kartInstanceId) == covered.end();
        if (p.slotState == SLOT_BUILT && isChassis(p.kartInstanceId) && fresh &&
            covered.size() < MAX_PRESETS_PER_CHAR) {
            covered.push_back(p.kartInstanceId);
            continue;
        }
        spare.push_back(p.presetId);
    }

    size_t reuse = 0;
    for (const auto& c : chassis) {
        if (c.kartInstanceId == 0) continue;
        if (std::find(covered.begin(), covered.end(), c.kartInstanceId) != covered.end()) continue;
        if (covered.size() + plan.fit.size() >= MAX_PRESETS_PER_CHAR) break;
        FactoryFit f;
        f.chassis = c;
        if (reuse < spare.size()) f.presetId = spare[reuse++];
        plan.fit.push_back(f);
    }

    // no chassis keeps one clean empty slot since a count 0 crashes stage 18
    if (covered.empty() && plan.fit.empty()) {
        if (reuse < spare.size()) {
            const uint32_t keep = spare[reuse++];
            for (const auto& p : rows) {
                if (p.presetId == keep && !cleanEmpty(p)) plan.resetPresetIds.push_back(keep);
            }
        } else {
            plan.seedEmpty = true;
        }
    }
    for (; reuse < spare.size(); ++reuse) plan.dropPresetIds.push_back(spare[reuse]);
    return plan;
}

std::array<uint32_t, 7> CustomCarPackets::basicSetKeys(const std::string& model,
                                                       const std::vector<CarPartDef>& defs) {
    std::array<uint32_t, 7> keys{};
    const std::string want = lowerAscii(model);
    if (want.empty()) return keys;
    for (const auto& d : defs) {
        if (d.category > 6 || lowerAscii(d.modelDirName) != want) continue;
        uint32_t& k = keys[d.category];
        if (k == 0 || d.partKey < k) k = d.partKey;
    }
    return keys;
}

CarPreset CustomCarPackets::chassisPreset(uint32_t presetId, uint32_t kartInstanceId,
                                          const std::array<uint32_t, 7>& instanceByCategory) {
    CarPreset p;
    p.presetId = presetId;
    p.slotState = SLOT_BUILT;
    p.kartInstanceId = kartInstanceId;
    for (uint32_t c = 0; c < 7; ++c) *slotOfCategory(p, c) = instanceByCategory[c];
    return p;
}

std::vector<std::pair<uint32_t, int32_t>> CustomCarPackets::refcountsFromPresets(
        const std::vector<CarPreset>& presets) {
    std::map<uint32_t, int32_t> counts;
    for (const auto& p : presets) {
        if (p.slotState != SLOT_BUILT) continue;
        for (uint32_t id : {p.partCover, p.partTire, p.partBooster, p.partBumper,
                            p.partFFender, p.partRFender, p.partWing}) {
            if (id != 0) ++counts[id];
        }
    }
    return std::vector<std::pair<uint32_t, int32_t>>(counts.begin(), counts.end());
}

uint32_t CustomCarPackets::pickTire(const std::string& model, const std::vector<CarPartDef>& defs,
                                    const std::vector<CarPartInstance>& owned) {
    const uint32_t tireCategory = static_cast<uint32_t>(CarPartCategory::Tire);
    const uint32_t basic = basicSetKeys(model, defs)[tireCategory];
    uint32_t any = 0;
    for (const auto& p : owned) {
        if (p.category != tireCategory || p.periodActive == 0) continue;
        if (basic != 0 && p.partKey == basic) return p.instanceId;
        if (any == 0) any = p.instanceId;
    }
    return any;
}

bool CustomCarPackets::applyFactoryDurability(Transaction& tx, uint32_t kartInstanceId) {
    if (kartInstanceId == 0) return false;
    auto rows = tx.query("SELECT COALESCE(v.is_factory_car, 0) AS f FROM owned_kart k "
                         "JOIN vehicle_templates v ON v.id = k.base_key WHERE k.id = ? LIMIT 1",
                         {kartInstanceId});
    if (rows.empty() || toI32(rows[0], "f") == 0) return false;
    return tx.execute("UPDATE owned_kart SET period_mode = ?, period_value = ?, active_flag = 1 WHERE id = ?",
                      {FACTORY_PERIOD_MODE, FACTORY_START_DURABILITY, kartInstanceId});
}

bool CustomCarPackets::syncFactoryLoadouts(int32_t characterId) {
    if (characterId <= 0) return false;
    const std::vector<CarPartDef> defs = loadPartDefs();

    auto tx = Database::instance().beginTransaction();
    if (!tx.valid()) return false;
    bool changed = false;
    bool ok = true;

    std::vector<FactoryChassis> chassis;
    for (const auto& r : tx.query(
             "SELECT k.id, v.name, k.period_mode FROM owned_kart k "
             "JOIN vehicle_templates v ON v.id = k.base_key AND COALESCE(v.is_enabled, 1) = 1 "
             "WHERE k.character_id = ? AND COALESCE(v.is_factory_car, 0) = 1 ORDER BY k.id FOR UPDATE",
             {characterId})) {
        FactoryChassis c;
        c.kartInstanceId = toU32(r, "id");
        c.model = toStr(r, "name");
        chassis.push_back(c);
        // a crafted kart runs on the durability bar 0x42AD20 draws only for mode 3
        if (toU32(r, "period_mode") != FACTORY_PERIOD_MODE) {
            ok = tx.execute("UPDATE owned_kart SET period_mode = ?, period_value = ? WHERE id = ?",
                            {FACTORY_PERIOD_MODE, FACTORY_START_DURABILITY, c.kartInstanceId}) && ok;
            changed = true;
        }
    }

    auto loadAll = [&tx, characterId]() {
        std::vector<CarPreset> out;
        for (const auto& r : tx.query(std::string("SELECT ") + kPresetColumns +
                                      " FROM custom_car_preset WHERE character_id = ? ORDER BY preset_id",
                                      {characterId})) {
            out.push_back(presetFromRow(r));
        }
        return out;
    };
    auto loadParts = [&tx, characterId]() {
        std::vector<CarPartInstance> out;
        for (const auto& r : tx.query(std::string("SELECT ") + kPartColumns +
                                      " FROM custom_car_part_instance WHERE character_id = ? ORDER BY instance_id",
                                      {characterId})) {
            out.push_back(partFromRow(r));
        }
        return out;
    };

    const FactoryPresetPlan plan = planFactoryPresets(loadAll(), chassis);
    for (uint32_t id : plan.dropPresetIds) {
        ok = tx.execute("DELETE FROM custom_car_preset WHERE preset_id = ? AND character_id = ?",
                        {id, characterId}) && ok;
        changed = true;
    }
    for (uint32_t id : plan.resetPresetIds) {
        ok = tx.execute("UPDATE custom_car_preset SET slot_state = 0, name = '', kart_instance_id = 0, "
                        "part_inst_cover = 0, part_inst_tire = 0, part_inst_booster = 0, part_inst_bumper = 0, "
                        "part_inst_ffender = 0, part_inst_rfender = 0, part_inst_wing = 0 "
                        "WHERE preset_id = ? AND character_id = ?",
                        {id, characterId}) && ok;
        changed = true;
    }
    if (plan.seedEmpty) {
        ok = tx.execute("INSERT INTO custom_car_preset (character_id, slot_state, name, kart_instance_id) "
                        "VALUES (?, 0, '', 0)",
                        {characterId}) && ok;
        changed = true;
    }

    // a chassis comes with its BASIC tires bumper fenders engine booster and spoiler installed
    for (const auto& f : plan.fit) {
        const std::array<uint32_t, 7> keys = basicSetKeys(f.chassis.model, defs);
        std::array<uint32_t, 7> inst{};
        for (uint32_t c = 0; c < 7; ++c) {
            if (keys[c] == 0) continue;
            inst[c] = grantBasicPart(tx, characterId, keys[c], c, defs);
            if (inst[c] == 0) ok = false;
        }
        if (inst[static_cast<uint32_t>(CarPartCategory::Tire)] == 0) {
            LOG_WARN("DB", "customcar chassis " + std::to_string(f.chassis.kartInstanceId) + " model " +
                           f.chassis.model + " has no basic tire in carcraft_part_def");
        }
        ok = writeBuiltPreset(tx, characterId, chassisPreset(f.presetId, f.chassis.kartInstanceId, inst)) && ok;
        changed = true;
        LOG_INFO("DB", "customcar chassis " + std::to_string(f.chassis.kartInstanceId) +
                       " fitted with its basic set for character " + std::to_string(characterId));
    }

    // sub 42F3E0 never saves a slot with no tire so a built slot always gets a live one back
    std::vector<CarPartInstance> parts = loadParts();
    for (auto& p : loadAll()) {
        if (p.slotState != SLOT_BUILT) continue;
        const CarPartInstance* tire = findOwned(parts, p.partTire);
        if (tire && tire->category == static_cast<uint32_t>(CarPartCategory::Tire) && tire->periodActive != 0) continue;
        std::string model;
        for (const auto& c : chassis) {
            if (c.kartInstanceId == p.kartInstanceId) model = c.model;
        }
        uint32_t pick = pickTire(model, defs, parts);
        if (pick == 0) {
            const uint32_t key = basicSetKeys(model, defs)[static_cast<uint32_t>(CarPartCategory::Tire)];
            if (key != 0) pick = grantBasicPart(tx, characterId, key, static_cast<uint32_t>(CarPartCategory::Tire), defs);
            parts = loadParts();
        }
        if (pick == 0) continue;
        ok = tx.execute("UPDATE custom_car_preset SET part_inst_tire = ? WHERE preset_id = ? AND character_id = ?",
                        {pick, p.presetId, characterId}) && ok;
        changed = true;
    }

    // the equip count is how many built slots hold the part
    const auto counts = refcountsFromPresets(loadAll());
    for (const auto& part : loadParts()) {
        int32_t want = 0;
        for (const auto& c : counts) {
            if (c.first == part.instanceId) want = c.second;
        }
        if (part.equipRefcount == want) continue;
        ok = tx.execute("UPDATE custom_car_part_instance SET equip_refcount = ? WHERE instance_id = ?",
                        {want, part.instanceId}) && ok;
        changed = true;
    }

    if (!ok) {
        tx.rollback();
        LOG_ERROR("DB", "customcar factory sync failed for character " + std::to_string(characterId));
        return false;
    }
    if (!changed) {
        tx.rollback();
        return false;
    }
    return tx.commit();
}

} // namespace knc
