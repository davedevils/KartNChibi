
#include "handlers/AuthHandler.h"
#include "logging/Logger.h"
#include <random>

namespace knc {

void AuthHandler::handleHeartbeat(Session::Ptr session, Packet&) {
    // client sends 0xA6 every 1000ms as a heartbeat do not respond with 0x12 since that triggers a lobby state change

    LOG_DEBUG("AUTH", "Heartbeat from " + session->remoteAddress() + " (ignored on LoginServer)");
}

std::string AuthHandler::generateToken() {
    static const char chars[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    std::string token;
    token.reserve(32);
    
    // uses random device for better randomness
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);
    
    for (int i = 0; i < 32; ++i) {
        token += chars[dis(gen)];
    }
    
    return token;
}

} // namespace knc
