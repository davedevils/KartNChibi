/// room craft handlers stage 19 open sends catalog owned decor then stage push save must ack or client hangs

#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/RoomCraftPackets.h"

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace knc {

class GameServer;

class RoomCraftHandler {
public:
    /// handles C2S 0x010E pushes missing catalog then decor via sub 452380 then the stage push sub 47D9D0 snapshots after
    void handleOpen(Session::Ptr session, GameServer* server);

    /// handles C2S 0x010F save must answer every path or the client hangs in MSG WAIT
    void handleSave(Session::Ptr session,
                    const std::vector<std::array<uint8_t, RoomCraftPackets::RECORD_SIZE>>& raw,
                    const std::vector<RoomObjectInstance>& recs,
                    GameServer* server);

    /// builds S2C 0x010C catalog per session deduped first burst is bulk out collects frames instead of sending for login burst
    size_t pushObjectCatalog(Session::Ptr session, std::vector<Packet>* out = nullptr);

    /// builds S2C 0x010D burst per session deduped first burst is bulk out collects frames instead of sending for login burst
    size_t pushOwnedInstances(Session::Ptr session, std::vector<Packet>* out = nullptr);

    /// drop the per session catalog and instance dedupe state
    void forgetSession(uint32_t sessionId);

    /// builds S2C 0x0013 room context sub 47FC20 clears decor then appends every record so a resend is safe
    static Packet roomContext(uint32_t roomId, const std::u16string& roomName,
                              int32_t maxPlayers, int32_t mode,
                              int32_t hostCharacterId, int32_t weather);

    /// writes the record count caller must not write it decor falls back to the per room table or bare floor
    static size_t appendRoomDecor(Packet& pkt, int32_t roomId, int32_t hostCharId = 0);

    /// one floor record the room has no collision mesh without it
    static std::vector<RoomObjectInstance> defaultFloorDecor();

    /// decor rows safe for the 0x0013 tail each has activeFlag 1 and an objectKey sub 488300 can resolve
    static std::vector<RoomObjectInstance> safeRoomDecor(int32_t roomId);

private:
    struct SentRoomCraft {
        std::weak_ptr<Session> owner;
        uint32_t characterId = 0;
        std::unordered_set<uint32_t> objectKeys;
        std::unordered_set<uint32_t> instanceIds;
    };

    void pruneClosedSessions();

    std::mutex m_sentMutex;
    std::unordered_map<uint32_t, SentRoomCraft> m_sent;
};

} // namespace knc
