#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include "packets/PacketBuilder.h"
#include <vector>

namespace knc {

class GameServer;

class InventoryHandler {
public:
    // requests
    void handleInventoryRequest(Session::Ptr session, GameServer* server);
    void handleEquipVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    void handleEquipAccessory(Session::Ptr session, Packet& packet, GameServer* server);
    void handleUseItem(Session::Ptr session, Packet& packet, GameServer* server);
    void handleSellItem(Session::Ptr session, Packet& packet, GameServer* server);
    
    // send inventory data
    void sendVehicleList(Session::Ptr session);
    void sendItemList(Session::Ptr session);
    void sendAccessoryList(Session::Ptr session);
    void sendFullInventory(Session::Ptr session);
    
    // load from db
    std::vector<VehicleInfo> loadVehicles(int32_t characterId);
    std::vector<ItemInfo> loadItems(int32_t characterId);
    std::vector<AccessoryInfo> loadAccessories(int32_t characterId);
};

} // namespace knc
