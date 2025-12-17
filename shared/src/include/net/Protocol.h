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
    constexpr uint8_t S_SHOW_MENU           = 0x11;  // Blue menu (UI=5)
    constexpr uint8_t S_SHOW_LOBBY          = 0x12;  // Lobby trigger (UI=8) - NO PAYLOAD!
    constexpr uint8_t S_HEARTBEAT_RESP      = 0x12;  // Heartbeat (same CMD, different context)
    // NOTE: 0x12 reads NO data, just sets UI state to 8
    constexpr uint8_t S_PLAYER_ROOM_DATA    = 0x13;  // Room data + players (UI=9)
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
    // NOTE: 0x24 is nullsub (ignored)
    constexpr uint8_t S_ROOM_STRING         = 0x25;
    constexpr uint8_t S_ROOM_INFO_ALT       = 0x27;
    constexpr uint8_t S_ENTITY_DATA         = 0x28;  // 104 bytes
    constexpr uint8_t S_WHISPER_ROOM        = 0x2A;  // Same as whisper enable
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

    // Game/Race (0x14, 0x31-0x5F)
    constexpr uint8_t S_GAME_MODE_14        = 0x14;  // Race/Game mode setup
    constexpr uint8_t S_GAME_18             = 0x18;  // Unknown game packet
    constexpr uint8_t S_POSITION            = 0x31;
    constexpr uint8_t S_POSITION_32         = 0x32;  // 8 bytes
    constexpr uint8_t S_GAME_STATE          = 0x33;
    constexpr uint8_t S_FLAG_34             = 0x34;  // 0 bytes
    constexpr uint8_t S_SCORE               = 0x35;
    // NOTE: 0x36-0x38 are NULLSUB in client (ignored)!
    // Do NOT use these for sending data - client won't process them
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
    constexpr uint8_t S_ROOM_DATA_5C        = 0x5C;  // 12 bytes (3×int32)
    constexpr uint8_t S_GAME_5F             = 0x5F;  // 8 bytes (2×int32) - FIXED!

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
    // NOTE: 0x8E has NO HANDLER in client! Do NOT use!
    constexpr uint8_t S_NO_HANDLER_8E       = 0x8E;  // ⚠️ IGNORED BY CLIENT!
    constexpr uint8_t S_UI_STATE_24         = 0x8F;  // 0 bytes -> UI State 24
    constexpr uint8_t S_UI_STATE_25         = 0x90;  // 8 bytes (2 int32) -> UI State 25
    constexpr uint8_t S_LIST_D4             = 0x95;
    constexpr uint8_t S_GIFT                = 0x98;  // 220 bytes
    constexpr uint8_t S_SINGLE_D4           = 0x99;
    constexpr uint8_t S_ITEM_SWITCH         = 0x9A;
    constexpr uint8_t S_REMOVE_ITEM_9B      = 0x9B;
    constexpr uint8_t S_ADD_VEHICLE         = 0x9D;  // 44 bytes
    constexpr uint8_t S_ADD_ITEM            = 0x9E;  // 56 bytes
    constexpr uint8_t S_ADD_ACCESSORY       = 0x9F;  // 28 bytes
    // NOTE: 0xA0 = nullsub_1 (ignored by client!)
    constexpr uint8_t S_MISSION_COMPLETE    = 0xA1;  // 4 bytes
    constexpr uint8_t S_MISSION_LIST        = 0xA2;  // count + N×12 bytes
    constexpr uint8_t S_REWARD_CLAIM        = 0xA3;  // Complex conditional format
    constexpr uint8_t S_SESSION_CONFIRM     = 0xA7;
    constexpr uint8_t S_PLAYER_PREVIEW      = 0xAA;
    constexpr uint8_t S_SYSTEM_MESSAGE      = 0xB4;
    constexpr uint8_t S_PLAYER_COMPARISON   = 0xB5;  // 2456+ bytes!
    constexpr uint8_t S_DISPLAY_TEXT        = 0xB6;
    constexpr uint8_t S_REMOVE_ITEM         = 0xB8;  // 8 bytes
    
    // Inventory Updates (0xB7-0xBA) - IDA verified
    constexpr uint8_t S_INVENTORY_UPDATE    = 0xB7;  // 183: type + int32 + int32 + Vehicle/Item/Accessory data
    constexpr uint8_t S_INVENTORY_REMOVE    = 0xB8;  // 184: type (4) + uniqueId (4) -> removes from inventory
    constexpr uint8_t S_INVENTORY_SLOT      = 0xB9;  // 185: complex slot/stats update
    constexpr uint8_t S_INVENTORY_OP        = 0xBA;  // 186: complex inventory operation
    
    // System Messages (0xCE) - IDA verified
    constexpr uint8_t S_GAME_MESSAGE        = 0xCE;  // 206: MSG_LEVEL_UP, MSG_COME_IN_FIRST, etc
    
    // Player Update (0xCF, 0xD9)
    constexpr uint8_t S_PLAYER_UPDATE_CF    = 0xCF;  // 207: 4×int32 player stats update
    constexpr uint8_t S_PLAYER_FULL_UPDATE  = 0xD9;  // 217: int32 + VehicleData + ItemData + 60b
    
    // ========================================================================
    // RACE PACKETS (190-198) - IDA VERIFIED
    // ========================================================================
    constexpr uint8_t S_RACE_INIT           = 0xBE;  // 190: Race initialization (no payload, clears all)
    constexpr uint8_t S_RACE_PLAYER_1       = 0xBF;  // 191: 5×int32 + wstring + 0x14 + wstring + wstring + count×0x10
    constexpr uint8_t S_RACE_PLAYER_2       = 0xC0;  // 192: Complex race player data (~164+ bytes)
    constexpr uint8_t S_RACE_PLAYER_3       = 0xC1;  // 193: Race player data variant
    constexpr uint8_t S_RACE_PLAYER_4       = 0xC2;  // 194: Race player data variant
    constexpr uint8_t S_RACE_PLAYER_5       = 0xC3;  // 195: Race player data variant
    constexpr uint8_t S_RACE_DATA           = 0xC4;  // 196: 4+4+wstring+wstring
    constexpr uint8_t S_RACE_PLAYER_6       = 0xC5;  // 197: Race player data variant
    constexpr uint8_t S_RACE_DATA_2         = 0xC6;  // 198: Race data
    
    // Extended Equipment (130-140) - IDA VERIFIED
    constexpr uint8_t S_EXT_DATA_130        = 0x82;  // 130: 0x18C bytes (396 bytes!) sub_47B710
    constexpr uint8_t S_EXT_DATA_131        = 0x83;  // 131: 0x18C bytes (396 bytes!) sub_47B7D0
    constexpr uint8_t S_EXT_DATA_132        = 0x84;  // 132: Extended data sub_47B8D0
    constexpr uint8_t S_EXT_DATA_133        = 0x85;  // 133: Extended data sub_47B900
    constexpr uint8_t S_EXT_DATA_136        = 0x88;  // 136: Extended data sub_47B930
    constexpr uint8_t S_EXT_DATA_138        = 0x8A;  // 138: Extended data sub_47B990
    constexpr uint8_t S_EXT_DATA_140        = 0x8C;  // 140: Equip item complex sub_47B9E0
    
    // Game Mode Setup (0x14) - IDA VERIFIED sub_479CC0
    // Format: int32 mode, if mode==3||8: 5×int32, then sets UI state 11
    constexpr uint8_t S_GAME_MODE_SETUP     = 0x14;  // 20: Race/game mode setup (4-24 bytes)
    
    // ========================================================================
    // RACE ENTITY PACKETS (237-309) - IDA VERIFIED - NEED FLAG > 0
    // ========================================================================
    constexpr uint16_t S_ENTITY_UPDATE      = 0xED;  // 237: Entity update (0x14 + 0x1C + type switch)
    constexpr uint16_t S_ENTITY_POSITION    = 0xEE;  // 238: int32 + 0x10 bytes
    constexpr uint16_t S_ENTITY_REMOVE      = 0xF0;  // 240: Entity remove
    constexpr uint16_t S_ENTITY_CLEAR       = 0xF1;  // 241: Entity clear
    constexpr uint16_t S_ENTITY_DATA_243    = 0xF3;  // 243: Entity data
    constexpr uint16_t S_ENTITY_DATA_244    = 0xF4;  // 244: Entity data
    constexpr uint16_t S_ENTITY_DATA_245    = 0xF5;  // 245: Entity data
    constexpr uint16_t S_ENTITY_DATA_246    = 0xF6;  // 246: Entity data
    constexpr uint16_t S_ENTITY_DATA_247    = 0xF7;  // 247: Entity data
    constexpr uint16_t S_ENTITY_DATA_248    = 0xF8;  // 248: Entity data
    constexpr uint16_t S_ENTITY_DATA_249    = 0xF9;  // 249: Entity data
    constexpr uint16_t S_ENTITY_DATA_251    = 0xFB;  // 251: Entity data
    constexpr uint16_t S_ENTITY_DATA_252    = 0xFC;  // 252: Entity data
    constexpr uint16_t S_ENTITY_DATA_254    = 0xFE;  // 254: Entity data
    constexpr uint16_t S_ENTITY_DATA_256    = 0x100; // 256: Entity data (flag=1!)
    constexpr uint16_t S_ENTITY_DATA_257    = 0x101; // 257: Entity data
    constexpr uint16_t S_ENTITY_DATA_258    = 0x102; // 258: Entity data
    constexpr uint16_t S_ENTITY_DATA_259    = 0x103; // 259: Entity data
    constexpr uint16_t S_ENTITY_DATA_260    = 0x104; // 260: Entity data
    constexpr uint16_t S_ENTITY_DATA_263    = 0x107; // 263: Entity data
    constexpr uint16_t S_ENTITY_DATA_264    = 0x108; // 264: Entity data
    constexpr uint16_t S_ENTITY_DATA_265    = 0x109; // 265: Entity data
    constexpr uint16_t S_ENTITY_DATA_266    = 0x10A; // 266: Entity data
    constexpr uint16_t S_ENTITY_DATA_267    = 0x10B; // 267: Entity data
    constexpr uint16_t S_ENTITY_DATA_268    = 0x10C; // 268: Entity data
    constexpr uint16_t S_ENTITY_DATA_269    = 0x10D; // 269: Entity data
    constexpr uint16_t S_ENTITY_DATA_270    = 0x10E; // 270: Entity data
    constexpr uint16_t S_ENTITY_DATA_271    = 0x10F; // 271: Entity data
    constexpr uint16_t S_ENTITY_DATA_274    = 0x112; // 274: Entity special
    constexpr uint16_t S_ENTITY_DATA_276    = 0x114; // 276: Entity data
    constexpr uint16_t S_ENTITY_DATA_277    = 0x115; // 277: Entity data
    constexpr uint16_t S_ENTITY_DATA_278    = 0x116; // 278: Entity data (wstring)
    constexpr uint16_t S_ENTITY_DATA_279    = 0x117; // 279: Entity data
    constexpr uint16_t S_ENTITY_DATA_280    = 0x118; // 280: Entity data
    constexpr uint16_t S_ENTITY_DATA_281    = 0x119; // 281: 4+4+wstring+wstring+wstring
    constexpr uint16_t S_ENTITY_DATA_282    = 0x11A; // 282: 8 bytes
    constexpr uint16_t S_ENTITY_DATA_283    = 0x11B; // 283: 8 bytes (same handler as 282)
    constexpr uint16_t S_ENTITY_DATA_284    = 0x11C; // 284: Entity data
    constexpr uint16_t S_ENTITY_DATA_285    = 0x11D; // 285: Entity data
    constexpr uint16_t S_ENTITY_DATA_286    = 0x11E; // 286: Entity data (wstring)
    constexpr uint16_t S_ENTITY_DATA_287    = 0x11F; // 287: Entity data
    constexpr uint16_t S_ENTITY_DATA_288    = 0x120; // 288: Entity data
    constexpr uint16_t S_ENTITY_DATA_290    = 0x122; // 290: Entity data
    constexpr uint16_t S_ENTITY_DATA_291    = 0x123; // 291: Entity data
    constexpr uint16_t S_ENTITY_DATA_292    = 0x124; // 292: Entity data
    constexpr uint16_t S_ENTITY_DATA_293    = 0x125; // 293: Entity data
    constexpr uint16_t S_ENTITY_DATA_294    = 0x126; // 294: Entity data (wstring)
    constexpr uint16_t S_ENTITY_DATA_302    = 0x12E; // 302: Entity data
    constexpr uint16_t S_ENTITY_DATA_303    = 0x12F; // 303: Entity data (wstring)
    constexpr uint16_t S_ENTITY_DATA_305    = 0x131; // 305: Entity data
    constexpr uint16_t S_ENTITY_DATA_306    = 0x132; // 306: Entity data
    constexpr uint16_t S_ENTITY_DATA_309    = 0x135; // 309: Entity data

    // ========================================================================
    // CLIENT -> SERVER COMMANDS
    // ========================================================================

    constexpr uint8_t C_CLIENT_AUTH         = 0x07;  // Login request / PlayerInfo
    constexpr uint8_t C_CHANNEL_SELECT      = 0x18;  // Channel selection (after redirect)
    constexpr uint8_t C_SERVER_QUERY        = 0x19;  // Server query
    constexpr uint8_t C_LEAVE_ROOM          = 0x22;  // Leave room request
    constexpr uint8_t C_PLAYER_READY        = 0x23;  // Ready state in room
    constexpr uint8_t C_STATE_CHANGE        = 0x2C;  // State change
    constexpr uint8_t C_CHAT_MESSAGE        = 0x2D;  // Chat message UTF-16LE
    constexpr uint8_t C_WHISPER             = 0x2F;  // Private message
    constexpr uint8_t C_ROOM_STATE_REQ      = 0x30;  // Room state request
    constexpr uint8_t C_POSITION            = 0x31;  // Position update
    constexpr uint8_t C_UNKNOWN_32          = 0x32;
    constexpr uint8_t C_LAP_COMPLETE        = 0x36;  // Lap completed
    constexpr uint8_t C_ITEM_PICKUP         = 0x37;  // Item box pickup
    constexpr uint8_t C_ITEM_HIT            = 0x38;  // Item hit
    constexpr uint8_t C_RACE_FINISH         = 0x39;  // Race finish
    constexpr uint8_t C_JOIN_ROOM           = 0x3F;  // Join room request
    constexpr uint8_t C_GAME_START          = 0x40;  // Start game request
    constexpr uint8_t C_ITEM_USE            = 0x45;  // Use item
    constexpr uint8_t C_REQUEST_DATA        = 0x4D;  // Request data (276 bytes)
    constexpr uint8_t C_CREATE_ROOM         = 0x63;  // Create room request
    constexpr uint8_t C_SHOP_BROWSE         = 0x68;  // Browse shop category
    constexpr uint8_t C_SELL_ITEM           = 0x6B;  // Sell item
    constexpr uint8_t C_PURCHASE            = 0x6E;  // Purchase item
    constexpr uint8_t C_SHOP_ENTER          = 0x70;  // Enter shop
    constexpr uint8_t C_SHOP_EXIT           = 0x71;  // Exit shop
    constexpr uint8_t C_DISCONNECT          = 0x73;  // Disconnect (0 bytes)
    constexpr uint8_t C_EQUIP_VEHICLE       = 0x8C;  // Equip vehicle
    constexpr uint8_t C_EQUIP_ACCESSORY     = 0x8D;  // Equip accessory
    constexpr uint8_t C_USE_ITEM            = 0x9A;  // Use consumable item
    constexpr uint8_t C_ACK                 = 0x8E;  // Client ACK
    constexpr uint8_t C_HEARTBEAT           = 0xA6;  // Heartbeat (4 bytes, every 1000ms)
    constexpr uint8_t C_LOBBY_CHAT          = 0xB4;  // Lobby chat message
    constexpr uint8_t C_CLIENT_INFO         = 0xD0;  // Client info
    constexpr uint8_t C_FULL_STATE          = 0xFA;  // Full state sync
    constexpr uint8_t C_LAUNCHER_LOGIN      = 0xFE;  // Launcher auth request
    constexpr uint8_t S_LAUNCHER_RESPONSE   = 0xFE;  // Launcher auth response
    
    // Tutorial/License
    constexpr uint8_t C_START_TUTORIAL      = 0xA9;  // Start tutorial request
    constexpr uint8_t C_TUTORIAL_COMPLETE   = 0xAA;  // Tutorial complete result
    constexpr uint8_t C_LICENSE_TEST        = 0xAB;  // License test request
    constexpr uint8_t C_LICENSE_RESULT      = 0xAC;  // License test result
    
    // Mission/Quest
    constexpr uint8_t C_MISSION_LIST        = 0xA1;  // Get mission list
    constexpr uint8_t C_MISSION_DETAILS     = 0xA2;  // Get mission details
    constexpr uint8_t C_CLAIM_REWARD        = 0xA3;  // Claim mission reward
    
    // Garage
    constexpr uint8_t C_OPEN_GARAGE         = 0x0F;  // Open garage
    constexpr uint8_t C_GARAGE_VEHICLES     = 0x1B;  // Get vehicle list
    constexpr uint8_t C_GARAGE_ITEMS        = 0x1C;  // Get item list
    constexpr uint8_t C_GARAGE_ACCESSORIES  = 0x1D;  // Get accessory list
    constexpr uint8_t C_UPGRADE_VEHICLE     = 0x9C;  // Upgrade vehicle stat
    constexpr uint8_t C_REPAIR_VEHICLE      = 0x9D;  // Repair vehicle
    constexpr uint8_t C_DELETE_ITEM         = 0x9E;  // Delete item
    
    // Quick Match
    constexpr uint8_t C_QUICK_MATCH         = 0x64;  // Quick match request
    
    // Friends
    constexpr uint8_t C_ADD_FRIEND          = 0xB0;  // Add friend
    constexpr uint8_t C_REMOVE_FRIEND       = 0xB1;  // Remove friend
    constexpr uint8_t C_BLOCK_PLAYER        = 0xB2;  // Block player
    constexpr uint8_t C_PLAYER_PROFILE      = 0xB3;  // Get player profile
    
    // Drift / Mini Turbo
    constexpr uint8_t C_DRIFT_START         = 0xBC;  // Start drifting
    constexpr uint8_t C_DRIFT_END           = 0xBD;  // End drift (with boost level)
    constexpr uint8_t C_BOOST_ACTIVATE      = 0xBE;  // Activate mini turbo boost
    constexpr uint8_t C_BOOST_END           = 0xBF;  // Boost ended
    
    // Ghost Mode
    constexpr uint8_t C_GHOST_MENU          = 0xC0;  // Open ghost menu
    constexpr uint8_t C_GHOST_SELECT_MAP    = 0xC1;  // Select map for ghost race
    constexpr uint8_t C_GHOST_START         = 0xC2;  // Start ghost race
    constexpr uint8_t C_GHOST_COMPLETE      = 0xC3;  // Ghost race complete
    constexpr uint8_t C_GHOST_SAVE          = 0xC4;  // Save ghost replay
    constexpr uint8_t C_GHOST_LIST          = 0xC5;  // Get ghost list for map
    constexpr uint8_t C_GHOST_DOWNLOAD      = 0xC6;  // Download ghost replay
    
    // Scenario Mode
    constexpr uint8_t C_SCENARIO_MENU       = 0xC7;  // Open scenario menu (state 22)
    constexpr uint8_t C_SCENARIO_CHAPTER    = 0xC8;  // Select chapter
    constexpr uint8_t C_SCENARIO_STAGE      = 0xC9;  // Select stage
    constexpr uint8_t C_SCENARIO_START      = 0xCA;  // Start scenario race
    constexpr uint8_t C_SCENARIO_COMPLETE   = 0xCB;  // Scenario complete
    constexpr uint8_t C_SCENARIO_PROGRESS   = 0xCC;  // Get progress
    constexpr uint8_t C_SCENARIO_CHAPTERS   = 0xCD;  // Get chapter list
    
    // Internal server-to-server
    constexpr uint8_t I_SERVER_REGISTER     = 0xF0;  // GameServer registration to LoginServer

} // namespace CMD

// ============================================================================
// DATA STRUCTURES (from docs/packets/STRUCTURES.md)
// ============================================================================

// ============================================================================
// DATA STRUCTURES - CERTIFIED from docs/packets/STRUCTURES.md (IDA+Ghidra)
// ============================================================================

#pragma pack(push, 1)

/**
 * VehicleData (0x2C = 44 bytes) - CERTIFIED
 * Used by: CMD 0x1B, 0x3E, 0x78, 0x9D
 */
struct VehicleData {
    int32_t vehicleId;        // +0x00 - Vehicle template ID
    int32_t uniqueId;         // +0x04 - Unique instance ID (DB key)
    int32_t durability;       // +0x08 - Current durability (0-100)
    int32_t maxDurability;    // +0x0C - Maximum durability
    int32_t statSpeed;        // +0x10 - Speed stat
    int32_t statAccel;        // +0x14 - Acceleration stat
    int32_t statHandling;     // +0x18 - Handling stat
    int32_t statDrift;        // +0x1C - Drift stat
    int32_t statBoost;        // +0x20 - Boost stat
    int32_t statWeight;       // +0x24 - Weight stat
    int32_t statSpecial;      // +0x28 - Special ability stat
};
static_assert(sizeof(VehicleData) == 44, "VehicleData must be 0x2C bytes");

/**
 * ItemData (0x38 = 56 bytes) - CERTIFIED
 * Used by: CMD 0x1C, 0x3E, 0x9E
 */
struct ItemData {
    int32_t itemId;           // +0x00 - Item template ID
    int32_t uniqueId;         // +0x04 - Unique instance ID (DB key)
    int32_t quantity;         // +0x08 - Stack count
    int32_t slot;             // +0x0C - Inventory slot position
    int32_t equipped;         // +0x10 - 0=inventory, 1=equipped
    int32_t expiration;       // +0x14 - Unix timestamp (0=permanent)
    int32_t enhancement;      // +0x18 - Enhancement level
    int32_t bound;            // +0x1C - 0=tradeable, 1=bound
    int32_t reserved[6];      // +0x20 - Reserved (24 bytes)
};
static_assert(sizeof(ItemData) == 56, "ItemData must be 0x38 bytes");

/**
 * AccessoryData (0x1C = 28 bytes) - CERTIFIED
 * Used by: CMD 0x1D, 0x1E, 0x9F
 */
struct AccessoryData {
    int32_t accessoryId;      // +0x00 - Accessory template ID
    int32_t uniqueId;         // +0x04 - Unique instance ID
    int32_t slot;             // +0x08 - Equipment slot (0-3)
    int32_t bonus1;           // +0x0C - Bonus stat 1 value
    int32_t bonus2;           // +0x10 - Bonus stat 2 value
    int32_t bonus3;           // +0x14 - Bonus stat 3 value
    int32_t equipped;         // +0x18 - 0=inventory, 1=equipped
};
static_assert(sizeof(AccessoryData) == 28, "AccessoryData must be 0x1C bytes");

/**
 * SmallItem (0x20 = 32 bytes) - CERTIFIED
 * Used by: CMD 0x79, 0x7A
 */
struct SmallItem {
    int32_t itemId;           // +0x00 - Item template ID
    int32_t uniqueId;         // +0x04 - Unique instance ID
    int32_t quantity;         // +0x08 - Stack count
    int32_t slot;             // +0x0C - Slot position
    int32_t flags;            // +0x10 - Item flags/status
    int32_t reserved[3];      // +0x14 - Reserved (12 bytes)
};
static_assert(sizeof(SmallItem) == 32, "SmallItem must be 0x20 bytes");

/**
 * PlayerInfo (0x4C8 = 1224 bytes) - CERTIFIED
 * Used by: CMD 0x07, 0xA7
 * Full structure at: docs/packets/auth/PLAYER_INFO.md
 */
struct PlayerInfo {
    // Account Data Zone (0x000-0x485, ~1158 bytes)
    int32_t  characterId;           // +0x000 - Character ID (first field)
    uint8_t  accountData[0x482];    // +0x004 - Stats, inventory refs, settings
    
    // Driver Zone (0x486-0x49F, 26 bytes)
    char16_t driverName[13];        // +0x486 - Player name (12 chars + null, UTF-16LE)
    
    // Flags Zone (0x4A0-0x4A3, 4 bytes)
    uint8_t  flag1;                 // +0x4A0 - Status flag 1
    uint8_t  flag2;                 // +0x4A1 - Status flag 2
    uint8_t  padding[2];            // +0x4A2 - Alignment padding
    
    // Currency Zone (0x4A4-0x4AF, 12 bytes)
    int32_t  gold;                  // +0x4A4 - In-game currency (Lucci)
    int32_t  astros;                // +0x4A8 - Premium currency (Astros)
    int32_t  cash;                  // +0x4AC - Experience/secondary currency
    
    // Equipment Zone (0x4B0-0x4B7, 8 bytes)
    int32_t  vehicleId;             // +0x4B0 - Equipped vehicle ID (-1 = none)
    int32_t  driverId;              // +0x4B4 - Equipped driver ID (-1 = none)
    
    // Flags Zone 2 (0x4B8-0x4BB, 4 bytes)
    uint8_t  flags[4];              // +0x4B8 - Additional flags
    
    // Trailing Zone (0x4BC-0x4C7, 12 bytes)
    int32_t  unknown1;              // +0x4BC
    int32_t  unknown2;              // +0x4C0
    int32_t  unknown3;              // +0x4C4
};
static_assert(sizeof(PlayerInfo) == 1224, "PlayerInfo must be 0x4C8 bytes");

/**
 * PlayerJoin (246 bytes) - CERTIFIED
 * Used by: CMD 0x3E
 */
struct PlayerJoinData {
    int32_t      playerId;          // +0x00 - Character ID (4 bytes)
    char16_t     playerName[35];    // +0x04 - Player name (70 bytes, null-padded)
    int32_t      level;             // +0x4A - Player level
    int32_t      team;              // +0x4E - Team ID (0=none, 1=red, 2=blue)
    VehicleData  vehicle;           // +0x52 - Equipped vehicle (44 bytes)
    ItemData     item;              // +0x7E - Equipped item (56 bytes)
    int32_t      slot;              // +0xB6 - Room slot position (0-7)
    uint8_t      extraData[60];     // +0xBA - Race state data (0x3C bytes)
};
static_assert(sizeof(PlayerJoinData) == 246, "PlayerJoinData must be 246 bytes");

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

