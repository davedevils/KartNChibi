
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "handlers/AuthHandler.h"
#include "security/LoginToken.h"

namespace knc {

class HandshakeHandler {
public:
    /// LoginServer hands its launcher token store in so a 32 char password is checked not trusted
    void setTokenCheck(LauncherTokenCheck check) { m_tokenCheck = std::move(check); }

    void handleFullState(Session::Ptr session, Packet& packet);
    void handleClientAuth(Session::Ptr session, Packet& packet);
    /// C2S 0x00A7 the channel return sub 480500 echoes the blob head instead of the credentials
    void handleReauth(Session::Ptr session, Packet& packet);
    
    void sendConnectionOK(Session::Ptr session);
    void sendServerRedirect(Session::Ptr session, const std::string& ip, int port, 
                            const std::string& token = "");
                            
private:
    bool validateLogin(Session::Ptr session, const std::string& username, 
                       const std::string& password);
    // 0x07 also answers reauth on 0xA7 blob may be empty during creation 1228 bytes sub 479080 twin sub 479020
    void sendSessionInfo(Session::Ptr session, uint16_t opcode = 0x07);
    /// stores the account and token pair the blob head carries so a later 0x00A7 can be matched
    void storeLoginTicket(Session::Ptr session);
    void sendServerList(Session::Ptr session);
    void sendDisplayMessage(Session::Ptr session, const std::u16string& message, int32_t code);
    
    AuthHandler m_authHandler;
    LauncherTokenCheck m_tokenCheck;
};

} // namespace knc

