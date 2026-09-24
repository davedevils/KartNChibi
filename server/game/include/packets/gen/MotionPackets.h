#pragma once
#include "net/Packet.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace knc {

/// yaw is compass heading 255 units per 360 around Z tx ty tz is a lerp target not rotation
struct CarState {
    float x  = 0.0f;
    float y  = 0.0f;
    float z  = 0.0f;   // up axis
    float tx = 0.0f;
    float ty = 0.0f;
    float tz = 0.0f;
    uint8_t  yaw       = 0;
    uint16_t stateBits = 0;
};

struct MotionEntry {
    uint32_t playerId = 0;
    /// client computes scale as 1000 over this value so 1000 means scale 1 and zero produces NaN
    int16_t  interpScale = 1000;
    CarState state;
};

/// body is the C2S payload verbatim 19 or 28 raw bytes so a relay skips decode and re encode
struct MotionRelayEntry {
    uint32_t playerId = 0;
    int16_t  interpScale = 1000;
    std::vector<uint8_t> body;
};

/// one entry of the position only bulk correction opcode 0xA5
struct PositionFixEntry {
    uint32_t playerId = 0;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

/// what one motion sample was judged to be
enum class MotionVerdict : uint8_t {
    FirstSample = 0,  ///< FirstSample no baseline yet becomes the baseline never accused Ok inside per axis budget of elapsed wire periods
    Ok          = 1,
    Snap        = 2,  ///< Snap past budget inside respawn reach remotes snap nobody accused Teleport past reach one period only case that accuses
    Teleport    = 3
};

/// motion gate thresholds every number comes from the client integrator not from a guess
struct MotionGateLimits {
    /// body chassis integrate k1 clamps velocity x and y at 0x5a6a14 the recordings peak at 120 05
    float axisXYWuS = 120.0f;
    /// the same integrator clamps velocity z at 0x5a6a68
    float axisZWuS = 60.0f;
    /// wire resolution is 0 01 and field 2 is a prediction so leave a quarter on top
    float tolerance = 1.25f;
    /// client self reports every 100 ms sub 495B20 the budget counts periods not receive ms
    uint32_t reportIntervalMs = 100;
    /// a stall never hands out more than one second of free travel
    uint32_t maxSteps = 10;
    /// respawn reprobe distance bound 0x5a32a0 the reach of one rescue the recordings peak at 38 6
    float snapAxisWu = 80.0f;
};

/// per player motion baseline in memory only
struct MotionGateState {
    bool     primed = false;
    float    x = 0.0f;
    float    y = 0.0f;
    float    z = 0.0f;
    uint64_t lastAcceptedMs = 0;
};

/// one start grid slot from track spawn drives S2C 0x68 hard placement
struct GridSpawn {
    int32_t gridIndex = 0;
    float   x = 0.0f;
    float   y = 0.0f;
    float   z = 0.0f;
    float   yawDegrees = 0.0f;
};

/// dispatcher sub 4777C0 and car table byte 1B19090 require a car active via sub 47FD30 before any motion opcode lands
struct MotionPackets {
    static constexpr uint8_t kOpMotion      = 0x40;  // kOpMotion S2C broadcast and C2S self report kOpEffect item or effect use broadcast
    static constexpr uint8_t kOpEffect      = 0x47;
    static constexpr uint8_t kOpTeleport    = 0x68;  // kOpTeleport hard placement kOpCarEffect per car effect trigger
    static constexpr uint8_t kOpCarEffect   = 0x69;
    static constexpr uint8_t kOpBlock       = 0x6A;  // kOpBlock motion blocker makes 0x40 a no op kOpPositionFix position only bulk correction
    static constexpr uint8_t kOpPositionFix = 0xA5;

    static constexpr int16_t kInterpScaleDefault   = 1000;
    static constexpr size_t  kEntrySizeCompressed  = 25;
    static constexpr size_t  kEntrySizeRaw         = 34;
    static constexpr size_t  kSelfReportSize       = 19;
    static constexpr size_t  kSelfReportSizeRaw    = 28;
    static constexpr size_t  kPositionFixEntrySize = 12;
    static constexpr size_t  kTeleportSize         = 20;
    static constexpr size_t  kEffectSize           = 24;
    static constexpr size_t  kCarEffectSize        = 7;
    static constexpr size_t  kBlockSize            = 6;
    static constexpr size_t  kMaxWireEntries       = 127;  // kMaxWireEntries count is a signed int8 kMaxCars car table slots sub 495B20
    static constexpr size_t  kMaxCars              = 30;

    static constexpr float kQuantMax        = 4095.99f;  // kQuantMax integer part saturates at 4095 kQuantStep is wire resolution
    static constexpr float kQuantStep       = 0.01f;
    static constexpr float kGroundClearance = 0.34f;     // kGroundClearance terrain snap offset sub 485970 kGridSpawnLift client side grid lift sub 486C20
    static constexpr float kGridSpawnLift   = 0.5f;
    static constexpr float kMaxSteerDegrees = 45.0f;     // kMaxSteerDegrees K at flt 5EB700 kSelfReportHz fixed 100 ms from sub 495B20
    static constexpr int   kSelfReportHz    = 10;

    static constexpr uint16_t kStateSteerMask  = 0xF000;  // kStateSteerMask steer index 0 to 15 kStateEngineMask engine index 0 to 15
    static constexpr uint16_t kStateEngineMask = 0x0F00;
    static constexpr uint16_t kStateDriftA     = 0x0080;  // kStateDriftA car offset 13056 one offset 13060 zero kStateDriftB offset 13056 one offset 13060 one
    static constexpr uint16_t kStateDriftB     = 0x0040;
    static constexpr uint16_t kStateFlag5      = 0x0020;  // kStateFlag5 car offset 13101 unnamed kStateFlag4 offset 13732 unnamed
    static constexpr uint16_t kStateFlag4      = 0x0010;
    static constexpr uint16_t kStateFlag3      = 0x0008;  // kStateFlag3 car offset 13808 unnamed kStateRollLeft body roll plus 30 degrees
    static constexpr uint16_t kStateRollLeft   = 0x0004;
    static constexpr uint16_t kStateRollRight  = 0x0002;  // body roll minus 30 degrees

    /// packs a vec3 into 8 wire bytes inverse of client sub 44E7F0
    static void packVec3(float x, float y, float z, uint8_t out[8]);

    /// false when octant nibble is not 1 to 8 client keeps stale sign bits sub 44E500 writes nothing
    static bool unpackVec3(const uint8_t in[8], float& x, float& y, float& z);

    /// degrees to the 255 per 360 wire byte
    static uint8_t yawToByte(float degrees);

    static float yawFromByte(uint8_t value);

    /// folds an angle into 0 to 360 the way client sub 44D9C0 does
    static float normalizeYaw(float degrees);

    /// steer degrees negative 45 to 45 to the 4 bit index the client encoder emits
    static uint8_t steerIndexFromDegrees(float steerDegrees);

    /// index back to the steer degrees the client decoder lands on
    static float steerDegreesFromIndex(uint8_t index);

    /// rpm to a 4 bit index clamp rpm minus 1000 between 0 and 9000 over 600
    static uint8_t engineIndexFromRpm(float rpm);

    /// index back to the rpm the client decoder eases toward
    static float rpmFromEngineIndex(uint8_t index);

    /// composes the u16 state field flags is a mask of the kState bits above
    static uint16_t makeStateBits(uint8_t steerIndex, uint8_t engineIndex, uint16_t flags);

    /// steer index out of a relayed state field
    static uint8_t stateSteerIndex(uint16_t stateBits);

    /// engine index out of a relayed state field
    static uint8_t stateEngineIndex(uint16_t stateBits);

    /// S2C 0x40 1 plus 25 times count bytes or 34 times count when rawWorld
    static Packet motionBroadcast(const std::vector<MotionEntry>& entries,
                                  bool rawWorld = false);

    /// motionBroadcast minus recipientId its local car never reads the 0x40 queue
    static Packet motionBroadcastFor(uint32_t recipientId,
                                     const std::vector<MotionEntry>& entries,
                                     bool rawWorld = false);

    /// S2C 0x40 built from stored C2S bodies byte identical relay with no re encode
    static Packet motionBroadcastVerbatim(const std::vector<MotionRelayEntry>& entries,
                                          bool rawWorld = false);

    /// motionBroadcastVerbatim minus recipientId the normal in race relay path
    static Packet motionBroadcastVerbatimFor(uint32_t recipientId,
                                             const std::vector<MotionRelayEntry>& entries,
                                             bool rawWorld = false);

    /// no player id or count on the wire caller attaches sender identity trailing bytes ignored
    static bool parseSelfReport(const uint8_t* data, size_t len, bool rawWorld, CarState& out);

    static bool parseSelfReport(const Packet& pkt, bool rawWorld, CarState& out);

    /// encodes a CarState as a C2S body for a bot relay or resending to a late joiner
    static std::vector<uint8_t> buildSelfReportBody(const CarState& state,
                                                    bool rawWorld = false);

    /// bypasses interpolation and flushes the 0x40 queue send before any 0x40 for exact grid placement
    static Packet teleport(uint32_t playerId, float x, float y, float z, float yawDegrees);

    static Packet teleportToGrid(uint32_t playerId, const GridSpawn& spawn);

    /// S2C 0x47 effect use 24 bytes params relayed as raw dwords
    static Packet effect(uint32_t playerId, uint32_t kind,
                         uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4);

    /// S2C 0x47 with float params for the kinds that read them as world coords
    static Packet effectAt(uint32_t playerId, uint32_t kind,
                           float p1, float p2, float p3, float p4);

    /// sets car offset 686272 so the 0x40 handler discards entries until the state machine clears
    static Packet motionBlock(uint32_t playerId, int16_t delayMs);

    /// S2C 0x69 per car effect trigger 7 bytes codes 100 200 300 700 and 1000
    static Packet carEffect(uint32_t playerId, int16_t code);

    /// S2C 0xA5 position only correction 1 plus 12 times count bytes skips yaw and state
    static Packet positionFix(const std::vector<PositionFixEntry>& entries);

    /// empty when the track has no authored grid then the client falls back to sub 486C20
    static std::vector<GridSpawn> gridSpawns(int32_t trackId);

    /// extra rows stack behind the last one on the up axis so a race never drops a seated racer
    static std::vector<GridSpawn> padGeneratedGrid(std::vector<GridSpawn> grid, size_t wanted);

    /// whole wire periods between two samples at least one so bunched frames never shrink the budget
    static uint32_t motionSteps(const MotionGateLimits& limits, uint64_t gapMs);

    /// judges one sample and always takes it as the new baseline so a gate can never wedge a car
    static MotionVerdict motionStep(MotionGateState& state, const MotionGateLimits& limits,
                                    float x, float y, float z, uint64_t nowMs,
                                    float* budgetXYOut = nullptr);
};

} // namespace knc
