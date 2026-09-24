/// from sub 4777C0 the client reads strings via sub 44EB60 and sub 44EB30 with no bound check

#pragma once

#include "net/Packet.h"
#include "net/Protocol.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace knc {

struct SocialPackets {

    static constexpr uint16_t OP_CHAT                = 0x00B4;
    static constexpr uint16_t OP_WHISPER             = 0x00B5;
    static constexpr uint16_t OP_WHISPER_PROMPT      = 0x002A;  ///< whisperPrompt S2C zero payload systemChatLine S2C mixed encoding
    static constexpr uint16_t OP_SYSTEM_CHAT_LINE    = 0x0126;
    static constexpr uint16_t OP_USERLIST_PAGE       = 0x0132;
    static constexpr uint16_t OP_USERINFO_REQ_BY_ID  = 0x0133;  ///< C2S only reply is USERINFO POPUP userInfoPopup itself is S2C 0x68 profile blob
    static constexpr uint16_t OP_USERINFO_POPUP      = 0x0028;
    static constexpr uint16_t OP_USERINFO_BY_NAME    = 0x0072;  ///< both directions same 0x68 blob
    static constexpr uint16_t OP_FRIEND_ADD          = 0x006F;
    static constexpr uint16_t OP_FRIEND_REQ_ACCEPT   = 0x0070;
    static constexpr uint16_t OP_FRIEND_REQ_REJECT   = 0x0071;
    static constexpr uint16_t OP_FRIEND_STATUS_FULL  = 0x0073;
    static constexpr uint16_t OP_FRIEND_DEL          = 0x0074;
    static constexpr uint16_t OP_FRIEND_LIST         = 0x0076;  ///< friendList S2C only server pushes friendStatusPart S2C only delta form
    static constexpr uint16_t OP_FRIEND_STATUS_PART  = 0x0077;
    static constexpr uint16_t OP_FRIEND_REQUEST_LIST = 0x0078;
    static constexpr uint16_t OP_BLOCK_LIST          = 0x0079;
    static constexpr uint16_t OP_BLOCK_ADD           = 0x007A;
    static constexpr uint16_t OP_BLOCK_DEL           = 0x007B;
    static constexpr uint16_t OP_SMALLTALK_REQ       = 0x007D;
    static constexpr uint16_t OP_SMALLTALK_ACCEPT    = 0x007E;  ///< C2S accept decline and close all have S2C replies consumed by a stub handler
    static constexpr uint16_t OP_SMALLTALK_DECLINE   = 0x007F;
    static constexpr uint16_t OP_SMALLTALK_CLOSE     = 0x0080;
    static constexpr uint16_t OP_NOTE                = 0x0081;
    static constexpr uint16_t OP_NOTE_APPEND         = 0x0082;  ///< noteAppend S2C only one 396 byte record arrives live noteAppendSorted S2C only same record client resorts after
    static constexpr uint16_t OP_NOTE_APPEND_SORTED  = 0x0083;
    static constexpr uint16_t OP_NOTE_MARK_READ      = 0x0084;  ///< both directions carry a u32 note id
    static constexpr uint16_t OP_NOTE_DELETE         = 0x0085;
    static constexpr uint16_t OP_ROOM_INVITE         = 0x006C;
    static constexpr uint16_t OP_ROOM_INVITE_ANSWER  = 0x006D;  ///< roomInviteAnswer C2S only deadDialog37 S2C parsed then discarded
    static constexpr uint16_t OP_DEAD_DIALOG_37      = 0x0025;
    static constexpr uint16_t OP_DEAD_DIALOG_39      = 0x0027;

    static constexpr uint32_t CHAT_NORMAL    = 0;  ///< NORMAL name text line plus 3d bubble shown in stage 9 for type 0 DROPPED handler jumps straight to epilogue
    static constexpr uint32_t CHAT_DROPPED   = 1;
    static constexpr uint32_t CHAT_RAW       = 2;  ///< RAW raw text uses color id 2 no name prefix NOTICE big on screen notice via sub 406FF0
    static constexpr uint32_t CHAT_NOTICE    = 3;
    static constexpr uint32_t CHAT_SMALLTALK = 4;  ///< SMALLTALK routed into 1 on 1 smalltalk popup TYPE MAX anything above is dropped
    static constexpr uint32_t CHAT_TYPE_MAX  = 4;

    static constexpr uint32_t MSG_FRIEND_ADD          = 0;
    static constexpr uint32_t MSG_FRIEND_ADD_OK       = 1;   ///< 0x006F carries a FriendRecord with this code
    static constexpr uint32_t MSG_FRIEND_DUP          = 2;
    static constexpr uint32_t MSG_FRIEND_MAX          = 3;
    static constexpr uint32_t MSG_FRIEND_ACCEPTER_MAX = 4;
    static constexpr uint32_t MSG_FRIEND_NOUSER       = 5;
    static constexpr uint32_t MSG_FRIEND_DEL          = 6;
    static constexpr uint32_t MSG_NOTE_INPUTUSER      = 7;
    static constexpr uint32_t MSG_NOTE_INPUTNOTE      = 8;
    static constexpr uint32_t MSG_NOTE_NOUSER         = 9;
    static constexpr uint32_t MSG_NOTE_SEND           = 10;
    static constexpr uint32_t MSG_NOTE_NOSEND         = 11;
    static constexpr uint32_t MSG_FRIEND_REQ_QUEUED   = 12;  ///< queued with no dialog it just appends a request record

    // 0x7A polarity is inverted from 0x6F here zero is the success code
    static constexpr uint32_t BLOCK_ADD_OK = 0;

    // 0x6D answer codes come from reversed routines 47B030 and 481D70
    static constexpr uint32_t INVITE_ANSWER_BUSY_SMALLTALK = 4;
    static constexpr uint32_t INVITE_ANSWER_DECLINED       = 5;
    static constexpr uint32_t INVITE_ANSWER_IN_RACE        = 6;
    static constexpr uint32_t INVITE_ANSWER_SAME_ROOM      = 7;

    static constexpr size_t CAP_FRIENDS         = 100;  ///< friends sub 44EF90 caps at 100 returns -1 past that friendRequests capped by routine at 44EE10
    static constexpr size_t CAP_FRIEND_REQUESTS = 100;
    static constexpr size_t CAP_BLOCKS          = 30;   ///< blocks capped by routine at 44F150 not 100 userlistSlots fixed always all 7 slots on wire
    static constexpr size_t USERLIST_SLOTS      = 7;

    static constexpr size_t MAX_NICKNAME_CHARS  = 12;   ///< nickname every client name caps at 12 chatText name length plus 3 plus text length must be 255 or under
    static constexpr size_t MAX_CHAT_TEXT_CHARS = 251;
    static constexpr size_t MAX_WHISPER_CHARS   = 255;  ///< whisper 0x00B5 text buffer is 256 wchar messageKey 0x126 uses char String2 sized 260
    static constexpr size_t MAX_MESSAGE_KEY_LEN = 259;
    static constexpr size_t MAX_DEAD_ASCII_LEN  = 255;  ///< deadAscii 0x25 and 0x27 use byte array sized 256 noteBody messenger body input cap
    static constexpr size_t MAX_NOTE_BODY_CHARS = 160;
    static constexpr size_t MAX_PENDANT_SLOT    = 63;   ///< pendant sprites are only loaded for slots 1 to 63

    static constexpr size_t SIZE_PROFILE        = 0x68;   ///< profile 0x0028 and 0x0072 blob userInfo 0x00B5 carries two of these
    static constexpr size_t SIZE_USERINFO       = 0x4C8;
    static constexpr size_t SIZE_FRIEND_REC     = 0x2C;
    static constexpr size_t SIZE_REQUEST_REC    = 0x24;
    static constexpr size_t SIZE_BLOCK_REC      = 0x20;
    static constexpr size_t SIZE_USERLIST_ENTRY = 0x28;
    static constexpr size_t SIZE_USERLIST_PAGE  = 0x128;  ///< fixed 296 bytes regardless of entry count

    /// zero is safe here since FUN 00429210 rewrites the vptr rather than reading it like sub 4533D0
    static constexpr uint32_t USERLIST_VPTR = 0x005A3D3Cu;

    /// 0x28 and 0x72 profile blob 68 bytes
    struct Profile {
        uint32_t playerId     = 0;  ///< playerId offset 0 compared against my own UserInfo 0 to pick buttons name offset 4 fixed 13 char wide string
        std::u16string name;
        int8_t   level        = 0;  ///< level offset 0x1E MOVSX plus 1 at or over 50 forces full exp bar characterKey offset 0x28 portrait sprite key
        uint32_t characterKey = 0;
        int32_t  expCurrent   = 0;  ///< expCurrent 0x30 EXP cur over next expLevelBase 0x3C bar is cur minus base over next minus base times 256
        int32_t  expLevelBase = 0;
        int32_t  expLevelNext = 0;  ///< expLevelNext 0x40 second EXP cur over next statA1 0x48 pairs statA2 at x plus 488 y plus 219
        int32_t  statA1       = 0;
        int32_t  statB1       = 0;  ///< statB1 0x4C pairs statB2 at x plus 488 y plus 252 statC1 0x50 pairs statC2 as percent of total
        int32_t  statC1       = 0;
        int32_t  statA2       = 0;  ///< statA2 offset 0x58 statB2 offset 0x5C
        int32_t  statB2       = 0;
        int32_t  statC2       = 0;  ///< statC2 offset 0x60 pendantKey a catalog miss renders no equip pendant message
        uint32_t pendantKey   = 0;
        // offsets 0x1F 0x20 to 0x27 0x2C 0x34 to 0x3B 0x44 and 0x54 have no reader and stay zero filled
    };

    /// 0x76 friend row 0x2C bytes
    struct FriendRow {
        uint32_t playerId = 0;   ///< playerId record offset 0 name record offset 4 fixed 28 byte slot
        std::u16string name;
        uint32_t level    = 0;   ///< level record offset 0x20 statusA record offset 0x24 negative greys row out -2 is offline reset value
        int32_t  statusA  = -2;
        int32_t  statusB  = -2;  ///< record offset 0x28 written by 0x73 and 0x77 but read nowhere in this build
    };

    /// 0x78 pending incoming request row 0x24 bytes
    struct RequestRow {
        uint32_t requesterId = 0;  ///< requesterId record offset 0 echoed back as C2S 0x70 or 0x71 payload name record offset 4 fixed 28 byte slot
        std::u16string name;
        uint32_t level       = 0;  ///< record offset 0x20
    };

    /// 0x79 block row 0x20 bytes
    struct BlockRow {
        uint32_t playerId = 0;  ///< playerId record offset 0 echoed back as C2S 0x7B payload name record offset 4 fixed 28 byte slot
        std::u16string name;
    };

    /// 0x73 presence row 12 bytes
    struct StatusRow {
        uint32_t playerId = 0;
        int32_t  statusA  = 0;  ///< negative greys the friend out
        int32_t  statusB  = 0;
    };

    /// 0x77 delta presence row 8 bytes touches statusB only
    struct StatusPartialRow {
        uint32_t playerId = 0;
        int32_t  statusB  = 0;
    };

    /// 0x132 user list row 0x28 bytes
    struct UserListEntry {
        uint32_t playerId    = 0;  ///< playerId entry offset 0 sent verbatim as C2S 0x133 request name entry offset 4 fixed 28 byte slot
        std::u16string name;
        uint32_t level       = 0;  ///< level entry offset 0x20 pendantSlot entry offset 0x24 zero or under draws default sprite valid range 1 to 63
        int32_t  pendantSlot = 0;
    };

    /// S2C 0x6C incoming room invite field labels follow the review
    struct RoomInvite {
        uint32_t replyKey = 0;       ///< replyKey offset 0 only field C2S 0x6D answer echoes back inviterName 26 byte slot inferred to be inviter
        std::u16string inviterName;
        uint32_t roomId   = 0;       ///< roomId proven compared against 0x13 room id roomPassword proven forwarded as field 1 of C2S opcode 110
        std::u16string roomPassword;
        uint8_t  flag     = 0;       ///< flag zero enables "already in that room" answer 7 branch ascii string and trailing dword unread stay empty and zero
    };

    static std::array<uint8_t, 0x2C> friendRecord(const FriendRow& row);
    static std::array<uint8_t, 0x24> requestRecord(const RequestRow& row);
    static std::array<uint8_t, 0x20> blockRecord(const BlockRow& row);
    static std::array<uint8_t, 0x68> profileBlob(const Profile& profile);

    /// minimal 0x4C8 UserInfo carrying only the two fields 0xB5 reads prefer the real 1224 byte block from login
    static std::array<uint8_t, 0x4C8> userInfoStub(uint32_t playerId,
                                                   const std::u16string& nickname);

    /// S2C 0xB4 chat line playerId shares the 0x07 0x13 0x21 id space sender needs its own copy
    static Packet chatBroadcast(uint32_t playerId,
                                const std::u16string& senderName,
                                const std::u16string& text,
                                uint32_t chatType = CHAT_NORMAL);

    /// S2C 0xB4 with chatType 3 the big on screen notice
    static Packet chatNotice(const std::u16string& text);

    /// S2C 0xB4 with chatType 4 routed into the smalltalk popup
    static Packet smallTalkLine(uint32_t playerId,
                                const std::u16string& senderName,
                                const std::u16string& text);

    /// S2C 0xB5 whisper delivery sends the same packet to both parties who tell it apart by id
    static Packet whisperDeliver(const std::array<uint8_t, 0x4C8>& senderUserInfo,
                                 const std::array<uint8_t, 0x4C8>& receiverUserInfo,
                                 const std::u16string& text);

    /// S2C 0x2A carries no fields it just forces the client into whisper compose
    static Packet whisperPrompt();

    /// S2C 0x126 system line mixes encodings name is UTF-16 messageKey is an ascii key
    static Packet systemChatLine(const std::u16string& name,
                                 const std::string& messageKey,
                                 uint32_t type = 5);

    /// S2C 0x132 user list page is a fixed 296 bytes and always writes all 7 slots zero filling unused ones
    static Packet userListPage(uint32_t currentPage,
                               uint32_t pageCount,
                               const std::vector<UserListEntry>& entries);

    /// S2C 0x28 profile popup fixed 104 bytes replies to C2S 0x133
    static Packet userInfoPopup(const Profile& profile);

    /// S2C 0x72 profile blob fixed 104 bytes renders in the messenger
    static Packet userInfoBlob(const Profile& profile);

    /// S2C 0x76 full friend list server pushed
    static Packet friendList(const std::vector<FriendRow>& rows);

    /// S2C 0x78 pending incoming requests server pushed
    static Packet friendRequestList(const std::vector<RequestRow>& rows);

    /// S2C 0x73 full presence lists only online friends every omitted friend resets to -2 and renders offline
    static Packet friendStatusFull(const std::vector<StatusRow>& rows);

    /// S2C 0x77 delta presence rewrites statusB only
    static Packet friendStatusPartial(const std::vector<StatusPartialRow>& rows);

    /// S2C 0x6F result only for codes other than 1 and 12
    static Packet friendAddResult(uint32_t messageCode);

    /// S2C 0x6F result 1 the pair is now mutual
    static Packet friendAddResultAccepted(const FriendRow& row);

    /// S2C 0x6F result 12 request queued no dialog shown
    static Packet friendAddResultQueued(const RequestRow& row);

    /// S2C 0x74 friend delete the client removes the row regardless of messageCode so this can never report failure
    static Packet friendDelResult(uint32_t friendPlayerId,
                                  uint32_t messageCode = MSG_FRIEND_DEL);

    /// S2C 0x70 or 0x71 drops the pending row server must also push a friend result or fresh list
    static Packet friendRequestResolved(uint32_t requesterId, bool rejected);

    /// S2C 0x79 block list capped at 30 server pushed
    static Packet blockList(const std::vector<BlockRow>& rows);

    /// S2C 0x7A success zero is the only success code
    static Packet blockAddResultOk(const BlockRow& row);

    /// S2C 0x7A failure any non zero code shows no dialog at all
    static Packet blockAddResultError(uint32_t result);

    /// S2C 0x7B unblock removal is unconditional result is discarded but the dword must still be present
    static Packet blockDelResult(uint32_t playerId, uint32_t result = 0);

    /// S2C 0x81 use the sub 465FC0 message codes as MSG NOTE values
    static Packet messengerResult(uint32_t messageCode);

    /// S2C 0x7D smalltalk invite the client can suppress the popup so the server cannot rely on an answer
    static Packet smallTalkInvite(uint32_t requesterId, const std::u16string& requesterName);

    /// S2C 0x6C room invite field order follows the review not first pass labels
    static Packet roomInvite(const RoomInvite& invite);

    /// S2C 0x25 parsed then discarded kept only so the emulator does not mistake it for a live feature
    static Packet deadDialog37(const std::u16string& name,
                               const std::string& asciiText,
                               uint32_t value);

    /// S2C 0x27 parsed then discarded mixed encoding
    static Packet deadDialog39(uint32_t id,
                               const std::u16string& name,
                               const std::string& asciiText);

    // every C2S parser returns false on a short unterminated or oversized payload and leaves out untouched

    /// C2S 0x00B4
    struct ChatSend {
        std::u16string text;    ///< text already profanity masked by client chatType every proven call site passes 0
        uint32_t chatType = 0;
    };

    /// C2S 0xB5 only the messenger whisper sub window sends this
    struct WhisperSend {
        uint32_t targetPlayerId = 0;
        std::u16string text;
    };

    /// C2S 0x81 note body arrives with single quotes already doubled
    struct NoteSend {
        std::u16string recipientName;
        std::u16string body;
    };

    /// one inbox row appended by sub 47B710 and sub 47B7D0 sort keys carry date and time as text
    struct NoteRow {
        uint32_t id = 0;
        std::u16string sender;
        std::u16string date;   ///< date YYYY-MM-DD time HH MM SS
        std::u16string time;
        bool read = false;
        std::u16string body;
    };
    static constexpr size_t NOTE_RECORD_SIZE = 0x18C;

    /// C2S 0x006D answer to an incoming room invite
    struct RoomInviteAnswer {
        uint32_t replyKey = 0;  ///< replyKey is 0x6C replyKey verbatim not room id answer 4 busy 5 declined 6 in race 7 same room
        uint32_t answer   = 0;
    };

    static bool parseChatSend(const Packet& pkt, ChatSend& out);
    static bool parseWhisperSend(const Packet& pkt, WhisperSend& out);
    static bool parseNoteSend(const Packet& pkt, NoteSend& out);

    /// S2C 0x82 one note into the 30 slot inbox no resort for a live arrival
    static Packet noteAppend(const NoteRow& row);
    /// S2C 0x83 same record but the client bubble sorts the inbox after for the backlog
    static Packet noteAppendSorted(const NoteRow& row);
    /// S2C 0x84 and 0x85 acks return the u32 id the client flips or drops the row itself
    static Packet noteAck(uint16_t op, uint32_t noteId);
    static bool parseRoomInviteAnswer(const Packet& pkt, RoomInviteAnswer& out);

    /// C2S 0x132 page index 4 bytes also arrives every 10 seconds unprompted
    static bool parseUserListPageReq(const Packet& pkt, uint32_t& pageIndex);

    /// C2S 0x133 profile by id 4 bytes reply on USERINFO POPUP
    static bool parseUserInfoReqById(const Packet& pkt, uint32_t& playerId);

    /// C2S 0x72 profile by name reply on USERINFO BY NAME
    static bool parseUserInfoReqByName(const Packet& pkt, std::u16string& name);

    /// C2S 0x73 presence poll must be exactly zero bytes
    static bool parseFriendStatusPoll(const Packet& pkt);

    /// C2S 0x6F 0x7A 0x7D and 0x6C are all a bare name
    static bool parseNameRequest(const Packet& pkt, std::u16string& name);

    /// C2S 0x70 0x71 0x74 0x7B 0x7E 0x7F and 0x80 are all a bare id
    static bool parseIdRequest(const Packet& pkt, uint32_t& playerId);

    /// which chat channel a raw C2S 0x00B4 line asked for
    enum class ChatScope {
        Normal,   ///< Normal no recognised prefix Whisper slash w target text
        Whisper,
        Team      ///< slash t text
    };

    /// result of splitChatCommand
    struct ChatCommand {
        ChatScope scope = ChatScope::Normal;
        std::u16string target;  ///< target filled for whisper only may be empty when user typed slash w text line with prefix and target removed
        std::u16string text;
    };

    /// splits the leading slash w or slash t token since client concatenates fixed format strings
    static ChatCommand splitChatCommand(const std::u16string& raw);
};

}  // namespace knc
