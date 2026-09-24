/// the two ability reports of the race wire the second item model that lived here was never routed

#include "handlers/ItemHandler.h"
#include "packets/gen/ItemPackets.h"
#include "game/Room.h"
#include "logging/Logger.h"

#include <atomic>
#include <string>

namespace knc {

namespace {

// opaque per hit correlation id for s2c 0xCD client only stores and echoes it a monotonic counter is wire safe
std::atomic<uint32_t> g_hitTokenSeq{1};

std::string ids(uint32_t v) { return std::to_string(v); }
std::string ids(int32_t v) { return std::to_string(v); }

} // namespace

void ItemHandler::handleAbilityFire(Session::Ptr session, Packet& packet, Room* room) {
    if (!session || !room) return;

    int32_t payload = 0;
    if (!ItemPackets::parseAbilityFire(packet, payload)) return;

    LOG_DEBUG("ITEM", "ability blocked a hit for racer " + ids(session->characterId) +
                      " payload " + ids(payload));
}

void ItemHandler::handleAbilityClass(Session::Ptr session, Packet& packet, Room* room) {
    if (!session || !room) return;

    int32_t abilityClass = 0;
    if (!ItemPackets::parseAbilityClass(packet, abilityClass)) return;

    if (abilityClass < 8 || abilityClass > 11) {
        // sub 4B82A0 only reaches sub 4833A0 for classes 8 9 10 and 11
        LOG_WARN("ITEM", "ability class " + ids(abilityClass) + " has no sender in the client");
        return;
    }

    const uint32_t pid = session->characterId;
    const bool shieldBroke = abilityClass == 10 || abilityClass == 11;

    if (shieldBroke) {
        // sub 4CACE0 fires 10 11 on real shield hit client already ran effect broadcast tells rest of room
        const uint32_t token = g_hitTokenSeq.fetch_add(1, std::memory_order_relaxed);
        room->broadcastExcept(ItemPackets::shieldAbsorbToken(pid, token), session->id());
        LOG_DEBUG("ITEM", "shield of racer " + ids(pid) + " popped");
    } else {
        LOG_DEBUG("ITEM", "racer " + ids(pid) + " took a hit with ability class " +
                          ids(abilityClass));
    }
}

} // namespace knc
