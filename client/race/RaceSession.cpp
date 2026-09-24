#include "RaceSession.h"

#include "net/Utf.h"

#include <algorithm>
#include <cstdio>

namespace KnC::Client {

using KnC::Kart::Client::MotionRecvEntry;
using KnC::Kart::Client::MotionSend0x40;
using KnC::Kart::Client::net_motion_pack_0x40;
using KnC::Kart::Client::net_motion_recv_0x40;

namespace {

uint32_t u32(Packet& p) { return p.remaining() >= 4 ? p.readUInt32() : 0; }
int32_t i32(Packet& p) { return p.remaining() >= 4 ? p.readInt32() : 0; }
float f32(Packet& p) { return p.remaining() >= 4 ? p.readFloat() : 0.f; }
uint8_t u8(Packet& p) { return p.remaining() >= 1 ? p.readUInt8() : 0; }
void skip(Packet& p, size_t n) { if (p.remaining() >= n) p.readBytes(n); else p.readBytes(p.remaining()); }

uint32_t rd32(const std::vector<uint8_t>& p, size_t at) {
    if (at + 4 > p.size()) return 0;
    return static_cast<uint32_t>(p[at]) | (static_cast<uint32_t>(p[at + 1]) << 8) |
           (static_cast<uint32_t>(p[at + 2]) << 16) | (static_cast<uint32_t>(p[at + 3]) << 24);
}

}

void RaceSession::begin(const RaceLaunch& launch, uint32_t localPlayerId) {
    m_launch = launch;
    m_localId = localPlayerId;
    m_phase = Phase::Loading;
    m_racers.clear();
    m_reward = FinishReward();
    m_goDelay = 0;
    m_boardOpen = false;
    m_resultTeam = 0;
    m_lapAdvances = 0;
    m_itemRolls.clear();
    m_slots.fill(-1);
    m_heldCount = 0;
    m_lastScore = 0xFFFFFFFFu;
    m_lastScoreAt = -1.0;
    m_sceneLoadedSent = 0;
}

Racer* RaceSession::racer(uint32_t playerId) {
    for (Racer& r : m_racers) if (r.playerId == playerId) return &r;
    return nullptr;
}

const Racer* RaceSession::local() const {
    for (const Racer& r : m_racers) if (r.local) return &r;
    return nullptr;
}

int RaceSession::localPosition() const {
    const Racer* me = local();
    return me ? me->position : -1;
}

void RaceSession::onFrame(uint16_t op, Packet& pkt) {
    if (m_phase == Phase::Idle) return;
    switch (op) {
    case 0x003E:
        parseGridSpawn(pkt);
        break;
    case 0x000D:
        // rank board rebuild lands after last grid row each gets its scene loaded answer
        if (m_phase == Phase::Loading) m_phase = Phase::Grid;
        if (onRankBoard) onRankBoard();
        break;
    case 0x0131: {
        m_itemRolls.clear();
        while (pkt.remaining() >= 4) m_itemRolls.push_back(pkt.readUInt32());
        break;
    }
    case 0x0068: {
        const uint32_t id = u32(pkt);
        const float x = f32(pkt), y = f32(pkt), z = f32(pkt), yaw = f32(pkt);
        if (onTeleport) onTeleport(id, x, y, z, yaw);
        break;
    }
    case 0x003A:
        m_goDelay = u32(pkt);
        m_phase = Phase::Go;
        std::printf("[race] GO 0x003A delay param %u\n", m_goDelay);
        if (onGo) onGo();
        break;
    case 0x0040: {
        const std::vector<uint8_t>& p = pkt.payload();
        const std::vector<MotionRecvEntry> entries = net_motion_recv_0x40(p.data(), p.size());
        if (!entries.empty() && onMotion) onMotion(entries);
        break;
    }
    case 0x0045:
        parseStandings(pkt);
        break;
    case 0x0044:
        ++m_lapAdvances;
        std::printf("[race] lap board advance %d\n", m_lapAdvances);
        break;
    case 0x003C:
        parseFinish(pkt);
        break;
    case 0x003D: {
        const uint32_t id = u32(pkt);
        const int32_t rank = i32(pkt);
        if (Racer* r = racer(id)) r->finishRank = rank;
        std::printf("[race] rank %d for %u\n", rank, id);
        break;
    }
    case 0x0058: {
        const uint32_t id = u32(pkt);
        const uint8_t state = u8(pkt);
        if (Racer* r = racer(id)) r->animState = state;
        break;
    }
    case 0x0047: {
        ItemSpawn s;
        s.playerId = u32(pkt);
        s.kind = i32(pkt);
        s.x = f32(pkt); s.y = f32(pkt); s.z = f32(pkt); s.yawDeg = f32(pkt);
        if (onItemSpawn) onItemSpawn(s);
        break;
    }
    case 0x0049: {
        const uint32_t id = u32(pkt);
        const int32_t item = i32(pkt);
        const int32_t slot = i32(pkt);
        if (onItemGrant) onItemGrant(id, item, slot);
        break;
    }
    case 0x0069: {
        const uint32_t id = u32(pkt);
        const int16_t code = pkt.remaining() >= 2 ? pkt.readInt16() : 0;
        std::printf("[race] effect %d on %u\n", code, id);
        if (onEffect) onEffect(id, code);
        break;
    }
    case 0x004B: {
        // sub 47A460 the sender only gates the liveness the kind the shooter and the target follow
        u32(pkt);
        const int32_t kind = i32(pkt);
        const uint32_t shooter = u32(pkt);
        const uint32_t target = u32(pkt);
        std::printf("[race] homing %d from %u on %u\n", kind, shooter, target);
        if (onHomingLaunch) onHomingLaunch(kind, shooter, target);
        break;
    }
    case 0x005C: {
        // sub 47A500 the sender then the shooter and the target of a turtle
        u32(pkt);
        const uint32_t shooter = u32(pkt);
        const uint32_t target = u32(pkt);
        std::printf("[race] turtle from %u on %u\n", shooter, target);
        if (onTurtleLaunch) onTurtleLaunch(shooter, target);
        break;
    }
    case 0x0057: {
        // sub 47AB40 only the target of the lock reacts
        const uint32_t target = u32(pkt);
        const int32_t phase = i32(pkt);
        const int32_t kind = i32(pkt);
        if (target == m_localId && onLockState) onLockState(kind, phase);
        break;
    }
    case 0x0046:
        parseScoreboard(pkt);
        // sub 47A760 stores rows shows board and starts race end at 2000 for everyone
        m_boardOpen = true;
        m_phase = Phase::Result;
        std::printf("[race] result board open the race end starts\n");
        if (onResultBoard) onResultBoard();
        break;
    case 0x0042: {
        const uint32_t mode = u32(pkt);
        // mode 5 opens ghost record popup FUN 004B6220 other modes set camera mode
        if (mode == 5) std::printf("[race] 0x0042 mode 5 the record popup\n");
        else std::printf("[race] 0x0042 camera mode %u\n", mode);
        break;
    }
    case 0x00CE: {
        // sub 47D250 ascii key wide name of at most 13 chars and number
        std::string key;
        while (pkt.remaining() >= 1) {
            const uint8_t c = pkt.readUInt8();
            if (c == 0) break;
            key.push_back(static_cast<char>(c));
        }
        const std::u16string name = pkt.remaining() >= 2 ? pkt.readWString(13) : std::u16string();
        const int32_t param = i32(pkt);
        std::printf("[race] game message %s %s %d\n", key.c_str(), u16ToUtf8(name).c_str(), param);
        if (onGameMessage) onGameMessage(key, name, param);
        break;
    }
    case 0x0039:
        std::printf("[race] 0x0039 modal close the race hud tears down\n");
        break;
    case 0x00F0: {
        const uint32_t id = u32(pkt);
        std::printf("[race] entity remove %u\n", id);
        if (onRacerLeft) onRacerLeft(id);
        break;
    }
    default:
        break;
    }
}

// 0x003E read order 4 wstr 4 4 0x2C 0x38 4 0x3C keys sit at offset 4 of both blobs
void RaceSession::parseGridSpawn(Packet& pkt) {
    Racer r;
    r.playerId = u32(pkt);
    r.name = pkt.readWString(64);
    r.gridIndex = u32(pkt);
    r.team = u32(pkt);
    const std::vector<uint8_t>& p = pkt.payload();
    const size_t at = p.size() - pkt.remaining();
    r.driverKey = rd32(p, at + 0x04);
    for (size_t i = 0; i < r.accessory.size(); ++i) r.accessory[i] = rd32(p, at + 0x08 + 4 * i);
    r.kartKey = rd32(p, at + 0x2C + 0x04);
    for (size_t i = 0; i < r.kartParts.size(); ++i) r.kartParts[i] = rd32(p, at + 0x2C + 0x08 + 4 * i);
    r.petKey = rd32(p, at + 0x2C + 0x38);
    for (size_t i = 0; i < r.customCar.size(); ++i) r.customCar[i] = rd32(p, at + 0x2C + 0x38 + 4 + 4 * i);
    skip(pkt, 0x2C + 0x38 + 4 + 0x3C);
    r.local = r.playerId == m_localId;
    if (Racer* old = racer(r.playerId)) *old = r;
    else m_racers.push_back(r);
    std::printf("[race] grid %u %s row %u driver %u kart %u pet %u%s\n", r.playerId, u16ToUtf8(r.name).c_str(),
                r.gridIndex, r.driverKey, r.kartKey, r.petKey, r.local ? " local" : "");
    if (onGridSpawn) onGridSpawn(r);
}

void RaceSession::parseStandings(Packet& pkt) {
    const uint32_t id = u32(pkt);
    const int32_t position = i32(pkt);
    const int32_t ping = i32(pkt);
    if (Racer* r = racer(id)) {
        r->position = position;
        r->pingMs = ping;
    }
    if (onStandings) onStandings();
}

// 0x003C is unicast to finisher four values land in wallet with no id guard
void RaceSession::parseFinish(Packet& pkt) {
    const uint32_t id = u32(pkt);
    m_reward.valid = true;
    m_reward.goldAfter = u32(pkt);
    m_reward.levelAfter = u8(pkt);
    m_reward.expAfter = u32(pkt);
    m_reward.finishRank = i32(pkt);
    std::printf("[race] finish 0x003C id %u gold %u level %u exp %u rank %d\n", id, m_reward.goldAfter,
                m_reward.levelAfter, m_reward.expAfter, m_reward.finishRank);
    if (id == m_localId || id == 0) {
        m_phase = Phase::Finished;
        // client stops its 0x0067 and answers own finish anim state
        sendAnimState(9);
        if (onLocalFinish) onLocalFinish();
    }
}

// 0x0046 exactly 14 reads per row two reward pairs interleaved gold exp gold exp
void RaceSession::parseScoreboard(Packet& pkt) {
    m_resultTeam = u32(pkt);
    const int32_t count = i32(pkt);
    for (int32_t i = 0; i < count && pkt.remaining() >= 46; ++i) {
        const uint32_t rank = u32(pkt);
        const int32_t timeMs = i32(pkt);
        const uint32_t id = u32(pkt);
        const std::u16string name = pkt.readWString(12);
        const uint8_t level = u8(pkt);
        const uint32_t team = u32(pkt);
        const uint32_t goldBase = u32(pkt);
        const uint32_t expBase = u32(pkt);
        const uint32_t goldBonus = u32(pkt);
        const uint32_t expBonus = u32(pkt);
        // badge is one signed byte then icon key unread dword and pet key
        (void)u8(pkt);
        u32(pkt);
        u32(pkt);
        u32(pkt);
        (void)level;
        Racer* r = racer(id);
        if (!r) {
            Racer fresh;
            fresh.playerId = id;
            fresh.name = name;
            fresh.team = team;
            fresh.local = id == m_localId;
            m_racers.push_back(fresh);
            r = &m_racers.back();
        }
        r->onBoard = true;
        r->finishRank = static_cast<int>(rank);
        r->finishTimeMs = timeMs;
        r->gold = goldBase;
        r->exp = expBase;
        r->goldBonus = goldBonus;
        r->expBonus = expBonus;
        std::printf("[race] board rank %u %s time %d gold %u+%u exp %u+%u\n", rank, u16ToUtf8(name).c_str(), timeMs,
                    goldBase, goldBonus, expBase, expBonus);
    }
}

// FUN 00402210 answers every S2C 0x000D next frame handler FUN 0047ADE0 sets 0xB23150
void RaceSession::sendSceneLoaded() {
    ++m_sceneLoadedSent;
    m_session.send(Packet::fromCmdFull(0x000D));
    std::printf("[race] scene loaded 0x000D sent %d\n", m_sceneLoadedSent);
}

void RaceSession::sendMotion(const MotionSend0x40& body) {
    const std::vector<uint8_t> bytes = net_motion_pack_0x40(body);
    Packet p = Packet::fromCmdFull(0x0040);
    p.writeBytes(bytes.data(), bytes.size());
    m_session.send(p);
}

void RaceSession::sendCheckpoint(uint32_t prev, uint32_t next) {
    Packet p = Packet::fromCmdFull(0x0041);
    p.writeUInt32(prev);
    p.writeUInt32(next);
    m_session.send(p);
}

void RaceSession::sendProgress(uint32_t score, double nowSeconds) {
    if (m_phase != Phase::Go) return;
    if (score == m_lastScore && m_lastScoreAt >= 0.0 && nowSeconds - m_lastScoreAt < 0.3) return;
    m_lastScore = score;
    m_lastScoreAt = nowSeconds;
    Packet p = Packet::fromCmdFull(0x0067);
    p.writeUInt32(score);
    m_session.send(p);
}

void RaceSession::sendSlotMirror() {
    Packet p = Packet::fromCmdFull(0x00CF);
    for (int32_t item : m_slots) p.writeInt32(item);
    m_session.send(p);
}

// FUN 004AECD0 refuses past the open slots the report carries the count held before the new item
bool RaceSession::sendItemGrant(int32_t item, int openSlots) {
    if (m_heldCount >= std::min(openSlots, kItemSlots)) return false;
    Packet g = Packet::fromCmdFull(0x0049);
    g.writeInt32(item);
    g.writeInt32(m_heldCount);
    m_session.send(g);
    sendAnimState(7);
    // FUN 004AEFA0 lands the item in the next free slot then reports the three slots
    m_slots[static_cast<size_t>(m_heldCount)] = item;
    ++m_heldCount;
    sendSlotMirror();
    return true;
}

// FUN 004AEED0 the used slot 0 goes and the others move up one
void RaceSession::sendItemUse(float x, float y, float z, float yawDeg) {
    if (m_heldCount <= 0) return;
    sendItemSpawn(m_slots[0], x, y, z, yawDeg);
    consumeItem();
}

// sub 481230 the use of a kind with the four floats the server echoes it as S2C 0x0047
void RaceSession::sendItemSpawn(int32_t kind, float x, float y, float z, float yawDeg) {
    Packet u = Packet::fromCmdFull(0x0047);
    u.writeInt32(kind);
    u.writeFloat(x);
    u.writeFloat(y);
    u.writeFloat(z);
    u.writeFloat(yawDeg);
    m_session.send(u);
}

void RaceSession::consumeItem() {
    if (m_heldCount <= 0) return;
    for (size_t i = 0; i + 1 < m_slots.size(); ++i) m_slots[i] = m_slots[i + 1];
    m_slots.back() = -1;
    --m_heldCount;
    sendSlotMirror();
}

// sub 481320 the launch of a locked rocket or magnet
void RaceSession::sendHomingLaunch(int32_t kind, uint32_t shooter, uint32_t target) {
    Packet p = Packet::fromCmdFull(0x004B);
    p.writeInt32(kind);
    p.writeUInt32(shooter);
    p.writeUInt32(target);
    m_session.send(p);
}

// sub 481430 the own turtle and the racer it chases
void RaceSession::sendTurtleLaunch(uint32_t shooter, uint32_t target) {
    Packet p = Packet::fromCmdFull(0x005C);
    p.writeUInt32(shooter);
    p.writeUInt32(target);
    m_session.send(p);
}

// sub 481520 the lock phase on a target the server relays it to that racer alone
void RaceSession::sendLockState(uint32_t target, int32_t phase, int32_t kind) {
    Packet p = Packet::fromCmdFull(0x0057);
    p.writeUInt32(target);
    p.writeInt32(phase);
    p.writeInt32(kind);
    m_session.send(p);
}

void RaceSession::sendHit(int16_t code) {
    Packet h = Packet::fromCmdFull(0x0069);
    h.writeInt16(code);
    h.writeUInt8(1);
    m_session.send(h);
}

void RaceSession::sendAnimState(uint8_t state) {
    Packet s = Packet::fromCmdFull(0x0058);
    s.writeUInt8(state);
    m_session.send(s);
}

void RaceSession::sendLeave() {
    m_session.leaveRace();
}

}
