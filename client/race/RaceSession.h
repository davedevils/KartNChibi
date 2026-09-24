// wire side of one race from 0x0014 launch to result board in Drive cpp order
#pragma once

#include "net/Session.h"

#include "games/kart/physics/client/motion_packet.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace KnC::Client {

// one racer from 0x003E grid spawn plus what race packets said since
struct Racer {
    uint32_t playerId = 0;
    std::u16string name;
    uint32_t gridIndex = 0;
    uint32_t team = 0;
    uint32_t driverKey = 0;
    uint32_t kartKey = 0;
    // 0x003E pet base key after two blobs zero means no pet
    uint32_t petKey = 0;
    bool local = false;
    // 0x0045 zero based position minus one until first row lands
    int position = -1;
    int pingMs = 0;
    // 0x003D finish rank minus one while racing
    int finishRank = -1;
    // 0x0046 row
    bool onBoard = false;
    int finishTimeMs = 0;
    uint32_t gold = 0, exp = 0, goldBonus = 0, expBonus = 0;
    uint8_t animState = 0;
};

// 0x003C reward for local player
struct FinishReward {
    bool valid = false;
    uint32_t goldAfter = 0;
    uint8_t levelAfter = 0;
    uint32_t expAfter = 0;
    int32_t finishRank = -1;
};

// 0x0047 spawn of an item effect
struct ItemSpawn {
    uint32_t playerId = 0;
    int32_t kind = 0;
    float x = 0.f, y = 0.f, z = 0.f, yawDeg = 0.f;
};

class RaceSession {
public:
    enum class Phase { Idle, Loading, Grid, Go, Finished, Result };

    explicit RaceSession(Session& session) : m_session(session) {}
    // fresh race launch fields come from 0x0014 session kept
    void begin(const RaceLaunch& launch, uint32_t localPlayerId);
    // every inbound frame race opcodes land here rest is ignored
    void onFrame(uint16_t opcode, Packet& pkt);

    // C2S 0x000D once per S2C 0x000D like stock our server waits for first before GO
    void sendSceneLoaded();
    // C2S 0x0040 19 byte packed body every 100 ms
    void sendMotion(const KnC::Kart::Client::MotionSend0x40& body);
    // C2S 0x0041 pair before and after increment
    void sendCheckpoint(uint32_t prev, uint32_t next);
    // C2S 0x0067 gated to one send per 300 ms unless value changed
    void sendProgress(uint32_t score, double nowSeconds);
    // C2S 0x0049 then 0x0058 state 7 then 0x00CF slot mirror
    void sendItemGrant(int32_t item);
    // C2S 0x0047 then empty 0x00CF slot mirror
    void sendItemUse(int32_t item, float x, float y, float z, float yawDeg);
    // C2S 0x0069 victim reports hit
    void sendHit(int16_t code);
    // C2S 0x0058 one byte
    void sendAnimState(uint8_t state);
    // C2S 0x003B race exit
    void sendLeave();

    Phase phase() const { return m_phase; }
    const std::vector<Racer>& racers() const { return m_racers; }
    Racer* racer(uint32_t playerId);
    const Racer* local() const;
    int localPosition() const;
    const FinishReward& reward() const { return m_reward; }
    uint32_t goDelayParam() const { return m_goDelay; }
    bool resultBoardOpen() const { return m_boardOpen; }
    uint32_t resultTeam() const { return m_resultTeam; }
    int lapBoardAdvances() const { return m_lapAdvances; }
    const std::vector<uint32_t>& itemRolls() const { return m_itemRolls; }
    uint32_t localPlayerId() const { return m_localId; }
    int32_t heldItem() const { return m_heldItem; }
    void setHeldItem(int32_t item) { m_heldItem = item; }

    std::function<void(const Racer&)> onGridSpawn;
    std::function<void()> onRankBoard;
    std::function<void()> onGo;
    std::function<void()> onStandings;
    std::function<void()> onLocalFinish;
    std::function<void()> onResultBoard;
    std::function<void(const std::vector<KnC::Kart::Client::MotionRecvEntry>&)> onMotion;
    std::function<void(uint32_t playerId, float x, float y, float z, float yawDeg)> onTeleport;
    std::function<void(uint32_t playerId, int code)> onEffect;
    std::function<void(const ItemSpawn&)> onItemSpawn;
    std::function<void(uint32_t playerId, int32_t item, int32_t slot)> onItemGrant;
    std::function<void(uint32_t playerId)> onRacerLeft;
    // S2C 0x00CE game message key player name and number hud prints top right
    std::function<void(const std::string& key, const std::u16string& name, int32_t param)> onGameMessage;

private:
    void parseGridSpawn(Packet& pkt);
    void parseStandings(Packet& pkt);
    void parseFinish(Packet& pkt);
    void parseScoreboard(Packet& pkt);
    void sendSlotMirror(int32_t slot0);

    Session& m_session;
    Phase m_phase = Phase::Idle;
    RaceLaunch m_launch;
    uint32_t m_localId = 0;
    std::vector<Racer> m_racers;
    FinishReward m_reward;
    uint32_t m_goDelay = 0;
    bool m_boardOpen = false;
    uint32_t m_resultTeam = 0;
    int m_lapAdvances = 0;
    std::vector<uint32_t> m_itemRolls;
    int32_t m_heldItem = -1;
    uint32_t m_lastScore = 0xFFFFFFFFu;
    double m_lastScoreAt = -1.0;
    int m_sceneLoadedSent = 0;
};

}
