/// mission and license wire builders and C2S parsers

#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"
#include "packets/gen/SpawnPackets.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

/// mission and license domain packets where delivery order matters and 0xA2 must be sent exactly once per session
struct MissionPackets {

    static constexpr size_t MISSION_DEF_MAX       = 20;   ///< sub 450C10 drops 21st sub 450AB0 drops past 20
    static constexpr size_t MISSION_PROGRESS_MAX  = 20;
    static constexpr size_t LICENSE_PROGRESS_MAX  = 64;   ///< sub 450920 off by one at 65 sub 453270 rejects past 16
    static constexpr size_t LICENSE_TEST_DEF_MAX  = 16;

    static constexpr size_t MISSION_DEF_SIZE      = 188;  ///< 0xBC one 0x87 payload
    static constexpr size_t MISSION_STR_SLOT      = 33;
    static constexpr size_t MISSION_COMPLETE_BASE = 20;   ///< 0x8C without the reward tail
    static constexpr size_t LICENSE_RESULT_BASE   = 20;

    struct MissionDefWire {
        uint32_t missionId       = 0;  ///< missionId 0x04 key for sub 450CA0 missionKind 0x08 1 collect goalCount 0 or 2 reach finish
        uint32_t missionKind     = 0;
        int32_t  goalCount       = 0;  ///< goalCount 0x10 display clamps at 999 timeLimitMs 0x14 deadline is now plus this
        int32_t  timeLimitMs     = 0;
        uint32_t rewardExtra     = 0;  ///< rewardExtra 0x18 entry fee Mission Back MISSION ENTER rewardMileage 0x1C UNIT MILEAGE sub 43C060 y 0x231
        uint32_t rewardMileage   = 0;
        uint32_t rewardExp       = 0;  ///< rewardExp 0x20 UNIT EXP sub 43C060 y 0x246 rewardItemType 0x28 0 to 7 picks 0x8C tail size
        uint32_t rewardItemType  = 0;
        uint32_t rewardItemKey   = 0;  ///< rewardItemKey 0x2C catalog key for reward type above worldName 0x38 World Mission name track
        std::string worldName;
        std::string strKeyTitle;       ///< strKeyTitle 0x59 strKeySub 0x7A localization keys
        std::string strKeySub;
        std::string strKeyDesc;        ///< strKeyDesc 0x9B localization key offsets 0x00 0x0C 0x24 0x30 0x34 unused stay zero
    };

    /// one 8 byte row of S2C 0x88 and the whole S2C 0x8A payload
    struct MissionProgressEntry {
        uint32_t missionId = 0;
        uint32_t cleared   = 0;  ///< 1 draws the Mission clear png
    };

    /// one 12 byte row of S2C 0xA2 third dword after passed unread here so the builder writes 0
    struct LicenseProgressEntry {
        uint32_t licenseKey = 0;  ///< observed key space 0 to 3 and 10 to 13 and 20 to 23
        uint32_t passed     = 0;
    };

    struct LicenseTestDefWire {
        uint32_t licenseKey = 0;             ///< licenseKey offset 0x04 matched by 0xA3 name offset 0x08 buffer 36 cap 35 chars
        std::string name;
        std::array<uint32_t, 13> params{};   ///< params offset 0x2C to 0x5C strKeyA offset 0x60 buffer 33 cap 32 chars
        std::string strKeyA;
        std::string strKeyB;                 ///< strKeyB offset 0x81 buffer 35 cap 34 chars params index1 offset 0x30 client exp predict index2 offset 0x34 gold
    };

    /// reward tail shared by S2C 0x8C and S2C 0xA3
    struct RewardBlob {
        uint32_t type = 0;            ///< type 0 to 7 selects byte count bytes must match rewardBlobSize for the type
        std::vector<uint8_t> bytes;
    };

    /// S2C 0xA3 license test result
    struct LicenseResultWire {
        uint32_t licenseKey    = 0;
        uint32_t passed        = 0;
        bool     hasCurrency   = false;
        uint32_t currencyKey   = 0;
        uint32_t currencyCount = 0;
        bool     hasItem       = false;
        RewardBlob item;
    };

    /// C2S 0x8C and C2S 0x90 payload
    struct MissionIdReq {
        uint32_t missionId = 0;
    };

    /// the two echoes in the payload are untrusted client input
    struct LicenseSubmitReq {
        uint32_t licenseKey    = 0;
        uint32_t echoTestdef2C = 0;
        uint32_t echoTestdef34 = 0;
    };

    /// C2S 0x18 payload sent by the lobby buttons while in stage 4
    struct StageReq {
        uint32_t targetStage = 0;  ///< 24 is the mission menu and 14 is the license screen
        uint32_t arg         = 0;
    };

    /// C2S 0x8D comes from popup kind 0x1F which no code opens in this build the value is its reset slot
    struct PanelCloseReq {
        uint32_t popupContext = 0;  ///< popup slot 0x66AC reset to -1 by sub 454FE0 nothing else writes it
    };

    static size_t rewardBlobSize(uint32_t type);

    /// true when bytes match the size the client will read for that type
    static bool validateRewardBlob(const RewardBlob& blob);

    static RewardBlob characterReward(int32_t instanceId, int32_t baseKey);

    /// 0x38 owned kart record paint 0x08 plate 0x0C mode 0x2C durability 0x30
    static RewardBlob kartReward(int32_t instanceId, int32_t baseKey,
                                 int32_t paintKey, int32_t plateKey, int32_t kartItemKey,
                                 int32_t periodMode, int32_t periodValue);

    /// type 7 the pendant row sub 47B9E0 removes that key then appends instance and key
    static RewardBlob pendantReward(uint32_t pendantKey);

    /// sub 43B9A0 a row plays when cleared or first or right after a cleared one rows sorted by id
    static bool missionPlayable(const std::vector<MissionProgressEntry>& rows, uint32_t missionId);

    /// the 0x88 list stops at the first row not cleared so a row cleared out of order waits hidden
    static std::vector<MissionProgressEntry> chainProgress(const std::vector<MissionProgressEntry>& allRows);

    /// the rows a first clear adds to the chain in id order each one goes out as a 0x8A append
    static std::vector<MissionProgressEntry> rowsOpenedByClear(const std::vector<MissionProgressEntry>& allRows,
                                                               uint32_t clearedId);

    /// the fee the menu shows and the start charges zero once the row is cleared
    static uint32_t entryFeeFor(uint32_t fee, bool cleared) { return cleared ? 0u : fee; }

    static Packet missionDefinition(const MissionDefWire& def);

    static Packet missionProgressList(const std::vector<MissionProgressEntry>& rows);

    static Packet missionUnlocked(uint32_t missionId, uint32_t cleared);

    /// zero payload that opens stage 24 then closes MSG WAIT
    static Packet missionMenuAck();

    static Packet missionStartAck(uint32_t missionId, uint32_t goldAfter);

    /// FUN 0047EA70 reads count then count rows of 12 byte xyz points from track COL after 0x90
    static Packet missionCheckpointPath(const std::vector<SpawnPackets::TrackVec3>& points);

    /// FUN 0047EB70 zero payload closes the briefing popup snaps to the start spline switches camera
    static Packet missionGo();

    static Packet missionComplete(uint32_t missionId, uint32_t goldAfter, uint32_t expAfter);

    /// has reward 2 sub 47B9E0 reads twelve bytes writes nothing and still leaves state 3000
    static Packet missionRefused(uint32_t missionId);

    /// S2C 0x8C mission complete plus reward tail sized by the definition
    static Packet missionCompleteWithReward(uint32_t missionId, uint32_t goldAfter,
                                            uint32_t expAfter, const RewardBlob& reward);

    /// zero payload that opens stage 14 then closes MSG WAIT
    static Packet licenseScreenAck();

    static Packet licenseProgressList(const std::vector<LicenseProgressEntry>& rows);

    static Packet licenseTestDefinition(const LicenseTestDefWire& def);

    /// S2C 0xA3 license test result and license entry grant
    static Packet licenseTestResult(const LicenseResultWire& result);

    /// writes PlayerInfo offset 0x4A0 and pops MSG LICENSE UP
    static Packet licenseGradeUp(uint8_t grade);

    static bool parseMissionComplete(const Packet& pkt, MissionIdReq& out);

    static bool parseMissionStart(const Packet& pkt, MissionIdReq& out);

    static bool parseMissionMenuOpen(const Packet& pkt);

    static bool parseMissionListRequest(const Packet& pkt);

    static bool parseFullStateRequest(const Packet& pkt);

    static bool parseLicenseScreenOpen(const Packet& pkt);

    static bool parseLicenseTestSubmit(const Packet& pkt, LicenseSubmitReq& out);

    static bool parseLicensePanelClose(const Packet& pkt, PanelCloseReq& out);

    static bool parseStageRequest(const Packet& pkt, StageReq& out);

    static std::vector<MissionDefWire> loadMissionDefs();
    static std::vector<MissionProgressEntry> loadMissionProgress(int32_t characterId);
    static std::vector<LicenseTestDefWire> loadLicenseTestDefs();
    static std::vector<LicenseProgressEntry> loadLicenseProgress(int32_t characterId);
};

} // namespace knc
