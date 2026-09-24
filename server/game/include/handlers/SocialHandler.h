/// messenger opcodes fixed dword 1A5CA48 blocks and dword 1A5BC30 requests wrongly routed through sub 4820F0 to shop purchase

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/SocialPackets.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace knc {

class GameServer;

class SocialHandler {
public:
    // chat whisper and small talk relay

    /// C2S 0x00B4 the only line the client types small talk swallows it since sub 475B90 skips its own echo
    void handleChatSend(Session::Ptr s, Packet& pkt, GameServer* srv);

    /// slash gm chat commands gated on the accounts gm level
    bool handleGmCommand(Session::Ptr s, const std::u16string& raw, GameServer* srv);
    /// getmoney getastro getexp and help console commands closed when KNC DEV COMMANDS is 0
    bool handleDevCommand(Session::Ptr s, const std::u16string& raw, GameServer* srv);


    /// slash notice type text fires S2C 0x116 at yourself to preview what it draws
    bool handleNoticeProbe(Session::Ptr s, const std::u16string& raw, GameServer* srv);

    /// C2S 0x00B5 whisper built by sub 480C00 carries a target id and wstring not 0x002F never a name
    void handleWhisperSend(Session::Ptr s, Packet& pkt, GameServer* srv);

    // messenger verbs

    /// C2S 0x6C 0x6F 0x7A 0x7D share one wstring body op alone picks invite add block or small talk
    void handleNameRequest(Session::Ptr s, uint16_t op, const std::u16string& name,
                           GameServer* srv);

    /// C2S 0x70 0x71 0x74 0x7B 0x7E 0x7F 0x80 share one u32 body op alone picks the verb
    void handleIdRequest(Session::Ptr s, uint16_t op, uint32_t playerId, GameServer* srv);

    /// what a C2S 0x6C invite meets before it goes out
    struct RoomInviteCheck {
        bool senderInRoom = false;
        bool roomAlive = false;
        bool targetOnline = false;
        bool targetIsSender = false;
        bool sameRoom = false;
        bool blocked = false;
    };

    /// S2C 0x126 key the inviter reads for a refused invite null on success covers lobby Invite item too FUN 00467A70
    static const char* roomInviteRefusal(const RoomInviteCheck& c) {
        if (!c.senderInRoom || !c.roomAlive || c.targetIsSender) return "MSG_UNSUPPORT";
        // the block is never leaked the missing user line covers it
        if (!c.targetOnline || c.blocked) return "MSG_NOT_FIND_USER";
        if (c.sameRoom) return "MSG_REJECT_SAME_ROOM";
        return nullptr;
    }

    /// C2S 0x6D answers S2C 0x6C invite sub 47B030 picks the code accept is really sub 45C1E0 via sub 481D70
    void handleRoomInviteAnswer(Session::Ptr s, const SocialPackets::RoomInviteAnswer& a,
                                GameServer* srv);

    /// C2S 0x72 profile by name answered on S2C 0x72 with the 0x68 blob
    void handleUserInfoByName(Session::Ptr s, const std::u16string& name, GameServer* srv);

    /// C2S 0x133 profile by id answered on S2C 0x28 with the same 0x68 blob
    void handleUserInfoById(Session::Ptr s, uint32_t playerId, GameServer* srv);

    /// C2S 0x73 presence poll every 3000 ms FUN 0047B340 writes activity only delta so idle stays off the wire
    void handleStatusPoll(Session::Ptr s, GameServer* srv);

    /// C2S 0x81 note answered on S2C 0x81 with an MSG NOTE code
    void handleNoteSend(Session::Ptr s, const SocialPackets::NoteSend& n, GameServer* srv);

    /// pushes the inbox on S2C 0x83 after login newest 30 rows oldest first
    void pushNoteBacklog(Session::Ptr s, GameServer* srv);

    /// C2S 0x84 mark read echoes the id that greys the row
    void handleNoteMarkRead(Session::Ptr s, uint32_t noteId);

    /// C2S 0x85 delete echoes the id that compacts the inbox
    void handleNoteDelete(Session::Ptr s, uint32_t noteId);

    /// C2S 0x132 lobby user list page answered with a fixed 296 byte page
    void handleUserListPage(Session::Ptr s, uint32_t page, GameServer* srv);

    /// pushes S2C 0x76 0x78 0x79 once per session sub 468A00 re reads live so arrival timing is unconstrained
    void pushSocialLists(Session::Ptr s, GameServer* srv);

    /// drops every per session table call this from the disconnect path
    void onSessionClosed(uint32_t sessionId);

private:
    // mirror of client friend container else the presence poll null derefs
    std::mutex m_mirrorMutex;
    std::unordered_map<uint32_t, std::unordered_set<uint32_t>> m_friendMirror;

    /// last presence answered this session drives the 0x73 versus 0x77 choice
    struct PresenceSnapshot {
        bool valid = false;
        std::unordered_set<uint32_t> online;
        std::unordered_map<uint32_t, int32_t> activity;
    };
    std::mutex m_presenceMutex;
    std::unordered_map<uint32_t, PresenceSnapshot> m_presence;

    /// server side copy of the client chat flood guard from sub 480990
    struct ChatState {
        std::deque<int64_t> sends;      // accept times of the last five lines
        int64_t mutedUntilMs = 0;
        bool    muteLoaded   = false;
    };
    std::mutex m_chatMutex;
    std::unordered_map<uint32_t, ChatState> m_chatState;

    // small talk keyed by session so the disconnect path can drop it blind
    std::mutex m_talkMutex;
    std::unordered_map<uint32_t, uint32_t> m_talkInviteFrom;  // maps target session to requester character talk partner maps session to partner character
    std::unordered_map<uint32_t, uint32_t> m_talkPartner;

    void mirrorSet(uint32_t sessionId, std::unordered_set<uint32_t> ids);
    void mirrorAdd(uint32_t sessionId, uint32_t friendId);
    void mirrorErase(uint32_t sessionId, uint32_t friendId);
    bool mirrorGet(uint32_t sessionId, std::unordered_set<uint32_t>& out);

    void doRoomInvite(Session::Ptr s, const std::u16string& name, GameServer* srv);
    void doFriendAdd(Session::Ptr s, const std::u16string& name, GameServer* srv);
    void doBlockAdd(Session::Ptr s, const std::u16string& name, GameServer* srv);
    void doSmallTalkRequest(Session::Ptr s, const std::u16string& name, GameServer* srv);

    void doFriendAccept(Session::Ptr s, uint32_t requesterId, GameServer* srv);
    void doFriendReject(Session::Ptr s, uint32_t requesterId, GameServer* srv);
    void doFriendDelete(Session::Ptr s, uint32_t friendId, GameServer* srv);
    void doBlockDelete(Session::Ptr s, uint32_t blockedId, GameServer* srv);

    void doSmallTalkAccept(Session::Ptr s, uint32_t peerId, GameServer* srv);
    void doSmallTalkDecline(Session::Ptr s, uint32_t peerId, GameServer* srv);
    void doSmallTalkClose(Session::Ptr s, uint32_t peerId, GameServer* srv);

    // both sides gain the row and both mirrors follow it
    void bindFriendPair(Session::Ptr s, uint32_t otherId, GameServer* srv);

    bool chatGate(Session::Ptr s, std::u16string& text);
    void doWhisperByName(Session::Ptr s, const std::u16string& targetName,
                         const std::u16string& text, GameServer* srv);
    void doTeamChat(Session::Ptr s, const std::u16string& text, GameServer* srv);
    void deliverWhisper(Session::Ptr sender, Session::Ptr target, const std::u16string& text);
    bool relaySmallTalk(Session::Ptr s, const std::u16string& text, GameServer* srv);
    void broadcastNormalChat(Session::Ptr s, const std::u16string& text, GameServer* srv);

    /// S2C 0x0025 and 0x0027 both dead FUN 00463CD0 is an empty stub so neither can render
    void deadDialogProbe(Session::Ptr s);
};

} // namespace knc
