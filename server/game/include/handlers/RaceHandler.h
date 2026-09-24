#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/MotionPackets.h"
#include "packets/gen/SpawnPackets.h"
#include "packets/gen/ResultsPackets.h"
#include "packets/gen/DriftBoostPackets.h"
#include "packets/gen/ItemPackets.h"
#include "handlers/RaceBots.h"
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <chrono>
#include <asio.hpp>
#include <functional>
#include <memory>
#include <mutex>

namespace knc {

class GameServer;
class Room;

struct RacePlayer {
    int32_t playerId = 0;
    uint8_t position = 0;
    uint8_t lap = 0;
    int32_t lastLapTime = 0;
    int32_t totalTime = 0;
    int32_t score = 0;      // score is the gold reward xpReward is stored for post race notifications
    int32_t xpReward = 0;
    bool finished = false;
    float x = 0, y = 0, z = 0, rot = 0;
    std::chrono::steady_clock::time_point lastUpdate;
    int32_t suspiciousCount = 0;
    std::vector<int32_t> lapTimes;

    uint8_t boostLevel = 0;        // mini turbo boost level 0 none 1 blue 2 orange 3 red max
    bool isBoosting = false;
    std::chrono::steady_clock::time_point boostStart;
    int32_t boostCount = 0;

    // rebuilt after the 21 aug checkout every field below is used by the cpp
    uint32_t gridIndex = 0;
    int32_t  finishRank = 0;
    int32_t  goldBonus = 0;
    int32_t  xpBonus = 0;
    uint32_t lapBoardRow = 0;
    bool     haveProgress = false;
    bool     progressWarned = false;
    int32_t  progressScore = 0;
    MotionGateState motionGate;         // per axis budget baseline counted in wire periods lastSuspicionDecayMs is the leaky bucket clock so honest strikes fade
    uint64_t lastSuspicionDecayMs = 0;
    SpawnPackets::LapTracker lapTracker;
    DriftBoostTrack  drift;
    DriftBoostTuning tuning;
    bool     tuningLoaded = false;

    // item and hit bookkeeping the anti cheat window reads
    uint32_t bonusItemKey = 0;
    uint32_t lastHitItem = 0;
    int32_t  lastHitVictim = 0;
    std::chrono::steady_clock::time_point lastItemUse{};
    std::chrono::steady_clock::time_point lastHitTime{};
};


/// one remote car the motion fan out repeats rebuilt from RaceHandler
struct MotionSlot {
    int32_t       playerId = 0;
    std::weak_ptr<Session> session;
    CarState      state{};
    uint64_t      lastSampleMs = 0;
    bool          hasSample = false;
    bool          needSnap = false;
    bool          stale = false;
};

/// one CPU car in a live race the driver plus its finish flag
struct BotRacer {
    int32_t   playerId = 0;
    BotDriver driver;
    bool      finished = false;
};

/// per room live race state rebuilt from RaceHandler
struct RoomRaceLive {
    int32_t  trackId = 0;
    uint32_t checkpointCount = 0;
    uint32_t totalLaps = 3;
    bool     checkpointWarned = false;
    bool     lapTrackingLive = false;
    bool     padsLoaded = false;    ///< the track COL gave its BOOST NNN cells the observer judges pad boosts
    bool     rawWorld = false;
    bool     dirty = false;
    uint64_t lastFanOutMs = 0;
    uint64_t lastStandingsMs = 0;   ///< S2C 0x45 runs at 2 Hz one row per racer
    std::vector<GridSpawn>  grid;
    std::vector<MotionSlot> motion;
    ItemPackets::ItemModel  items;
    std::vector<SpawnPackets::ColPadCell> pads;  ///< BOOST NNN cells with their boost ini kind

    BotTrack                   botTrack;      ///< racing line and boxes empty when track has none bots arm at GO and die with the room
    std::vector<BotRacer>      bots;
    std::vector<BotHazard>     hazards;       ///< spikes bombs and ice on the road bots drive into pendingHits are rockets and turtles in flight toward a bot
    std::vector<BotPendingHit> pendingHits;
    uint64_t lastBotTickMs = 0;
    uint64_t raceStartMs   = 0;               ///< nowMs at GO bot finish times count from it
};

/// humans owe C2S 0x0D between grid and GO answered by stock handler FUN 0047ADE0
struct SceneLoadWait {
    /// the stock under wine loads in 8 to 16 s our old client took 27 s the cap covers both
    static constexpr uint64_t kWaitMs = 30000;
    std::set<int32_t> owed;
    uint64_t gridMs = 0;

    void arm(std::set<int32_t> humans, uint64_t nowMs) {
        owed = std::move(humans);
        gridMs = nowMs;
    }
    /// one racer said loaded or left the room true once nobody owes an answer
    bool clear(int32_t playerId) {
        owed.erase(playerId);
        return owed.empty();
    }
    bool owes(int32_t playerId) const { return owed.count(playerId) != 0; }
    bool expired(uint64_t nowMs) const { return nowMs >= gridMs + kWaitMs; }
};

class RaceHandler {
public:
    // re declared from the cpp after the checkout took the header
    void loadTrackData(Room* room);
    void sendTrackCatalog(Room* room);
    void sendRaceSetup(Room* room);
    void initRaceLive(Room* room);
    void checkTickClock(uint64_t now);
    void tick(uint64_t now, GameServer* server = nullptr);
    void completeFinish(Session::Ptr session, Room* room, int32_t finishTime, GameServer* server);
    void handleItemGrantReport(Session::Ptr session, Packet& packet, Room* room);
    void handleItemSpawn(Session::Ptr session, Packet& packet, Room* room);
    void handleHomingLaunch(Session::Ptr session, Packet& packet, Room* room);
    void handleTurtleLaunch(Session::Ptr session, Packet& packet, Room* room);
    void handleLockState(Session::Ptr session, Packet& packet, Room* room);
    void handleHitReport(Session::Ptr session, Packet& packet, Room* room);
    void handleSlotSync(Session::Ptr session, Packet& packet, Room* room);
    void handlePetReached(Session::Ptr session, Packet& packet, Room* room);
    void handleRaceValue(Session::Ptr session, Packet& packet, Room* room);
    void handleSwapTicket(Session::Ptr session, Packet& packet, Room* room);
    void handleCheckpoint(Session::Ptr session, Packet& packet, Room* room, GameServer* server);
    void handleProgress(Session::Ptr session, Packet& packet, Room* room);
    void handleRespawn(Session::Ptr session, Packet& packet, Room* room);
    void handleAnimState(Session::Ptr session, Packet& packet, Room* room);
    void sendGrid(Room* room);
    void broadcastScoreboard(Room* room);
    void handlePlayerLeave(Room* room, int32_t playerId, GameServer* server);
    void scheduleRaceWatchdog(uint32_t roomId, GameServer* server);
    void scheduleBotFinishers(uint32_t roomId, GameServer* server);
    void finishBot(uint32_t roomId, int32_t botId, int32_t finishMs, GameServer* server);
    void scheduleFinishGrace(uint32_t roomId, GameServer* server);

    /// every bot in the roster goes onto the racing line at its grid slot false when there is no line
    bool armBots(Room* room);
    /// advance the bots and publish their motion items and hits runs first in tick
    void tickBots(uint64_t now, GameServer* server);
    /// a hit a bot takes later a rocket or a turtle a client fired at it
    void queueBotHit(uint32_t roomId, int32_t victimId, int16_t code, uint64_t delayMs);
    /// a hazard anyone dropped on the road that bots drive into
    void noteHazard(uint32_t roomId, int32_t ownerId, int32_t kind, float x, float y, float z);

    /// steady clock milliseconds shared by the race tick and the bot timers
    static uint64_t nowMs();

    /// C2S 0x40 body size picks the raw world form from the packed one
    void handleMotion(Session::Ptr session, Packet& packet, Room* room);
    void handleMotion(Session::Ptr session, Packet& packet, Room* room,
                      const CarState& state, bool rawWorld);
    /// C2S 0x40 in the waiting room drives the room field stage 9 relays the seat with no race judge
    void handleRoomMotion(Session::Ptr session, Packet& packet, Room* room);

    void handleStartRace(Session::Ptr session, Room* room, GameServer* server);

    void startCountdown(uint32_t roomId, GameServer* server);
    void sendGridPhase(uint32_t roomId, GameServer* server);
    /// C2S 0x0D the client is in the race scene GO waits for every human
    void handleSceneLoaded(Session::Ptr session, Room* room, GameServer* server);
    void goRace(uint32_t roomId, GameServer* server);
    void startRace(Room* room);
    void endRace(Room* room, GameServer* server);
    void updatePositions(Room* room);

    // called periodically to force end races where players disconnected before sending CMD 0x39 returns true on timeout
    bool checkRaceTimeout(Room* room, GameServer* server);

    RacePlayer* getPlayer(uint32_t roomId, int32_t playerId);
    void addPlayer(uint32_t roomId, int32_t playerId);
    void removePlayer(uint32_t roomId, int32_t playerId);
    void clearRoom(uint32_t roomId);
    
private:
    MotionSlot& motionSlot(RoomRaceLive& live, int32_t playerId, const Session::Ptr& session);
    void ensureItemState(RoomRaceLive& live, uint32_t playerId);
    bool loadTuning(int32_t characterId, int32_t kartId, DriftBoostTuning& out);

    std::map<uint32_t, RoomRaceLive> m_roomLive;
    bool m_tickClockChecked = false;
    std::map<uint32_t, std::shared_ptr<asio::steady_timer>> m_countdownTimers;
    std::map<uint32_t, std::shared_ptr<std::function<void()>>> m_countdownChains;
    std::map<uint32_t, SceneLoadWait> m_awaitLoaded;
    std::map<uint32_t, std::shared_ptr<asio::steady_timer>> m_loadFallbackTimers;
    std::map<uint32_t, std::shared_ptr<asio::steady_timer>> m_raceWatchdogTimers;
    std::map<uint32_t, std::vector<std::shared_ptr<asio::steady_timer>>> m_botFinishTimers;
    /// the first car over the line starts a twenty second client rest counter and stragglers retire at the end
    std::map<uint32_t, std::shared_ptr<asio::steady_timer>> m_finishGraceTimers;

    void sendCountdown(Room* room, int32_t seconds);
    void sendRaceStart(Room* room);
    void sendResults(Room* room);
    void calculatePositions(uint32_t roomId);
    
    // room id maps to players
    std::unordered_map<uint32_t, std::vector<RacePlayer>> m_racePlayers;
    std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> m_raceStartTimes;
    mutable std::mutex m_raceMutex;  // protects m racePlayers and m raceStartTimes
};

} // namespace knc

