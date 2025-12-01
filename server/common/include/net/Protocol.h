/**
 * @file Protocol.h
 * @brief KnC Protocol constants - ALL packet CMDs and structures
 * 
 * Based on reverse engineering from docs/packets/
 * Header format: [Size:2][CMD:1][Flag:1][Reserved:4][Payload:N]
 */

#pragma once
#include <cstdint>

namespace knc {

// ============================================================================
// PACKET HEADER
// ============================================================================

constexpr size_t PACKET_HEADER_SIZE = 8;
constexpr size_t PACKET_MAX_SIZE = 65535;

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t size;      // Payload size (little-endian)
    uint8_t  cmd;       // Command/opcode
    uint8_t  flag;      // Sub-command / variant
    uint32_t reserved;  // Usually 0
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");

// ============================================================================
// SERVER -> CLIENT COMMANDS
// ============================================================================

namespace CMD {
    // Auth (0x01-0x0D)
    constexpr uint8_t S_LOGIN_RESPONSE      = 0x01;  // Login result
    constexpr uint8_t S_DISPLAY_MESSAGE     = 0x02;  // wstring + int32
    constexpr uint8_t S_TRIGGER             = 0x03;  // 0 bytes, state trigger
    constexpr uint8_t S_REGISTRATION_RESP   = 0x04;  // Registration result
    constexpr uint8_t S_CONNECTION_OK       = 0x0A;  // 38 bytes, first packet
    constexpr uint8_t S_ACK                 = 0x0B;  // 0 bytes
    constexpr uint8_t S_DATA_PAIRS          = 0x0C;  // 4+8N bytes
    constexpr uint8_t S_FLAG_SET            = 0x0D;  // 0 bytes

    // UI States (0x0E-0x16)
    constexpr uint8_t S_CHANNEL_LIST        = 0x0E;  // Channel/server list
    constexpr uint8_t S_SHOW_GARAGE         = 0x0F;
    constexpr uint8_t S_SHOW_SHOP           = 0x10;
    constexpr uint8_t S_SHOW_MENU           = 0x11;  // Blue menu
    constexpr uint8_t S_SHOW_LOBBY          = 0x12;  // Main lobby
    constexpr uint8_t S_PLAYER_ROOM_DATA    = 0x13;
    constexpr uint8_t S_UI_STATE_14         = 0x16;

    // Inventory (0x1B-0x1E, 0x76-0x7D)
    constexpr uint8_t S_INVENTORY_VEHICLES  = 0x1B;  // 4+44N bytes
    constexpr uint8_t S_INVENTORY_ITEMS     = 0x1C;  // 4+56N bytes
    constexpr uint8_t S_INVENTORY_ACCESSORY = 0x1D;  // 4+28N bytes
    constexpr uint8_t S_ITEM_UPDATE         = 0x1E;  // 28 bytes
    constexpr uint8_t S_INVENTORY_LIST      = 0x76;
    constexpr uint8_t S_UPDATE_LIST         = 0x77;
    constexpr uint8_t S_ITEM_LIST           = 0x78;
    constexpr uint8_t S_ITEM_LIST_B         = 0x79;
    constexpr uint8_t S_ITEM_ADD            = 0x7A;
    constexpr uint8_t S_INV_SLOT            = 0x7B;
    constexpr uint8_t S_NOTIFICATION        = 0x7D;

    // Room (0x21-0x30, 0x3D-0x3F, 0x62-0x65)
    constexpr uint8_t S_ROOM_FULL           = 0x21;
    constexpr uint8_t S_LEAVE_ROOM          = 0x22;
    constexpr uint8_t S_PLAYER_UPDATE       = 0x23;
    constexpr uint8_t S_ROOM_STRING         = 0x25;
    constexpr uint8_t S_ROOM_INFO_ALT       = 0x27;
    constexpr uint8_t S_ENTITY_DATA         = 0x28;  // 104 bytes
    constexpr uint8_t S_ROOM_STATE          = 0x30;
    constexpr uint8_t S_ROOM_STATE_3D       = 0x3D;  // 8 bytes
    constexpr uint8_t S_PLAYER_JOIN         = 0x3E;  // ~180 bytes
    constexpr uint8_t S_ROOM_INFO           = 0x3F;
    constexpr uint8_t S_TUTORIAL_FAIL       = 0x62;
    constexpr uint8_t S_CREATE_ROOM         = 0x63;
    constexpr uint8_t S_ROOM_STATUS         = 0x64;
    constexpr uint8_t S_SPEED_UPDATE        = 0x65;

    // Chat (0x2A-0x2E)
    constexpr uint8_t S_WHISPER_ENABLE      = 0x2A;  // 0 bytes
    constexpr uint8_t S_WHISPER_DISABLE     = 0x2B;  // 0 bytes
    constexpr uint8_t S_CHAT_MESSAGE        = 0x2D;  // ~116 bytes
    constexpr uint8_t S_PLAYER_LEFT         = 0x2E;

    // Game/Race (0x31-0x5F)
    constexpr uint8_t S_POSITION            = 0x31;
    constexpr uint8_t S_POSITION_32         = 0x32;  // 8 bytes
    constexpr uint8_t S_GAME_STATE          = 0x33;
    constexpr uint8_t S_FLAG_34             = 0x34;  // 0 bytes
    constexpr uint8_t S_SCORE               = 0x35;
    constexpr uint8_t S_FINISH              = 0x39;
    constexpr uint8_t S_RESULTS             = 0x3A;
    constexpr uint8_t S_COUNTDOWN           = 0x3B;
    constexpr uint8_t S_RACE_END            = 0x3C;
    constexpr uint8_t S_GAME_STATE_40       = 0x40;
    constexpr uint8_t S_GAME_MODE           = 0x42;  // 4 bytes
    constexpr uint8_t S_GAME_UPDATE         = 0x44;  // 4 bytes
    constexpr uint8_t S_ITEM_USAGE          = 0x45;  // 12 bytes
    constexpr uint8_t S_LARGE_GAME_STATE    = 0x46;  // ~1160 bytes!
    constexpr uint8_t S_PLAYER_ACTION       = 0x47;  // 24 bytes
    constexpr uint8_t S_PLAYER_DATA         = 0x49;  // 12 bytes
    constexpr uint8_t S_GAME_DATA_4B        = 0x4B;  // 16 bytes
    constexpr uint8_t S_TIMESTAMP           = 0x4E;  // 0 bytes
    constexpr uint8_t S_SERVER_REDIRECT     = 0x54;  // ~264 bytes CRITICAL
    constexpr uint8_t S_RACE_STATUS         = 0x57;  // 12 bytes
    constexpr uint8_t S_PLAYER_STATUS       = 0x58;  // 5 bytes
    constexpr uint8_t S_ROOM_DATA_5C        = 0x5C;  // 12 bytes
    constexpr uint8_t S_GAME_5F             = 0x5F;  // 4 bytes

    // Shop (0x68-0x74)
    constexpr uint8_t S_SHOP_LOOKUP         = 0x68;
    constexpr uint8_t S_SHOP_ITEM           = 0x69;  // 7 bytes
    constexpr uint8_t S_SHOP_UPDATE         = 0x6A;  // 6 bytes
    constexpr uint8_t S_SHOP_CHAT           = 0x6C;
    constexpr uint8_t S_SHOP_CALL           = 0x6E;  // 0 bytes
    constexpr uint8_t S_SHOP_RESPONSE       = 0x6F;
    constexpr uint8_t S_SHOP_EVENT          = 0x70;  // 8 bytes
    constexpr uint8_t S_DATA_BLOCK          = 0x72;  // 104 bytes
    constexpr uint8_t S_SLOT_UPDATE         = 0x73;

    // Extended (0x81-0xBA)
    constexpr uint8_t S_EXT_81              = 0x81;
    constexpr uint8_t S_EXT_82              = 0x82;
    constexpr uint8_t S_EXT_87              = 0x87;  // 188 bytes
    constexpr uint8_t S_EXT_88              = 0x88;
    constexpr uint8_t S_EQUIP_ITEM          = 0x8C;
    constexpr uint8_t S_INIT_RESPONSE       = 0x8E;
    constexpr uint8_t S_ACK_UI24            = 0x8F;  // 0 bytes
    constexpr uint8_t S_INIT_UI25           = 0x90;  // 8 bytes
    constexpr uint8_t S_LIST_D4             = 0x95;
    constexpr uint8_t S_GIFT                = 0x98;  // 220 bytes
    constexpr uint8_t S_SINGLE_D4           = 0x99;
    constexpr uint8_t S_ITEM_SWITCH         = 0x9A;
    constexpr uint8_t S_REMOVE_ITEM_9B      = 0x9B;
    constexpr uint8_t S_ADD_VEHICLE         = 0x9D;  // 44 bytes
    constexpr uint8_t S_ADD_ITEM            = 0x9E;  // 56 bytes
    constexpr uint8_t S_ADD_ACCESSORY       = 0x9F;  // 28 bytes
    constexpr uint8_t S_SESSION_CONFIRM     = 0xA7;
    constexpr uint8_t S_PLAYER_PREVIEW      = 0xAA;
    constexpr uint8_t S_SYSTEM_MESSAGE      = 0xB4;
    constexpr uint8_t S_PLAYER_COMPARISON   = 0xB5;  // 2456+ bytes!
    constexpr uint8_t S_DISPLAY_TEXT        = 0xB6;
    constexpr uint8_t S_REMOVE_ITEM         = 0xB8;  // 8 bytes

    // ========================================================================
    // CLIENT -> SERVER COMMANDS
    // ========================================================================

    constexpr uint8_t C_CLIENT_AUTH         = 0x07;  // PlayerInfo 1224 bytes
    constexpr uint8_t C_SERVER_QUERY        = 0x19;
    constexpr uint8_t C_STATE_CHANGE        = 0x2C;
    constexpr uint8_t C_CHAT_MESSAGE        = 0x2D;  // UTF-16LE
    constexpr uint8_t C_WHISPER             = 0x2F;
    constexpr uint8_t C_UNKNOWN_32          = 0x32;
    constexpr uint8_t C_REQUEST_DATA        = 0x4D;  // 276 bytes
    constexpr uint8_t C_PURCHASE            = 0x6E;
    constexpr uint8_t C_SHOP_ENTER          = 0x70;
    constexpr uint8_t C_SHOP_EXIT           = 0x71;
    constexpr uint8_t C_DISCONNECT          = 0x73;  // 0 bytes
    constexpr uint8_t C_HEARTBEAT           = 0xA6;  // 4 bytes, every 1000ms
    constexpr uint8_t C_CLIENT_INFO         = 0xD0;
    constexpr uint8_t C_FULL_STATE          = 0xFA;
    constexpr uint8_t C_LAUNCHER_LOGIN      = 0xFE;  // Launcher auth request
    constexpr uint8_t S_LAUNCHER_RESPONSE   = 0xFE;  // Launcher auth response

} // namespace CMD

// ============================================================================
// DATA STRUCTURES (from docs/packets/STRUCTURES.md)
// ============================================================================

#pragma pack(push, 1)

struct VehicleData {
    int32_t vehicleId;
    int32_t ownerId;
    int32_t durability;
    int32_t maxDurability;
    int32_t stats[7];
};
static_assert(sizeof(VehicleData) == 44, "VehicleData must be 0x2C bytes");

struct ItemData {
    int32_t itemId;
    int32_t ownerId;
    int32_t quantity;
    int32_t unknown[11];
};
static_assert(sizeof(ItemData) == 56, "ItemData must be 0x38 bytes");

struct AccessoryData {
    int32_t accessoryId;
    int32_t uniqueId;
    int32_t slot;
    int32_t bonus1;
    int32_t bonus2;
    int32_t bonus3;
    int32_t equipped;
};
static_assert(sizeof(AccessoryData) == 28, "AccessoryData must be 0x1C bytes");

struct SmallItem {
    int32_t itemId;
    int32_t uniqueId;
    int32_t quantity;
    int32_t unknown[5];
};
static_assert(sizeof(SmallItem) == 32, "SmallItem must be 0x20 bytes");

#pragma pack(pop)

// ============================================================================
// RATE LIMITS (ms between packets)
// ============================================================================

namespace RateLimit {
    constexpr int HEARTBEAT_MIN_INTERVAL = 500;
    constexpr int CHAT_MIN_INTERVAL = 200;
    constexpr int DEFAULT_MIN_INTERVAL = 50;
    constexpr int GLOBAL_MAX_PACKETS_SEC = 100;
}

// ============================================================================
// GAME CONSTANTS
// ============================================================================

namespace GameConst {
    constexpr float MAX_SPEED = 500.0f;
    constexpr float MAX_POSITION_DELTA = 100.0f;
    constexpr int MAX_PLAYERS_PER_ROOM = 8;
    constexpr int MAX_ROOMS = 100;
    constexpr int TICK_RATE = 20;
}

} // namespace knc

