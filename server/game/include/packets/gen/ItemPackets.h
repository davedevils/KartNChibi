/// client rolls loot itself and reports via grant hit reporting covers six sites self echo required for many kinds

#pragma once
#include "net/Packet.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace knc {

/// in race item packets and the per player item state they mutate
struct ItemPackets {

    static constexpr uint8_t kOpStandings    = 0x45;  ///< S2C only race standings row
    static constexpr uint8_t kOpSpawn        = 0x47;
    static constexpr uint8_t kOpGrant        = 0x49;
    static constexpr uint8_t kOpHoming       = 0x4B;
    static constexpr uint8_t kOpLock         = 0x57;  ///< pure relay both ways
    static constexpr uint8_t kOpTurtle       = 0x5C;
    static constexpr uint8_t kOpPetReached   = 0x5F;  ///< pure relay both ways
    static constexpr uint8_t kOpHit          = 0x69;
    static constexpr uint8_t kOpRaceValue    = 0x6A;
    static constexpr uint8_t kOpSwapTicket   = 0xCB;
    static constexpr uint8_t kOpAbilityFire  = 0xCD;  ///< C2S ability fire S2C shield absorb token same opcode both directions
    static constexpr uint8_t kOpSlotSync     = 0xCF;
    static constexpr uint8_t kOpAbilityClass = 0xF2;
    static constexpr uint8_t kOpSoundCue     = 0xC9;  ///< remote item use sound cue

    static constexpr size_t kSpawnC2SSize      = 20;
    static constexpr size_t kSpawnS2CSize      = 24;
    static constexpr size_t kGrantC2SSize      = 8;
    static constexpr size_t kGrantS2CSize      = 12;
    static constexpr size_t kHomingC2SSize     = 12;
    static constexpr size_t kHomingS2CSize     = 16;
    static constexpr size_t kLockRelaySize     = 12;
    static constexpr size_t kTurtleC2SSize     = 8;
    static constexpr size_t kTurtleS2CSize     = 12;
    static constexpr size_t kPetReachedSize    = 8;
    static constexpr size_t kHitC2SSize        = 3;
    static constexpr size_t kHitS2CSize        = 7;
    static constexpr size_t kRaceValueC2SSize  = 2;
    static constexpr size_t kRaceValueS2CSize  = 6;
    static constexpr size_t kSlotSyncC2SSize   = 12;
    static constexpr size_t kSlotSyncS2CSize   = 16;
    static constexpr size_t kStandingsS2CSize  = 12;
    static constexpr size_t kSwapTicketC2SSize = 29;
    static constexpr size_t kSwapRecordSize    = 28;  ///< the 0x1C blob inside 0xCB
    static constexpr size_t kAbilityC2SSize    = 4;
    static constexpr size_t kSoundCueS2CSize   = 4;
    static constexpr size_t kHitTokenS2CSize   = 8;  ///< S2C shield absorb token shares the opcode with ability fire

    static constexpr int32_t ITEM_EMPTY      = -1;  ///< the client sentinel for an empty slot
    static constexpr int32_t ITEM_BOOSTER    = 0;
    static constexpr int32_t ITEM_BIG_BOOSTER = 1;
    static constexpr int32_t ITEM_SPIKE      = 2;
    static constexpr int32_t ITEM_STORM      = 3;
    static constexpr int32_t ITEM_THUNDER    = 4;
    static constexpr int32_t ITEM_HANDLE     = 5;
    static constexpr int32_t ITEM_TURTLE     = 6;
    static constexpr int32_t ITEM_RABBIT     = 7;
    static constexpr int32_t ITEM_SHIELD     = 8;
    static constexpr int32_t ITEM_SMOKE      = 9;
    static constexpr int32_t ITEM_ROCKET     = 10;
    static constexpr int32_t ITEM_HIVE       = 11;
    static constexpr int32_t ITEM_ANGEL      = 12;
    static constexpr int32_t ITEM_BLUERABBIT = 13;
    static constexpr int32_t ITEM_ICE        = 14;
    static constexpr int32_t ITEM_FLASH      = 15;
    static constexpr int32_t ITEM_MAGNET     = 16;
    static constexpr int32_t ITEM_HAMMER     = 17;
    static constexpr int32_t ITEM_BOMB       = 18;
    static constexpr int32_t ITEM_DUNG       = 19;
    static constexpr int32_t ITEM_DEVIL      = 20;
    static constexpr int32_t ITEM_DEVILRED   = 21;
    static constexpr int32_t ITEM_COUNT      = 22;

    // these are the wire ids never map through shop keys names come from exe string table at 0x5EB8F0

    static constexpr int16_t EFFECT_NONE             = 0;
    static constexpr int16_t EFFECT_SPIN             = 100;
    static constexpr int16_t EFFECT_BUMP             = 200;
    static constexpr int16_t EFFECT_CRASH            = 300;
    static constexpr int16_t EFFECT_THUNDER_AREA     = 400;
    static constexpr int16_t EFFECT_TURTLE_ZONE      = 500;
    static constexpr int16_t EFFECT_RABBIT_LATCH     = 600;
    static constexpr int16_t EFFECT_HIVE             = 700;
    static constexpr int16_t EFFECT_HEAVY_STUN       = 800;
    static constexpr int16_t EFFECT_BLUERABBIT_LATCH = 900;
    static constexpr int16_t EFFECT_ICE              = 1000;
    static constexpr int16_t EFFECT_FLASH            = 1100;

    // effect codes stored at car offset 13992 timer at car offset 13996

    static constexpr int32_t LOCK_LOST      = 0;
    static constexpr int32_t LOCK_SEARCHING = 1;
    static constexpr int32_t LOCK_LOCKED    = 2;

    static constexpr int32_t kSlotCount          = 3;
    static constexpr int32_t kBaseCapacity       = 2;
    static constexpr int32_t kUnlockedCapacity   = 3;
    static constexpr int32_t kSwapTicketTemplate = 1000;
    static constexpr int32_t kThirdSlotTemplate  = 5000;  ///< quantity gt zero unlocks slot three
    static constexpr int32_t kMaxCars            = 30;

    static constexpr uint64_t kCommitDelayMs        = 50;    ///< pending to slot delay hex 0x32
    static constexpr uint64_t kSwapCooldownMs       = 300;
    static constexpr uint64_t kShieldMs             = 4000;  ///< shield duration from sub 4CB160 record offset 0x168
    static constexpr uint64_t kShieldAbilityBonusMs = 2000;
    static constexpr uint64_t kSpinDurationMs       = 1080;  ///< only proven fixed effect life
    static constexpr uint64_t kLockResendMs         = 1000;
    static constexpr uint64_t kEffectAccumCapMs     = 1080;  ///< cap on codes 400 and 1000

    static constexpr int32_t kItemBoxMaxPositions = 100;  ///< itembox ini parser cap
    static constexpr int32_t kItemBoxMaxLive      = 128;
    static constexpr uint64_t kItemBoxRespawnMs   = 2600; ///< 500 plus 2000 plus grow client only

    struct UseRequest {
        int32_t kind   = ITEM_EMPTY;
        float   x      = 0.0f;
        float   y      = 0.0f;
        float   z      = 0.0f;
        float   yawDeg = 0.0f;  ///< launch angle instead of heading for kinds 7 and 13
    };

    /// C2S 0x47 20 bytes no player id on the wire
    struct HomingLaunch {
        int32_t kind           = ITEM_EMPTY;
        int32_t shooterPlayerId = -1;
        int32_t targetPlayerId  = -1;
    };

    /// C2S 0x4B 12 bytes second press of rocket or magnet
    struct TurtleLaunch {
        int32_t shooterPlayerId = -1;
        int32_t targetPlayerId  = -1;
    };

    /// C2S 0x5C 8 bytes turtle only target picked client side
    struct LockState {
        int32_t targetPlayerId = -1;
        int32_t phase          = LOCK_LOST;
        int32_t kind           = ITEM_EMPTY;  ///< only 10 and 16 exist
    };

    /// C2S 0x57 12 bytes relayed untouched
    struct HitReport {
        int16_t code = EFFECT_NONE;
        uint8_t flag = 1;  ///< C2S 0x69 3 bytes code first then the flag byte
    };

    /// every call site passes one receiver discards it
    struct GrantReport {
        int32_t itemId    = ITEM_EMPTY;  ///< C2S 0x49 8 bytes what the client rolled for itself
        int32_t slotIndex = 0;
    };

    /// C2S 0xCF 12 bytes whole slot array after any change
    struct SlotSync {
        int32_t slot[3] = { ITEM_EMPTY, ITEM_EMPTY, ITEM_EMPTY };
    };

    /// C2S 0x5F 8 bytes rabbit or bluerabbit latched on
    struct PetReached {
        int32_t victimPlayerId = -1;
        int32_t kind           = ITEM_EMPTY;  ///< 7 rabbit or 13 bluerabbit
    };

    /// C2S 0xCB 29 bytes the owned item row of base key 1000 verbatim sub 4516D0 returns the record base
    struct SwapTicket {
        int32_t instanceId  = 0;  ///< offset 0x00 rec 0x00
        int32_t baseKey     = 0;
        int32_t priceKey    = 0;  ///< offset 0x08 rec 0x08 0xC6 price row key
        int32_t periodMode  = 0;
        int32_t periodValue = 0;  ///< periodValue 0x10 remaining uses already decremented activeFlag 0x14 must be nonzero for swap
        int32_t activeFlag  = 0;
        int32_t inUseFlag   = 0;
        uint8_t flag        = 0;  ///< inUseFlag 0x18 sub 450660 wants equal one flag 0x1C 0 sync 1 use consumed
    };

    /// exe string table name or invalid when out of 0 to 21
    static const char* itemName(int32_t kind);

    /// true when kind is 0 to 21
    static bool itemKindValid(int32_t kind);

    /// true when value is negative one or 0 to 21 the range a slot dword may hold
    static bool slotValueValid(int32_t value);

    /// false for kinds 0 1 6 10 and 16 booster and big booster never spawn visibly remote
    static bool spawnHasClientCase(int32_t kind);

    /// kinds the shooter also spawns locally 0 1 10 12 16 21
    static bool spawnAppliesLocally(int32_t kind);

    /// kinds whose online client has no local spawn dropping self echo breaks the shooter
    static bool spawnNeedsSelfEcho(int32_t kind);

    /// live instance cap of the client manager zero when not verified
    static int32_t managerCap(int32_t kind);

    /// codes S2C 0x69 acts on 100 200 300 700 1000 else drops
    static bool effectActionable(int16_t code);

    /// codes a client can actually emit 100 300 700 1000 1100
    static bool effectReportedByClient(int16_t code);

    /// actionable and reachable the only codes worth rebroadcasting
    static bool hitShouldRebroadcast(int16_t code);

    /// per frame xy velocity multiplier one when the code does not damp
    static float effectVelocityMultiplier(int16_t code);

    /// server lifetime in ms zero unless the client ends it only code 100 has a proven fixed life
    static uint64_t effectDurationMs(int16_t code);

    /// closest thing to a pickup message box id never travels the wire clamp slotIndex before any S2C builder
    static bool parseGrant(const Packet& pkt, GrantReport& out);

    /// alias of parseGrant there is no separate pickup opcode
    static bool parsePickup(const Packet& pkt, GrantReport& out);

    /// C2S 0x47 item use 20 bytes
    static bool parseUse(const Packet& pkt, UseRequest& out);

    /// C2S 0x69 hit report 3 bytes sent by the victim
    static bool parseHit(const Packet& pkt, HitReport& out);

    /// C2S 0x4B rocket or magnet launch 12 bytes
    static bool parseHomingLaunch(const Packet& pkt, HomingLaunch& out);

    /// C2S 0x5C turtle launch 8 bytes
    static bool parseTurtleLaunch(const Packet& pkt, TurtleLaunch& out);

    /// C2S 0x57 lock state 12 bytes relay it untouched
    static bool parseLockState(const Packet& pkt, LockState& out);

    /// C2S 0xCF slot array 12 bytes
    static bool parseSlotSync(const Packet& pkt, SlotSync& out);

    /// C2S 0x5F rabbit reached victim 8 bytes
    static bool parsePetReached(const Packet& pkt, PetReached& out);

    /// C2S 0x6A small race value 2 bytes
    static bool parseRaceValue(const Packet& pkt, int16_t& out);

    /// C2S 0xCB swap ticket 29 bytes offsets are shifted see SwapTicket
    static bool parseSwapTicket(const Packet& pkt, SwapTicket& out);

    /// C2S 0xCD ability fired 4 bytes payload is a global
    static bool parseAbilityFire(const Packet& pkt, int32_t& out);

    /// C2S 0xF2 ability class 8 9 10 or 11 fired 4 bytes
    static bool parseAbilityClass(const Packet& pkt, int32_t& out);

    /// S2C 0x47 spawn 24 bytes send to everyone including the sender
    static Packet itemSpawn(uint32_t playerId, int32_t kind,
                            float x, float y, float z, float yawDeg);

    /// itemSpawn straight from a parsed C2S body
    static Packet itemSpawnEcho(uint32_t senderPlayerId, const UseRequest& req);

    /// S2C 0x49 grant 12 bytes slotIndex clamped 0 to 2 itemId outside range becomes empty
    static Packet grantBroadcast(uint32_t playerId, int32_t itemId, int32_t slotIndex);

    /// S2C 0x4B homing launch 16 bytes ids relayed raw
    static Packet homingLaunch(uint32_t senderPlayerId, int32_t kind,
                               int32_t shooterPlayerId, int32_t targetPlayerId);

    /// S2C 0x5C turtle launch 12 bytes
    static Packet turtleLaunch(uint32_t senderPlayerId,
                               int32_t shooterPlayerId, int32_t targetPlayerId);

    /// S2C 0x57 lock state 12 bytes no sender id prepended
    static Packet lockStateRelay(int32_t targetPlayerId, int32_t phase, int32_t kind);

    /// S2C 0x69 hit relay 7 bytes victim id first gate with hitShouldRebroadcast
    static Packet hitBroadcast(uint32_t victimPlayerId, int16_t code, uint8_t flag = 1);

    /// S2C 0x69 again for a server authored effect rather than a relay
    static Packet effectApply(uint32_t playerId, int16_t code);

    /// S2C 0xCF slot array 16 bytes paints the remote kart icons
    static Packet slotSync(uint32_t playerId, int32_t slot0, int32_t slot1, int32_t slot2);

    /// S2C 0x5F rabbit reached victim 8 bytes pure relay no id prepended
    static Packet petReachedRelay(int32_t victimPlayerId, int32_t kind);

    /// S2C 0x6A small race value 6 bytes bails mid stream on a dead id
    static Packet raceValue(uint32_t playerId, int16_t value);

    /// S2C 0x45 standings row 12 bytes the third dword is a ping in ms sub 447AE0 picks the link icon
    static Packet standingsUpdate(uint32_t playerId, int32_t position, int32_t pingMs = 30);

    /// S2C 0xC9 remote sound cue 4 bytes client sub 47CD20 resolves the player the same way other broadcasts do
    static Packet playSoundCue(uint32_t playerId);

    /// S2C 0xCD shield absorb token 8 bytes client sub 47D1C0 stores it when class 10 or 11 blocked a hit
    static Packet shieldAbsorbToken(uint32_t playerId, uint32_t hitToken);

    /// what consumeProtection burnt
    enum class Protection { None, Shield, Angel };

    /// one effect sitting on one car
    struct ActiveEffect {
        int16_t  code        = EFFECT_NONE;
        uint64_t startedAtMs = 0;
        uint64_t expiresAtMs = 0;  ///< zero means the client ends it tick will not
    };

    /// everything the server tracks for one racer inside one race
    struct PlayerItemState {
        uint32_t playerId  = 0;
        int32_t  slots[3]  = { ITEM_EMPTY, ITEM_EMPTY, ITEM_EMPTY };
        int32_t  heldCount = 0;
        int32_t  capacity  = kBaseCapacity;

        int32_t  pendingItem  = ITEM_EMPTY;  ///< rolled but not committed yet
        int32_t  pendingSlot  = 0;
        uint64_t pendingAtMs  = 0;

        uint64_t lastGrantMs = 0;
        uint64_t lastUseMs   = 0;
        uint64_t lastSwapMs  = 0;

        int32_t swapTicketQty = 0;  ///< swapTicketQty mirrors template 1000 thirdSlot mirrors template 5000
        bool    thirdSlot     = false;

        bool     shieldActive    = false;
        uint64_t shieldExpiresMs = 0;
        bool     angelActive     = false;

        ActiveEffect effect;

        int32_t  lockPhase      = LOCK_LOST;
        int32_t  lockTargetId   = -1;
        uint64_t lastLockSendMs = 0;

        int32_t rank = 0;  ///< zero based the loot roll reads this from the standings
    };

    /// one pending item that became a real slot entry during tick
    struct TickCommit {
        uint32_t playerId  = 0;
        int32_t  itemId    = ITEM_EMPTY;
        int32_t  slotIndex = 0;
    };

    /// one effect that ran out of its server side lifetime during tick
    struct TickExpiry {
        uint32_t playerId = 0;
        int16_t  code     = EFFECT_NONE;
    };

    /// everything one tick produced the orchestrator turns it into packets
    struct TickResult {
        std::vector<TickCommit> commits;         ///< commits emit slotSync effectsExpired have no wire clear state only
        std::vector<TickExpiry> effectsExpired;
        std::vector<uint32_t>   shieldsExpired;
    };

    /// per race item state one entry per racer call tick once per frame with a monotonic clock
    struct ItemModel {
        std::map<uint32_t, PlayerItemState> players;

        /// adds or resets one racer capacity follows thirdSlotUnlocked
        PlayerItemState& addPlayer(uint32_t playerId, bool thirdSlotUnlocked = false,
                                   int32_t swapTicketQty = 0);

        void removePlayer(uint32_t playerId);
        void clear();

        PlayerItemState* find(uint32_t playerId);
        const PlayerItemState* find(uint32_t playerId) const;

        /// recompute capacity from the template 5000 flag
        void setThirdSlot(uint32_t playerId, bool unlocked);

        /// zero based rank the loot roll and the standings table agree on
        void setRank(uint32_t playerId, int32_t rank);

        /// commits any older pending item first like sub 4AECD0 then queues the new one refuses when slots are full
        bool applyGrant(uint32_t playerId, int32_t itemId, int32_t slotIndex,
                        uint64_t nowMs, int32_t& outSlotIndex);

        /// force the pending item into a slot without waiting for the tick
        bool commitPending(uint32_t playerId, uint64_t nowMs);

        /// accepts a slot sync invalid ids become empty
        bool applySlotSync(uint32_t playerId, int32_t slot0, int32_t slot1, int32_t slot2);

        /// every fire gate the client checks that the server can also check
        bool canUse(uint32_t playerId, uint64_t nowMs) const;

        /// verifies slot zero holds the kind arms shield or angel kinds 10 and 16 do not consume here
        bool applyUse(uint32_t playerId, int32_t kind, uint64_t nowMs);

        /// shifts slots down and drops the held count mirrors sub 4AEED0
        bool consumeSlot0(uint32_t playerId, uint64_t nowMs);

        /// one Slot Exchange use left the row the 0xCF before it already carried the swapped slots
        void noteSwapTicketUsed(uint32_t playerId);

        /// arms the shield for 4000 ms or 6000 with the driver ability
        void grantShield(uint32_t playerId, uint64_t nowMs, bool abilityBonus = false);

        /// arm the one shot angel block
        void grantAngel(uint32_t playerId);

        /// burns one protection shield first then angel mirrors sub 4B7DD0 returns which one absorbed the hit
        Protection consumeProtection(uint32_t playerId, uint64_t nowMs);

        /// sets the effect on a car refuses to stack like sub 495C30 false when already running or unknown
        bool applyEffect(uint32_t playerId, int16_t code, uint64_t nowMs);

        /// drops the effect there is no clear opcode so this only keeps the server view honest
        void clearEffect(uint32_t playerId);

        /// remember a relayed 0x57 so the server knows who is locked
        void setLock(uint32_t playerId, int32_t targetPlayerId, int32_t phase, uint64_t nowMs);

        /// advance pending commits shield expiry and timed effects
        TickResult tick(uint64_t nowMs);
    };

    /// remaining swap tickets owned item base key 1000 column mapping is unverified
    static int32_t loadSwapTicketQuantity(uint32_t playerId);

    /// third slot unlock owned item base key 5000 with a positive quantity
    static bool loadThirdSlotUnlocked(uint32_t playerId);
};

} // namespace knc
