#include "packets/gen/PartStatPackets.h"
#include "packets/gen/CustomCarPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"
#include "util/KartDurability.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace knc {

namespace {

constexpr size_t OPTION_ROW_SIZE = 0x10;

// byte ctor truncates above 0xFF so route every opcode through one place
Packet make(uint16_t op) {
    if (op > 0xFF) return Packet::fromCmdFull(op);
    return Packet::fromCmdFull(op);   // never narrow an opcode since 0x108 is not 0x08
}

// shop option already stores the four dwords of one option row
constexpr uint32_t OPT_CATEGORY_ITEM      = 2;
constexpr uint32_t OPT_CATEGORY_KART_PART = 3;

int32_t toI32(const std::map<std::string, std::string>& row, const char* key,
              int32_t fallback = 0) {
    return static_cast<int32_t>(rowInt64Throwing(row, key, fallback));
}

uint32_t toU32(const std::map<std::string, std::string>& row, const char* key,
               uint32_t fallback = 0) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, fallback));
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

// column values stop at the first NUL so blobs travel as hex text
bool decodeHex(const std::string& hex, uint8_t* out, size_t bytes) {
    std::memset(out, 0, bytes);
    if (hex.size() < bytes * 2) return false;
    for (size_t i = 0; i < bytes; ++i) {
        const int hi = hexNibble(hex[i * 2]);
        const int lo = hexNibble(hex[i * 2 + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
    return true;
}

void decodeStatBlock(const std::string& hex, std::array<float, 17>& out) {
    out.fill(0.0f);
    uint8_t raw[68] = {0};
    if (!decodeHex(hex, raw, sizeof(raw))) return;
    std::memcpy(out.data(), raw, sizeof(raw));
}

float bitsToFloat(uint32_t bits) {
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

// handler copies strlen plus one into a fixed frame buffer with no bound check
std::string clampAscii(const std::string& in, size_t bufferBytes, const char* what,
                       uint32_t key) {
    const size_t maxChars = bufferBytes > 0 ? bufferBytes - 1 : 0;
    size_t nulProcessedLen = 0;
    std::string out = clampAsciiCore(in, maxChars, AsciiNulMode::kStopAtNul, nulProcessedLen);
    if (out.size() < in.size()) {
        LOG_WARN("PACKET", std::string("partstat ") + what + " of key " +
                           std::to_string(key) + " truncated to " +
                           std::to_string(out.size()) + " chars");
    }
    return out;
}

size_t writeOptions(Packet& pkt, const std::vector<DefPriceOption>& rows,
                    const char* what, uint32_t key) {
    size_t n = rows.size();
    if (n > PartStatPackets::CAP_PRICE_ROWS) {
        LOG_WARN("PACKET", std::string("partstat ") + what + " key " +
                           std::to_string(key) + " has " + std::to_string(n) +
                           " options client keeps four");
        n = PartStatPackets::CAP_PRICE_ROWS;
    }
    // sub 451D30 never ship an empty list since the tile draw dereferences the first row unchecked
    if (n == 0) {
        pkt.writeUInt32(1);
        for (int i = 0; i < 4; ++i) pkt.writeUInt32(0);
        return 1;
    }
    pkt.writeUInt32(static_cast<uint32_t>(n));
    for (size_t i = 0; i < n; ++i) {
        pkt.writeUInt32(rows[i].priceTableKey);
        pkt.writeUInt32(rows[i].periodType);
        pkt.writeUInt32(rows[i].periodValue);
        pkt.writeUInt32(rows[i].active);
    }
    return n;
}

float clampF(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// bar term is a min max normalise that goes flat when the catalog cannot spread
float barFromSum(float sum, float minV, float maxV, float& fracOut, bool& degenerate) {
    const float range = maxV - minV;
    float frac = 0.0f;
    degenerate = !(range > 0.0f);
    if (!degenerate) {
        frac = (sum - minV) / range * 0.2f;
        if (frac > 0.3f) frac = 0.3f;
    }
    fracOut = frac;
    float bar = (frac + 0.6f) * 100.0f;
    if (bar > 100.0f) bar = 100.0f;
    else if (bar < 40.0f) bar = 40.0f;
    return bar;
}

// index zero seeds both ends so an empty catalog leaves a zero width range
void scanRange(const std::vector<std::array<float, 17>>& catalog,
               const size_t* idx, size_t idxCount, float& minOut, float& maxOut) {
    minOut = 0.0f;
    maxOut = 0.0f;
    for (size_t i = 0; i < catalog.size(); ++i) {
        float v = 0.0f;
        for (size_t k = 0; k < idxCount; ++k) v = v + catalog[i][idx[k]];
        if (i == 0 || v > maxOut) maxOut = v;
        if (i == 0 || v < minOut) minOut = v;
    }
}

std::vector<std::vector<DefPriceOption>> loadOptionsFor(uint32_t category,
                                                        const std::vector<uint32_t>& keys) {
    std::vector<std::vector<DefPriceOption>> out(keys.size());
    auto rows = Database::instance().queryPrepared(
        "SELECT base_key, price_key, opt_word1, opt_word2, opt_word3 FROM shop_option "
        "WHERE category = ? AND slot < 4 ORDER BY base_key, slot",
        {static_cast<int>(category)});

    for (const auto& r : rows) {
        const uint32_t key = toU32(r, "base_key");
        for (size_t i = 0; i < keys.size(); ++i) {
            if (keys[i] != key) continue;
            if (out[i].size() >= PartStatPackets::CAP_PRICE_ROWS) break;
            DefPriceOption o;
            o.priceTableKey = toU32(r, "price_key");
            o.periodType    = toU32(r, "opt_word1");
            o.periodValue   = toU32(r, "opt_word2");
            o.active        = toU32(r, "opt_word3", 1);
            out[i].push_back(o);
            break;
        }
    }
    return out;
}

}  // namespace

uint32_t PartStatPackets::categoryForSlot(size_t slot) {
    switch (slot) {
        case SLOT_COVER:   return CATEGORY_COVER;
        case SLOT_TIRES:   return CATEGORY_TIRES;
        case SLOT_BOOSTER: return CATEGORY_BOOSTER;
        case SLOT_BUMPER:  return CATEGORY_BUMPER;
        case SLOT_FFENDER: return CATEGORY_FFENDER;
        case SLOT_RFENDER: return CATEGORY_RFENDER;
        case SLOT_WING:    return CATEGORY_WING;
        default:           return CATEGORY_COVER;
    }
}

Packet PartStatPackets::itemDef(const ItemDefRow& row) {
    Packet pkt = make(OP_S_ITEM_DEF);

    // record 0x14 is the png stem and 0x35 the title key the client reads them in this order
    const std::string s1 = clampAscii(row.iconName, KART_BUF1, "item icon name", row.itemKey);
    const std::string s2 = clampAscii(row.displayNameKey, KART_BUF2, "item name key", row.itemKey);
    const std::string s3 = clampAscii(row.descriptionKey, KART_BUF3, "item desc key", row.itemKey);

    pkt.writeUInt32(row.visible);
    pkt.writeUInt32(row.badge);
    pkt.writeUInt32(row.itemKey);
    pkt.writeUInt32(row.useType);
    pkt.writeUInt32(row.requiredLevel);

    pkt.writeString(s1);
    pkt.writeString(s2);
    pkt.writeString(s3);

    const size_t n = writeOptions(pkt, row.priceRows, "item def", row.itemKey);

    const size_t expected = 20 + s1.size() + 1 + s2.size() + 1 + s3.size() + 1 +
                            4 + OPTION_ROW_SIZE * n;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "itemDef size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet PartStatPackets::kartPartDef(const KartPartDefRow& row) {
    Packet pkt = make(OP_S_KART_PART_DEF);

    const std::string model = clampAscii(row.modelName, PART_BUF1, "part model name", row.partKey);
    const std::string disp  = clampAscii(row.displayNameKey, PART_BUF2, "part name key", row.partKey);
    const std::string desc  = clampAscii(row.descriptionKey, PART_BUF3, "part desc key", row.partKey);

    pkt.writeUInt32(row.visible);
    pkt.writeUInt32(row.badge);
    pkt.writeUInt32(row.partKey);
    pkt.writeUInt32(row.requiredLevel);

    pkt.writeString(model);                  // first string sits between the word groups

    pkt.writeUInt32(row.restrictTarget);
    pkt.writeUInt32(row.equipSlot);
    pkt.writeUInt32(row.restrictKey);

    pkt.writeString(disp);
    pkt.writeString(desc);

    pkt.writeBytes(row.abilityPair0.data(), row.abilityPair0.size());
    pkt.writeBytes(row.abilityPair1.data(), row.abilityPair1.size());

    const size_t n = writeOptions(pkt, row.priceRows, "kart part def", row.partKey);

    const size_t expected = 16 + model.size() + 1 + 12 + disp.size() + 1 + desc.size() + 1 +
                            16 + 4 + OPTION_ROW_SIZE * n;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "kartPartDef size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

std::vector<Packet> PartStatPackets::itemDefTable(const std::vector<ItemDefRow>& rows) {
    std::vector<Packet> out;
    out.reserve(rows.size());
    for (const auto& r : rows) out.push_back(itemDef(r));
    return out;
}

std::vector<Packet> PartStatPackets::kartPartDefTable(const std::vector<KartPartDefRow>& rows) {
    std::vector<Packet> out;
    size_t n = rows.size();
    if (n > CAP_PART_DEFS) {
        LOG_WARN("PACKET", "partstat kart part catalog " + std::to_string(n) + " rows capping");
        n = CAP_PART_DEFS;
    }
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) out.push_back(kartPartDef(rows[i]));
    return out;
}

int32_t PartStatPackets::gradeScale(int32_t grade) {
    // C truncates toward zero which is what the magic number division does
    return grade / GRADE_BUCKET;
}

int32_t PartStatPackets::gradeMultiplier(int32_t grade) {
    return 1 + gradeScale(grade);
}

int32_t PartStatPackets::clampGrade(int32_t grade) {
    if (grade < GRADE_MIN) return GRADE_MIN;
    if (grade > GRADE_MAX) return GRADE_MAX;
    return grade;
}

int32_t PartStatPackets::rarityTier(int32_t grade) {
    if (grade < TIER_UNIQUE) return 0;
    if (grade < TIER_EPIC)   return 1;
    if (grade < TIER_LEGEND) return 2;
    return 3;
}

int32_t PartStatPackets::chassisTier(const std::array<EquippedPart, 7>& slots) {
    int32_t sum = 0;
    int32_t count = 0;
    for (const auto& s : slots) {
        if (!s.present) continue;
        sum += s.grade;
        ++count;
    }
    if (sum == 0 || count == 0) return 0;   // client falls back to basic here
    return rarityTier(sum / count);
}

void PartStatPackets::accumulatePart(std::array<float, 17>& acc,
                                     const std::array<float, 17>& stats,
                                     int32_t grade) {
    const double scale = static_cast<double>(gradeScale(grade));
    for (size_t k = 0; k < GRADE_SCALED_STATS; ++k) {
        const double s = static_cast<double>(stats[k]);
        acc[k] = static_cast<float>(scale * s + s + static_cast<double>(acc[k]));
    }
    for (size_t k = GRADE_SCALED_STATS; k < STAT_COUNT; ++k) {
        acc[k] = static_cast<float>(static_cast<double>(stats[k]) +
                                    static_cast<double>(acc[k]));
    }
}

StatAggregate PartStatPackets::aggregate(const std::array<float, 17>& kartBase,
                                         bool factoryScheme,
                                         const std::array<EquippedPart, 7>& slots,
                                         const std::string& kartModelName) {
    StatAggregate out;
    out.factoryScheme = factoryScheme;
    out.base = kartBase;
    out.bonus.fill(0.0f);
    out.tireExtra.fill(0.0f);

    if (factoryScheme) {
        for (const auto& s : slots) {
            if (!s.present) continue;
            accumulatePart(out.bonus, s.stats, s.grade);
        }
        // extras only ever come from the tires and they ignore grade
        if (slots[SLOT_TIRES].present) out.tireExtra = slots[SLOT_TIRES].tireExtra;

        bool matched = !kartModelName.empty();
        for (const auto& s : slots) {
            if (!s.present || s.modelName != kartModelName) { matched = false; break; }
        }
        out.fullSetMatched = matched;
    }

    for (size_t k = 0; k < STAT_COUNT; ++k) out.total[k] = out.base[k] + out.bonus[k];
    return out;
}

GarageBars PartStatPackets::garageBars(const std::array<float, 17>& kartBase,
                                       const std::array<float, 17>& bonus,
                                       const std::vector<std::array<float, 17>>& catalog) {
    GarageBars out;

    // garage stat bars compute 0x428AB0 wire bar one 0 and 1 two 2 three 8 10 11 four 3
    const size_t idxBar1[2] = {STAT_MAX_SPEED, STAT_BODY_SETUP};
    const size_t idxBar2[1] = {STAT_STEERING_GAIN};
    const size_t idxBar3[3] = {STAT_MINI_TURBO_HOLD, STAT_MINI_TURBO_THRESHOLD, STAT_DRIFT_CHARGE_RATE};
    const size_t idxBar4[1] = {STAT_MINI_TURBO_TARGET};

    float lo = 0.0f;
    float hi = 0.0f;

    scanRange(catalog, idxBar1, 2, lo, hi);
    float sum = bonus[STAT_MAX_SPEED] + kartBase[STAT_MAX_SPEED] +
                kartBase[STAT_BODY_SETUP] + bonus[STAT_BODY_SETUP];
    out.bar[0] = barFromSum(sum, lo, hi, out.fraction[0], out.degenerate[0]);

    scanRange(catalog, idxBar2, 1, lo, hi);
    sum = bonus[STAT_STEERING_GAIN] + kartBase[STAT_STEERING_GAIN];
    out.bar[1] = barFromSum(sum, lo, hi, out.fraction[1], out.degenerate[1]);

    scanRange(catalog, idxBar3, 3, lo, hi);
    sum = bonus[STAT_MINI_TURBO_HOLD] + bonus[STAT_MINI_TURBO_THRESHOLD] + bonus[STAT_DRIFT_CHARGE_RATE] +
          kartBase[STAT_MINI_TURBO_HOLD] + kartBase[STAT_MINI_TURBO_THRESHOLD] + kartBase[STAT_DRIFT_CHARGE_RATE];
    out.bar[2] = barFromSum(sum, lo, hi, out.fraction[2], out.degenerate[2]);

    scanRange(catalog, idxBar4, 1, lo, hi);
    sum = bonus[STAT_MINI_TURBO_TARGET] + kartBase[STAT_MINI_TURBO_TARGET];
    out.bar[3] = barFromSum(sum, lo, hi, out.fraction[3], out.degenerate[3]);

    return out;
}

DerivedPhysics PartStatPackets::derive(const std::array<float, 17>& total, bool drifting) {
    DerivedPhysics d;

    // wire indices the old enum sat two slots high
    d.accelScale        = clampF(total[STAT_BODY_SETUP] * 0.01f + 1.0f, 1.0f, 1.01f);
    d.maxSpeedMul       = clampF(total[STAT_MAX_SPEED] + 1.0f, 1.0f, 2.0f);
    d.steeringGain      = clampF(total[STAT_STEERING_GAIN] * 3.0f + 1.0f, 1.0f, 4.0f);
    d.miniTurboSpeedMul = clampF(total[STAT_MINI_TURBO_TARGET] * 0.2f + 1.0f, 1.0f, 1.2f);

    // original snaps down instead of clamping up keep the bug
    float turn = total[STAT_TURN_FORCE] + 1.0f;
    if (turn < 1.0f) turn = 1.0f;
    else if (turn > 2.0f) turn = drifting ? 1.6f : 1.0f;
    d.turnForceMul = turn;

    d.wheelSpin       = total[STAT_WHEEL_SPIN];
    d.wheelSteerAngle = total[STAT_WHEEL_STEER_ANGLE];

    d.driftChargeRate = clampF(total[STAT_DRIFT_CHARGE_RATE] * 0.5f + 0.3f, 0.3f, 0.8f);
    d.driftSteer      = clampF(total[STAT_DRIFT_STEER] * 0.6f + 1.2f, 1.2f, 1.8f);

    // inverted so a higher stat means an easier mini turbo
    d.miniTurboThreshold = clampF(1.0f - total[STAT_MINI_TURBO_THRESHOLD] * 0.8f, 0.2f, 1.0f);
    d.miniTurboHold      = clampF(1.0f - total[STAT_MINI_TURBO_HOLD] * 0.8f, 0.2f, 1.0f);

    d.grip = total[STAT_GRIP];   // no clamp anywhere in the client
    return d;
}

std::vector<std::string> PartStatPackets::statBlockIssues(const std::array<float, 17>& total) {
    std::vector<std::string> out;

    for (size_t k = 0; k < STAT_COUNT; ++k) {
        if (!std::isfinite(total[k])) {
            out.push_back("stat index " + std::to_string(k) + " is not finite");
        }
    }

    if (total[STAT_GRIP] == 0.0f) {
        out.push_back("wire stat 12 is zero, the per wheel grip collapses and the kart cannot move");
    }
    if (total[STAT_GRIP] < 0.0f) {
        out.push_back("wire stat 12 is negative, the grip term inverts");
    }
    if (total[STAT_TURN_FORCE] > 1.0f) {
        out.push_back("wire stat 5 above one snaps the turn force back down to one");
    }
    if (total[STAT_MAX_SPEED] <= 0.0f) {
        out.push_back("wire stat 1 gives no max speed gain, the ceiling stays at 320");
    }
    if (total[STAT_NO_READ_13] != 0.0f) {
        out.push_back("wire stat 13 has no read site in this build");
    }
    if (total[STAT_CAMERA_DISTANCE] <= 0.0f) {
        out.push_back("wire stat 14 is the chase camera distance, zero puts the camera in the car, the shipped rows carry 9");
    }
    return out;
}

std::array<float, 17> PartStatPackets::unprovenPlayableStatBlock() {
    // wire 12 grip 1 so the kart moves and the shipped camera tail 9 37 3 5
    std::array<float, 17> s{};
    s.fill(0.0f);
    s[STAT_GRIP] = 1.0f;
    s[STAT_CAMERA_DISTANCE] = 9.0f;
    s[STAT_CAMERA_PITCH] = 37.0f;
    s[STAT_CAMERA_HEIGHT] = 3.5f;
    return s;
}

bool PartStatPackets::parsePartUseNotify(const Packet& pkt, PartUseNotify& out) {
    out = PartUseNotify();
    const std::vector<uint8_t>& p = pkt.payload();
    if (p.size() < 0x1D) {
        LOG_WARN("PACKET", "partstat use notify short " + std::to_string(p.size()) + " bytes");
        return false;
    }

    Packet copy = pkt;
    copy.resetReadPos();
    out.instanceId  = copy.readUInt32();
    out.baseKey     = copy.readUInt32();
    out.word08      = copy.readUInt32();
    out.word0C      = copy.readUInt32();
    out.periodMode  = copy.readUInt32();
    out.periodValue = copy.readInt32();
    out.activeFlag  = copy.readUInt32();
    out.flag        = copy.readUInt8();
    out.valid       = true;

    if (out.periodMode != 2) {
        // client only ever sends this on mode two so anything else is forged
        LOG_WARN("PACKET", "partstat use notify instance " + std::to_string(out.instanceId) +
                           " claims period mode " + std::to_string(out.periodMode));
    }
    return true;
}

std::vector<ItemDefRow> PartStatPackets::loadItemDefs() {
    std::vector<ItemDefRow> out;
    // the Item tab draws rows in wire order the video shows Repair Kit Gold Coin then Slot Exchange
    auto rows = Database::instance().queryPrepared(
        "SELECT item_key, visible, badge, use_type, word_10, display_name_key, "
        "icon_key, description_key FROM def_item_wire ORDER BY item_key DESC", {});

    std::vector<uint32_t> keys;
    for (const auto& r : rows) {
        ItemDefRow d;
        d.itemKey        = toU32(r, "item_key");
        d.visible        = toU32(r, "visible", 1);
        d.badge          = toU32(r, "badge");
        d.useType        = toU32(r, "use_type");
        d.requiredLevel  = toU32(r, "word_10");   // def item wire keeps the old column name
        d.iconName       = toStr(r, "icon_key");
        d.displayNameKey = toStr(r, "display_name_key");
        d.descriptionKey = toStr(r, "description_key");
        keys.push_back(d.itemKey);
        out.push_back(std::move(d));
    }

    auto opts = loadOptionsFor(OPT_CATEGORY_ITEM, keys);
    for (size_t i = 0; i < out.size(); ++i) out[i].priceRows = opts[i];
    return out;
}

std::vector<KartPartDefRow> PartStatPackets::loadKartPartDefs() {
    std::vector<KartPartDefRow> out;
    auto rows = Database::instance().queryPrepared(
        "SELECT part_key, visible, badge, required_level, model_name, restrict_target, equip_slot, "
        "restrict_key, display_name_key, description_key, ability_pair0_hex, ability_pair1_hex "
        "FROM def_kart_part_wire ORDER BY part_key LIMIT 256", {});

    std::vector<uint32_t> keys;
    for (const auto& r : rows) {
        KartPartDefRow d;
        d.partKey        = toU32(r, "part_key");
        d.visible        = toU32(r, "visible", 1);
        d.badge          = toU32(r, "badge");
        d.requiredLevel  = toU32(r, "required_level");
        d.modelName      = toStr(r, "model_name");
        d.restrictTarget = toU32(r, "restrict_target");
        d.equipSlot      = toU32(r, "equip_slot");
        d.restrictKey    = toU32(r, "restrict_key");
        d.displayNameKey = toStr(r, "display_name_key");
        d.descriptionKey = toStr(r, "description_key");
        decodeHex(toStr(r, "ability_pair0_hex"), d.abilityPair0.data(), d.abilityPair0.size());
        decodeHex(toStr(r, "ability_pair1_hex"), d.abilityPair1.data(), d.abilityPair1.size());
        keys.push_back(d.partKey);
        out.push_back(std::move(d));
    }

    auto opts = loadOptionsFor(OPT_CATEGORY_KART_PART, keys);
    for (size_t i = 0; i < out.size(); ++i) out[i].priceRows = opts[i];
    return out;
}

std::vector<std::array<float, 17>> PartStatPackets::loadKartStatCatalog() {
    std::vector<std::array<float, 17>> out;
    auto rows = Database::instance().queryPrepared(
        "SELECT stat_block_hex FROM def_kart_wire ORDER BY kart_key LIMIT 64", {});
    for (const auto& r : rows) {
        std::array<float, 17> s{};
        decodeStatBlock(toStr(r, "stat_block_hex"), s);
        out.push_back(s);
    }
    return out;
}

bool PartStatPackets::loadKartBase(uint32_t kartKey,
                                   std::array<float, 17>& statsOut,
                                   uint32_t& schemeOut,
                                   std::string& modelNameOut) {
    statsOut.fill(0.0f);
    schemeOut = 0;
    modelNameOut.clear();
    if (kartKey == 0) return false;

    auto rows = Database::instance().queryPrepared(
        "SELECT scheme_selector, model_name, stat_block_hex FROM def_kart_wire "
        "WHERE kart_key = ?",
        {static_cast<int>(kartKey)});
    if (rows.empty()) {
        LOG_WARN("DB", "partstat no kart wire row for key " + std::to_string(kartKey));
        return false;
    }

    schemeOut    = toU32(rows[0], "scheme_selector");
    modelNameOut = toStr(rows[0], "model_name");
    decodeStatBlock(toStr(rows[0], "stat_block_hex"), statsOut);
    return true;
}

bool PartStatPackets::loadPartBonus(uint32_t partKey, EquippedPart& out) {
    out = EquippedPart();
    if (partKey == 0) return false;

    auto rows = Database::instance().queryPrepared(
        "SELECT part_key, category, model_dir_name, stat_block_hex, wheel_attach_0, "
        "wheel_attach_1, wheel_attach_2 FROM carcraft_part_def WHERE part_key = ?",
        {static_cast<int>(partKey)});
    if (rows.empty()) {
        LOG_ERROR("DB", "partstat part key " + std::to_string(partKey) +
                        " has no definition, the garage aggregator derefs it unguarded");
        return false;
    }

    out.partKey   = toU32(rows[0], "part_key");
    out.modelName = toStr(rows[0], "model_dir_name");
    decodeStatBlock(toStr(rows[0], "stat_block_hex"), out.stats);

    // wire dwords are reused as float bits so read them back the same way
    const uint32_t w0 = toU32(rows[0], "wheel_attach_0");
    const uint32_t w1 = toU32(rows[0], "wheel_attach_1");
    const uint32_t w2 = toU32(rows[0], "wheel_attach_2");
    out.tireExtra[0] = bitsToFloat(w0);
    out.tireExtra[1] = bitsToFloat(w1);
    out.tireExtra[2] = bitsToFloat(w2);

    out.present = true;
    return true;
}

uint32_t PartStatPackets::kartKeyForInstance(uint32_t kartInstanceId) {
    if (kartInstanceId == 0) return 0;

    auto rows = Database::instance().queryPrepared(
        "SELECT base_key FROM owned_kart WHERE id = ?", {static_cast<int>(kartInstanceId)});
    if (rows.empty()) return 0;
    return toU32(rows[0], "base_key");
}

bool PartStatPackets::loadEquippedSlots(int32_t characterId,
                                        uint32_t kartInstanceId,
                                        std::array<EquippedPart, 7>& out) {
    for (auto& s : out) s = EquippedPart();

    auto rows = Database::instance().queryPrepared(
        "SELECT part_inst_cover, part_inst_tire, part_inst_booster, part_inst_bumper, "
        "part_inst_ffender, part_inst_rfender, part_inst_wing FROM custom_car_preset "
        "WHERE character_id = ? AND kart_instance_id = ? LIMIT 1",
        {characterId, static_cast<int>(kartInstanceId)});
    if (rows.empty()) return false;

    const char* cols[7] = {
        "part_inst_cover", "part_inst_tire", "part_inst_booster", "part_inst_bumper",
        "part_inst_ffender", "part_inst_rfender", "part_inst_wing"
    };

    for (size_t slot = 0; slot < SLOT_COUNT; ++slot) {
        const uint32_t instanceId = toU32(rows[0], cols[slot]);
        if (instanceId == 0) continue;

        auto inst = Database::instance().queryPrepared(
            "SELECT part_key, category, grade, equip_refcount, period_active "
            "FROM custom_car_part_instance WHERE character_id = ? AND instance_id = ?",
            {characterId, static_cast<int>(instanceId)});
        if (inst.empty()) {
            LOG_WARN("DB", "partstat preset slot " + std::to_string(slot) +
                           " points at unowned instance " + std::to_string(instanceId));
            continue;
        }
        if (toI32(inst[0], "equip_refcount") == 0) continue;   // zero means not equipped

        const uint32_t category = toU32(inst[0], "category");
        if (category != categoryForSlot(slot)) {
            LOG_WARN("DB", "partstat instance " + std::to_string(instanceId) +
                           " category " + std::to_string(category) +
                           " does not fit slot " + std::to_string(slot));
            continue;
        }

        EquippedPart part;
        if (!loadPartBonus(toU32(inst[0], "part_key"), part)) continue;
        part.grade = clampGrade(toI32(inst[0], "grade"));
        out[slot] = part;
    }
    return true;
}

bool PartStatPackets::aggregateForCharacter(int32_t characterId,
                                            uint32_t kartInstanceId,
                                            StatAggregate& out) {
    out = StatAggregate();

    const uint32_t kartKey = kartKeyForInstance(kartInstanceId);
    if (kartKey == 0) {
        LOG_WARN("DB", "partstat kart instance " + std::to_string(kartInstanceId) +
                       " has no definition key");
        return false;
    }

    std::array<float, 17> base{};
    uint32_t scheme = 0;
    std::string modelName;
    if (!loadKartBase(kartKey, base, scheme, modelName)) return false;

    std::array<EquippedPart, 7> slots{};
    loadEquippedSlots(characterId, kartInstanceId, slots);

    out = aggregate(base, scheme == 1, slots, modelName);
    return true;
}

bool PartStatPackets::barsForCharacter(int32_t characterId,
                                       uint32_t kartInstanceId,
                                       GarageBars& out) {
    out = GarageBars();
    StatAggregate agg;
    if (!aggregateForCharacter(characterId, kartInstanceId, agg)) return false;
    out = garageBars(agg.base, agg.bonus, loadKartStatCatalog());
    return true;
}

UpgradeResult PartStatPackets::applyUpgrade(int32_t characterId,
                                            uint32_t partInstanceId,
                                            int32_t gradeDelta) {
    UpgradeResult res;
    res.instanceId = partInstanceId;

    auto rows = Database::instance().queryPrepared(
        "SELECT part_key, grade FROM custom_car_part_instance "
        "WHERE character_id = ? AND instance_id = ?",
        {characterId, static_cast<int>(partInstanceId)});
    if (rows.empty()) {
        res.reason = "instance not owned";
        return res;
    }

    res.partKey  = toU32(rows[0], "part_key");
    res.oldGrade = toI32(rows[0], "grade");
    return setGrade(characterId, partInstanceId, res.oldGrade + gradeDelta);
}

UpgradeResult PartStatPackets::setGrade(int32_t characterId,
                                        uint32_t partInstanceId,
                                        int32_t newGrade) {
    UpgradeResult res;
    res.instanceId = partInstanceId;

    auto rows = Database::instance().queryPrepared(
        "SELECT part_key, grade FROM custom_car_part_instance "
        "WHERE character_id = ? AND instance_id = ?",
        {characterId, static_cast<int>(partInstanceId)});
    if (rows.empty()) {
        res.reason = "instance not owned";
        return res;
    }

    res.partKey  = toU32(rows[0], "part_key");
    res.oldGrade = toI32(rows[0], "grade");
    res.newGrade = clampGrade(newGrade);

    if (res.newGrade != newGrade) {
        LOG_WARN("DB", "partstat grade " + std::to_string(newGrade) + " clamped to " +
                       std::to_string(res.newGrade) + " for instance " +
                       std::to_string(partInstanceId));
    }

    if (!Database::instance().executePrepared(
            "UPDATE custom_car_part_instance SET grade = ? "
            "WHERE character_id = ? AND instance_id = ?",
            {res.newGrade, characterId, static_cast<int>(partInstanceId)})) {
        res.reason = "grade write failed";
        return res;
    }

    res.ok = true;
    LOG_INFO("DB", "partstat instance " + std::to_string(partInstanceId) + " grade " +
                   std::to_string(res.oldGrade) + " to " + std::to_string(res.newGrade));
    return res;
}

Packet PartStatPackets::upgradeAck(int32_t characterId, uint32_t partInstanceId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT instance_id, part_key, category, equip_refcount, price_table_key, "
        "period_type, period_value, period_active, grade FROM custom_car_part_instance "
        "WHERE character_id = ? AND instance_id = ?",
        {characterId, static_cast<int>(partInstanceId)});

    if (rows.empty()) {
        // pushing a record the client cannot resolve is worse than pushing nothing
        LOG_ERROR("PACKET", "partstat upgrade ack for unknown instance " +
                            std::to_string(partInstanceId));
        return make(OP_S_PART_INSTANCE);
    }

    CarPartInstance inst;
    inst.instanceId    = toU32(rows[0], "instance_id");
    inst.partKey       = toU32(rows[0], "part_key");
    inst.category      = toU32(rows[0], "category");
    inst.equipRefcount = toI32(rows[0], "equip_refcount");
    inst.priceTableKey = toU32(rows[0], "price_table_key");
    inst.periodType    = toU32(rows[0], "period_type");
    inst.periodValue   = toI32(rows[0], "period_value");
    inst.periodActive  = toU32(rows[0], "period_active", 1);
    inst.grade         = clampGrade(toI32(rows[0], "grade"));

    return CustomCarPackets::partInstance(inst);
}

bool PartStatPackets::isRepairItemType(uint32_t useType) {
    return useType >= REPAIR_USE_TYPE_MIN && useType <= REPAIR_USE_TYPE_MAX;
}

bool PartStatPackets::loadItemUseType(uint32_t itemKey, uint32_t& useTypeOut) {
    useTypeOut = 0;
    if (itemKey == 0) return false;
    auto rows = Database::instance().queryPrepared(
        "SELECT use_type FROM def_item_wire WHERE item_key = ?",
        {static_cast<int>(itemKey)});
    if (rows.empty()) return false;
    useTypeOut = toU32(rows[0], "use_type");
    return true;
}

bool PartStatPackets::loadDurability(int32_t characterId,
                                     uint32_t kartInstanceId,
                                     int32_t& out) {
    out = 0;
    auto rows = Database::instance().queryPrepared(
        "SELECT period_mode, period_value FROM owned_kart WHERE id = ? AND character_id = ?",
        {static_cast<int>(kartInstanceId), characterId});
    if (rows.empty()) return false;
    if (toU32(rows[0], "period_mode") != DURABILITY_PERIOD_MODE) return false;
    out = toI32(rows[0], "period_value");
    return true;
}

RepairResult PartStatPackets::applyRepair(int32_t characterId,
                                          uint32_t kartInstanceId,
                                          uint32_t itemKey,
                                          int32_t restoreAmount,
                                          int32_t durabilityCap) {
    RepairResult res;
    res.kartInstanceId = kartInstanceId;
    res.itemKey        = itemKey;

    if (!loadItemUseType(itemKey, res.useType)) {
        res.reason = "item definition missing";
        return res;
    }
    if (!isRepairItemType(res.useType)) {
        // client refuses the same way before it draws MSG REPAIR USE
        res.reason = "item is not a repair scroll";
        return res;
    }

    auto rows = Database::instance().queryPrepared(
        "SELECT period_mode, period_value FROM owned_kart WHERE id = ? AND character_id = ?",
        {static_cast<int>(kartInstanceId), characterId});
    if (rows.empty()) {
        res.reason = "kart not owned";
        return res;
    }
    if (toU32(rows[0], "period_mode") != DURABILITY_PERIOD_MODE) {
        res.reason = "kart is not on durability mode";
        return res;
    }

    res.oldDurability = toI32(rows[0], "period_value");
    res.wasLow        = res.oldDurability <= DURABILITY_LOW_WARN;

    // the shared rule adds capped at the client max a smaller caller cap still wins
    int32_t next = res.oldDurability;
    kartDurabilityAfterRepair(DURABILITY_PERIOD_MODE, res.oldDurability, restoreAmount, next);
    if (durabilityCap >= 0 && next > durabilityCap) next = durabilityCap;
    res.newDurability = next;

    if (!Database::instance().executePrepared(
            "UPDATE owned_kart SET period_value = ? WHERE id = ? AND character_id = ?",
            {res.newDurability, static_cast<int>(kartInstanceId), characterId})) {
        res.reason = "durability write failed";
        return res;
    }

    res.ok = true;
    LOG_INFO("DB", "partstat repaired kart " + std::to_string(kartInstanceId) + " from " +
                   std::to_string(res.oldDurability) + " to " +
                   std::to_string(res.newDurability));
    return res;
}

bool PartStatPackets::consumeDurability(int32_t characterId,
                                        uint32_t kartInstanceId,
                                        int32_t amount) {
    if (amount <= 0) return true;

    int32_t current = 0;
    if (!loadDurability(characterId, kartInstanceId, current)) return false;

    // the shared rule floors at zero the client warns at zero and never goes under
    int32_t next = current;
    kartDurabilityAfterWear(DURABILITY_PERIOD_MODE, current, amount, next);

    return Database::instance().executePrepared(
        "UPDATE owned_kart SET period_value = ? WHERE id = ? AND character_id = ?",
        {next, static_cast<int>(kartInstanceId), characterId});
}

}  // namespace knc
