#pragma once
/// drift and boost are client authority sub 496BE0 and sub 49AA90 relay is C2S 0x40 per FUN 0049BEB0

#include "net/Packet.h"
#include "packets/gen/SpawnPackets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// decoded u16 at C2S 0x40 offset 17 and S2C 0x40 offset 23 relay raw and decode only to observe
struct DriftBoostState {
    uint8_t steerIndex        = 0;      ///< steerIndex b15 to 12 0 is hard left 15 is hard right engineIndex b11 to 8 is rpm bucket
    uint8_t engineIndex       = 0;
    bool    boostClass0       = false;  ///< boostClass0 b7 offset 13056 set 13060 zero is mini turbo boostClass1 b6 offset 13056 set 13060 nonzero is item boost
    bool    boostClass1       = false;
    bool    reverseGear       = false;  ///< reverseGear b5 offset 13101 gear index below zero drifting b4 offset 13732 nonzero left vs right not on wire
    bool    drifting          = false;
    bool    miniTurboCharged  = false;  ///< miniTurboCharged b3 offset 13808 equals 1 charged while still drifting steerInputLeft b2 offset 686308 equals 1
    bool    steerInputLeft    = false;
    bool    steerInputRight   = false;  ///< steerInputRight b1 offset 686308 equals 2
};

/// which grant path produced an observed boost as far as the server can tell
enum class BoostSource : uint8_t {
    None      = 0,
    MiniTurbo = 1,  ///< MiniTurbo is the gas edge after a drift or a 3% lucky roll Item needs a matching C2S 0x47
    Item      = 2,
    Unmatched = 3,  ///< Unmatched no drift item or pad explains it suspicious not proof Pad is BOOST NNN cell the client starts alone
    Pad       = 4
};

/// coarse drift phase inferred from the bit 4 and bit 3 transitions
enum class DriftPhase : uint8_t {
    Idle      = 0,
    Drifting  = 1,  ///< Drifting bit 4 on bit 3 off stage 0 Charged bit 4 and 3 on stage 1 key held
    Charged   = 2,
    Releasing = 3,  ///< Releasing bit 3 drops bit 4 still on stage 2 gauge decays Released bit 4 drops after charged drift
    Released  = 4
};

/// one tick worth of edges everything false means nothing changed
struct DriftBoostEvents {
    bool driftStarted   = false;
    bool driftCharged   = false;
    bool driftEnded     = false;
    bool driftReleased  = false;  ///< drift ended while charged so mini turbo is now armed
    bool boostStarted   = false;
    bool boostEnded     = false;
    bool boostClassFlip = false;  ///< both class bits or a class change without an end
    BoostSource source  = BoostSource::None;
    uint32_t violations = 0;      ///< mask of DriftBoostPackets violation bits
};

/// C2S 0x47 20 bytes sender sub 481230 the only explicit boost signal
struct ItemUseRequest {
    int32_t itemType = 0;  ///< itemType 0 is normal boost 1 is super boost others are other items p1 is inferred forwarded as raw dwords
    int32_t p1 = 0;
    int32_t p2 = 0;
    int32_t p3 = 0;
    int32_t p4 = 0;
};

/// C2S 0x4B 12 bytes sender sub 481320 targetA and targetB are player ids
struct GameEventRequest {
    int32_t kind    = 0;   ///< kind client only ever sends 10 or 16 targetA player id -1 when the source slot was negative
    int32_t targetA = -1;
    int32_t targetB = -1;
};

/// the 17 float stat block shared index space between 0x00C0 and 0x0108
using KartStatBlock = std::array<float, 17>;

/// sub 47F4F0 sub 44E910 sub 44F510 field 3 is one byte on the wire emitting four bytes ruins what follows
struct KartDef {
    uint32_t visibleFlag   = 0;   ///< visibleFlag 0x00 zero hides row badge 0x04 tile overlay 1 new 2 hot
    uint32_t badge         = 0;
    uint32_t kartId        = 0;   ///< kartId 0x08 the container key unk0c 0x0C one byte no reader in this build
    uint8_t  unk0c         = 0;
    int32_t  vehicleKind   = 0;   ///< vehicleKind 0x10 car kind 2 bike 5 spinner modelScheme 0x14 0 catalogue 1 factory car parts apply on 1
    int32_t  modelScheme   = 1;
    int32_t  unk18         = 0;   ///< unk18 0x18 no reader in this build requiredLevel 0x1C player level gate against byte 0x1A20B09
    int32_t  requiredLevel = 0;

    std::string modelName;        ///< modelName ascii client stack buffer 33 bytes with NUL displayNameKey record 0x41 buffer 33 def trans title
    std::string displayNameKey;
    std::string descriptionKey;   ///< descriptionKey record 0x62 buffer 34 def trans description

    std::array<uint8_t, 32> defaultSkinKeys{};   ///< defaultSkinKeys record 0x84 eight default skin keys stats record 0xA4 seventeen floats
    KartStatBlock           stats{};
    std::array<uint8_t, 8>  abilityPair0{};      ///< abilityPair0 record 0x130 ability id then percent abilityPair1 record 0x138 same shape
    std::array<uint8_t, 8>  abilityPair1{};

    std::vector<std::array<uint8_t, 16>> options;
};

/// one equipped part slot the partId and level pair from the 0x3E custom block
struct DriftEquippedPart {
    uint32_t partId = 0;
    int32_t  level  = 0;  ///< an integer that the client divides by 50 with floor semantics
};

/// derived per kart numbers the anti cheat thresholds hang off wire stat indices
struct DriftBoostTuning {
    float speedCeilingKmh        = 320.0f;  ///< speedCeilingKmh wire 1 max speed clamp on working and target speed accelScale wire 2 steering gain clamp 1 to 4
    float accelScale             = 1.0f;
    float torqueTrim             = 1.0f;    ///< torqueTrim wire 0 body setup velocity gain at most plus one percent miniTurboDurationMs wire 3 400 to 800
    float miniTurboDurationMs    = 400.0f;
    float miniTurboTargetKmh     = 120.0f;  ///< miniTurboTargetKmh wire 3 120 to 144 driftRampRate wire 8 drift charge rate 0 3 to 0 8
    float driftRampRate          = 0.3f;
    float steerScale             = 1.2f;    ///< steerScale wire 9 drift steer 1 2 to 1 8 driftSteerThresholdDeg wire 10 mini turbo threshold 2 to 10
    float driftSteerThresholdDeg = 10.0f;
    float driftChargeTimeMs      = 800.0f;  ///< driftChargeTimeMs wire 11 mini turbo hold 160 to 800
};

/// outcome of one speed sample
enum class SpeedVerdict : uint8_t {
    Ok          = 0,
    FirstSample = 1,  ///< FirstSample no baseline yet or too old sample is snapped not judged Rewind timestamp went backwards or repeated sample dropped
    Rewind      = 2,
    Teleport    = 3,  ///< Teleport single step distance past the hard geometric cap OverCeiling sustained speed past the kart ceiling times tolerance
    OverCeiling = 4,
    BadSample   = 5   ///< BadSample non finite input
};

/// thresholds are server policy not proven since the client has no matching check keep loose until captured live
struct SpeedLimits {
    float ceilingWorldUnitsPerSec = 0.0f;    ///< ceilingWorldUnitsPerSec zero disables the ceiling test entirely tolerance is the multiplier applied to the ceiling
    float tolerance               = 1.35f;
    float teleportUnits           = 300.0f;  ///< teleportUnits is the hard single step cap policy staleResyncMs an older baseline just snaps and never accuses
    uint32_t staleResyncMs        = 3000;
    uint32_t minDeltaMs           = 5;       ///< minDeltaMs under this the quantizer noise dominates
};

/// per racing player observer state in memory only reset at every spawn teleport or respawn
struct DriftBoostTrack {
    bool     haveState      = false;
    uint16_t lastStateBits  = 0;
    uint32_t reportIntervalMs = 100;  ///< reportIntervalMs is client send period the timing tests count samples sampleIndex state words fed so far identical ones included
    uint64_t sampleIndex    = 0;

    DriftPhase driftPhase    = DriftPhase::Idle;
    uint64_t   driftStartMs  = 0;
    uint64_t   driftChargeMs = 0;
    uint64_t   driftReleaseMs = 0;
    uint64_t   driftStartSample   = 0;  ///< driftStartSample sample of bit 4 rise driftKeyUpSample sample of bit 3 drop zero when not seen
    uint64_t   driftKeyUpSample   = 0;
    uint64_t   driftReleaseSample = 0;  ///< driftReleaseSample sample of the bit 4 drop of an armed drift driftCharged is bit 3 seen during the current drift
    bool       driftCharged       = false;

    // the position of the last observed sample the pad sweep runs from it
    bool     haveStatePos = false;
    float    stateX       = 0.0f;
    float    stateY       = 0.0f;

    bool        boostActive  = false;
    uint8_t     boostClass   = 0;  ///< 0 or 1 only the class is on the wire never the type
    BoostSource boostSource  = BoostSource::None;
    uint64_t    boostStartMs = 0;

    // pairing a class 1 boost to a real C2S 0x47
    bool     pendingItemUse   = false;
    int32_t  pendingItemType  = 0;
    uint64_t pendingItemUseMs = 0;

    bool     havePos    = false;
    float    lastX      = 0.0f;
    float    lastY      = 0.0f;
    float    lastZ      = 0.0f;
    uint64_t lastPosMs  = 0;
    float    lastSpeed  = 0.0f;  ///< world units per second

    SpeedLimits limits;
    uint32_t    violations = 0;  ///< sticky mask cleared only by resetTrack
    uint32_t    violationCount = 0;
};

/// drift and boost wire layer owning the kart catalog C2S parsers state codec and observer
struct DriftBoostPackets {

    static constexpr uint16_t OP_MOTION     = 0x0040;  ///< MOTION both directions carries state word ITEM USE C2S 20 bytes sub 481230 S2C 24 bytes sub 47A110
    static constexpr uint16_t OP_ITEM_USE   = 0x0047;
    static constexpr uint16_t OP_GAME_EVENT = 0x004B;  ///< GAME EVENT C2S 12 bytes sub 481320 S2C 16 bytes sub 47A460 KART DEF is S2C kart definition sub 47F4F0
    static constexpr uint16_t OP_KART_DEF   = 0x00C0;

    static constexpr size_t SELF_REPORT_SIZE     = 19;  ///< SELF REPORT SIZE is C2S 0x40 compressed SELF REPORT SIZE RAW is C2S 0x40 raw world
    static constexpr size_t SELF_REPORT_SIZE_RAW = 28;
    static constexpr size_t STATE_OFFSET         = 17;  ///< STATE OFFSET is inside the compressed body STATE OFFSET RAW is inside the raw body
    static constexpr size_t STATE_OFFSET_RAW     = 26;
    static constexpr size_t ITEM_USE_C2S_SIZE    = 20;
    static constexpr size_t ITEM_USE_S2C_SIZE    = 24;
    static constexpr size_t GAME_EVENT_C2S_SIZE  = 12;
    static constexpr size_t GAME_EVENT_S2C_SIZE  = 16;

    static constexpr size_t PART_SLOTS       = 7;   ///< the 0x3E custom block carries seven

    static constexpr uint16_t BIT_STEER    = 0xF000;  ///< STEER is b15 to 12 ENGINE is b11 to 8
    static constexpr uint16_t BIT_ENGINE   = 0x0F00;
    static constexpr uint16_t BIT_BOOST0   = 0x0080;  ///< BOOST0 is mini turbo class BOOST1 is item class
    static constexpr uint16_t BIT_BOOST1   = 0x0040;
    static constexpr uint16_t BIT_REVERSE  = 0x0020;
    static constexpr uint16_t BIT_DRIFT    = 0x0010;
    static constexpr uint16_t BIT_CHARGED  = 0x0008;
    static constexpr uint16_t BIT_STEER_L  = 0x0004;
    static constexpr uint16_t BIT_STEER_R  = 0x0002;
    static constexpr uint16_t BIT_UNUSED   = 0x0001;  ///< UNUSED no OR DI 1 anywhere never set FLAG MASK is every real flag bit 0 excluded
    static constexpr uint16_t FLAG_MASK    = 0x00FE;

    static constexpr float MAX_STEER_DEG        = 45.0f;    ///< flt 5EB700
    static constexpr float RPM_FLOOR            = 1000.0f;
    static constexpr float RPM_SPAN             = 9000.0f;  ///< RPM SPAN is 0x5A6AB8 RPM STEP is inverse of the per tick time constant at 0x5A6AB4
    static constexpr float RPM_STEP             = 600.0f;
    static constexpr int   SELF_REPORT_MS       = 100;      ///< dword 1396E90

    static constexpr float DRIFT_ENTRY_KMH      = 20.0f;    ///< ENTRY KMH speedo gate in sub 49AA90 CHARGE BASE MS is flt 5EB708
    static constexpr float DRIFT_CHARGE_BASE_MS = 800.0f;
    static constexpr float DRIFT_STEER_BASE_DEG = 10.0f;    ///< STEER BASE DEG is flt 5EB704 FIRE WINDOW MS is flt 5EB70C alive while gauge decays under 0 4 s
    static constexpr float FIRE_WINDOW_MS       = 1500.0f;
    static constexpr float GAUGE_DECAY_PER_SEC  = 120.0f;   ///< GAUGE DECAY is 1 5x tick scale 80 RELEASE GRACE SAMPLES adds one to the bit 4 drop sample
    static constexpr int   RELEASE_GRACE_SAMPLES = 1;
    static constexpr int   BOOST_TAIL_MS_KIND0  = 260;      ///< TAIL KIND0 decays over 12 ticks to 1 then clears TAIL OTHER decays to 1 on first tick then clears
    static constexpr int   BOOST_TAIL_MS_OTHER  = 40;
    static constexpr float PAD_TOLERANCE_UNITS  = 4.0f;     ///< PAD TOLERANCE is how far a wheel may sit from the wire centre PAD STEPS points tested between two samples
    static constexpr int   PAD_PATH_STEPS       = 8;
    static constexpr float SPEED_CEILING_BASE   = 320.0f;   ///< SPEED CEILING BASE is flt 5EB6FC TARGET SPEED BASE is dword 5EB6F8
    static constexpr float TARGET_SPEED_BASE    = 90.0f;
    static constexpr float MINI_TURBO_BASE_MS   = 400.0f;
    static constexpr float MINI_TURBO_BASE_KMH  = 120.0f;
    static constexpr float BOOST_DECAY          = 0.86f;    ///< BOOST DECAY per tick on impulse LUCKY UPGRADE PCT rand percent 100 at most 2 is three in a hundred
    static constexpr int   LUCKY_UPGRADE_PCT    = 3;
    static constexpr int   BOOST_EXTEND_MS      = 500;      ///< the type 22 object extension

    /// speedometer km per h to world units per second unproven from a single data point
    static constexpr float KMH_TO_WORLD_UNPROVEN = 0.5788f;

    static constexpr uint32_t VIOL_SPEED           = 1u << 0;  ///< SPEED sustained over the ceiling TELEPORT single step past the hard cap
    static constexpr uint32_t VIOL_TELEPORT        = 1u << 1;
    static constexpr uint32_t VIOL_BOOST_UNMATCHED = 1u << 2;  ///< BOOST UNMATCHED class 1 with no C2S 0x47 BOOST SPAM a new boost inside the old one
    static constexpr uint32_t VIOL_BOOST_SPAM      = 1u << 3;
    static constexpr uint32_t VIOL_DRIFT_CHARGE    = 1u << 4;  ///< DRIFT CHARGE charged faster than kart allows BOTH CLASSES bits 7 and 6 both set encoder cannot produce
    static constexpr uint32_t VIOL_BOTH_CLASSES    = 1u << 5;
    static constexpr uint32_t VIOL_RESERVED_BIT    = 1u << 6;  ///< RESERVED BIT 0 set encoder never sets MINITURBO FAST boost class 0 with no drift before it
    static constexpr uint32_t VIOL_MINITURBO_FAST  = 1u << 7;

    // shared state word codec with motion motion owns the transform and this file owns the flag meaning

    /// decodes the u16 into named flags never fails unknown bits ignored
    static DriftBoostState decodeStateBits(uint16_t stateBits);

    /// encodes named flags back to the u16 bit 0 always left clear
    static uint16_t encodeStateBits(const DriftBoostState& state);

    /// steer index nibble out of a relayed word
    static uint8_t stateSteerIndex(uint16_t stateBits);

    /// engine index nibble out of a relayed word
    static uint8_t stateEngineIndex(uint16_t stateBits);

    /// converts steer degrees to the encoder index and must clamp or 16 wraps to hard left
    static uint8_t steerIndexFromDegrees(float steerDegrees);

    /// index back to degrees asymmetric on purpose since a steer of 0 round trips to plus 3 degrees
    static float steerDegreesFromIndex(uint8_t index);

    /// clamp of rpm -1000 between 0 and 9000 over 600 clamped to the nibble
    static uint8_t engineIndexFromRpm(float rpm);

    /// index back to rpm asymmetric since the decoded value is not the encoded one
    static float rpmFromEngineIndex(uint8_t index);

    /// composes a word from the two nibbles and a flag mask
    static uint16_t makeStateBits(uint8_t steerIndex, uint8_t engineIndex, uint16_t flags);

    /// pulls only the state word out of a C2S 0x40 body for a cheap relay
    static bool stateBitsFromSelfReport(const uint8_t* data, size_t len, bool rawWorld,
                                        uint16_t& out);

    /// same as above on a whole C2S 0x40 packet payload
    static bool stateBitsFromSelfReport(const Packet& pkt, bool rawWorld, uint16_t& out);

    /// flags in the word the client encoder can never produce returns a violation mask
    static uint32_t stateBitsAnomalies(uint16_t stateBits);

    /// parses C2S 0x47 20 bytes 5 signed dwords itemType 0 or 1 also grants the boost locally via sub 496BE0
    static bool parseItemUse(const uint8_t* data, size_t len, ItemUseRequest& out);

    /// same on a whole packet payload
    static bool parseItemUse(const Packet& pkt, ItemUseRequest& out);

    /// true for the two item types that mean boost 0 and 1
    static bool isBoostItemType(int32_t itemType);

    /// true when S2C 0x47 does anything on the receiver via sub 47A110 some types fall through and draw nothing
    static bool s2cItemTypeIsHandled(int32_t itemType);

    /// parses C2S 0x4B 12 bytes kind targetA targetB are player ids not slots
    static bool parseGameEvent(const uint8_t* data, size_t len, GameEventRequest& out);

    /// same on a whole packet payload
    static bool parseGameEvent(const Packet& pkt, GameEventRequest& out);

    /// true for the two kinds sub 47A460 actually branches on 10 and 16
    static bool isLiveEventKind(int32_t kind);

    /// S2C 0x4B relay 16 bytes logs loudly for a kind outside 10 and 16 since the receiver drops it
    static Packet gameEvent(uint32_t playerId, int32_t kind, int32_t p1, int32_t p2);

    /// WIRE index k of the 0xC0 block at 0xA4 the client reads car 0x3448 plus 4 k bonus 0xA7940
    enum StatIndex : size_t {
        STAT_BODY_SETUP           = 0,   ///< BODY SETUP tick 0x49CDDA garage bar one MAX SPEED tick 0x49CA95 garage bar one
        STAT_MAX_SPEED            = 1,
        STAT_STEERING_GAIN        = 2,   ///< STEERING GAIN drift update 0x49B1B2 garage bar two MINI TURBO TARGET car boost start 0x496D2B garage bar four
        STAT_MINI_TURBO_TARGET    = 3,
        STAT_BOOST_LEAN_LIFT      = 4,   ///< BOOST LEAN LIFT car effect lean update 0x49B4C0 nose lift visual only TURN FORCE tick 0x49CB45 clamp to 2
        STAT_TURN_FORCE           = 5,
        STAT_WHEEL_SPIN           = 6,   ///< WHEEL SPIN tick 0x49D8D6 kind 2 spin torque WHEEL STEER ANGLE tick 0x49D78F kind 2 wheel angle
        STAT_WHEEL_STEER_ANGLE    = 7,
        STAT_DRIFT_CHARGE_RATE    = 8,   ///< DRIFT CHARGE RATE drift update 0x49AC11 garage bar three DRIFT STEER tick 0x49D3C9 times 0 6 plus 1 2
        STAT_DRIFT_STEER          = 9,
        STAT_MINI_TURBO_THRESHOLD = 10,  ///< MINI TURBO THRESHOLD drift update 0x49AEF5 bar three MINI TURBO HOLD drift update 0x49AF47 times 800 ms bar three
        STAT_MINI_TURBO_HOLD      = 11,
        STAT_GRIP                 = 12,  ///< GRIP tick 0x49C9D6 per wheel grip NO READ 13 has no read site in the exe
        STAT_NO_READ_13           = 13,
        STAT_CAMERA_DISTANCE      = 14,  ///< CAMERA DISTANCE camera update 0x43F529 chase distance 9 shipped CAMERA PITCH camera update 0x43F50A chase pitch 37 shipped
        STAT_CAMERA_PITCH         = 15,
        STAT_CAMERA_HEIGHT        = 16   ///< CAMERA HEIGHT camera update 0x43F510 look point height 3 5 shipped
    };

    /// wire slots below this index take the part grade multiplier 10 to 16 take it raw 0x48F710
    static constexpr size_t STAT_LEVEL_SCALED_END = 10;

    /// part level multiplier is 1 plus floor of level over 50 integer division
    static int partLevelMultiplier(int32_t level);

    /// kart stat plus part overlay applied only when the kart definition has modelScheme equal to 1
    static KartStatBlock effectiveStats(const KartDef& kart,
                                        const std::vector<DriftEquippedPart>& parts,
                                        const std::vector<KartStatBlock>& partStats);

    /// derives the tuning numbers the observers need all clamps come from the client image
    static DriftBoostTuning tuning(const KartStatBlock& effective);

    /// clamp of 1 plus wire stat 1 between 1 and 2 times 320 speedometer km per h 320 to 640
    static float speedCeilingKmh(float effectiveSpeedStat);

    /// sub 49C0D0 cruising speed term is non monotonic a handling stat over 1 lowers the target speed
    static float targetSpeedKmh(float effectiveHandlingStat, bool drifting, float yawTerm);

    /// boost duration ms type 0 scales with wire stat 3 types 1 and 2 add 500 with the extend item
    static float boostDurationMs(int boostType, float effectiveBoostStat, bool hasExtendItem);

    /// how long the wire bit stays up the duration plus the decay tail of car boost update 0x496E50
    static float boostWireMs(int boostType, float effectiveBoostStat, bool hasExtendItem);

    /// sub 496BE0 target speed a boost drives toward before the client floors it at speed plus 10
    static float boostTargetKmh(int boostType, float effectiveBoostStat);

    /// sub 496BE0 whether a new boost may preempt an active one normal mode lets a higher type cut in
    static bool boostCanPreempt(int activeType, int newType, bool rawWorld);

    /// clears a track and installs the kart derived speed ceiling
    static void resetTrack(DriftBoostTrack& track, const DriftBoostTuning& tuning);

    /// clears a track with no kart knowledge only the teleport test stays live
    static void resetTrack(DriftBoostTrack& track);

    /// drops the position baseline after a teleport or respawn keeps drift state
    static void resyncPosition(DriftBoostTrack& track);

    /// records a C2S 0x47 so a later class 1 boost can be matched to a real item use
    static void noteItemUse(DriftBoostTrack& track, int32_t itemType, uint64_t serverMs);

    /// pairing window for a class 1 boost against a logged C2S 0x47
    static constexpr uint32_t PAIR_WINDOW_MS = 600;

    /// one pad lookup hit false means no cell kind minus one means the ini gave none
    struct PadHit {
        bool    hit  = false;
        int32_t kind = -1;
        int32_t row  = 0;
    };

    /// the pad cell under a wire point widened by tol units
    static PadHit padAt(const std::vector<SpawnPackets::ColPadCell>& pads, float x, float y, float tol);

    /// the first pad cell on the straight path from a to b sampled PAD PATH STEPS times
    static PadHit padOnPath(const std::vector<SpawnPackets::ColPadCell>& pads, float x0, float y0,
                            float x1, float y1, float tol);

    /// true when a pad hit explains a boost of that class kind 0 is class 0 other kinds class 1
    static bool padMatchesClass(const PadHit& hit, uint8_t boostClass);

    /// feeds one state word with the car position pads null means no track knowledge and no pad judgement
    static DriftBoostEvents observeState(DriftBoostTrack& track, uint16_t stateBits,
                                         uint64_t serverMs, const DriftBoostTuning& tuning,
                                         const std::vector<SpawnPackets::ColPadCell>* pads,
                                         float x, float y);

    /// feeds one state word returns edges and violations idempotent on a repeated identical word
    static DriftBoostEvents observeState(DriftBoostTrack& track, uint16_t stateBits,
                                         uint64_t serverMs, const DriftBoostTuning& tuning);

    /// observeState with no kart knowledge skips the charge time test
    static DriftBoostEvents observeState(DriftBoostTrack& track, uint16_t stateBits,
                                         uint64_t serverMs);

    /// feeds one world position returns a verdict and speed first sample and stale gaps never accuse
    static SpeedVerdict checkSpeed(DriftBoostTrack& track, float x, float y, float z,
                                   uint64_t serverMs, float& speedOut);

    /// human readable verdict for a log line
    static const char* verdictName(SpeedVerdict verdict);

    /// space separated names of the set violation bits empty when none
    static std::string violationNames(uint32_t mask);

    /// one kart definition by id false when the row is absent feeds loadEffectiveStats only
    static bool loadKartDef(uint32_t kartId, KartDef& out);

    /// part bonus stat blocks reuse the car craft definition table same 17 float index space
    static bool loadPartStats(uint32_t partId, KartStatBlock& out);

    /// the 7 equipped part slots of one owned kart
    static std::vector<DriftEquippedPart> loadEquippedParts(uint32_t characterId, uint32_t kartId);

    /// resolves kart plus parts into effective stats cache per player since it never changes mid race
    static bool loadEffectiveStats(uint32_t characterId, uint32_t kartId, KartStatBlock& out);
};

} // namespace knc
