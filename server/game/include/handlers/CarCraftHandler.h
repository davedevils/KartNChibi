/// car craft stage 18 sends S2C 0x0107 0x0108 0x0109 0x010A 0x010B
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/gen/CustomCarPackets.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace knc {

class GameServer;

/// sub 430EF0 orders screens sub 44F9F0 appends part defs once sub 490A70 copies the custom car block
class CarCraftHandler {
public:
    /// C2S 0x010A is zero bytes and pushes 0x0107 and 0x0109 then the ack
    void handleOpen(Session::Ptr session, GameServer* server);

    /// C2S 0x010B client waits on MSG WAIT until 0x010B answers
    void handleSave(Session::Ptr session, const CarSaveRequest& req, GameServer* server);

    /// C2S 0x0114 preset rename answers with 0x0114 then 0x0124 to refresh that row
    void handleRename(Session::Ptr session, const CarRenameRequest& req, GameServer* server);

    /// login step 4 push sends 0x0108 defs then 0x0107 and one 0x0109 per owned part
    void sendLoginSnapshot(Session::Ptr session);

    /// after a chassis grant the basic set 0x0109 rows then the full 0x0107 so the garage lists it
    void pushFactoryLoadout(Session::Ptr session);

    /// drops per session 0x0108 and 0x0109 dedupe state call on disconnect
    void forgetSession(uint32_t sessionId);

    /// resolved custom car 0x3C block for 0x0021 and 0x003E tails sub 490A70 zero unless factory car
    static std::array<uint8_t, 0x3C> customCarFor(int32_t characterId,
                                                  uint32_t kartInstanceId);

    /// drop every cached block of one character after a preset write
    static void invalidateCustomCar(int32_t characterId);

    /// S2C 0x010B is 0x30 plus 0x84 times n plus 8 header frame must stay under 0x2000
    static constexpr std::size_t MAX_PARTS_IN_SAVE_RESULT = 60;

private:
    struct CarCraftView {
        bool ok = false;                        ///< false means do not open the stage
        std::vector<CarPreset> presets;
        std::vector<CarPartInstance> parts;     ///< only parts with a 0x0108 def
        std::unordered_set<uint32_t> ownedKarts;
        std::vector<uint32_t> factoryKarts;     ///< owned karts the chassis tab lists scheme 1 only
    };

    CarCraftView buildView(int32_t characterId);

    /// 0x0108 catalog burst sent once per session as a blind append on the client
    void sendPartDefs(Session::Ptr session);

    /// 0x0109 for instance ids this session has not received yet
    void sendPartInstances(Session::Ptr session, const std::vector<CarPartInstance>& parts);

    void pruneClosedSessions();

    struct SentParts {
        std::weak_ptr<Session> owner;
        uint32_t characterId = 0;
        bool defsSent = false;
        std::unordered_set<uint32_t> instanceIds;
    };

    std::mutex m_sentMutex;
    std::unordered_map<uint32_t, SentParts> m_sent;
};

} // namespace knc
