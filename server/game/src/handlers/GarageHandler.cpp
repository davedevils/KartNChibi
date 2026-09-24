// garage open plus the logged fallback of the install and remove verbs

#include "handlers/GarageHandler.h"
#include "GameServer.h"
#include "packets/PacketBuilder.h"
#include "logging/Logger.h"

namespace knc {

void GarageHandler::handleOpenGarage(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)server;

    // client sends 0x0F with no payload category comes from which tab the user clicked initial entry sends all four lists
    int32_t category = packet.remaining() >= 4 ? packet.readInt32() : 0;
    (void)category;  // logged for reference full dataset sent regardless

    LOG_DEBUG("GARAGE", "Open garage char=" + std::to_string(session->characterId) +
              " requested_category=" + std::to_string(category));

    if (session->characterId == 0) {
        session->send(PacketBuilder::displayMessage(u"You are not logged in.", 2));
        return;
    }

    // a working server answers garage open with only 0x0F no follow up 0x0C or old diagnostic return needed
    session->send(PacketBuilder::showGarage());
    LOG_INFO("GARAGE", "garage opened for char " + std::to_string(session->characterId));
}

void GarageHandler::handleGarageInstall(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)packet; (void)server;
    if (!session) return;
    LOG_INFO("GARAGE", "part install char " + std::to_string(session->characterId));
}

void GarageHandler::handleGarageRemove(Session::Ptr session, Packet& packet, GameServer* server) {
    (void)packet; (void)server;
    if (!session) return;
    LOG_INFO("GARAGE", "part remove char " + std::to_string(session->characterId));
}

} // namespace knc
