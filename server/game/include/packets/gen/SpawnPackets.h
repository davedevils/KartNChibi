/// yaw is degrees where sub 4EC290 multiplies by pi over 180 and the wire byte scale is 255 over 360

#pragma once
#include "net/Packet.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// grid lap and respawn wire layer plus the shipped track data readers all nested to avoid name collisions
struct SpawnPackets {

    static constexpr uint8_t OP_RANK_BOARD    = 0x0D;  ///< rankBoard S2C rank and minimap rebuild 0 bytes gridSpawn S2C the only packet that grids a car
    static constexpr uint8_t OP_GRID_SPAWN    = 0x3E;
    static constexpr uint8_t OP_CHECKPOINT    = 0x41;  ///< checkpoint C2S only checkpoint transition cameraMode S2C camera mode 5 opens the board
    static constexpr uint8_t OP_CAMERA_MODE   = 0x42;
    static constexpr uint8_t OP_LAP_BOARD     = 0x44;  ///< lapBoard S2C lap time board row advance progress C2S only client rank metric
    static constexpr uint8_t OP_PROGRESS      = 0x67;
    static constexpr uint8_t OP_RESPAWN       = 0x68;  ///< respawn C2S 16 result S2C 20 teleport 0xC3 goes to FUN 0047F990 not FUN 0047FAE0 licence container 0xC5
    static constexpr uint8_t OP_TRACK_CATALOG = 0xC3;  ///< trackCatalog S2C one track catalog row themeCatalog S2C one theme catalog row
    static constexpr uint8_t OP_THEME_CATALOG = 0xC4;

    static constexpr size_t SIZE_GRID_SPAWN_FIXED    = 176;  ///< plus 2 names each including a NUL terminator
    static constexpr size_t SIZE_RANK_BOARD          = 0;
    static constexpr size_t SIZE_CHECKPOINT          = 8;
    static constexpr size_t SIZE_CAMERA_MODE         = 4;
    static constexpr size_t SIZE_LAP_BOARD           = 4;
    static constexpr size_t SIZE_PROGRESS            = 4;
    static constexpr size_t SIZE_RESPAWN_C2S         = 16;
    static constexpr size_t SIZE_RESPAWN_S2C         = 20;
    /// 3 int32 then folder NUL then 14 int32 then tail NUL stride 0x8C plus strlen folder plus strlen tail
    static constexpr size_t SIZE_TRACK_CATALOG_FIXED = 70;
    static constexpr size_t SIZE_THEME_CATALOG_FIXED = 10;   ///< plus strlen folder plus strlen name

    /// sub 44EB60 writes wchar t String 35 with no cap so 35 units smashes the frame
    static constexpr size_t MAX_NAME_UNITS = 34;
    /// 0xC3 folder name and tail string land in char 36 slots
    static constexpr size_t MAX_FOLDER_CHARS = 35;
    /// 0xC4 theme folder lands in char 33
    static constexpr size_t MAX_THEME_FOLDER_CHARS = 32;
    /// 0xC4 display name lands in char 35
    static constexpr size_t MAX_THEME_NAME_CHARS = 34;

    /// sub 4B5160 skips any grid index outside 0 to 15 so that racer vanishes
    static constexpr uint32_t MAX_GRID_INDEX = 15;
    /// sub 495280 returns -1 when all 30 slots are taken so the 31st never spawns
    static constexpr size_t MAX_CARS = 30;
    /// sub 453020 returns -1 past row 128
    static constexpr size_t MAX_TRACK_ROWS = 128;
    /// sub 452F60 memsets 16 slots of 76 bytes
    static constexpr size_t MAX_THEME_ROWS = 16;

    static constexpr size_t MAX_START_ROWS   = 100;  ///< startRows sub 48A800 followNodes sub 489730 per path
    static constexpr size_t MAX_FOLLOW_NODES = 400;
    static constexpr size_t MAX_WARP_ROWS    = 50;   ///< warpRows sub 48AA10 followPaths follow 01 ini to follow 04 ini
    static constexpr int    MAX_FOLLOW_PATHS = 4;

    /// sub 48A710 pulls every ini through the pack VFS and hard fails at this size
    static constexpr size_t MAX_INI_BYTES = 40960;

    static constexpr float GRID_LIFT_Z   = 0.5f;  ///< gridLiftZ sub 486C20 adds to z in range only rescueLiftZ sub 489B40 adds to follow node z
    static constexpr float RESCUE_LIFT_Z = 3.0f;

    static constexpr uint32_t PROGRESS_PER_LAP     = 5000;  ///< progressPerLap sub 4A07F0 lap bucket cameraResultBoard 0x42 value that opens sub 4B6220
    static constexpr uint32_t CAMERA_RESULT_BOARD  = 5;
    static constexpr int32_t  LAP_BOARD_MIN        = 1;     ///< lapBoardMin sub 4B0DA0 clamp low lapBoardMax sub 4B0DA0 clamp high
    static constexpr int32_t  LAP_BOARD_MAX        = 9;
    static constexpr int32_t  FALL_TIMEOUT_MS      = 500;   ///< fallTimeoutMs sub 4871D0 default fallTimeoutBonus added while world offset 78648 armed
    static constexpr int32_t  FALL_TIMEOUT_BONUS   = 2000;
    /// sub 487230 compares theme rec offset 4 against 0x1312D00 so rally themes send no 0x41
    static constexpr int32_t  RALLY_THEME_ID = 20000000;
    /// sub 4A3BD0 accepts detected between expected and expected plus 5 with no wrap
    static constexpr uint32_t CHECKPOINT_WINDOW = 5;

    /// C2S 0x58 kind emitted right before respawn phase 100 relay lives in ResultsPackets
    static constexpr uint8_t ANIM_FELL_OFF = 6;

    /// one racer as S2C 0x3E puts it on the grid
    struct GridEntry {
        uint32_t       playerId  = 0;
        std::u16string displayName;              ///< displayName truncated to 34 utf16 units gridIndex 0 to 15 unique below start ini row count
        uint32_t       gridIndex = 0;
        uint32_t       team      = 0;
        std::array<uint8_t, 0x2C> character{};   ///< character is PacketBuilder characterRecord kart is PacketBuilder kartRecord
        std::array<uint8_t, 0x38> kart{};
        uint32_t       petBaseKey = 0;           ///< inferred as the 4th arg of sub 48CBB0
        std::array<uint8_t, 0x3C> customCar{};
    };

    /// decoded C2S 0x41 where next is always prev plus 1 mod checkpointCount
    struct CheckpointReport {
        uint32_t prev = 0;
        uint32_t next = 0;
    };

    /// decoded C2S 0x68 the same field set S2C 0x68 writes back
    struct RespawnReport {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
    };

    /// one row of start ini follow NN ini or warp ini all four share a shape
    struct TrackPoint {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
        float yawDegrees = 0.0f;
        bool  hadYawColumn = true;  ///< false when the line only had 3 columns
    };

    /// one entry of the checkpoint array at the head of a track COL
    struct TrackVec3 {
        float x = 0.0f;
        float y = 0.0f;
        float z = 0.0f;
    };

    /// one BOOST NNN cell of the track COL the client starts a pad boost when a wheel lands in it
    struct ColPadCell {
        float   a[3] = {0.0f, 0.0f, 0.0f};  ///< the three signed half planes inside when a x plus b y plus c is at or over zero
        float   b[3] = {0.0f, 0.0f, 0.0f};
        float   c[3] = {0.0f, 0.0f, 0.0f};
        int32_t row  = 0;   ///< row NNN of boost line kind number kind 0 mini turbo else item class minus one when ini missing
        int32_t kind = -1;
    };

    /// decoded head of a track COL plus the face name audit
    struct ColCheckpoints {
        int32_t                checkpointCount = 0;  ///< checkpointCount is countA index 0 is START points ordered along track in ini space
        std::vector<TrackVec3> points;
        int32_t faceCount = 0;
        int32_t edgeCount = 0;
        int32_t vertexCount = 0;
        int32_t distinctCheckNames = 0;  ///< distinctCheckNames distinct CHECK percent 03d names found on faces maxCheckNumber highest N seen differs from count on 2 tracks
        int32_t maxCheckNumber = 0;
        bool    hasStartFace = false;
        std::vector<ColPadCell> pads;    ///< BOOST NNN cells in the col frame the client tests minus x minus y
    };

    /// one S2C 0xC3 track row every field named by its client reader see docs packets opcodes 0x00C3
    struct TrackCatalogRow {
        uint32_t    visibleFlag  = 0;   ///< visibleFlag 0 zero hides row track catalog by index 0x453070 flag mode trackId 4 lookup key from S2C 0x14
        uint32_t    trackId      = 0;
        uint32_t    themeId      = 0;   ///< themeId 8 joins 0xC4 table folderName 12 second path component such as Forest 01
        std::string folderName;
        uint32_t    tuningEngineSetupBits = 1053609165;  ///< tuningEngineSetupBits 48 raw bits client global 0x5EB6F0 tuningEngineForceBits 52 raw bits client global 0x5EB6F4
        uint32_t    tuningEngineForceBits = 1058642330;
        float       tuningTurnForce = 0.0f;   ///< tuningTurnForce 56 turn force baseline client global 0x5EB6F8 unknown60 60 no reader in this build
        uint32_t    unknown60 = 0;
        uint32_t    fallOffTimeoutMs = FALL_TIMEOUT_MS;  ///< fallOffTimeoutMs 64 world fall timeout get 0x4871D0 difficulty 68 track select tile draw 0x475030 stars value plus one of six
        uint32_t    difficulty = 0;
        uint32_t    requiredLicense = 0;  ///< requiredLicense 72 license class gate against byte 0x1A20B08 specialModeOnly 76 above zero hides row room game mode 0 and 1
        uint32_t    specialModeOnly = 0;
        uint32_t    lapCount = 3;       ///< lapCount 80 board total client clamps 1 to 9 fogNear 84 into fog set stub 0x58B370 no effect this build
        float       fogNear = 0.0f;
        float       fogFar = 0.0f;      ///< fogFar 88 camera far clip set 0x43E520 in world track init lensFlareX 92 lens flare init 0x4D2A30 sun position
        float       lensFlareX = 0.0f;
        float       lensFlareY = 0.0f;  ///< lensFlareY 96 lens flare sun position lensFlareZ 100 negative turns flare off
        float       lensFlareZ = 0.0f;
        std::string displayNameKey;     ///< 104 def trans key and the %s INFO label

        /// server side only never on the wire checkpointCount is countA of track COL needed to read 0x41 and 0x67
        int32_t checkpointCount = 0;
        int32_t startRowCount   = 0;  ///< start ini line count clamps grid index
    };

    /// one S2C 0xC4 theme row supplies the first path component
    struct ThemeCatalogRow {
        uint32_t    recordField0 = 0;  ///< recordField0 0 unknown themeId 4
        uint32_t    themeId      = 0;
        std::string themeFolder;       ///< themeFolder 8 such as Forest or Room Floor displayName 41 inferred
        std::string displayName;
    };

    /// S2C 0x3E resolves grid index through sub 4A05C0 send it only after sub 4875C0 has loaded the world
    static Packet gridSpawn(const GridEntry& entry);

    /// S2C 0x0D rank rebuild send once after the last 0x3E sub 4B5160 skips grid index outside 0 to 15
    static Packet rankBoardRebuild();

    /// S2C 0x44 lap advance unicast per completed lap never at the finish sub 4B0CF0 promotes the running time
    static Packet lapBoardAdvance();

    /// S2C 0x42 4 bytes value 5 opens the result board anything else is a camera
    static Packet cameraMode(uint32_t mode);

    /// cameraMode 5 the post race board opener
    static Packet openResultBoard();

    /// S2C 0x68 relays a C2S 0x68 to everyone except the sender without calling sub 4EC290
    static Packet respawnRelay(uint32_t playerId, const RespawnReport& report);

    /// S2C 0xC3 track catalog row push the whole table before the client leaves the lobby capacity 128 rows
    static Packet trackCatalogEntry(const TrackCatalogRow& row);

    /// S2C 0xC4 theme catalog row capacity 16 rows must be pushed before the 0xC3 rows that use it
    static Packet themeCatalogEntry(const ThemeCatalogRow& row);

    /// C2S 0x41 checkpoint transition 8 bytes only the local car emits it outside rally themes
    static bool parseCheckpoint(const uint8_t* data, size_t len, CheckpointReport& out);
    static bool parseCheckpoint(const Packet& pkt, CheckpointReport& out);

    /// C2S 0x67 progress score gated by value changed or 300 ms elapsed arrives about once per client frame
    static bool parseProgress(const uint8_t* data, size_t len, uint32_t& outScore);
    static bool parseProgress(const Packet& pkt, uint32_t& outScore);

    /// C2S 0x68 respawn or warp result 16 bytes the client picks its own point server only relays
    static bool parseRespawn(const uint8_t* data, size_t len, RespawnReport& out);
    static bool parseRespawn(const Packet& pkt, RespawnReport& out);

    /// hands out unique grid indices capped at 16 and startRowCount since sub 486C20 drops any car past that row count
    static std::vector<uint32_t> assignGridIndices(size_t racerCount, int32_t startRowCount);

    /// reject a grid that would corrupt the rank board logging the first offender
    static bool validateGrid(const std::vector<GridEntry>& entries, int32_t startRowCount);

    /// resolves grid index exactly as sub 4A05C0 does in range or the origin with no lift out of range
    static bool resolveGridPose(const std::vector<TrackPoint>& startRows,
                                int32_t gridIndex, TrackPoint& out);

    /// completed laps carried by a C2S 0x67 score
    static uint32_t progressLaps(uint32_t score);

    /// fraction of the current lap from 0 to 1 carried by a C2S 0x67 score
    static float progressLapFraction(uint32_t score);

    /// rebuilds the client score from state for anti cheat comparison per the sub 4A07F0 formula treat mismatch as noise
    static uint32_t progressScore(uint32_t lap, uint32_t nextCheckpoint,
                                  float segmentFraction, int32_t checkpointCount);

    /// the client side accept test uses plain arithmetic with no modular wrap
    static bool withinClientWindow(uint32_t detected, uint32_t expected);

    /// mirrors the client checkpoint pointer at car offset 13108 a mismatch means a lost or reordered packet
    class LapTracker {
    public:
        enum class Result : uint8_t {
            Ignored,      ///< Ignored track has no checkpoints nothing to count Advanced ordinary mid lap step
            Advanced,
            GridCross,    ///< GridCross the free crossing grid position produces at t zero LapComplete one more lap in the bag
            LapComplete,
            Rejected      ///< pair is impossible for this track state left untouched
        };

        /// arm for one race checkpointCount is countA of the main track COL
        void reset(int32_t checkpointCount, int32_t totalLaps);

        /// consume one parsed C2S 0x41
        Result onCheckpoint(const CheckpointReport& report);

        int32_t lapsCompleted() const { return m_lapsCompleted; }
        int32_t nextCheckpoint() const { return m_expected; }
        int32_t checkpointCount() const { return m_checkpointCount; }
        int32_t totalLaps() const { return m_totalLaps; }
        int32_t crossings() const { return m_crossings; }

        /// true when the last accepted report did not line up with expectation
        bool desynced() const { return m_desynced; }

        /// server side finish test nothing in the client ever ends a stage 11 race
        bool raceComplete() const {
            return m_totalLaps > 0 && m_lapsCompleted >= m_totalLaps;
        }

    private:
        int32_t m_checkpointCount = 0;
        int32_t m_totalLaps = 0;
        int32_t m_expected = 0;
        int32_t m_lapsCompleted = 0;
        int32_t m_crossings = 0;
        bool    m_desynced = false;
    };

    /// root of the deployed client tree KNC DATA ROOT or the repo DevClient path
    static std::string dataRoot();

    /// dataRoot World themeFolder trackFolder the base sub 4875C0 builds
    static std::string trackBase(const std::string& themeFolder,
                                 const std::string& trackFolder);

    /// a short line leaves missing columns at whatever the previous track wrote since sub 48A800 never clears the array
    static std::vector<TrackPoint> parseIniPoints(const std::string& text, size_t maxRows);

    /// reads and parses one ini file enforcing sub 48A710 pack VFS limits of 0 to 40960 bytes
    static std::vector<TrackPoint> loadIniPoints(const std::string& path, size_t maxRows);

    /// base start ini empty means the client track load would fail too
    static std::vector<TrackPoint> loadStartGrid(const std::string& themeFolder,
                                                 const std::string& trackFolder);

    /// base follow percent 02d ini pathIndex 1 to 4 feeds rescue prediction and AI
    static std::vector<TrackPoint> loadFollowPath(const std::string& themeFolder,
                                                  const std::string& trackFolder,
                                                  int pathIndex);

    /// base warp ini only two shipped tracks have one
    static std::vector<TrackPoint> loadWarpPoints(const std::string& themeFolder,
                                                  const std::string& trackFolder);

    /// reads track COL where A is the checkpoint count index 0 is START audit off mutes the name warnings
    static bool loadColCheckpoints(const std::string& path, ColCheckpoints& out, bool audit = true);

    /// loadColCheckpoints on base track COL the only col that sets the count then the pad kinds off boost ini
    static bool loadTrackCheckpoints(const std::string& themeFolder,
                                     const std::string& trackFolder,
                                     ColCheckpoints& out);

    /// boost ini lines are kind then four floats line N minus 1 gives cell BOOST N its kind
    static bool loadBoostPadKinds(const std::string& path, std::vector<ColPadCell>& pads);

    /// true when the wire point lies in the cell widened by tol units col frame is minus x minus y
    static bool padCellContains(const ColPadCell& cell, float x, float y, float tol);

    /// nearest ground unaware guess at what the client will report after a rescue
    static bool predictRescuePoint(const std::vector<TrackPoint>& followNodes,
                                   float fromX, float fromY, float fromZ,
                                   RespawnReport& out);

    /// one track catalog row false when the track id is not in the table
    static bool trackCatalog(int32_t trackId, TrackCatalogRow& out);

    /// whole track catalog ordered by track id capped at the 128 client rows
    static std::vector<TrackCatalogRow> trackCatalogAll();

    /// whole theme catalog ordered by theme id capped at the 16 client rows
    static std::vector<ThemeCatalogRow> themeCatalogAll();
};

} // namespace knc
