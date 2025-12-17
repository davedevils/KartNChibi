/**
 * @file Player.h
 * @brief Player/Character data structures
 */

#pragma once
#include "net/Protocol.h"
#include <string>
#include <vector>
#include <cstdint>

namespace knc {

struct PlayerStats {
    uint32_t wins = 0;
    uint32_t losses = 0;
    uint32_t totalRaces = 0;
    uint32_t experience = 0;
    uint32_t level = 1;
};

struct PlayerCurrency {
    uint32_t gold = 10000;
    uint32_t cash = 0;  // Premium currency
};

struct EquippedItems {
    int32_t vehicleId = -1;
    int32_t accessorySlot[4] = {-1, -1, -1, -1};
};

class Player {
public:
    Player() = default;
    Player(uint32_t id, const std::string& name);
    
    // Identifiers
    uint32_t id() const { return m_id; }
    uint32_t accountId() const { return m_accountId; }
    const std::string& name() const { return m_name; }
    
    // Stats
    PlayerStats& stats() { return m_stats; }
    const PlayerStats& stats() const { return m_stats; }
    
    // Currency
    PlayerCurrency& currency() { return m_currency; }
    const PlayerCurrency& currency() const { return m_currency; }
    
    // Equipment
    EquippedItems& equipped() { return m_equipped; }
    const EquippedItems& equipped() const { return m_equipped; }
    
    // Inventory
    std::vector<VehicleData>& vehicles() { return m_vehicles; }
    std::vector<ItemData>& items() { return m_items; }
    std::vector<AccessoryData>& accessories() { return m_accessories; }
    
    // State
    enum class State { Offline, Menu, Garage, Shop, Lobby, Room, Loading, Racing, Results };
    State state() const { return m_state; }
    void setState(State s) { m_state = s; }
    
    // Room
    int32_t roomId() const { return m_roomId; }
    void setRoomId(int32_t id) { m_roomId = id; }
    uint8_t slot() const { return m_slot; }
    void setSlot(uint8_t s) { m_slot = s; }
    uint8_t team() const { return m_team; }
    void setTeam(uint8_t t) { m_team = t; }
    bool isReady() const { return m_ready; }
    void setReady(bool r) { m_ready = r; }
    
    // Serialization for network
    void serializeToPacket(class Packet& pkt) const;
    void deserializeFromPacket(class Packet& pkt);

private:
    uint32_t m_id = 0;
    uint32_t m_accountId = 0;
    std::string m_name;
    
    PlayerStats m_stats;
    PlayerCurrency m_currency;
    EquippedItems m_equipped;
    
    std::vector<VehicleData> m_vehicles;
    std::vector<ItemData> m_items;
    std::vector<AccessoryData> m_accessories;
    
    State m_state = State::Offline;
    int32_t m_roomId = -1;
    uint8_t m_slot = 0;
    uint8_t m_team = 0;
    bool m_ready = false;
};

} // namespace knc

