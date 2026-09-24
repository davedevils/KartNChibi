
#pragma once
#include "net/Session.h"
#include "net/Packet.h"

namespace knc {

class AuthHandler {
public:
    void handleHeartbeat(Session::Ptr session, Packet& packet);
    std::string generateToken();
};

} // namespace knc

