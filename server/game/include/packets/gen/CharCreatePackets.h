/// wire spec source KnC exe version 178 handlers sub 478DA0 sub 478D40 sub 479220 and more

#pragma once
#include "net/Packet.h"
#include "net/Protocol.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct CharCreatePackets {

    // opcodes below 0x100 use the exact Packet uint8 ctor fromCmdFull is never needed here

    static constexpr uint8_t OP_S_MESSAGE_KEY      = 0x01;  // MESSAGE KEY was CMD S LOGIN RESPONSE MESSAGE TEXT was CMD S DISPLAY MESSAGE
    static constexpr uint8_t OP_S_MESSAGE_TEXT     = 0x02;
    static constexpr uint8_t OP_S_OPEN_CHAR_CREATE = 0x03;  // OPEN CHAR CREATE was CMD S SET GAME VAR CREATE RESULT was CMD S REGISTER NICK RES
    static constexpr uint8_t OP_S_CREATE_RESULT    = 0x04;
    static constexpr uint8_t OP_C_LOGIN            = 0x07;  // LOGIN was CMD C CLIENT AUTH LOGIN ACCEPT shares 0x07 no CMD name exists for it
    static constexpr uint8_t OP_S_LOGIN_ACCEPT     = 0x07;
    static constexpr uint8_t OP_S_PROFILE_REFRESH  = 0x0A;  // PROFILE REFRESH was CMD S CONNECTION OK
    static constexpr uint8_t OP_S_CHANNEL_LIST     = 0x0E;
    static constexpr uint8_t OP_S_STAGE_GARAGE     = 0x0F;  // STAGE GARAGE was CMD S SHOW GARAGE STAGE SHOP was CMD S SHOW SHOP
    static constexpr uint8_t OP_S_STAGE_SHOP       = 0x10;
    static constexpr uint8_t OP_S_STAGE_MENU       = 0x11;  // STAGE MENU was CMD S SHOW MENU STAGE LOBBY was CMD S SHOW LOBBY
    static constexpr uint8_t OP_S_STAGE_LOBBY      = 0x12;
    static constexpr uint8_t OP_C_CHANNEL_SELECT   = 0x18;
    static constexpr uint8_t OP_S_SERVER_REDIRECT  = 0x19;  // SERVER REDIRECT was CMD S SERVER REDIRECT listed as 0x54 wrong STAGE TUTORIAL was CMD S TUTORIAL FAIL misnamed
    static constexpr uint8_t OP_S_STAGE_TUTORIAL   = 0x62;
    static constexpr uint8_t OP_C_REAUTH           = 0xA7;  // REAUTH and REAUTH ACCEPT share 0xA7 both were CMD S SESSION CONFIRM
    static constexpr uint8_t OP_S_REAUTH_ACCEPT    = 0xA7;
    static constexpr uint8_t OP_S_CLEAR_CATALOGS   = 0xBE;  // CLEAR CATALOGS was CMD S RACE INIT misnamed

    /// literal at 0x005A05E4 sent by both C2S 0x0007 and C2S 0x00A7
    static constexpr const char* CLIENT_VERSION = "178";

    /// no buttons at all the client cannot dismiss it never send this
    static constexpr uint32_t BOX_TYPE_WAIT_NO_BUTTON = 0;
    /// OK button dismissible socket stays open
    static constexpr uint32_t BOX_TYPE_OK             = 1;
    /// fatal sub 464030 calls sub 4775c0 and closes the socket itself
    static constexpr uint32_t BOX_TYPE_FATAL          = 2;

    // no numeric login result code the key alone identifies failure per sub 478DA0 s substring match

    /// unregistered account the client force disconnects on the key alone
    static constexpr const char* KEY_INVALID_ID        = "MSG_INVALID_ID";
    /// bad password or bad launcher token the client force disconnects
    static constexpr const char* KEY_REINPUT_IDPASS    = "MSG_REINPUT_IDPASS";
    /// database down the client force disconnects
    static constexpr const char* KEY_DB_ACCESS_FAIL    = "MSG_DB_ACCESS_FAIL";
    /// server starting or full box only socket stays open
    static constexpr const char* KEY_SERVER_NOT_READY  = "MSG_SERVER_NOT_READY";
    /// suspended account no special case so pair it with BOX TYPE FATAL
    static constexpr const char* KEY_BLOCK_USER        = "MSG_BLOCK_USER";
    /// account already logged in no special case
    static constexpr const char* KEY_USED_ID           = "MSG_USED_ID";
    /// kicked by an operator no special case
    static constexpr const char* KEY_USER_BAN          = "MSG_USER_BAN";
    /// gateway at capacity no special case
    static constexpr const char* KEY_GATEWAY_FULL      = "MSG_GATEWAY_USER_FULL_NOT_MOVE";
    /// client build mismatch no special case
    static constexpr const char* KEY_INVALID_VERSION   = "MSG_INVALID_VERSION";
    /// generic failure the client itself raises this one with type 2
    static constexpr const char* KEY_UNKNOWN_ERROR     = "MSG_UNKNOWN_ERROR";
    /// nickname rejected the creation popup reopens by itself
    static constexpr const char* KEY_INVALID_NICK      = "MSG_INVALID_NICK";
    /// nickname taken the creation popup reopens by itself
    static constexpr const char* KEY_ALREADY_REGIST    = "MSG_ALREADY_REGIST";

    // outside this set falls to MSG UNKNOWN ERROR type 2 per sub 479230 s switch and hard disconnects

    static constexpr uint32_t CREATE_OK               = 0;  // success body follows invalid nick maps to MSG INVALID NICK
    static constexpr uint32_t CREATE_INVALID_NICK     = 1;
    static constexpr uint32_t CREATE_ALREADY_REGIST   = 2;  // already regist maps to MSG ALREADY REGIST invalid nick alt is same box as result 1
    static constexpr uint32_t CREATE_INVALID_NICK_ALT = 3;

    // not a closed set a redirect forwards whatever mode S2C 0x0019 carried

    static constexpr uint32_t TARGET_STAGE_LOGIN    = 4;   // login is first login and channel server redirect is the redirect handler literal
    static constexpr uint32_t TARGET_STAGE_REDIRECT = 8;
    static constexpr uint32_t TARGET_STAGE_ROOM     = 9;   // room is sub 45c130 room server connect game is the game server
    static constexpr uint32_t TARGET_STAGE_GAME     = 11;

    static constexpr uint32_t REQ_STAGE_GARAGE        = 6;
    static constexpr uint32_t REQ_STAGE_SHOP          = 7;
    static constexpr uint32_t REQ_STAGE_LOBBY         = 8;
    static constexpr uint32_t REQ_STAGE_TUTORIAL_MENU = 14;  // what a band zero account asks for

    /// S2C 0x0007 and S2C 0x00A7 payload exact 0x4CC
    static constexpr size_t PROFILE_BLOB_SIZE = 1228;
    /// S2C 0x000A payload exact packed with no padding
    static constexpr size_t PROFILE_REFRESH_SIZE = 38;
    /// 0x2C character record same bytes as the S2C 0x001B entries
    static constexpr size_t CHARACTER_RECORD_SIZE = 0x2C;
    /// 0x38 kart record same bytes as the S2C 0x001C entries
    static constexpr size_t KART_RECORD_SIZE = 0x38;
    /// blob plus 0x4B4 and plus 0x4B8 when the account owns nothing yet
    static constexpr uint32_t NO_SELECTION = 0xFFFFFFFFu;
    /// blob plus 0x48A destination is 26 bytes so 12 chars plus NUL
    static constexpr size_t NICKNAME_MAX_CHARS = 12;
    /// client edit maxlen argument in sub 473730
    static constexpr size_t NICKNAME_EDIT_MAX_CHARS = 11;
    /// sub 473950 requires wcslen greater than 3
    static constexpr size_t NICKNAME_MIN_CHARS = 4;
    /// reauth token must fit the 1154 byte fixed region including its NUL
    static constexpr size_t REAUTH_TOKEN_MAX_CHARS = 576;
    /// sub 424250 refuses entry 81 and the client then disconnects
    static constexpr size_t CHANNEL_LIST_MAX = 80;
    /// channel name record slot is 256 bytes so 127 chars plus NUL
    static constexpr size_t CHANNEL_NAME_MAX_CHARS = 127;
    /// login screen id and password edits clamp at 255 in sub 4475a0
    static constexpr size_t ACCOUNT_FIELD_MAX_CHARS = 255;

    /// 1228 byte account profile stored at obj plus 0x80E1A8 unread fields go out as zero
    struct LoginProfile {
        uint32_t playerId = 0;                    ///< playerId plus 0x000 same id space as 0x0021 player id reauthSessionId plus 0x004 echoed as tail u32 of C2S 0x00A7
        uint32_t reauthSessionId = 0;
        std::u16string reauthToken;               ///< reauthToken plus 0x008 echoed as wstring of C2S 0x00A7 nickname plus 0x48A 12 chars max empty if fresh
        std::u16string nickname;
        uint8_t channelLevelBand = 0;             ///< channelLevelBand plus 0x4A4 and level plus 0x4A5 both gate the channel buttons
        uint8_t level = 0;
        uint32_t expCurrent = 0;                  ///< expCurrent plus 0x4A8 numerator of the EXP display astro plus 0x4AC is premium currency
        uint32_t astro = 0;
        uint32_t gold = 0;                        ///< gold plus 0x4B0 is game currency selectedCharacterId plus 0x4B4 is an owned instance id
        uint32_t selectedCharacterId = NO_SELECTION;
        uint32_t selectedKartId = NO_SELECTION;       ///< selectedKartId plus 0x4B8 owned instance id sessionRole plus 0x4BD 6 observer 2 hides create room else normal
        uint8_t sessionRole = 0;
        uint32_t expFloorCurrentLevel = 0;        ///< expFloorCurrentLevel plus 0x4C0 is the exp bar floor expRequiredNextLevel plus 0x4C4 denominator of EXP display
        uint32_t expRequiredNextLevel = 0;
        uint32_t equippedTitleKey = 0;            ///< equippedTitleKey plus 0x4C8 badge lookup key
    };

    /// raw 1228 byte body shared by S2C 0x0007 and S2C 0x00A7 exposed for diffing
    static std::array<uint8_t, PROFILE_BLOB_SIZE> profileBlob(const LoginProfile& profile);

    /// S2C 0x0007 login accept flips client to C2S 0x00A7 for later auth needs a stage packet within 9 seconds
    static Packet loginAccept(const LoginProfile& profile);

    /// S2C 0x00A7 reauth accept byte identical to 0x0007 but skips the launcher notification
    static Packet reauthAccept(const LoginProfile& profile);

    /// S2C 0x000A partial profile refresh same destinations as blob plus 0x4A4 to plus 0x4CB without padding
    static Packet profileRefresh(const LoginProfile& profile);

    /// S2C 0x0001 message key box key must match exactly by substring and stay under 255 chars
    static Packet messageKeyBox(const std::string& messageKey, uint32_t boxType);

    /// S2C 0x0002 wide text box destination is wchar t 160 so keep text under 159 chars
    static Packet messageTextBox(const std::u16string& text, uint32_t boxType);

    /// unknown account the client disconnects itself on the key
    static Packet loginFailInvalidId();
    /// wrong password or token the client disconnects itself on the key
    static Packet loginFailBadCredentials();
    /// database unreachable the client disconnects itself on the key
    static Packet loginFailDbDown();
    /// server starting or full box only socket survives
    static Packet loginFailServerNotReady();
    /// suspended account needs type 2 to actually close the socket
    static Packet loginFailBanned();
    /// already logged in elsewhere type 2
    static Packet loginFailDuplicate();
    /// kicked by an operator type 2
    static Packet loginFailKicked();
    /// gateway at capacity type 1
    static Packet loginFailGatewayFull();
    /// client build is not 178 type 1
    static Packet loginFailVersion();

    /// S2C 0x0003 opens popup needs 0xBF catalog first or sub 473730 divides by zero
    static Packet openCharacterCreate();

    /// S2C 0x00BE clears all catalogs mandatory before the 0xBF burst never send it after
    static Packet clearCatalogs();

    /// S2C 0x0004 rejection result must be one of the three CREATE codes popup reopens by itself
    static Packet createCharacterReject(uint32_t result);

    /// S2C 0x0004 success nickname truncated to 12 chars records match S2C 0x001B and 0x001C
    static Packet createCharacterSuccess(const std::u16string& nickname,
                                         const std::array<uint8_t, CHARACTER_RECORD_SIZE>& characterRecord,
                                         const std::array<uint8_t, KART_RECORD_SIZE>& kartRecord);

    /// server side mirror of the client length gate in sub 473950 does not check the taboo word list
    static uint32_t validateNickname(const std::u16string& nickname);

    /// sub 4E15E0 drops the 32 marks of the table at 0x703278 and lowers A to Z before the word search
    static std::u16string tabooFold(const std::u16string& text);

    /// true when a folded word of the taboo list sits inside the folded name the client wcsstr rule
    static bool tabooHit(const std::u16string& nickname, const std::vector<std::u16string>& words);

    /// one row of the S2C 0x000E channel list
    struct ChannelEntry {
        uint32_t id = 0;             ///< id echoed back as 2nd u32 of C2S 0x0018 name is 127 chars max
        std::u16string name;
        uint32_t population = 0;     ///< population is the gauge numerator capacity is the gauge denominator
        uint32_t capacity = 0;
        uint32_t tier = 0;           ///< tier is 0 1 or 2 picks the button row
    };

    /// S2C 0x000E channel list enters stage 4 more than 80 entries disconnects the client
    static Packet channelList(const std::vector<ChannelEntry>& channels,
                              const std::u16string& notice);

    /// S2C 0x000F stage 6 garage zero payload
    static Packet stageGarage();
    /// S2C 0x0010 stage 7 shop zero payload
    static Packet stageShop();
    /// S2C 0x0011 stage 5 menu zero payload
    static Packet stageMenu();
    /// S2C 0x0012 stage 8 lobby zero payload may warn on kart durability
    static Packet stageLobby();
    /// S2C 0x0062 stage 13 tutorial zero payload closes the modal first
    static Packet stageTutorial();

    /// S2C 0x0019 redirect client reconnects after 1000ms and ignores port when mode is 4
    static Packet serverRedirect(const std::string& host, uint32_t port, uint32_t mode);

    /// C2S 0x0007 the very first authentication of a client process
    struct LoginRequest {
        bool ok = false;                ///< false means malformed drop the client clientVersion must equal CLIENT VERSION
        std::string clientVersion;
        uint32_t targetStage = 0;       ///< targetStage is 4 on both first login call sites accountId is launcher userid or login screen id edit
        std::u16string accountId;
        std::u16string password;        ///< password is launcher token or the masked password edit
    };
    static LoginRequest parseLogin(const Packet& pkt);

    /// C2S 0x00A7 every authentication after the first one of a process
    struct ReauthRequest {
        bool ok = false;
        std::string clientVersion;
        uint32_t targetStage = 0;       ///< targetStage is the mode carried by the S2C 0x0019 that caused it reauthToken echoes S2C 0x0007 plus 0x008
        std::u16string reauthToken;
        uint32_t reauthSessionId = 0;   ///< reauthSessionId echoes S2C 0x0007 plus 0x004
    };
    static ReauthRequest parseReauth(const Packet& pkt);

    /// C2S 0x0004 nickname submit
    struct CreateCharacterRequest {
        bool ok = false;
        uint32_t driverKey = 0;         ///< the 0xBF record plus 0x0C base key not an index
        std::u16string nickname;
    };
    static CreateCharacterRequest parseCreateCharacter(const Packet& pkt);

    /// C2S 0x0018 channel button click
    struct ChannelSelectRequest {
        bool ok = false;
        uint32_t requestedStage = 0;    ///< requestedStage is 6 7 8 or 14 channelId is -1 for the shop and garage buttons
        int32_t channelId = -1;
    };
    static ChannelSelectRequest parseChannelSelect(const Packet& pkt);

    /// exact compare against the "178" literal the client sends
    static bool isSupportedVersion(const std::string& clientVersion);
};

} // namespace knc
