/**
 * @file GarageHandler.h
 * @brief Handles garage/inventory management packets
 */
#pragma once
#include "net/Session.h"
#include "net/Packet.h"
#include <memory>
#include <vector>

namespace knc {

class GameServer;

// Garage categories (from client)
enum class GarageCategory {
    Character = 0,  // Drivers
    Car = 1,        // Vehicles
    Item = 2,       // Items/Consumables
    Etc = 3         // Accessories and other
};

struct GarageVehicle {
    int32_t id;
    int32_t templateId;
    int32_t durability;
    int32_t maxDurability;
    int32_t stats[7];  // speed, accel, handling, drift, boost, weight, special
    bool isEquipped;
};

struct GarageItem {
    int32_t id;
    int32_t templateId;
    int32_t quantity;
    int32_t slot;
    bool isEquipped;
    int32_t expiresAt;  // Timestamp, 0 = permanent
};

class GarageHandler {
public:
    // Garage requests
    static void handleOpenGarage(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleGetVehicleList(Session::Ptr session, GameServer* server);
    static void handleGetItemList(Session::Ptr session, GameServer* server);
    static void handleGetAccessoryList(Session::Ptr session, GameServer* server);
    static void handleGetDriverList(Session::Ptr session, GameServer* server);
    
    // Equipment actions
    static void handleEquipVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleEquipItem(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleEquipAccessory(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleEquipDriver(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Modification
    static void handleUpgradeVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleRepairVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleDeleteItem(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Car Factory (vehicle customization)
    static void handleEnterCarFactory(Session::Ptr session, GameServer* server);
    static void handleCustomizeVehicle(Session::Ptr session, Packet& packet, GameServer* server);
    static void handleSaveCustomization(Session::Ptr session, Packet& packet, GameServer* server);
    
    // Helpers
    static void sendVehicleList(Session::Ptr session, int characterId);
    static void sendItemList(Session::Ptr session, int characterId);
    static void sendAccessoryList(Session::Ptr session, int characterId);
    static void sendDriverList(Session::Ptr session, int characterId);
    
    static std::vector<GarageVehicle> getPlayerVehicles(int characterId);
    static std::vector<GarageItem> getPlayerItems(int characterId);
};

} // namespace knc

