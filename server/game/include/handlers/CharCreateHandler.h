/// sub 473730 divides by the driver catalog count so sub 4793F0 and sub 404410 must never see it empty

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/CharCreatePackets.h"

#include <cstdint>
#include <string>

namespace knc {

class GameServer;

/// character creation and login handshake handlers all static no cached state
class CharCreateHandler {
public:
    /// every S2C 0x0001 refusal the client has a reaction for
    enum class LoginFailure : uint8_t {
        InvalidId = 0,    ///< unknown account client disconnects on the key alone
        BadCredentials,
        DbDown,
        ServerNotReady,   ///< starting or full shows a box only socket survives
        Banned,
        Duplicate,        ///< already logged in elsewhere
        Kicked,
        GatewayFull,
        Version           ///< client build is not "178"
    };

    /// parses C2S 0007 checks the client version opens creation or sends login accept and channel list
    static bool handleLogin(Session::Ptr session, Packet& packet, GameServer* server);

    /// verifies the echoed reauth token and session id before accepting same contract as handleLogin
    static bool handleReauth(Session::Ptr session, Packet& packet, GameServer* server);

    /// creates character driver kart and legacy vehicle row in one transaction then answers never sends 0003 on failure
    static void handleCreateCharacter(Session::Ptr session, Packet& packet, GameServer* server);

    /// answers a channel button click with the matching stage opcode
    static void handleChannelSelect(Session::Ptr session, Packet& packet, GameServer* server);

    /// sends 0BE the 0BF burst and 0011 skips 0003 when empty since sub 473730 would divide by zero
    static void openCreateScreen(Session::Ptr session, GameServer* server);

    /// sends 0BE then one 0BF per enabled driver returns rows sent
    static size_t sendDriverCatalog(Session::Ptr session);

    /// sends the channel list this is the stage 4 ack nothing may follow it
    static void sendChannelList(Session::Ptr session);

    /// sends the 1228 byte login accept does not move the stage by itself
    static void sendLoginAccept(Session::Ptr session, uint32_t characterId);

    /// sends the reauth accept byte identical body to the login accept
    static void sendReauthAccept(Session::Ptr session, uint32_t characterId);

    /// sends the 38 byte partial profile refresh safe at any stage
    static void sendProfileRefresh(Session::Ptr session, uint32_t characterId);

    /// sends a message key box for one of the nine proven login refusals
    static void sendLoginFailure(Session::Ptr session, LoginFailure reason);

    /// sends a wide text box keep the text under 159 characters
    static void sendNotice(Session::Ptr session, const std::u16string& text, uint32_t boxType);

    /// sends the stage opcode matching a channel select value unknown values fall back to lobby with a warning
    static void sendStage(Session::Ptr session, uint32_t requestedStage);

    /// sends a server redirect mode 4 makes the client ignore the port and use its network2 ini port instead
    static void sendRedirect(Session::Ptr session, const std::string& host,
                             uint32_t port, uint32_t mode);

    /// returns the account's character id false when it owns none yet
    static bool accountCharacterId(uint32_t accountId, uint32_t& characterIdOut);

    /// fills the login profile blob so the profile refresh and login blob can never disagree
    static bool buildLoginProfile(uint32_t accountId, uint32_t characterId,
                                  CharCreatePackets::LoginProfile& out);

    /// checks length banned words and uniqueness returns create ok invalid nick or already regist
    static uint32_t validateNickname(const std::u16string& nickname);

    /// hex dumps the exact 1228 bytes for a wire diff against a capture
    static std::string profileBlobHex(const CharCreatePackets::LoginProfile& profile);
};

} // namespace knc
