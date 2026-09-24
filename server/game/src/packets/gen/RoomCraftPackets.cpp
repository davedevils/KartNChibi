#include "packets/gen/RoomCraftPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace knc {

namespace {

void putU32(uint8_t* p, size_t off, uint32_t v) {
    p[off + 0] = static_cast<uint8_t>(v & 0xFF);
    p[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

uint32_t getU32(const uint8_t* p, size_t off) {
    return static_cast<uint32_t>(p[off + 0]) |
           (static_cast<uint32_t>(p[off + 1]) << 8) |
           (static_cast<uint32_t>(p[off + 2]) << 16) |
           (static_cast<uint32_t>(p[off + 3]) << 24);
}

void putF32(uint8_t* p, size_t off, float v) {
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    putU32(p, off, bits);
}

float getF32(const uint8_t* p, size_t off) {
    uint32_t bits = getU32(p, off);
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

// sub 44EB30 copies strlen plus one into a fixed stack buffer overlong smashes the frame
std::string clampAscii(const std::string& src, size_t cap, const char* what) {
    size_t nulProcessedLen = 0;
    std::string out = clampAsciiCore(src, cap, AsciiNulMode::kStopAtNul, nulProcessedLen);
    if (nulProcessedLen > cap) {
        LOG_ERROR("PACKET", std::string("roomcraft ") + what + " len " +
                            std::to_string(nulProcessedLen) + " over cap " + std::to_string(cap));
    }
    return out;
}

uint32_t rowU32(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<uint32_t>(rowUInt64Throwing(row, key, 0));
}

float rowF32(const std::map<std::string, std::string>& row, const char* key) {
    return rowFloatThrowing(row, key, 0.0f);
}

std::string rowStr(const std::map<std::string, std::string>& row, const char* key) {
    return rowStrCore(row, key);
}

RoomObjectInstance instanceFromRow(const std::map<std::string, std::string>& row) {
    RoomObjectInstance inst;
    inst.instanceId  = rowU32(row, "instance_id");
    inst.objectKey   = rowU32(row, "object_key");
    inst.category    = rowU32(row, "category");
    inst.pos0        = rowF32(row, "pos_x");
    inst.pos1        = rowF32(row, "pos_y");
    inst.pos2        = rowF32(row, "pos_z");
    inst.yaw         = rowF32(row, "yaw");
    inst.placedFlag  = rowU32(row, "placed");
    inst.priceKey    = rowU32(row, "price_key");
    inst.periodType  = rowU32(row, "period_type");
    inst.periodValue = rowU32(row, "period_value");
    inst.activeFlag  = rowU32(row, "active");
    return inst;
}

} // namespace


std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>
RoomCraftPackets::decorRecord(const RoomObjectInstance& inst) {
    std::array<uint8_t, RECORD_SIZE> r{};
    putU32(r.data(), 0x00, inst.instanceId);
    putU32(r.data(), 0x04, inst.objectKey);
    putU32(r.data(), 0x08, inst.category);
    // raw float bits never reinterpret the axis order is unresolved
    putF32(r.data(), 0x0C, inst.pos0);
    putF32(r.data(), 0x10, inst.pos1);
    putF32(r.data(), 0x14, inst.pos2);
    putF32(r.data(), 0x18, inst.yaw);
    putU32(r.data(), 0x1C, inst.placedFlag);
    putU32(r.data(), 0x20, inst.priceKey);
    putU32(r.data(), 0x24, inst.periodType);
    putU32(r.data(), 0x28, inst.periodValue);
    putU32(r.data(), 0x2C, inst.activeFlag);
    return r;
}

RoomObjectInstance RoomCraftPackets::decodeRecord(const uint8_t* rec48) {
    RoomObjectInstance inst;
    if (!rec48) return inst;
    inst.instanceId  = getU32(rec48, 0x00);
    inst.objectKey   = getU32(rec48, 0x04);
    inst.category    = getU32(rec48, 0x08);
    inst.pos0        = getF32(rec48, 0x0C);
    inst.pos1        = getF32(rec48, 0x10);
    inst.pos2        = getF32(rec48, 0x14);
    inst.yaw         = getF32(rec48, 0x18);
    inst.placedFlag  = getU32(rec48, 0x1C);
    inst.priceKey    = getU32(rec48, 0x20);
    inst.periodType  = getU32(rec48, 0x24);
    inst.periodValue = getU32(rec48, 0x28);
    inst.activeFlag  = getU32(rec48, 0x2C);
    return inst;
}


Packet RoomCraftPackets::objectDefinition(const RoomObjectDef& def) {
    Packet pkt = Packet::fromCmdFull(OP_OBJECT_DEF);

    pkt.writeUInt32(def.enabled);
    pkt.writeUInt32(def.badge);
    pkt.writeUInt32(def.objectKey);
    pkt.writeUInt32(def.category);
    pkt.writeUInt32(def.maxPlaceable);
    pkt.writeUInt32(def.requiredLevel);

    // three back to back NUL terminated ascii strings no padding no length prefix
    const std::string asset = clampAscii(def.assetFolder, ASSET_FOLDER_MAX, "asset_folder");
    const std::string name  = clampAscii(def.nameLocKey, NAME_LOC_KEY_MAX, "name_loc_key");
    const std::string desc  = clampAscii(def.descLocKey, DESC_LOC_KEY_MAX, "desc_loc_key");
    pkt.writeString(asset);
    pkt.writeString(name);
    pkt.writeString(desc);

    // sub 451CF0 refuses slot five so never claim more than four
    const size_t count = std::min(def.prices.size(), PRICE_SLOT_CAP);
    if (def.prices.size() > PRICE_SLOT_CAP) {
        LOG_WARN("PACKET", "roomcraft objectDefinition key " + std::to_string(def.objectKey) +
                           " price slots " + std::to_string(def.prices.size()) + " trimmed to 4");
    }
    // an empty list null derefs the tile draw one zero row is inert
    const size_t written = count == 0 ? 1 : count;
    pkt.writeInt32(static_cast<int32_t>(written));
    if (count == 0) for (int i = 0; i < 4; ++i) pkt.writeUInt32(0);
    for (size_t i = 0; i < count; ++i) {
        const RoomObjectPrice& p = def.prices[i];
        pkt.writeUInt32(p.currencyKey);
        pkt.writeUInt32(p.periodType);
        pkt.writeUInt32(p.periodValue);
        pkt.writeUInt32(p.extra);
    }

    // wrong size here silently desyncs the whole stream loud is better
    const size_t expected = 24 + (asset.size() + 1) + (name.size() + 1) + (desc.size() + 1) +
                            4 + 16 * written;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "objectDefinition size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

std::vector<Packet> RoomCraftPackets::objectCatalog(const std::vector<RoomObjectDef>& defs) {
    std::vector<Packet> out;
    const size_t count = std::min(defs.size(), CATALOG_CAP);
    if (defs.size() > CATALOG_CAP) {
        LOG_WARN("PACKET", "roomcraft catalog " + std::to_string(defs.size()) +
                           " over cap trimmed to " + std::to_string(CATALOG_CAP));
    }
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.push_back(objectDefinition(defs[i]));
    }
    return out;
}

Packet RoomCraftPackets::placedObject(const RoomObjectInstance& inst) {
    Packet pkt = Packet::fromCmdFull(OP_PLACED_OBJECT);

    const std::array<uint8_t, RECORD_SIZE> rec = decorRecord(inst);
    pkt.writeBytes(rec.data(), rec.size());

    if (pkt.payload().size() != RECORD_SIZE) {
        LOG_ERROR("PACKET", "placedObject size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(RECORD_SIZE));
    }
    return pkt;
}

std::vector<Packet> RoomCraftPackets::ownedInstances(const std::vector<RoomObjectInstance>& insts) {
    std::vector<Packet> out;
    const size_t count = std::min(insts.size(), INSTANCE_CAP);
    if (insts.size() > INSTANCE_CAP) {
        // dropped ids become 0x010F ack crash bait later
        LOG_WARN("PACKET", "roomcraft instances " + std::to_string(insts.size()) +
                           " over cap trimmed to " + std::to_string(INSTANCE_CAP));
    }
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        out.push_back(placedObject(insts[i]));
    }
    return out;
}

Packet RoomCraftPackets::stagePush() {
    // sub 47D9D0 reads no payload at all any byte here desyncs the stream
    Packet pkt = Packet::fromCmdFull(OP_STAGE_PUSH);
    if (pkt.payload().size() != 0) {
        LOG_ERROR("PACKET", "stagePush size " + std::to_string(pkt.payload().size()) +
                            " expected 0");
    }
    return pkt;
}

Packet RoomCraftPackets::saveAck(const std::vector<RoomObjectInstance>& records) {
    Packet pkt = Packet::fromCmdFull(OP_SAVE);

    const size_t count = std::min(records.size(), SAVE_ACK_MAX_RECS);
    if (records.size() > SAVE_ACK_MAX_RECS) {
        LOG_WARN("PACKET", "roomcraft saveAck " + std::to_string(records.size()) +
                           " over frame budget trimmed to " + std::to_string(SAVE_ACK_MAX_RECS));
    }
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const std::array<uint8_t, RECORD_SIZE> rec = decorRecord(records[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    const size_t expected = 4 + RECORD_SIZE * count;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "saveAck size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet RoomCraftPackets::saveAckEcho(const std::vector<std::array<uint8_t, RECORD_SIZE>>& raw) {
    Packet pkt = Packet::fromCmdFull(OP_SAVE);

    const size_t count = std::min(raw.size(), SAVE_ACK_MAX_RECS);
    if (raw.size() > SAVE_ACK_MAX_RECS) {
        LOG_WARN("PACKET", "roomcraft saveAckEcho " + std::to_string(raw.size()) +
                           " over frame budget trimmed to " + std::to_string(SAVE_ACK_MAX_RECS));
    }
    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        pkt.writeBytes(raw[i].data(), raw[i].size());
    }

    const size_t expected = 4 + RECORD_SIZE * count;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "saveAckEcho size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return pkt;
}

Packet RoomCraftPackets::saveAckEmpty() {
    Packet pkt = Packet::fromCmdFull(OP_SAVE);
    pkt.writeInt32(0);
    if (pkt.payload().size() != 4) {
        LOG_ERROR("PACKET", "saveAckEmpty size " + std::to_string(pkt.payload().size()) +
                            " expected 4");
    }
    return pkt;
}

size_t RoomCraftPackets::appendDecorTail(Packet& pkt, const std::vector<RoomObjectInstance>& decor) {
    const size_t used = pkt.payload().size();
    size_t budget = 0;
    if (FRAME_PAYLOAD_MAX > used + 4) {
        budget = (FRAME_PAYLOAD_MAX - used - 4) / RECORD_SIZE;
    }

    size_t count = std::min(decor.size(), budget);
    count = std::min(count, INSTANCE_CAP);
    if (count < decor.size()) {
        LOG_WARN("PACKET", "roomcraft decor tail " + std::to_string(decor.size()) +
                           " trimmed to " + std::to_string(count));
    }

    pkt.writeInt32(static_cast<int32_t>(count));
    for (size_t i = 0; i < count; ++i) {
        const std::array<uint8_t, RECORD_SIZE> rec = decorRecord(decor[i]);
        pkt.writeBytes(rec.data(), rec.size());
    }

    const size_t expected = used + 4 + RECORD_SIZE * count;
    if (pkt.payload().size() != expected) {
        LOG_ERROR("PACKET", "appendDecorTail size " + std::to_string(pkt.payload().size()) +
                            " expected " + std::to_string(expected));
    }
    return count;
}


bool RoomCraftPackets::parseSaveRequest(const std::vector<uint8_t>& payload,
                                        std::vector<std::array<uint8_t, RECORD_SIZE>>& rawOut,
                                        std::vector<RoomObjectInstance>& out) {
    rawOut.clear();
    out.clear();

    if (payload.size() < 4) {
        LOG_WARN("PACKET", "roomcraft save request too short " + std::to_string(payload.size()));
        return false;
    }

    const uint32_t rawCount = getU32(payload.data(), 0);
    // client count comes straight off a container field so treat anything huge as garbage
    if (rawCount > INSTANCE_CAP) {
        LOG_WARN("PACKET", "roomcraft save request count " + std::to_string(rawCount) +
                           " over cap rejected");
        return false;
    }

    const size_t count = static_cast<size_t>(rawCount);
    if (payload.size() < 4 + RECORD_SIZE * count) {
        LOG_WARN("PACKET", "roomcraft save request truncated payload " +
                           std::to_string(payload.size()) + " count " + std::to_string(count));
        return false;
    }

    rawOut.reserve(count);
    out.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const uint8_t* src = payload.data() + 4 + RECORD_SIZE * i;
        std::array<uint8_t, RECORD_SIZE> blob{};
        std::memcpy(blob.data(), src, RECORD_SIZE);
        rawOut.push_back(blob);
        out.push_back(decodeRecord(src));
    }
    return true;
}


size_t RoomCraftPackets::sanitizeDecor(const std::vector<RoomObjectDef>& catalog,
                                       std::vector<RoomObjectInstance>& decor) {
    std::unordered_set<uint32_t> keys;
    keys.reserve(catalog.size());
    for (const RoomObjectDef& d : catalog) keys.insert(d.objectKey);

    size_t killed = 0;
    for (RoomObjectInstance& inst : decor) {
        if (inst.activeFlag != 1) continue;

        bool ok = true;
        if (inst.category == 3) {
            // sub 488300 switches on the raw key outside the range nothing renders
            ok = inst.objectKey >= PROP_KEY_MIN && inst.objectKey <= PROP_KEY_MAX;
        } else if (inst.category <= 4) {
            // a missing catalog row makes sub 452B50 return zero then sprintf reads 0x18
            ok = keys.find(inst.objectKey) != keys.end();
        } else {
            ok = false;
        }

        if (!ok) {
            inst.activeFlag = 0;
            ++killed;
            LOG_WARN("PACKET", "roomcraft decor id " + std::to_string(inst.instanceId) +
                               " key " + std::to_string(inst.objectKey) +
                               " cat " + std::to_string(inst.category) + " deactivated");
        }
    }
    return killed;
}

bool RoomCraftPackets::checkPlacementCaps(const std::vector<RoomObjectDef>& catalog,
                                          const std::vector<RoomObjectInstance>& insts,
                                          std::string& reason) {
    std::unordered_map<uint32_t, uint32_t> maxByKey;
    maxByKey.reserve(catalog.size());
    for (const RoomObjectDef& d : catalog) maxByKey[d.objectKey] = d.maxPlaceable;

    std::unordered_map<uint32_t, size_t> perCategory;
    std::unordered_map<uint32_t, size_t> perKey;
    for (const RoomObjectInstance& inst : insts) {
        if (inst.placedFlag != 1) continue;
        ++perCategory[inst.category];
        ++perKey[inst.objectKey];
    }

    for (const auto& kv : perCategory) {
        if (kv.second > PLACED_PER_CATEGORY_CAP) {
            reason = "category " + std::to_string(kv.first) + " has " +
                     std::to_string(kv.second) + " placed over cap " +
                     std::to_string(PLACED_PER_CATEGORY_CAP);
            return false;
        }
    }

    for (const auto& kv : perKey) {
        auto it = maxByKey.find(kv.first);
        if (it == maxByKey.end()) {
            reason = "object key " + std::to_string(kv.first) + " absent from catalog";
            return false;
        }
        if (kv.second > static_cast<size_t>(it->second)) {
            reason = "object key " + std::to_string(kv.first) + " placed " +
                     std::to_string(kv.second) + " over max " + std::to_string(it->second);
            return false;
        }
    }

    reason.clear();
    return true;
}

bool RoomCraftPackets::ackIdsAreHeld(const std::vector<RoomObjectInstance>& held,
                                     const std::vector<RoomObjectInstance>& ack) {
    std::unordered_set<uint32_t> ids;
    ids.reserve(held.size());
    for (const RoomObjectInstance& h : held) ids.insert(h.instanceId);

    for (const RoomObjectInstance& a : ack) {
        // sub 47DA00 stores through the lookup result with no null test
        if (ids.find(a.instanceId) == ids.end()) {
            LOG_ERROR("PACKET", "roomcraft ack id " + std::to_string(a.instanceId) +
                                " not held by client would crash sub_47DA00");
            return false;
        }
    }
    return true;
}


std::vector<RoomObjectDef> RoomCraftPackets::loadObjectDefs() {
    std::vector<RoomObjectDef> defs;

    auto rows = Database::instance().queryPrepared(
        "SELECT object_key, enabled, badge, category, max_placeable, required_level, "
        "asset_folder, name_loc_key, desc_loc_key "
        "FROM room_object_def ORDER BY object_key", {});

    defs.reserve(rows.size());
    std::unordered_map<uint32_t, size_t> indexByKey;
    for (const auto& row : rows) {
        RoomObjectDef d;
        d.objectKey     = rowU32(row, "object_key");
        d.enabled       = rowU32(row, "enabled");
        d.badge         = rowU32(row, "badge");
        d.category      = rowU32(row, "category");
        d.maxPlaceable  = rowU32(row, "max_placeable");
        d.requiredLevel = rowU32(row, "required_level");
        d.assetFolder   = rowStr(row, "asset_folder");
        d.nameLocKey    = rowStr(row, "name_loc_key");
        d.descLocKey    = rowStr(row, "desc_loc_key");
        indexByKey[d.objectKey] = defs.size();
        defs.push_back(std::move(d));
    }

    auto priceRows = Database::instance().queryPrepared(
        "SELECT object_key, currency_key, period_type, period_value, extra "
        "FROM room_object_price ORDER BY object_key, slot_index", {});

    for (const auto& row : priceRows) {
        auto it = indexByKey.find(rowU32(row, "object_key"));
        if (it == indexByKey.end()) continue;
        RoomObjectDef& d = defs[it->second];
        if (d.prices.size() >= PRICE_SLOT_CAP) continue;
        RoomObjectPrice p;
        p.currencyKey = rowU32(row, "currency_key");
        p.periodType  = rowU32(row, "period_type");
        p.periodValue = rowU32(row, "period_value");
        p.extra       = rowU32(row, "extra");
        d.prices.push_back(p);
    }

    LOG_INFO("PACKET", "roomcraft loaded " + std::to_string(defs.size()) + " object defs");
    return defs;
}

std::vector<RoomObjectInstance> RoomCraftPackets::loadPlayerInstances(int32_t playerId) {
    std::vector<RoomObjectInstance> out;

    auto rows = Database::instance().queryPrepared(
        "SELECT instance_id, object_key, category, pos_x, pos_y, pos_z, yaw, placed, "
        "price_key, period_type, period_value, active "
        "FROM player_item_instance WHERE player_id = ? ORDER BY instance_id",
        { playerId });

    out.reserve(rows.size());
    for (const auto& row : rows) {
        out.push_back(instanceFromRow(row));
    }
    return out;
}

std::vector<RoomObjectInstance> RoomCraftPackets::loadRoomDecor(int32_t roomId) {
    std::vector<RoomObjectInstance> out;

    // inner join drops any key absent from the catalog the client was given
    auto rows = Database::instance().queryPrepared(
        "SELECT d.instance_id, d.object_key, d.category, d.pos_x, d.pos_y, d.pos_z, d.yaw, "
        "d.placed, d.price_key, d.period_type, d.period_value, d.active "
        "FROM room_decor d "
        "INNER JOIN room_object_def o ON o.object_key = d.object_key "
        "WHERE d.room_id = ? ORDER BY d.instance_id",
        { roomId });

    out.reserve(rows.size());
    for (const auto& row : rows) {
        out.push_back(instanceFromRow(row));
    }
    return out;
}

bool RoomCraftPackets::persistSave(int32_t playerId,
                                   const std::vector<RoomObjectInstance>& records) {
    if (records.empty()) return true;

    // client only diffs these five fields everything else stays server owned
    const char* sql =
        "UPDATE player_item_instance SET pos_x = ?, pos_y = ?, pos_z = ?, yaw = ?, placed = ? "
        "WHERE instance_id = ? AND player_id = ?";

    Transaction tx = Database::instance().beginTransaction();
    if (tx.valid()) {
        for (const RoomObjectInstance& r : records) {
            if (!tx.execute(sql, { r.pos0, r.pos1, r.pos2, r.yaw,
                                   static_cast<int>(r.placedFlag),
                                   static_cast<int>(r.instanceId), playerId })) {
                LOG_ERROR("PACKET", "roomcraft persistSave failed id " +
                                    std::to_string(r.instanceId));
                return false;
            }
        }
        return tx.commit();
    }

    bool ok = true;
    for (const RoomObjectInstance& r : records) {
        if (!Database::instance().executePrepared(sql, { r.pos0, r.pos1, r.pos2, r.yaw,
                                                         static_cast<int>(r.placedFlag),
                                                         static_cast<int>(r.instanceId),
                                                         playerId })) {
            LOG_ERROR("PACKET", "roomcraft persistSave failed id " +
                                std::to_string(r.instanceId));
            ok = false;
        }
    }
    return ok;
}

} // namespace knc
