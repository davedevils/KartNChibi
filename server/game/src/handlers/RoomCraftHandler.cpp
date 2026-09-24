/// room craft handlers RoomEditer stage 19

#include "handlers/RoomCraftHandler.h"
#include "GameServer.h"
#include "game/Room.h"
#include "net/Protocol.h"
#include "db/Database.h"
#include "logging/Logger.h"

#include <algorithm>
#include <cmath>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace knc {

namespace {

// catalog rows past the client cap never reach it so their instances resolve to zero
void auditCatalogSizeOnce() {
    static std::once_flag flag;
    std::call_once(flag, []() {
        auto rows = Database::instance().queryPrepared(
            "SELECT COUNT(*) AS n FROM room_object_def", {});
        if (rows.empty()) return;

        size_t n = 0;
        auto it = rows[0].find("n");
        if (it != rows[0].end() && !it->second.empty()) {
            try {
                n = static_cast<size_t>(std::stoul(it->second));
            } catch (const std::exception&) {
                return;
            }
        }

        if (n == 0) {
            LOG_ERROR("ROOMCRAFT", "room_object_def is empty so every decor key misses "
                                   "and object count must stay zero on 0x0013");
            return;
        }
        if (n > RoomCraftPackets::CATALOG_CAP) {
            LOG_ERROR("ROOMCRAFT", "room_object_def has " + std::to_string(n) +
                                   " rows over client cap " +
                                   std::to_string(RoomCraftPackets::CATALOG_CAP) +
                                   " extra keys render nothing");
        }
    });
}

bool recordFloatsSane(const RoomObjectInstance& r) {
    return std::isfinite(r.pos0) && std::isfinite(r.pos1) &&
           std::isfinite(r.pos2) && std::isfinite(r.yaw);
}

// object key category and period stay server owned the client only moves things
bool serverFieldsMatch(const RoomObjectInstance& held, const RoomObjectInstance& wire) {
    return held.objectKey == wire.objectKey &&
           held.category == wire.category &&
           held.priceKey == wire.priceKey &&
           held.periodType == wire.periodType &&
           held.periodValue == wire.periodValue &&
           held.activeFlag == wire.activeFlag;
}

std::u16string widen(const std::string& in) {
    std::u16string out;
    out.reserve(in.size());
    for (unsigned char c : in) out.push_back(static_cast<char16_t>(c));
    return out;
}

} // namespace

// a room needs a category 1 record so sub 488300 loads track COL or it answers Set body fail 1
std::vector<RoomObjectInstance> RoomCraftHandler::defaultFloorDecor() {
    std::vector<RoomObjectInstance> out;
    auto& db = Database::instance();

    // category so the instance ids stay stable and unique
    const std::pair<uint32_t, uint32_t> wanted[] = { {0u, 1u}, {1u, 2u} };
    for (const auto& w : wanted) {
        auto rows = db.queryPrepared(
            "SELECT object_key FROM room_object_def WHERE category = ? AND enabled = 1 "
            "ORDER BY object_key LIMIT 1", {static_cast<int32_t>(w.first)});
        if (rows.empty()) {
            if (w.first == 1) {
                LOG_ERROR("ROOMCRAFT", "room_object_def has no floor row so every room will "
                                       "answer Set body fail, seed category 1 first");
            }
            continue;
        }
        RoomObjectInstance obj;
        obj.instanceId = w.second;
        obj.objectKey  = static_cast<uint32_t>(std::stoul(rows[0].at("object_key")));
        obj.category   = w.first;
        obj.placedFlag = 1;
        obj.activeFlag = 1;   // anything else and sub 488300 skips the record
        out.push_back(obj);
    }
    return out;
}

size_t RoomCraftHandler::appendRoomDecor(Packet& pkt, int32_t roomId, int32_t hostCharId) {
    std::vector<RoomObjectInstance> decor;
    // the room master's saved Room Craft is the room only placed rows go out the tray stays in the editor
    if (hostCharId > 0) {
        for (const RoomObjectInstance& r : RoomCraftPackets::loadPlayerInstances(hostCharId)) {
            if (r.placedFlag == 1 && r.activeFlag == 1) decor.push_back(r);
        }
    }
    if (decor.empty() && roomId > 0) decor = safeRoomDecor(roomId);

    const bool hasFloor = std::any_of(decor.begin(), decor.end(),
                                      [](const RoomObjectInstance& r) { return r.category == 1; });
    if (!hasFloor) {
        auto fallback = defaultFloorDecor();
        decor.insert(decor.begin(), fallback.begin(), fallback.end());
    }
    return RoomCraftPackets::appendDecorTail(pkt, decor);
}

Packet RoomCraftHandler::roomContext(uint32_t roomId, const std::u16string& roomName,
                                     int32_t maxPlayers, int32_t mode,
                                     int32_t hostCharacterId, int32_t weather) {
    Packet pkt(CMD::S_PLAYER_ROOM_DATA);

    // same layout as PacketBuilder playerRoomData read off the chibikart wire this used to put mode and host wrong
    (void)weather;
    // BCE1B0 id BCE22C seats BCE210 mode BCE220 host BCE244 flags BCE224 mirrors 0x118 BCE214 BCE218 BCE21C no reader
    pkt.writeInt32(static_cast<int32_t>(roomId));
    pkt.writeWString(roomName);

    pkt.writeInt32(maxPlayers);
    pkt.writeInt32(mode);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(0);
    pkt.writeInt32(hostCharacterId);
    pkt.writeInt32(0);

    appendRoomDecor(pkt, static_cast<int32_t>(roomId));
    return pkt;
}

void RoomCraftHandler::pruneClosedSessions() {
    for (auto it = m_sent.begin(); it != m_sent.end();) {
        if (it->second.owner.expired()) it = m_sent.erase(it);
        else ++it;
    }
}

void RoomCraftHandler::forgetSession(uint32_t sessionId) {
    std::lock_guard<std::mutex> lock(m_sentMutex);
    m_sent.erase(sessionId);
}

size_t RoomCraftHandler::pushObjectCatalog(Session::Ptr session, std::vector<Packet>* out) {
    if (!session) return 0;

    std::vector<RoomObjectDef> defs = RoomCraftPackets::loadObjectDefs();
    if (defs.empty()) return 0;

    std::vector<RoomObjectDef> fresh;
    bool firstBurst = false;
    {
        std::lock_guard<std::mutex> lock(m_sentMutex);
        pruneClosedSessions();

        SentRoomCraft& entry = m_sent[session->id()];
        entry.owner = session;
        // catalog is global so only the owned set follows the character
        if (entry.characterId != session->characterId) {
            entry.characterId = session->characterId;
            entry.instanceIds.clear();
        }
        firstBurst = entry.objectKeys.empty();
        for (const auto& d : defs) {
            // sub 4528E0 appends with no key compare so a resend duplicates the row
            if (!entry.objectKeys.insert(d.objectKey).second) continue;
            fresh.push_back(d);
        }
    }

    if (fresh.empty()) return 0;

    std::vector<Packet> frames;
    if (firstBurst) {
        // whole catalog in one pass so the cap trim happens in the builder
        frames = RoomCraftPackets::objectCatalog(fresh);
    } else {
        // rows added since the first burst so send them one by one
        frames.reserve(fresh.size());
        for (const auto& d : fresh) {
            frames.push_back(RoomCraftPackets::objectDefinition(d));
        }
        LOG_INFO("ROOMCRAFT", "catalog top up " + std::to_string(fresh.size()) +
                              " new keys for session " + std::to_string(session->id()));
    }

    if (out) for (Packet& p : frames) out->push_back(std::move(p));
    else for (const Packet& p : frames) session->send(p);

    LOG_INFO("ROOMCRAFT", "session " + std::to_string(session->id()) + " sent " +
                          std::to_string(frames.size()) + " object definitions");
    return frames.size();
}

size_t RoomCraftHandler::pushOwnedInstances(Session::Ptr session, std::vector<Packet>* out) {
    if (!session) return 0;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0) {
        LOG_WARN("ROOMCRAFT", "instance push skipped, session has no character");
        return 0;
    }

    // a fresh room owns a sky and a floor both placed as the reference burst grants them
    {
        auto have = Database::instance().queryPrepared(
            "SELECT COUNT(*) AS n FROM player_item_instance WHERE player_id = ?", {charId});
        if (!have.empty() && have[0].at("n") == "0") {
            Database::instance().executePrepared(
                "INSERT INTO player_item_instance (instance_id, player_id, object_key, category, "
                "pos_x, pos_y, pos_z, yaw, placed, price_key, period_type, period_value, active) VALUES "
                "(?, ?, 1001, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1), (?, ?, 2001, 1, 0, 0, 0, 0, 1, 0, 0, 0, 1)",
                {charId * 100 + 1, charId, charId * 100 + 2, charId});
            LOG_INFO("ROOMCRAFT", "default sky and floor placed for char " + std::to_string(charId));
        }
    }
    std::vector<RoomObjectInstance> insts = RoomCraftPackets::loadPlayerInstances(charId);
    if (insts.size() > RoomCraftPackets::INSTANCE_CAP) {
        // client drops the tail so keep the live rows in front
        std::stable_partition(insts.begin(), insts.end(),
                              [](const RoomObjectInstance& r) { return r.activeFlag == 1; });
        LOG_ERROR("ROOMCRAFT", "char " + std::to_string(charId) + " owns " +
                               std::to_string(insts.size()) + " instances over client cap " +
                               std::to_string(RoomCraftPackets::INSTANCE_CAP));
        insts.resize(RoomCraftPackets::INSTANCE_CAP);
    }

    std::vector<RoomObjectInstance> fresh;
    bool firstBurst = false;
    {
        std::lock_guard<std::mutex> lock(m_sentMutex);
        pruneClosedSessions();

        SentRoomCraft& entry = m_sent[session->id()];
        entry.owner = session;
        // catalog is global so only the owned set follows the character
        if (entry.characterId != session->characterId) {
            entry.characterId = session->characterId;
            entry.instanceIds.clear();
        }
        firstBurst = entry.instanceIds.empty();
        for (const auto& r : insts) {
            // sub 4523A0 appends with no key compare so a resend duplicates the row
            if (!entry.instanceIds.insert(r.instanceId).second) continue;
            fresh.push_back(r);
        }
    }

    if (fresh.empty()) return 0;

    std::vector<Packet> frames;
    if (firstBurst) {
        frames = RoomCraftPackets::ownedInstances(fresh);
    } else {
        // bought between two opens so only the new rows may go out
        frames.reserve(fresh.size());
        for (const auto& r : fresh) {
            frames.push_back(RoomCraftPackets::placedObject(r));
        }
        LOG_INFO("ROOMCRAFT", "instance top up " + std::to_string(fresh.size()) +
                              " new ids for char " + std::to_string(charId));
    }

    if (out) for (Packet& p : frames) out->push_back(std::move(p));
    else for (const Packet& p : frames) session->send(p);

    LOG_INFO("ROOMCRAFT", "char " + std::to_string(charId) + " sent " +
                          std::to_string(frames.size()) + " owned instances");
    return frames.size();
}

void RoomCraftHandler::handleOpen(Session::Ptr session, GameServer* server) {
    if (!session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);
    if (charId == 0) {
        LOG_WARN("ROOMCRAFT", "open from " + session->remoteAddress() + " with no character");
    }

    // live chibikart answers 0x10E empty since the catalog and owned rows went out at login stage push stays last
    auditCatalogSizeOnce();

    // catalog first every instance key the client cannot resolve derefs plus 0x18
    pushObjectCatalog(session);
    pushOwnedInstances(session);
    session->send(PacketBuilder::emptyAck(CMD::S_ACK_10E));

    // sub 47FC20 clears the decor container so this is the editor working set
    if (server != nullptr && session->roomId != 0) {
        auto room = server->getRoom(session->roomId);
        if (room) {
            session->send(roomContext(room->id(), widen(room->settings().name),
                                      static_cast<int32_t>(room->settings().maxPlayers),
                                      static_cast<int32_t>(room->settings().mode),
                                      room->hostCharacterId(), 0));
        } else {
            LOG_WARN("ROOMCRAFT", "session " + std::to_string(session->id()) +
                                  " names room " + std::to_string(session->roomId) +
                                  " that no longer exists");
        }
    }

    // stage init snapshots every container so nothing may follow this frame
    session->send(RoomCraftPackets::stagePush());

    LOG_INFO("ROOMCRAFT", "stage nineteen pushed for char " + std::to_string(charId));
}

void RoomCraftHandler::handleSave(
        Session::Ptr session,
        const std::vector<std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>>& raw,
        const std::vector<RoomObjectInstance>& recs,
        GameServer* server) {
    (void)server;
    if (!session) return;

    const int32_t charId = static_cast<int32_t>(session->characterId);

    // every early out still answers else MSG WAIT never closes
    if (charId == 0) {
        LOG_WARN("ROOMCRAFT", "save from " + session->remoteAddress() + " with no character");
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    if (raw.empty()) {
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    // decode the bytes we would echo not a parallel decode the dispatcher made
    std::vector<RoomObjectInstance> wire;
    wire.reserve(raw.size());
    for (const auto& blob : raw) {
        wire.push_back(RoomCraftPackets::decodeRecord(blob.data()));
    }

    if (recs.size() != wire.size()) {
        LOG_ERROR("ROOMCRAFT", "save raw " + std::to_string(wire.size()) + " decoded " +
                               std::to_string(recs.size()) + " mismatch");
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }
    for (size_t i = 0; i < wire.size(); ++i) {
        if (wire[i].instanceId == recs[i].instanceId) continue;
        LOG_ERROR("ROOMCRAFT", "save blob " + std::to_string(i) + " decodes to id " +
                               std::to_string(wire[i].instanceId) + " not " +
                               std::to_string(recs[i].instanceId));
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    for (const RoomObjectInstance& r : wire) {
        // float column write and the client setter both choke on a non finite
        if (!recordFloatsSane(r)) {
            LOG_WARN("ROOMCRAFT", "save char " + std::to_string(charId) + " id " +
                                  std::to_string(r.instanceId) + " has non finite position");
            session->send(RoomCraftPackets::saveAckEmpty());
            return;
        }
    }

    std::vector<RoomObjectInstance> held = RoomCraftPackets::loadPlayerInstances(charId);
    if (held.empty()) {
        LOG_WARN("ROOMCRAFT", "save char " + std::to_string(charId) + " owns nothing");
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    // sub 47DA00 stores through the sub 452830 result with no null test
    if (!RoomCraftPackets::ackIdsAreHeld(held, wire)) {
        LOG_ERROR("ROOMCRAFT", "save char " + std::to_string(charId) +
                               " sent an unowned instance id, ack suppressed");
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    std::unordered_map<uint32_t, size_t> indexById;
    indexById.reserve(held.size());
    for (size_t i = 0; i < held.size(); ++i) {
        indexById[held[i].instanceId] = i;
    }

    std::vector<RoomObjectInstance> dirty;
    dirty.reserve(wire.size());
    std::unordered_set<uint32_t> seen;
    seen.reserve(wire.size());
    bool corrected = false;

    for (const RoomObjectInstance& r : wire) {
        auto it = indexById.find(r.instanceId);
        if (it == indexById.end()) continue;

        if (!seen.insert(r.instanceId).second) {
            LOG_WARN("ROOMCRAFT", "save char " + std::to_string(charId) + " repeats id " +
                                  std::to_string(r.instanceId));
            corrected = true;
            continue;
        }

        // only pos and yaw come from the wire request the rest is copied from held
        RoomObjectInstance merged = held[it->second];
        merged.pos0 = r.pos0;
        merged.pos1 = r.pos1;
        merged.pos2 = r.pos2;
        merged.yaw  = r.yaw;
        // sub 4523F0 counts one so anything else is not a placed row
        merged.placedFlag = (r.placedFlag != 0) ? 1u : 0u;

        if (!serverFieldsMatch(merged, r) || merged.placedFlag != r.placedFlag) {
            corrected = true;
        }

        held[it->second] = merged;
        dirty.push_back(merged);
    }

    if (dirty.empty()) {
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    // client skips both caps on the category two and four replace branch
    std::vector<RoomObjectDef> catalog = RoomCraftPackets::loadObjectDefs();
    std::string reason;
    if (!RoomCraftPackets::checkPlacementCaps(catalog, held, reason)) {
        LOG_WARN("ROOMCRAFT", "save char " + std::to_string(charId) + " refused, " + reason);
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    if (!RoomCraftPackets::persistSave(charId, dirty)) {
        LOG_ERROR("ROOMCRAFT", "save char " + std::to_string(charId) + " persist failed");
        session->send(RoomCraftPackets::saveAckEmpty());
        return;
    }

    if (corrected) {
        // client owns a wrong record so hand back server truth not its own bytes
        session->send(RoomCraftPackets::saveAck(dirty));
        LOG_WARN("ROOMCRAFT", "save char " + std::to_string(charId) +
                              " acked with server records the client bytes were wrong");
    } else {
        // echo the verbatim blobs so every acked id is one the client already holds
        session->send(RoomCraftPackets::saveAckEcho(raw));
    }

    LOG_INFO("ROOMCRAFT", "save char " + std::to_string(charId) + " stored " +
                          std::to_string(dirty.size()) + " records");
}

std::vector<RoomObjectInstance> RoomCraftHandler::safeRoomDecor(int32_t roomId) {
    std::vector<RoomObjectInstance> decor = RoomCraftPackets::loadRoomDecor(roomId);
    if (decor.empty()) return decor;

    std::vector<RoomObjectDef> catalog = RoomCraftPackets::loadObjectDefs();
    const size_t killed = RoomCraftPackets::sanitizeDecor(catalog, decor);
    if (killed != 0) {
        LOG_WARN("ROOMCRAFT", "room " + std::to_string(roomId) + " dropped " +
                              std::to_string(killed) + " decor records with an unknown key");
    }

    // sub 488300 skips a record whose active flag is not one so it only wastes a frame
    decor.erase(std::remove_if(decor.begin(), decor.end(),
                               [](const RoomObjectInstance& r) { return r.activeFlag != 1; }),
                decor.end());
    return decor;
}

} // namespace knc
