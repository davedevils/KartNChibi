#pragma once
#include "net/Packet.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// 28 byte replay frame shared by rep files and the wire sub 4A04F0 loader sub 49FF90 consumer proven identical
struct ReplayFrame {
    float    posX  = 0.0f;
    float    posY  = 0.0f;
    float    posZ  = 0.0f;   ///< posZ is up axis yaw is 255 units per 360 degrees
    uint8_t  yaw   = 0;
    uint32_t flags = 0;      ///< flags is the kFlag mask steerSpeed low nibble is steer high nibble is speed
    uint32_t steerSpeed = 0;
    uint8_t  inputMask  = 0; ///< inputMask is the kInput mask
};

/// 176 byte leaderboard row only name at 0x04 and time at 0xAC are read rest is zero filled
struct GhostRecordEntry {
    std::u16string name;        ///< name at 0x04 is UTF-16LE plus NUL timeMs at 0xAC zero or less draws an empty row
    int32_t        timeMs = 0;
};

/// everything S2C 0xAA carries to spawn the ghost and enter stage 15 name is variable width
struct GhostSessionInfo {
    uint32_t                 trackId = 0;
    std::u16string           name;              ///< name is the ghost display nickname 13 units max carKind only 3 and 4 are distinguishable
    uint32_t                 carKind = 3;
    int32_t                  recordTimeMs = 0;  ///< recordTimeMs is inert see kRecordTimeIsInert charBlock is the same 44 byte block as S2C 0x3E
    std::array<uint8_t, 0x2C> charBlock{};
    std::array<uint8_t, 0x38> kartBlock{};      ///< kartBlock is the same 56 byte block as S2C 0x3E
};

/// a ghost record row as persisted without the replay frames
struct GhostRecord {
    int32_t                  trackId  = 0;
    uint32_t                 charId   = 0;
    std::u16string           name;
    int32_t                  timeMs   = 0;
    uint32_t                 carKind  = 3;
    uint32_t                 frameCount = 0;
    std::array<uint8_t, 0x2C> charBlock{};
    std::array<uint8_t, 0x38> kartBlock{};
};

/// tracks one upload burst because the client never waits for an ack so a mismatch rejects the time submit
struct GhostUploadSession {
    int32_t  trackId        = -1;     ///< trackId from C2S 0xAA minus one means no ghost race running started is C2S 0xB1 seen
    bool     started        = false;
    bool     finishedLap    = false;  ///< finishedLap is C2S 0xB2 seen announcedCount is from C2S 0xAE
    uint32_t announcedCount = 0;
    std::vector<ReplayFrame> frames;  ///< frames appended by C2S 0xAF
};

/// board is 0xAB then trackCount x 0xAC then 0xAD race is 0xAE then 0xAF then 0xAA last
struct GhostPackets {

    static constexpr uint8_t kOpMenuSelect   = 0x2C;  ///< MenuSelect is C2S top menu ghost is kind 1 GhostEnter is C2S pick track S2C ghost session
    static constexpr uint8_t kOpGhostEnter   = 0xAA;
    static constexpr uint8_t kOpBoardHeader  = 0xAB;  ///< BoardHeader is S2C track count BoardTrack is S2C one track of the board
    static constexpr uint8_t kOpBoardTrack   = 0xAC;
    static constexpr uint8_t kOpBoardOpen    = 0xAD;  ///< BoardOpen is S2C open the board ReplayCount is S2C and C2S frame count
    static constexpr uint8_t kOpReplayCount  = 0xAE;
    static constexpr uint8_t kOpReplayChunk  = 0xAF;  ///< ReplayChunk is S2C and C2S frame append Submit is C2S time submit S2C result
    static constexpr uint8_t kOpSubmit       = 0xB0;
    static constexpr uint8_t kOpStageBegin   = 0xB1;  ///< StageBegin is C2S stage 15 started FinalLap is C2S final lap crossed
    static constexpr uint8_t kOpFinalLap     = 0xB2;
    static constexpr uint8_t kOpReplayCountAlt = 0xF6;  ///< ReplayCountAlt is S2C same as 0xAE for stage 17 ReplayChunkAlt is S2C same as 0xAF for stage 17
    static constexpr uint8_t kOpReplayChunkAlt = 0xF7;

    static constexpr uint32_t kMenuKindGhost = 1;  ///< C2S 0x2C field 1 for the ghost board

    static constexpr size_t kFrameSize        = 28;   ///< kFrameSize is one ReplayFrame on the wire kRecordEntrySize is 0xB0
    static constexpr size_t kRecordEntrySize  = 176;
    static constexpr size_t kCharBlockSize    = 0x2C;
    static constexpr size_t kKartBlockSize    = 0x38;

    /// name at offset 0x04 and time at 0xAC so name plus NUL must fit in 168 bytes
    static constexpr size_t kRecordNameMaxUnits = 83;

    /// S2C 0xAA name destination is 28 bytes past ghost stage offset 24 then the char block
    static constexpr size_t kGhostNameMaxUnits = 13;

    static constexpr uint32_t kMaxTracks       = 55;    ///< kMaxTracks board holds 55 track slots kMaxBoardEntries list refuses past 30
    static constexpr size_t   kMaxBoardEntries = 30;
    static constexpr size_t   kMaxTopRecords   = 3;     ///< S2C 0xB0 clamps to 3 kMaxFramesPerChunk is 136 frames per S2C 0xAF chunk above this client stores zero frames
    static constexpr size_t   kMaxFramesPerChunk = 136;
    static constexpr size_t   kChunkFrames     = 136;   ///< kChunkFrames is what the client uploader uses kMaxFramesArray is the 672000 byte span from offset 14164 to 686164
    static constexpr uint32_t kMaxFramesArray  = 24000;
    static constexpr uint32_t kMaxFramesGhost  = 2400;  ///< kMaxFramesGhost is the recorder cap outside stage 13 kUploadClamp is client clamps 0xAE to this
    static constexpr uint32_t kUploadClamp     = 2399;

    /// only two car kinds survive the stage init anything else is forced to 3
    static constexpr uint32_t kCarKindA = 3;
    static constexpr uint32_t kCarKindB = 4;

    static constexpr float kSimStepSeconds   = 0.02f;  ///< kSimStepSeconds sub 44D1A0 returns this kDecimationGhost is one frame per ten ticks
    static constexpr int   kDecimationGhost  = 10;
    static constexpr int   kDecimationStage13 = 1;     ///< license stage keeps every tick
    static constexpr int   kFrameMsGhost     = 200;
    static constexpr int   kFrameMsStage13   = 20;

    static constexpr float kMaxSteerDegrees  = 45.0f;  ///< kMaxSteerDegrees is flt 5EB700 kSpeedStep is flt 5A6B18
    static constexpr float kSpeedStep        = 600.0f;
    static constexpr float kSpeedFloor       = 1000.0f;///< kSpeedFloor is what the encoder subtracts kSpeedSpan is what the encoder clamps to
    static constexpr float kSpeedSpan        = 9000.0f;

    static constexpr uint32_t kFlagDriftA   = 0x0080;  ///< DriftA car offset 13056 one and offset 13060 zero DriftB car offset 13056 one and offset 13060 one
    static constexpr uint32_t kFlagDriftB   = 0x0040;
    static constexpr uint32_t kFlagState5   = 0x0020;  ///< State5 car offset 13101 equals one State4A car offset 13732 equals zero
    static constexpr uint32_t kFlagState4A  = 0x0010;
    static constexpr uint32_t kFlagState4B  = 0x0100;  ///< State4B car offset 13732 equals one State4C car offset 13732 equals two
    static constexpr uint32_t kFlagState4C  = 0x0200;
    static constexpr uint32_t kFlagState3   = 0x0008;  ///< State3 car offset 13808 equals one RollA car offset 686308 equals one
    static constexpr uint32_t kFlagRollA    = 0x0004;
    static constexpr uint32_t kFlagRollB    = 0x0002;  ///< RollB car offset 686308 equals two KnownMask is everything else stays zero
    static constexpr uint32_t kFlagKnownMask = 0x03FE;

    static constexpr uint8_t kInputUp    = 0x80;  ///< input bits via sub 49FAD0 encode sub 4A03D0 decode Up is VK 38 Down is VK 40
    static constexpr uint8_t kInputDown  = 0x40;
    static constexpr uint8_t kInputLeft  = 0x20;  ///< Left is VK 37 Right is VK 39
    static constexpr uint8_t kInputRight = 0x10;
    static constexpr uint8_t kInputDrift = 0x08;  ///< Drift is VK 16 Item is VK 17
    static constexpr uint8_t kInputItem  = 0x04;
    static constexpr uint8_t kInputSlot  = 0x02;  ///< Slot is VK 18 KnownMask bit 0x01 is never recorded
    static constexpr uint8_t kInputKnownMask = 0xFE;

    // S2C 0xAA field 4 picks between two strings both overwritten before display so it changes nothing on screen

    /// degrees to the 255 per 360 wire byte mirrors sub 44D9C0 then the multiply
    static uint8_t yawToByte(float degrees);

    /// wire byte back to the degrees the client decoder lands on
    static float yawFromByte(uint8_t value);

    /// steer degrees from minus 45 to 45 to the 4 bit encoder bucket
    static uint8_t steerBucket(float steerDegrees);

    /// bucket back to degrees client decode is bucket times 6 minus 45 plus 6 not the encoder inverse
    static float steerDegreesFromBucket(uint8_t bucket);

    /// raw speed to the 4 bit bucket clamp v minus 1000 between 0 and 9000 over 600
    static uint8_t speedBucket(float rawSpeed);

    /// bucket back to raw speed client drops the 1000 floor so this round trips with a bias
    static float speedFromBucket(uint8_t bucket);

    /// compose the steerSpeed dword
    static uint32_t makeSteerSpeed(uint8_t steer, uint8_t speed);

    /// steer bucket out of a steerSpeed dword
    static uint8_t steerOf(uint32_t steerSpeed);

    /// speed bucket out of a steerSpeed dword
    static uint8_t speedOf(uint32_t steerSpeed);

    /// serialize one frame into 28 bytes both pad runs zeroed
    static std::array<uint8_t, kFrameSize> encodeFrame(const ReplayFrame& frame);

    /// deserialize 28 bytes into a frame unset bits are kept as is not masked false when short
    static bool decodeFrame(const uint8_t* data, size_t len, ReplayFrame& out);

    /// deserialize a packed run of 28 byte frames
    static bool decodeFrames(const uint8_t* data, size_t len, size_t count,
                             std::vector<ReplayFrame>& out);

    /// serialize frames into a packed byte run of 28 bytes each
    static std::vector<uint8_t> encodeFrames(const std::vector<ReplayFrame>& frames);

    /// reads a rep file mirrors sub 4A04F0 zero or less count succeeds with no frames above 24000 is refused
    static bool readRepFile(const std::string& path, std::vector<ReplayFrame>& out);

    /// decode an in memory rep image
    static bool decodeRep(const uint8_t* data, size_t len, std::vector<ReplayFrame>& out);

    /// writes a rep file no known writer exists this layout is proven only against loader sub 4A04F0
    static bool writeRepFile(const std::string& path, const std::vector<ReplayFrame>& frames);

    /// encode an in memory rep image 4 plus 28 times n bytes
    static std::vector<uint8_t> encodeRep(const std::vector<ReplayFrame>& frames);

    /// builds the 176 byte row only name and time are filled names longer than the max are truncated
    static std::array<uint8_t, kRecordEntrySize> recordEntryBytes(const GhostRecordEntry& e);

    /// S2C 0xAB announces how many 0xAC packets follow trackCount above 55 is clamped to stay in bounds
    static Packet boardHeader(uint32_t trackCount);

    /// S2C 0xAC is one track of the board client keys by arrival order entries past 30 drop
    static Packet boardTrack(uint32_t trackIdEcho, const GhostRecordEntry& best,
                             const std::vector<GhostRecordEntry>& entries);

    /// S2C 0xAD empty closes the wait popup and opens the board
    static Packet boardOpen();

    /// S2C 0xAE sets the ghost frame count and resets its cursor always send even zero or an old ghost survives
    static Packet ghostFrameCount(uint32_t frameCount);

    /// S2C 0xAF 4 plus 28 times n bytes appends frames to the ghost car
    static Packet ghostFrameChunk(const ReplayFrame* frames, size_t count);

    /// splits frames into S2C 0xAF packets refuses past 24000 frames because the cursor is not bounds checked
    static std::vector<Packet> ghostFrameChunks(const std::vector<ReplayFrame>& frames);

    /// S2C 0xAA spawns the ghost and enters stage 15 send last name capped at 13 units before the block
    static Packet ghostSession(const GhostSessionInfo& info);

    /// S2C 0xF6 4 bytes same shape as 0xAE for the other replay stage
    static Packet altFrameCount(uint32_t frameCount);

    /// S2C 0xF7 4 plus 28 times n bytes same shape as 0xAF for the other replay stage
    static Packet altFrameChunk(const ReplayFrame* frames, size_t count);

    /// S2C 0xB0 answers C2S 0xB0 only ranks 1 2 3 draw a badge a short top list inflates placement
    static Packet submitResult(uint32_t rank, const GhostRecordEntry& mine,
                               const std::vector<GhostRecordEntry>& top);

    /// C2S 0x2C 8 bytes menuKind 1 is the ghost board request
    static bool parseMenuSelect(const Packet& pkt, uint32_t& outMenuKind,
                                uint32_t& outSubKind);

    /// C2S 0xAA picks a track one send site has no socket check so re check the level server side
    static bool parseGhostEnter(const Packet& pkt, uint32_t& outTrackId);

    /// C2S 0xB1 empty stage 15 began recording started
    static bool parseStageBegin(const Packet& pkt);

    /// C2S 0xB2 empty final lap crossed
    static bool parseFinalLap(const Packet& pkt);

    /// C2S 0xAE starts the upload client clamps its count to 2399 so anything above is forged
    static bool parseUploadCount(const Packet& pkt, uint32_t& outFrameCount);

    /// C2S 0xAF 4 plus 28 times n bytes one upload chunk normally 136 frames
    static bool parseUploadChunk(const Packet& pkt, std::vector<ReplayFrame>& outFrames);

    /// C2S 0xB0 12 bytes the lap time submit that must be answered
    static bool parseSubmit(const Packet& pkt, uint32_t& outTrackId,
                            uint32_t& outTotalTimeMs, uint32_t& outCarKind);

    /// apply a parsed C2S 0xAE resets any frames already collected
    static bool beginUpload(GhostUploadSession& session, uint32_t frameCount);

    /// apply a parsed C2S 0xAF refuses to grow past what 0xAE announced
    static bool appendUpload(GhostUploadSession& session,
                             const std::vector<ReplayFrame>& frames);

    /// true when the burst is coherent needs a track a 0xB1 a 0xB2 and exactly the frames 0xAE announced
    static bool uploadComplete(const GhostUploadSession& session);

    /// clear a session between races
    static void resetUpload(GhostUploadSession& session);

    /// char block the offline path uses dwords 0 0 12000 12100 12200 -1 -1
    static std::array<uint8_t, 0x2C> defaultCharBlock();

    /// kart block the offline path uses dwords 0 0 1000 1100 -1
    static std::array<uint8_t, 0x38> defaultKartBlock();

    /// persists a record and its frames keeping only the best time frames are chunked past the db column cap
    static bool saveRecord(const GhostRecord& record, const std::vector<ReplayFrame>& frames);

    /// best rows for a track fastest first timeMs above zero only
    static std::vector<GhostRecord> trackLeaderboard(int32_t trackId, size_t limit);

    /// leaderboard mapped to board rows
    static std::vector<GhostRecordEntry> trackEntries(int32_t trackId, size_t limit);

    /// fastest row of a track
    static bool trackBest(int32_t trackId, GhostRecord& out);

    /// the one row a character holds on a track false when none
    static bool playerBest(int32_t trackId, uint32_t charId, GhostRecord& out);

    /// the ghost a character races is the record just ahead of its own or the best row when none exists
    static bool ghostAhead(int32_t trackId, uint32_t charId, GhostRecord& out);

    /// frames of one stored replay empty when the record has none
    static bool loadReplay(int32_t trackId, uint32_t charId, std::vector<ReplayFrame>& out);

    /// one based placement a time would take feed straight to submitResult only 1 2 3 draw a badge
    static uint32_t rankOf(int32_t trackId, int32_t timeMs);
};

} // namespace knc
