/// handles c2s 0x4E delayed ack and sends the post login burst s2c probe

#include "handlers/KeepaliveAnticheatHandler.h"
#include "packets/gen/KeepaliveAnticheatPackets.h"
#include "packets/PacketBuilder.h"
#include "db/Database.h"
#include "logging/Logger.h"

#include <chrono>
#include <cstdint>
#include <unordered_map>

namespace knc {

void KeepaliveAnticheatHandler::handleDelayedAckFire(Session::Ptr session, Packet& packet) {
    // FUN 00485290 sends this as a bare timer echo with no fields either direction
    if (!packet.payload().empty()) {
        LOG_WARN("KEEPALIVE", "C2S 0x4E DelayedAckFire expected zero payload got " +
                              std::to_string(packet.payload().size()));
    }

    if (session->accountId == 0) return;

    // same throttle as GameServer handleHeartbeat this is a liveness signal so the db is not touched every time
    static thread_local std::unordered_map<uint32_t, uint64_t> lastTouch;
    const uint64_t now = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    auto& t = lastTouch[session->id()];
    if (now - t < 60) return;
    t = now;

    Database::instance().executePrepared(
        "UPDATE online_players SET last_seen = NOW() WHERE account_id = ? AND game_session = ?",
        {static_cast<int32_t>(session->accountId), static_cast<int32_t>(session->id())});
}

void KeepaliveAnticheatHandler::armPostLogin(Session::Ptr session) {
    // both are zero byte sends receipt alone is the whole client side trigger
    session->send(KeepaliveAnticheatPackets::clientInfoProbe());
    session->send(PacketBuilder::timestamp());
    LOG_INFO("KEEPALIVE", "post-login probes sent (0x4D client info, 0x4E delayed-ack arm) to " +
                          session->remoteAddress());
}

} // namespace knc
