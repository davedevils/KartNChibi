/**
 * @file HandshakeHandler.h
 * @brief Handles connection handshake
 */

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "handlers/AuthHandler.h"

namespace knc {

class HandshakeHandler {
public:
    void handleFullState(Session::Ptr session, Packet& packet);
    void handleClientAuth(Session::Ptr session, Packet& packet);
    void handleCharacterCreate(Session::Ptr session, Packet& packet);
    
    void sendConnectionOK(Session::Ptr session);
    void sendServerRedirect(Session::Ptr session, const std::string& ip, int port, 
                            const std::string& token = "");
                            
private:
    bool validateLogin(Session::Ptr session, const std::string& username, 
                       const std::string& password);
    bool sendSessionInfo(Session::Ptr session);  // Returns true if character exists
    void sendServerList(Session::Ptr session);
    void sendDisplayMessage(Session::Ptr session, const std::u16string& message, int32_t code);
    void sendCharacterCreationScreen(Session::Ptr session);
    
    AuthHandler m_authHandler;
};

} // namespace knc

