/**
 * @file AuthHandler.h
 * @brief Handles authentication packets
 */

#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class AuthHandler {
public:
    void handleHeartbeat(Session::Ptr session, Packet& packet);
    void handleLogin(Session::Ptr session, const std::string& username, const std::string& password);
    void sendLoginResponse(Session::Ptr session, bool success, const std::string& message = "");
    std::string generateToken();
};

} // namespace knc

