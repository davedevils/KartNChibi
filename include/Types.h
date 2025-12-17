// Types.h - common type definitions
#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>

// game states
enum class GameState {
    None = 0,
    Logo = 1,
    Title = 2,
    Login = 3,
    Channel = 4,
    Menu = 5,
    Garage = 6,
    Shop = 7,
    Lobby = 8,
    Room = 9,
    Racing = 11,
};

// connection state (matches original dword_F727F4)
enum class ConnectionState {
    Disconnected = 0,
    Connected = 1,
    Kicked = 2,
};

// packet header (8 bytes)
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t size;      // payload size (not including header)
    uint8_t  cmd;       // command ID
    uint8_t  flag;      // sub-command/variant
    uint8_t  reserved[4];
};
static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");
#pragma pack(pop)

// player info structure (1224 bytes, offset 0x4C8)
#pragma pack(push, 1)
struct PlayerInfo {
    int32_t  characterId;           // +0x000
    uint8_t  accountData[0x482];    // +0x004
    wchar_t  driverName[13];        // +0x486
    uint8_t  flag1;                 // +0x4A0
    uint8_t  flag2;                 // +0x4A1
    uint8_t  padding[2];            // +0x4A2
    int32_t  gold;                  // +0x4A4
    int32_t  astros;                // +0x4A8
    int32_t  cash;                  // +0x4AC
    int32_t  vehicleId;             // +0x4B0 (-1 = none)
    int32_t  driverId;              // +0x4B4 (-1 = none)
    uint8_t  flags[4];              // +0x4B8
    int32_t  unknown1;              // +0x4BC
    int32_t  unknown2;              // +0x4C0
    int32_t  unknown3;              // +0x4C4
};
static_assert(sizeof(PlayerInfo) == 0x4C8, "PlayerInfo must be 0x4C8 bytes");
#pragma pack(pop)

// vehicle data (44 bytes)
#pragma pack(push, 1)
struct VehicleData {
    int32_t vehicleId;
    int32_t uniqueId;
    int32_t durability;
    int32_t maxDurability;
    int32_t statSpeed;
    int32_t statAccel;
    int32_t statHandling;
    int32_t statDrift;
    int32_t statBoost;
    int32_t statWeight;
    int32_t statSpecial;
};
static_assert(sizeof(VehicleData) == 44, "VehicleData must be 44 bytes");
#pragma pack(pop)

// item data (56 bytes)
#pragma pack(push, 1)
struct ItemData {
    int32_t itemId;
    int32_t uniqueId;
    int32_t quantity;
    int32_t slot;
    int32_t equipped;
    int32_t expiration;
    int32_t enhancement;
    int32_t bound;
    int32_t reserved[6];
};
static_assert(sizeof(ItemData) == 56, "ItemData must be 56 bytes");
#pragma pack(pop)

// accessory data (28 bytes)
#pragma pack(push, 1)
struct AccessoryData {
    int32_t accessoryId;
    int32_t uniqueId;
    int32_t slot;
    int32_t bonus1;
    int32_t bonus2;
    int32_t bonus3;
    int32_t equipped;
};
static_assert(sizeof(AccessoryData) == 28, "AccessoryData must be 28 bytes");
#pragma pack(pop)

