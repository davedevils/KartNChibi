#pragma once
#include "packets/gen/InventoryPackets.h"
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/PacketBuilder.h"
#include <vector>

namespace knc {

class GameServer;

class InventoryHandler {
public:
    void handleInventoryRequest(Session::Ptr session, GameServer* server);
    void handleEquipVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    void handleEquipAccessory(Session::Ptr session, Packet& packet, GameServer* server);
    void handleUseItem(Session::Ptr session, Packet& packet, GameServer* server);
    void handleSellItem(Session::Ptr session, Packet& packet, GameServer* server);

    void sendVehicleList(Session::Ptr session);
    void sendItemList(Session::Ptr session);
    void sendAccessoryList(Session::Ptr session);
    void sendFullInventory(Session::Ptr session);
    
    // lobby stand and room member blobs build from the client persisted kart selection
    static bool selectedKartRow(int32_t characterId, InventoryPackets::KartRow& out);
    static bool selectedCharacterRow(int32_t characterId, InventoryPackets::CharacterRow& out);

    // the 0x1C full replace list of one character sent at login and again after a race wore a kart
    static Packet ownedKartListPacket(int32_t characterId);

    /// lost in the 21 aug checkout rewritten from call sites takes the character id only for a db worker
    void buildLoginBurst(int32_t characterId, std::vector<Packet>& out);
    bool handleDelete(Session::Ptr session, Packet& packet, GameServer* server);
    bool handleInstall(Session::Ptr session, Packet& packet, GameServer* server);
    bool handleRemove(Session::Ptr session, Packet& packet, GameServer* server);
    // 0xBC resend since the stand only repaints once it sees the new pair
    void sendEquipmentSet(Session::Ptr session);
    bool isItemStateNotify(const Packet& packet) const;
    void handleItemStateNotify(Session::Ptr session, Packet& packet, GameServer* server);

    std::vector<ItemInfo> loadItems(int32_t characterId);
    std::vector<AccessoryInfo> loadAccessories(int32_t characterId);
};

} // namespace knc
