#include "packets/gen/ItemPackets.h"
#include "packets/gen/MotionPackets.h"
#include "db/Database.h"
#include "logging/Logger.h"
#include "util/DbRowWire.h"

#include <cstring>
#include <cstdlib>
#include <map>
#include <string>

namespace knc {

namespace {

const char* const kItemNames[ItemPackets::ITEM_COUNT] = {
    "booster", "big_booster", "spike", "storm", "thunder", "handle",
    "turtle", "rabbit", "shield", "smoke", "rocket", "hive",
    "angel", "bluerabbit", "ice", "flash", "magnet", "hammer",
    "bomb", "dung", "devil", "devilRed"
};

int32_t readI32(const uint8_t* p) {
    int32_t v = 0;
    std::memcpy(&v, p, 4);
    return v;
}

int16_t readI16(const uint8_t* p) {
    int16_t v = 0;
    std::memcpy(&v, p, 2);
    return v;
}

float readF32(const uint8_t* p) {
    float v = 0.0f;
    std::memcpy(&v, p, 4);
    return v;
}

bool sizeOk(const char* tag, const std::vector<uint8_t>& body, size_t expected) {
    if (body.size() == expected) return true;
    LOG_WARN("PACKET", std::string(tag) + " size " + std::to_string(body.size()) +
                       " expected " + std::to_string(expected));
    return false;
}

void assertSize(const char* tag, const Packet& pkt, size_t expected) {
    if (pkt.payload().size() == expected) return;
    LOG_ERROR("PACKET", std::string(tag) + " size " + std::to_string(pkt.payload().size()) +
                        " expected " + std::to_string(expected));
}

int32_t clampSlotIndex(const char* tag, int32_t idx) {
    // client indexes the car table with this and never bounds checks it
    if (idx < 0 || idx >= ItemPackets::kSlotCount) {
        LOG_ERROR("PACKET", std::string(tag) + " slot index " + std::to_string(idx) + " clamped");
        if (idx < 0) return 0;
        return ItemPackets::kSlotCount - 1;
    }
    return idx;
}

int32_t safeSlotValue(const char* tag, int32_t value) {
    if (ItemPackets::slotValueValid(value)) return value;
    LOG_ERROR("PACKET", std::string(tag) + " item id " + std::to_string(value) + " out of range");
    return ItemPackets::ITEM_EMPTY;
}

bool effectCodeKnown(int16_t code) {
    switch (code) {
        case ItemPackets::EFFECT_SPIN:
        case ItemPackets::EFFECT_BUMP:
        case ItemPackets::EFFECT_CRASH:
        case ItemPackets::EFFECT_THUNDER_AREA:
        case ItemPackets::EFFECT_TURTLE_ZONE:
        case ItemPackets::EFFECT_RABBIT_LATCH:
        case ItemPackets::EFFECT_HIVE:
        case ItemPackets::EFFECT_HEAVY_STUN:
        case ItemPackets::EFFECT_BLUERABBIT_LATCH:
        case ItemPackets::EFFECT_ICE:
        case ItemPackets::EFFECT_FLASH:
            return true;
        default:
            return false;
    }
}

int32_t countHeld(const ItemPackets::PlayerItemState& st) {
    int32_t n = 0;
    for (int i = 0; i < ItemPackets::kSlotCount; ++i) {
        if (st.slots[i] != ItemPackets::ITEM_EMPTY) ++n;
    }
    return n;
}

// pending to slot with no delay check sub 4AECD0 does this before a new roll lands
bool commitNow(ItemPackets::PlayerItemState& st) {
    if (st.pendingItem == ItemPackets::ITEM_EMPTY) return false;

    int32_t idx = st.pendingSlot;
    if (idx < 0 || idx >= ItemPackets::kSlotCount) idx = 0;

    st.slots[idx]   = st.pendingItem;
    st.pendingItem  = ItemPackets::ITEM_EMPTY;
    st.pendingSlot  = 0;
    st.pendingAtMs  = 0;
    st.heldCount    = countHeld(st);
    return true;
}

int32_t rowInt(const std::map<std::string, std::string>& row, const char* key) {
    return static_cast<int32_t>(rowInt64NoThrow(row, key, 0));
}

} // namespace

bool ItemPackets::itemKindValid(int32_t kind) {
    return kind >= 0 && kind < ITEM_COUNT;
}

bool ItemPackets::slotValueValid(int32_t value) {
    return value == ITEM_EMPTY || itemKindValid(value);
}

const char* ItemPackets::itemName(int32_t kind) {
    if (!itemKindValid(kind)) return "invalid";
    return kItemNames[kind];
}

bool ItemPackets::spawnHasClientCase(int32_t kind) {
    switch (kind) {
        case ITEM_SPIKE: case ITEM_STORM: case ITEM_THUNDER: case ITEM_HANDLE:
        case ITEM_RABBIT: case ITEM_SHIELD: case ITEM_SMOKE: case ITEM_HIVE:
        case ITEM_ANGEL: case ITEM_BLUERABBIT: case ITEM_ICE: case ITEM_FLASH:
        case ITEM_HAMMER: case ITEM_BOMB: case ITEM_DUNG: case ITEM_DEVIL:
        case ITEM_DEVILRED:
            return true;
        default:
            return false;
    }
}

bool ItemPackets::spawnAppliesLocally(int32_t kind) {
    switch (kind) {
        case ITEM_BOOSTER: case ITEM_BIG_BOOSTER: case ITEM_ROCKET:
        case ITEM_ANGEL: case ITEM_MAGNET: case ITEM_DEVILRED:
            return true;
        default:
            return false;
    }
}

bool ItemPackets::spawnNeedsSelfEcho(int32_t kind) {
    // no local spawn online and a real S2C case so drop the echo and the shooter sees nothing
    return spawnHasClientCase(kind) && !spawnAppliesLocally(kind);
}

int32_t ItemPackets::managerCap(int32_t kind) {
    switch (kind) {
        case ITEM_SPIKE:  return 16;
        case ITEM_TURTLE: return 16;
        case ITEM_SHIELD: return 16;
        case ITEM_FLASH:  return 16;
        case ITEM_BOMB:   return 16;
        case ITEM_STORM:      return 8;
        case ITEM_RABBIT:     return 8;
        case ITEM_SMOKE:      return 8;
        case ITEM_HIVE:       return 8;
        case ITEM_BLUERABBIT: return 8;
        case ITEM_ICE:        return 8;
        default: return 0;  // not read out of the binary
    }
}

bool ItemPackets::effectActionable(int16_t code) {
    return code == EFFECT_SPIN || code == EFFECT_BUMP || code == EFFECT_CRASH ||
           code == EFFECT_HIVE || code == EFFECT_ICE;
}

bool ItemPackets::effectReportedByClient(int16_t code) {
    return code == EFFECT_SPIN || code == EFFECT_CRASH || code == EFFECT_HIVE ||
           code == EFFECT_ICE  || code == EFFECT_FLASH;
}

bool ItemPackets::hitShouldRebroadcast(int16_t code) {
    return effectActionable(code) && effectReportedByClient(code);
}

float ItemPackets::effectVelocityMultiplier(int16_t code) {
    switch (code) {
        case EFFECT_SPIN:         return 0.968f;
        case EFFECT_BUMP:         return 0.98f;
        case EFFECT_CRASH:        return 0.0f;
        case EFFECT_THUNDER_AREA: return 0.88f;
        case EFFECT_TURTLE_ZONE:  return 0.934f;
        case EFFECT_HIVE:         return 0.94f;
        case EFFECT_ICE:          return 0.88f;
        default: return 1.0f;
    }
}

uint64_t ItemPackets::effectDurationMs(int16_t code) {
    // only spin has a proven fixed end the rest die on client side membership tests
    if (code == EFFECT_SPIN) return kSpinDurationMs;
    return 0;
}

bool ItemPackets::parseGrant(const Packet& pkt, GrantReport& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("itemGrant", b, kGrantC2SSize)) return false;

    out.itemId    = readI32(&b[0]);
    out.slotIndex = readI32(&b[4]);

    if (!itemKindValid(out.itemId)) {
        LOG_WARN("PACKET", "itemGrant id " + std::to_string(out.itemId) + " out of 0..21");
        return false;
    }
    return true;
}

bool ItemPackets::parsePickup(const Packet& pkt, GrantReport& out) {
    return parseGrant(pkt, out);
}

bool ItemPackets::parseUse(const Packet& pkt, UseRequest& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("itemUse", b, kSpawnC2SSize)) return false;

    out.kind   = readI32(&b[0]);
    out.x      = readF32(&b[4]);
    out.y      = readF32(&b[8]);
    out.z      = readF32(&b[12]);
    out.yawDeg = readF32(&b[16]);

    if (!itemKindValid(out.kind)) {
        LOG_WARN("PACKET", "itemUse kind " + std::to_string(out.kind) + " out of 0..21");
        return false;
    }
    return true;
}

bool ItemPackets::parseHit(const Packet& pkt, HitReport& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("itemHit", b, kHitC2SSize)) return false;

    out.code = readI16(&b[0]);
    out.flag = b[2];

    if (!effectCodeKnown(out.code)) {
        LOG_WARN("PACKET", "itemHit unknown code " + std::to_string(out.code));
        return false;
    }
    if (!effectReportedByClient(out.code)) {
        // client never emits these treat as tampering
        LOG_WARN("PACKET", "itemHit code " + std::to_string(out.code) + " no client sender exists");
    }
    return true;
}

bool ItemPackets::parseHomingLaunch(const Packet& pkt, HomingLaunch& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("homingLaunch", b, kHomingC2SSize)) return false;

    out.kind            = readI32(&b[0]);
    out.shooterPlayerId = readI32(&b[4]);
    out.targetPlayerId  = readI32(&b[8]);

    if (out.kind != ITEM_ROCKET && out.kind != ITEM_MAGNET) {
        LOG_WARN("PACKET", "homingLaunch kind " + std::to_string(out.kind) + " not 10 or 16");
        return false;
    }
    return true;
}

bool ItemPackets::parseTurtleLaunch(const Packet& pkt, TurtleLaunch& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("turtleLaunch", b, kTurtleC2SSize)) return false;

    out.shooterPlayerId = readI32(&b[0]);
    out.targetPlayerId  = readI32(&b[4]);
    return true;
}

bool ItemPackets::parseLockState(const Packet& pkt, LockState& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("lockState", b, kLockRelaySize)) return false;

    out.targetPlayerId = readI32(&b[0]);
    out.phase          = readI32(&b[4]);
    out.kind           = readI32(&b[8]);

    if (out.phase < LOCK_LOST || out.phase > LOCK_LOCKED) {
        LOG_WARN("PACKET", "lockState phase " + std::to_string(out.phase) + " not 0..2");
        return false;
    }
    if (out.kind != ITEM_ROCKET && out.kind != ITEM_MAGNET) {
        LOG_WARN("PACKET", "lockState kind " + std::to_string(out.kind) + " not 10 or 16");
        return false;
    }
    return true;
}

bool ItemPackets::parseSlotSync(const Packet& pkt, SlotSync& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("slotSync", b, kSlotSyncC2SSize)) return false;

    for (int i = 0; i < kSlotCount; ++i) {
        out.slot[i] = safeSlotValue("slotSync", readI32(&b[i * 4]));
    }
    return true;
}

bool ItemPackets::parsePetReached(const Packet& pkt, PetReached& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("petReached", b, kPetReachedSize)) return false;

    out.victimPlayerId = readI32(&b[0]);
    out.kind           = readI32(&b[4]);

    if (out.kind != ITEM_RABBIT && out.kind != ITEM_BLUERABBIT) {
        LOG_WARN("PACKET", "petReached kind " + std::to_string(out.kind) + " not 7 or 13");
        return false;
    }
    return true;
}

bool ItemPackets::parseRaceValue(const Packet& pkt, int16_t& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("raceValue", b, kRaceValueC2SSize)) return false;

    out = readI16(&b[0]);
    return true;
}

bool ItemPackets::parseSwapTicket(const Packet& pkt, SwapTicket& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("swapTicket", b, kSwapTicketC2SSize)) return false;

    out.instanceId  = readI32(&b[0x00]);
    out.baseKey     = readI32(&b[0x04]);
    out.priceKey    = readI32(&b[0x08]);
    out.periodMode  = readI32(&b[0x0C]);
    out.periodValue = readI32(&b[0x10]);
    out.activeFlag  = readI32(&b[0x14]);
    out.inUseFlag   = readI32(&b[0x18]);
    out.flag        = b[0x1C];

    if (out.baseKey != kSwapTicketTemplate) {
        LOG_WARN("PACKET", "swapTicket base key " + std::to_string(out.baseKey) +
                           " expected " + std::to_string(kSwapTicketTemplate));
        return false;
    }
    return true;
}

bool ItemPackets::parseAbilityFire(const Packet& pkt, int32_t& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("abilityFire", b, kAbilityC2SSize)) return false;

    out = readI32(&b[0]);
    return true;
}

bool ItemPackets::parseAbilityClass(const Packet& pkt, int32_t& out) {
    const std::vector<uint8_t>& b = pkt.payload();
    if (!sizeOk("abilityClass", b, kAbilityC2SSize)) return false;

    out = readI32(&b[0]);
    return true;
}

Packet ItemPackets::itemSpawn(uint32_t playerId, int32_t kind,
                              float x, float y, float z, float yawDeg) {
    if (!itemKindValid(kind)) {
        LOG_ERROR("PACKET", "itemSpawn kind " + std::to_string(kind) + " out of 0..21");
    } else if (!spawnHasClientCase(kind)) {
        LOG_WARN("PACKET", "itemSpawn kind " + std::string(itemName(kind)) +
                           " has no receiver case");
    }

    // same 24 bytes as the motion layer effect broadcast no reason to rebuild them
    Packet pkt = MotionPackets::effectAt(playerId, static_cast<uint32_t>(kind), x, y, z, yawDeg);
    assertSize("itemSpawn", pkt, kSpawnS2CSize);
    return pkt;
}

Packet ItemPackets::itemSpawnEcho(uint32_t senderPlayerId, const UseRequest& req) {
    return itemSpawn(senderPlayerId, req.kind, req.x, req.y, req.z, req.yawDeg);
}

Packet ItemPackets::grantBroadcast(uint32_t playerId, int32_t itemId, int32_t slotIndex) {
    Packet pkt(kOpGrant);
    pkt.writeUInt32(playerId);
    pkt.writeInt32(safeSlotValue("itemGrant", itemId));
    pkt.writeInt32(clampSlotIndex("itemGrant", slotIndex));

    assertSize("grantBroadcast", pkt, kGrantS2CSize);
    return pkt;
}

Packet ItemPackets::homingLaunch(uint32_t senderPlayerId, int32_t kind,
                                 int32_t shooterPlayerId, int32_t targetPlayerId) {
    if (kind != ITEM_ROCKET && kind != ITEM_MAGNET) {
        LOG_WARN("PACKET", "homingLaunch kind " + std::to_string(kind) + " is a receiver no op");
    }

    Packet pkt(kOpHoming);
    pkt.writeUInt32(senderPlayerId);
    pkt.writeInt32(kind);
    pkt.writeInt32(shooterPlayerId);
    pkt.writeInt32(targetPlayerId);

    assertSize("homingLaunch", pkt, kHomingS2CSize);
    return pkt;
}

Packet ItemPackets::turtleLaunch(uint32_t senderPlayerId,
                                 int32_t shooterPlayerId, int32_t targetPlayerId) {
    Packet pkt(kOpTurtle);
    pkt.writeUInt32(senderPlayerId);
    pkt.writeInt32(shooterPlayerId);
    pkt.writeInt32(targetPlayerId);

    assertSize("turtleLaunch", pkt, kTurtleS2CSize);
    return pkt;
}

Packet ItemPackets::lockStateRelay(int32_t targetPlayerId, int32_t phase, int32_t kind) {
    // no sender id here C2S is 12 bytes and the handler reads 12
    Packet pkt(kOpLock);
    pkt.writeInt32(targetPlayerId);
    pkt.writeInt32(phase);
    pkt.writeInt32(kind);

    assertSize("lockStateRelay", pkt, kLockRelaySize);
    return pkt;
}

Packet ItemPackets::hitBroadcast(uint32_t victimPlayerId, int16_t code, uint8_t flag) {
    if (!effectActionable(code)) {
        LOG_WARN("PACKET", "hitBroadcast code " + std::to_string(code) + " dropped by receivers");
    }

    Packet pkt(kOpHit);
    pkt.writeUInt32(victimPlayerId);
    pkt.writeInt16(code);
    pkt.writeUInt8(flag);  // receiver reads then discards it

    assertSize("hitBroadcast", pkt, kHitS2CSize);
    return pkt;
}

Packet ItemPackets::effectApply(uint32_t playerId, int16_t code) {
    return hitBroadcast(playerId, code, 1);
}

Packet ItemPackets::slotSync(uint32_t playerId, int32_t slot0, int32_t slot1, int32_t slot2) {
    Packet pkt(kOpSlotSync);
    pkt.writeUInt32(playerId);
    pkt.writeInt32(safeSlotValue("slotSync", slot0));
    pkt.writeInt32(safeSlotValue("slotSync", slot1));
    pkt.writeInt32(safeSlotValue("slotSync", slot2));

    assertSize("slotSync", pkt, kSlotSyncS2CSize);
    return pkt;
}

Packet ItemPackets::petReachedRelay(int32_t victimPlayerId, int32_t kind) {
    // pure relay 8 bytes in and 8 out
    Packet pkt(kOpPetReached);
    pkt.writeInt32(victimPlayerId);
    pkt.writeInt32(kind);

    assertSize("petReachedRelay", pkt, kPetReachedSize);
    return pkt;
}

Packet ItemPackets::raceValue(uint32_t playerId, int16_t value) {
    Packet pkt = MotionPackets::motionBlock(playerId, value);
    assertSize("raceValue", pkt, kRaceValueS2CSize);
    return pkt;
}

Packet ItemPackets::standingsUpdate(uint32_t playerId, int32_t position, int32_t pingMs) {
    Packet pkt(kOpStandings);
    pkt.writeUInt32(playerId);
    pkt.writeInt32(position);
    // sub 447AE0 draws Icon link 4 for 1 to 100 ms and link 1 past 400 ms chibikart sends 30
    pkt.writeInt32(pingMs);

    assertSize("standingsUpdate", pkt, kStandingsS2CSize);
    return pkt;
}

Packet ItemPackets::playSoundCue(uint32_t playerId) {
    Packet pkt(kOpSoundCue);
    pkt.writeUInt32(playerId);

    assertSize("playSoundCue", pkt, kSoundCueS2CSize);
    return pkt;
}

Packet ItemPackets::shieldAbsorbToken(uint32_t playerId, uint32_t hitToken) {
    // same opcode byte as C2S kOpAbilityFire this direction carries two u32 not one
    Packet pkt(kOpAbilityFire);
    pkt.writeUInt32(playerId);
    pkt.writeUInt32(hitToken);

    assertSize("shieldAbsorbToken", pkt, kHitTokenS2CSize);
    return pkt;
}

ItemPackets::PlayerItemState& ItemPackets::ItemModel::addPlayer(uint32_t playerId,
                                                                bool thirdSlotUnlocked,
                                                                int32_t swapTicketQty) {
    PlayerItemState& st = players[playerId];
    st = PlayerItemState{};
    st.playerId      = playerId;
    st.thirdSlot     = thirdSlotUnlocked;
    st.capacity      = thirdSlotUnlocked ? kUnlockedCapacity : kBaseCapacity;
    st.swapTicketQty = swapTicketQty > 0 ? swapTicketQty : 0;
    return st;
}

void ItemPackets::ItemModel::removePlayer(uint32_t playerId) {
    players.erase(playerId);
}

void ItemPackets::ItemModel::clear() {
    players.clear();
}

ItemPackets::PlayerItemState* ItemPackets::ItemModel::find(uint32_t playerId) {
    auto it = players.find(playerId);
    return it == players.end() ? nullptr : &it->second;
}

const ItemPackets::PlayerItemState* ItemPackets::ItemModel::find(uint32_t playerId) const {
    auto it = players.find(playerId);
    return it == players.end() ? nullptr : &it->second;
}

void ItemPackets::ItemModel::setThirdSlot(uint32_t playerId, bool unlocked) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->thirdSlot = unlocked;
    st->capacity  = unlocked ? kUnlockedCapacity : kBaseCapacity;
}

void ItemPackets::ItemModel::setRank(uint32_t playerId, int32_t rank) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->rank = rank;
}

bool ItemPackets::ItemModel::applyGrant(uint32_t playerId, int32_t itemId, int32_t slotIndex,
                                        uint64_t nowMs, int32_t& outSlotIndex) {
    outSlotIndex = 0;

    PlayerItemState* st = find(playerId);
    if (!st) {
        LOG_WARN("ITEM", "grant for unknown player " + std::to_string(playerId));
        return false;
    }
    if (!itemKindValid(itemId)) {
        LOG_ERROR("ITEM", "grant item " + std::to_string(itemId) + " out of 0..21");
        return false;
    }

    // older pending lands first exactly like sub 4AECD0
    commitNow(*st);

    if (st->heldCount >= st->capacity) {
        LOG_INFO("ITEM", "grant refused slots full player " + std::to_string(playerId));
        return false;
    }

    const int32_t clamped = clampSlotIndex("grant", slotIndex);
    if (clamped != st->heldCount) {
        LOG_WARN("ITEM", "grant slot " + std::to_string(clamped) + " server holds " +
                         std::to_string(st->heldCount));
    }

    outSlotIndex     = clampSlotIndex("grant", st->heldCount);
    st->pendingItem  = itemId;
    st->pendingSlot  = outSlotIndex;
    st->pendingAtMs  = nowMs;
    st->lastGrantMs  = nowMs;
    return true;
}

bool ItemPackets::ItemModel::commitPending(uint32_t playerId, uint64_t nowMs) {
    PlayerItemState* st = find(playerId);
    if (!st) return false;
    (void)nowMs;
    return commitNow(*st);
}

bool ItemPackets::ItemModel::applySlotSync(uint32_t playerId,
                                           int32_t slot0, int32_t slot1, int32_t slot2) {
    PlayerItemState* st = find(playerId);
    if (!st) {
        LOG_WARN("ITEM", "slot sync for unknown player " + std::to_string(playerId));
        return false;
    }

    st->slots[0] = safeSlotValue("slotSync", slot0);
    st->slots[1] = safeSlotValue("slotSync", slot1);
    st->slots[2] = safeSlotValue("slotSync", slot2);
    st->heldCount = countHeld(*st);

    if (st->heldCount > st->capacity) {
        LOG_WARN("ITEM", "slot sync held " + std::to_string(st->heldCount) + " over capacity " +
                         std::to_string(st->capacity) + " player " + std::to_string(playerId));
    }
    if (st->slots[0] == ITEM_EMPTY && st->heldCount > 0) {
        LOG_WARN("ITEM", "slot sync hole at zero player " + std::to_string(playerId));
    }
    return true;
}

bool ItemPackets::ItemModel::canUse(uint32_t playerId, uint64_t nowMs) const {
    (void)nowMs;
    const PlayerItemState* st = find(playerId);
    if (!st) return false;
    if (st->heldCount <= 0) return false;
    if (st->slots[0] == ITEM_EMPTY) return false;
    if (st->effect.code != EFFECT_NONE) return false;
    return true;
}

bool ItemPackets::ItemModel::applyUse(uint32_t playerId, int32_t kind, uint64_t nowMs) {
    PlayerItemState* st = find(playerId);
    if (!st) {
        LOG_WARN("ITEM", "use for unknown player " + std::to_string(playerId));
        return false;
    }
    if (!itemKindValid(kind)) return false;

    if (st->effect.code != EFFECT_NONE) {
        LOG_WARN("ITEM", "use while effect " + std::to_string(st->effect.code) +
                         " player " + std::to_string(playerId));
        return false;
    }
    if (st->slots[0] != kind) {
        LOG_WARN("ITEM", "use kind " + std::string(itemName(kind)) + " slot zero holds " +
                         std::to_string(st->slots[0]) + " player " + std::to_string(playerId));
        return false;
    }

    st->lastUseMs = nowMs;

    if (kind == ITEM_SHIELD) grantShield(playerId, nowMs, false);
    if (kind == ITEM_ANGEL)  grantAngel(playerId);

    // first press only starts the search 0x4B consumes it later
    if (kind == ITEM_ROCKET || kind == ITEM_MAGNET) return true;

    consumeSlot0(playerId, nowMs);
    return true;
}

bool ItemPackets::ItemModel::consumeSlot0(uint32_t playerId, uint64_t nowMs) {
    (void)nowMs;
    PlayerItemState* st = find(playerId);
    if (!st) return false;
    if (st->heldCount <= 0) return false;

    st->slots[0] = st->slots[1];
    st->slots[1] = st->slots[2];
    st->slots[2] = ITEM_EMPTY;
    st->heldCount = countHeld(*st);
    return true;
}

void ItemPackets::ItemModel::noteSwapTicketUsed(uint32_t playerId) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    if (st->swapTicketQty > 0) --st->swapTicketQty;
}

void ItemPackets::ItemModel::grantShield(uint32_t playerId, uint64_t nowMs, bool abilityBonus) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->shieldActive    = true;
    st->shieldExpiresMs = nowMs + kShieldMs + (abilityBonus ? kShieldAbilityBonusMs : 0);
}

void ItemPackets::ItemModel::grantAngel(uint32_t playerId) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->angelActive = true;  // no expiry proven one shot only
}

ItemPackets::Protection ItemPackets::ItemModel::consumeProtection(uint32_t playerId,
                                                                  uint64_t nowMs) {
    PlayerItemState* st = find(playerId);
    if (!st) return Protection::None;

    // shield first then angel both one shot mirrors sub 4B7DD0
    if (st->shieldActive && (st->shieldExpiresMs == 0 || nowMs < st->shieldExpiresMs)) {
        st->shieldActive    = false;
        st->shieldExpiresMs = 0;
        return Protection::Shield;
    }
    if (st->angelActive) {
        st->angelActive = false;
        return Protection::Angel;
    }
    return Protection::None;
}

bool ItemPackets::ItemModel::applyEffect(uint32_t playerId, int16_t code, uint64_t nowMs) {
    PlayerItemState* st = find(playerId);
    if (!st) return false;
    if (!effectCodeKnown(code)) {
        LOG_WARN("ITEM", "effect code " + std::to_string(code) + " unknown");
        return false;
    }
    if (st->effect.code != EFFECT_NONE) return false;  // sub 495C30 refuses to stack

    const uint64_t life = effectDurationMs(code);
    st->effect.code        = code;
    st->effect.startedAtMs = nowMs;
    st->effect.expiresAtMs = life ? nowMs + life : 0;
    return true;
}

void ItemPackets::ItemModel::clearEffect(uint32_t playerId) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->effect = ActiveEffect{};
}

void ItemPackets::ItemModel::setLock(uint32_t playerId, int32_t targetPlayerId,
                                     int32_t phase, uint64_t nowMs) {
    PlayerItemState* st = find(playerId);
    if (!st) return;
    st->lockPhase      = phase;
    st->lockTargetId   = phase == LOCK_LOST ? -1 : targetPlayerId;
    st->lastLockSendMs = nowMs;
}

ItemPackets::TickResult ItemPackets::ItemModel::tick(uint64_t nowMs) {
    TickResult result;

    for (auto& entry : players) {
        PlayerItemState& st = entry.second;

        if (st.pendingItem != ITEM_EMPTY && nowMs >= st.pendingAtMs + kCommitDelayMs) {
            TickCommit c;
            c.playerId  = st.playerId;
            c.itemId    = st.pendingItem;
            c.slotIndex = st.pendingSlot;
            if (commitNow(st)) result.commits.push_back(c);
        }

        if (st.shieldActive && st.shieldExpiresMs != 0 && nowMs >= st.shieldExpiresMs) {
            st.shieldActive    = false;
            st.shieldExpiresMs = 0;
            result.shieldsExpired.push_back(st.playerId);
        }

        if (st.effect.code != EFFECT_NONE && st.effect.expiresAtMs != 0 &&
            nowMs >= st.effect.expiresAtMs) {
            TickExpiry e;
            e.playerId = st.playerId;
            e.code     = st.effect.code;
            st.effect  = ActiveEffect{};
            result.effectsExpired.push_back(e);
        }
    }

    return result;
}

int32_t ItemPackets::loadSwapTicketQuantity(uint32_t playerId) {
    auto rows = Database::instance().queryPrepared(
        "SELECT period_value, active_flag FROM owned_item "
        "WHERE character_id = ? AND base_key = ?",
        { playerId, kSwapTicketTemplate });

    if (rows.empty()) return 0;
    if (rowInt(rows[0], "active_flag") == 0) return 0;

    const int32_t qty = rowInt(rows[0], "period_value");
    return qty > 0 ? qty : 0;
}

bool ItemPackets::loadThirdSlotUnlocked(uint32_t playerId) {
    // capacity three needs quantity only sub 450660 never rechecks the enabled flag
    auto rows = Database::instance().queryPrepared(
        "SELECT period_value FROM owned_item WHERE character_id = ? AND base_key = ?",
        { playerId, kThirdSlotTemplate });

    if (rows.empty()) return false;
    return rowInt(rows[0], "period_value") > 0;
}

} // namespace knc
