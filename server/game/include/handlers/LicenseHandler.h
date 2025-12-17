/**
 * @file LicenseHandler.h
 * @brief Handles license/tutorial packets
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>

namespace knc {

class GameServer;

class LicenseHandler {
public:
    // Tutorial/License requests
    static void handleStartTutorial(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleTutorialComplete(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleLicenseTest(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleLicenseResult(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Progress queries
    static void handleGetLicenseProgress(Session::Ptr session, GameServer* server);
    
    // Helper functions
    static void sendTutorialInfo(Session::Ptr session, int tutorialId);
    static void sendLicenseProgress(Session::Ptr session, int characterId);
    static bool updateLicenseLevel(int characterId, int newLevel);
};

} // namespace knc

