/// packet CMD constants and wire structs

#pragma once
#include <cstdint>
#include <cstddef>

namespace knc {

constexpr size_t PACKET_HEADER_SIZE = 8;
constexpr size_t PACKET_MAX_SIZE = 65535;
// the biggest stock C2S frame is the 136 frame ghost chunk near 3812 bytes
constexpr size_t PACKET_MAX_C2S_PAYLOAD = 0x2000;

// header has opcode at offset 2 cmd and flag are legacy aliases for its low and high byte
#pragma pack(push, 1)
struct PacketHeader {
    uint16_t size;
    union {
        uint16_t opcode;       // preferred full uint16 opcode low endian
        struct {
            uint8_t cmd;
            uint8_t flag;      // legacy alias high byte of opcode nonzero only for opcode 256 and above
        };
    };
    uint32_t reserved;         // bytes 4 7 client alignment pad always zero
};
#pragma pack(pop)
static_assert(sizeof(PacketHeader) == 8, "PacketHeader must be 8 bytes");

// server to client commands
namespace CMD {
    constexpr uint8_t S_LOGIN_RESPONSE      = 0x01;  // Login result
    constexpr uint8_t S_DISPLAY_MESSAGE     = 0x02;
    // 0x03 opens nickname registration screen via sub 479220 calling sub 473730
    constexpr uint8_t S_SET_GAME_VAR        = 0x03;
    constexpr uint8_t S_TRIGGER             = 0x03;
    // 0x04 via sub 479230 layout int32 result wstring nickname VehicleData 44 bytes ItemData 56 bytes
    constexpr uint8_t S_REGISTER_NICK_RES   = 0x04;
    constexpr uint8_t S_REGISTRATION_RESP   = 0x04;
    // 0x05 0x06 0x09 share sub 478AB0 stub handler 0x08 is nullsub client ignores it
    constexpr uint8_t S_NO_HANDLER_08       = 0x08;
    constexpr uint8_t S_CONNECTION_OK       = 0x0A;  // 38 bytes first packet
    constexpr uint8_t S_ACK                 = 0x0B;
    constexpr uint8_t S_DATA_PAIRS          = 0x0C;  // 4 8N bytes
    constexpr uint8_t S_FLAG_SET            = 0x0D;

    constexpr uint8_t S_CHANNEL_LIST        = 0x0E;  // Channel server list
    constexpr uint8_t S_SHOW_GARAGE         = 0x0F;
    constexpr uint8_t S_SHOW_SHOP           = 0x10;
    constexpr uint8_t S_SHOW_MENU           = 0x11;
    constexpr uint8_t S_SHOW_LOBBY          = 0x12;
    constexpr uint8_t S_HEARTBEAT_RESP      = 0x12;
    constexpr uint8_t S_PLAYER_ROOM_DATA    = 0x13;
    constexpr uint8_t S_UI_STATE_14         = 0x16;

    constexpr uint8_t S_INVENTORY_VEHICLES  = 0x1B;  // 4 44N bytes
    constexpr uint8_t S_INVENTORY_ITEMS     = 0x1C;
    constexpr uint8_t S_INVENTORY_ACCESSORY = 0x1D;  // 4 28N bytes
    constexpr uint8_t S_ITEM_UPDATE         = 0x1E;
    constexpr uint8_t S_INVENTORY_LIST      = 0x76;
    constexpr uint8_t S_UPDATE_LIST         = 0x77;
    constexpr uint8_t S_ITEM_LIST           = 0x78;
    constexpr uint8_t S_ITEM_LIST_B         = 0x79;
    constexpr uint8_t S_ITEM_ADD            = 0x7A;
    constexpr uint8_t S_INV_SLOT            = 0x7B;
    constexpr uint8_t S_NOTIFICATION        = 0x7D;

    // 0x21 is the room member commit via sub 40D650 not a room full error
    constexpr uint8_t S_ROOM_MEMBER         = 0x21;
    constexpr uint8_t S_ROOM_FULL           = 0x21;  // legacy wrong name kept for old call sites
    constexpr uint8_t S_LEAVE_ROOM          = 0x22;
    constexpr uint8_t S_PLAYER_UPDATE       = 0x23;
    // NOTE 0x24 is nullsub ignored
    constexpr uint8_t S_ROOM_STRING         = 0x25;
    constexpr uint8_t S_ROOM_INFO_ALT       = 0x27;
    constexpr uint8_t S_ENTITY_DATA         = 0x28;  // 104 bytes
    constexpr uint8_t S_WHISPER_ROOM        = 0x2A;
    constexpr uint8_t S_ROOM_STATE          = 0x30;
    constexpr uint8_t S_ROOM_STATE_3D       = 0x3D;
    constexpr uint8_t S_PLAYER_JOIN         = 0x3E;
    constexpr uint8_t S_ROOM_INFO           = 0x3F;
    constexpr uint8_t S_TUTORIAL_FAIL       = 0x62;
    // 0x63 is the server reply to the create room request 0x2D with payload int32 roomId
    constexpr uint8_t S_CREATE_ROOM_ACK     = 0x63;
    constexpr uint8_t S_CREATE_ROOM         = 0x63;  // legacy alias
    constexpr uint8_t S_ROOM_STATUS         = 0x64;
    constexpr uint8_t S_SPEED_UPDATE        = 0x65;

    constexpr uint8_t S_WHISPER_ENABLE      = 0x2A;  // 0 bytes
    constexpr uint8_t S_WHISPER_DISABLE     = 0x2B;
    // 0x2D server to client room announce broadcast via sub 479630 not chat repurposed GOA opcode
    constexpr uint8_t S_ROOM_ANNOUNCE       = 0x2D;
    constexpr uint8_t S_CHAT_MESSAGE        = 0x2D;  // WRONG never send chat here
    constexpr uint8_t S_PLAYER_LEFT         = 0x2E;

    // lobby room grid built by sub 408020 drawn by sub 407EA0 channel forced 0 by sub 408EB0
    constexpr uint8_t S_LOBBY_ROOM_ADD      = 0x2D;
    constexpr uint8_t S_LOBBY_ROOM_REMOVE   = 0x2E;  // handled via sub 479710 to sub 4084B0
    constexpr uint8_t S_LOBBY_ROOM_STATE    = 0x23;

    constexpr uint8_t S_GAME_MODE_14        = 0x14;  // Race Game mode setup
    constexpr uint8_t S_GAME_18             = 0x18;
    constexpr uint8_t S_POSITION            = 0x31;
    // 0x32 slot enable via sub 479C70 to sub 409F90 not a position update
    constexpr uint8_t S_ROOM_SLOT_ENABLED   = 0x32;
    constexpr uint8_t S_POSITION_32         = 0x32;  // legacy wrong name kept for old call sites
    constexpr uint8_t S_GAME_STATE          = 0x33;
    constexpr uint8_t S_FLAG_34             = 0x34;  // 0 bytes
    constexpr uint8_t S_SCORE               = 0x35;
    // 0x36 and 0x38 are nullsub in client ignored do not use for sending data
    constexpr uint8_t S_FINISH              = 0x39;
    constexpr uint8_t S_RESULTS             = 0x3A;
    constexpr uint8_t S_COUNTDOWN           = 0x3B;
    constexpr uint8_t S_RACE_END            = 0x3C;
    constexpr uint8_t S_GAME_STATE_40       = 0x40;
    constexpr uint8_t S_GAME_MODE           = 0x42;  // 4 bytes
    constexpr uint8_t S_GAME_UPDATE         = 0x44;
    constexpr uint8_t S_ITEM_USAGE          = 0x45;  // 12 bytes
    constexpr uint8_t S_LARGE_GAME_STATE    = 0x46;
    constexpr uint8_t S_PLAYER_ACTION       = 0x47;  // 24 bytes
    constexpr uint8_t S_PLAYER_DATA         = 0x49;
    constexpr uint8_t S_GAME_DATA_4B        = 0x4B;  // 16 bytes
    constexpr uint8_t S_TIMESTAMP           = 0x4E;
    constexpr uint8_t S_SERVER_REDIRECT     = 0x54;  // 264 bytes CRITICAL
    constexpr uint8_t S_RACE_STATUS         = 0x57;
    constexpr uint8_t S_PLAYER_STATUS       = 0x58;  // 5 bytes
    constexpr uint8_t S_ROOM_DATA_5C        = 0x5C;
    constexpr uint8_t S_GAME_5F             = 0x5F;  // 8 bytes 2 int32 FIXED

    constexpr uint8_t S_SHOP_LOOKUP         = 0x68;
    constexpr uint8_t S_SHOP_ITEM           = 0x69;  // 7 bytes
    constexpr uint8_t S_SHOP_UPDATE         = 0x6A;
    // not shop chat sub 47B030 reads fields into one global struct this is the invite popup
    constexpr uint8_t S_INVITE_POPUP        = 0x6C;
    constexpr uint8_t S_SHOP_CALL           = 0x6E;  // 0 bytes
    constexpr uint8_t S_SHOP_RESPONSE       = 0x6F;
    constexpr uint8_t S_SHOP_EVENT          = 0x70;  // 8 bytes
    constexpr uint8_t S_DATA_BLOCK          = 0x72;
    constexpr uint8_t S_SLOT_UPDATE         = 0x73;

    constexpr uint8_t S_EXT_81              = 0x81;
    constexpr uint8_t S_EXT_82              = 0x82;
    constexpr uint8_t S_EXT_87              = 0x87;  // 188 bytes
    constexpr uint8_t S_EXT_88              = 0x88;
    constexpr uint8_t S_EQUIP_ITEM          = 0x8C;
    // NOTE 0x8E has NO HANDLER in client Do NOT use
    constexpr uint8_t S_NO_HANDLER_8E       = 0x8E;
    constexpr uint8_t S_UI_STATE_24         = 0x8F;  // 0 bytes UI State 24
    constexpr uint8_t S_UI_STATE_25         = 0x90;
    // gift inbox 0x96 0x9C verified 0x95 is a legacy alias real containers are 0x96 0x97
    constexpr uint8_t S_LIST_D4             = 0x95;
    constexpr uint8_t S_GIFT_INBOX_RECV     = 0x96;  // int32 count N 0xD4
    constexpr uint8_t S_GIFT_INBOX_SENT     = 0x97;
    constexpr uint8_t S_GIFT                = 0x98;  // gift send confirm 220 bytes
    constexpr uint8_t S_SINGLE_D4           = 0x99;
    constexpr uint8_t S_GIFT_REWARD_GRANT   = 0x9A;
    // 0x99 appends one 212 byte record via sub 47BE80 no count no clear
    constexpr uint8_t S_GIFT_ARRIVED        = 0x99;
    constexpr uint8_t S_GIFT_DELETE         = 0x9B;  // delete by id
    constexpr uint8_t S_GIFT_MARK_READ      = 0x9C;
    constexpr uint8_t S_ITEM_SWITCH         = 0x9A;
    constexpr uint8_t S_REMOVE_ITEM_9B      = 0x9B;
    constexpr uint8_t S_ADD_VEHICLE         = 0x9D;  // 44 bytes
    constexpr uint8_t S_ADD_ITEM            = 0x9E;
    constexpr uint8_t S_ADD_ACCESSORY       = 0x9F;
    // 0xA0 is nullsub ignored by client
    constexpr uint8_t S_MISSION_COMPLETE    = 0xA1;
    constexpr uint8_t S_MISSION_LIST        = 0xA2;  // count N 12 bytes
    constexpr uint8_t S_REWARD_CLAIM        = 0xA3;
    constexpr uint8_t S_SESSION_CONFIRM     = 0xA7;
    // 0xAA historically player preview but sub 47CBA0 calls sub 404410 for a gacha roll popup
    constexpr uint8_t S_PLAYER_PREVIEW      = 0xAA;
    constexpr uint8_t S_GACHA_ROLL_RESULT   = 0xAA;
    constexpr uint8_t S_GACHA_BANNER_HEADER = 0xAB;  // handled by sub 47C9C0
    constexpr uint8_t S_GACHA_BANNER_ENTRY  = 0xAC;
    // 0xB1 is not sub 47CC20 that is the ghost family it has no client handler payload is dropped
    constexpr uint8_t S_GACHA_ROLL_PAYOUT   = 0xB1;
    // 0xB0 submits a ghost body u32 track then time in ms then zero proven live twice
    constexpr uint8_t S_GHOST_RESULT        = 0xB0;
    // friend block list 0xCA to 0xFC the real handler sub 47CD20 is opcode 0xC9
    constexpr uint8_t S_FRIEND_STATE_UPDATE = 0xC9;
    constexpr uint8_t S_FRIEND_ONLINE_FLIP  = 0xCD;  // handled by sub 47D1C0
    constexpr uint8_t S_FRIEND_STATS_UPDATE = 0xCF;
    constexpr uint8_t S_FRIEND_BLOB         = 0xEE;  // handled by sub 47D880 0x10 bytes

    constexpr uint8_t S_FRIEND_REMOVE       = 0xF0;  // handled by sub 47D930

    constexpr uint8_t S_FRIEND_RECORD       = 0xF1;  // handled by sub 47D970 0x24 bytes

    constexpr uint8_t S_BLOCK_LIST_REFRESH  = 0xF4;  // handled by sub 47DB00 int32 count then 8 bytes
    constexpr uint8_t S_BUDDY_LIST_ENTRY    = 0xFB;
    constexpr uint8_t S_BUDDY_STATUS_LIST   = 0xFC;  // handled by sub 47DBB0 int32 count then 0xC bytes
    constexpr uint8_t S_SYSTEM_MESSAGE      = 0xB4;
    constexpr uint8_t S_PLAYER_COMPARISON   = 0xB5;  // 2456 bytes
    constexpr uint8_t S_DISPLAY_TEXT        = 0xB6;
    constexpr uint8_t S_REMOVE_ITEM         = 0xB8;  // 8 bytes
    
    // Inventory Updates 0xB7 0xBA IDA verified
    constexpr uint8_t S_INVENTORY_UPDATE    = 0xB7;
    constexpr uint8_t S_INVENTORY_REMOVE    = 0xB8;  // 184 type 4 uniqueId 4 removes from inventory
    constexpr uint8_t S_INVENTORY_SLOT      = 0xB9;
    constexpr uint8_t S_INVENTORY_OP        = 0xBA;  // 186 complex inventory operation
    
    // System Messages 0xCE IDA verified
    constexpr uint8_t S_GAME_MESSAGE        = 0xCE;
    
    constexpr uint8_t S_PLAYER_UPDATE_CF    = 0xCF;  // 207 4 int32 player stats update
    constexpr uint8_t S_PLAYER_FULL_UPDATE  = 0xD9;
    
    // race packets 190 to 198 verified
    constexpr uint8_t S_RACE_INIT           = 0xBE;
    // 0xBF and 0xC0 handlers via sub 47F4F0 and sub 44F510 were swapped leaving no driver or kart
    constexpr uint8_t S_DRIVER_CATALOG      = 0xBF;
    constexpr uint8_t S_KART_CATALOG        = 0xC0;
    constexpr uint8_t S_RACE_PLAYER_1       = 0xBF;  // legacy alias
    constexpr uint8_t S_RACE_PLAYER_2       = 0xC0;
    // 0xC1 is the item catalog kart part keys resolve against it via sub 4510C0
    constexpr uint8_t S_ITEM_CATALOG        = 0xC1;
    constexpr uint8_t S_PART_CATALOG        = 0xC2;
    constexpr uint8_t S_RACE_PLAYER_3       = 0xC1;  // legacy alias
    constexpr uint8_t S_RACE_PLAYER_4       = 0xC2;
    constexpr uint8_t S_RACE_PLAYER_5       = 0xC3;  // 195 Race player data variant
    constexpr uint8_t S_RACE_DATA           = 0xC4;
    constexpr uint8_t S_RACE_PLAYER_6       = 0xC5;  // 197 Race player data variant

    constexpr uint8_t S_PRICE_ROW           = 0xC6;
    constexpr uint8_t S_RACE_DATA_2         = 0xC6;  // legacy misname kept for compat
    

    constexpr uint8_t S_EXT_DATA_130        = 0x82;  // 0x82 payload 396 bytes handled by sub 47B710
    constexpr uint8_t S_EXT_DATA_131        = 0x83;
    constexpr uint8_t S_EXT_DATA_132        = 0x84;  // 0x84 handled by sub 47B8D0
    constexpr uint8_t S_EXT_DATA_133        = 0x85;
    constexpr uint8_t S_EXT_DATA_136        = 0x88;  // 0x88 handled by sub 47B930
    constexpr uint8_t S_EXT_DATA_138        = 0x8A;
    constexpr uint8_t S_EXT_DATA_140        = 0x8C;  // 0x8C equip item complex handled by sub 47B9E0
    
    // 0x14 game mode setup via sub 479CC0 format int32 mode then optional int32
    constexpr uint8_t S_GAME_MODE_SETUP     = 0x14;
    
    // race entity packets 237 to 309 verified need flag 0
    constexpr uint16_t S_ENTITY_UPDATE      = 0xED;
    constexpr uint16_t S_ENTITY_POSITION    = 0xEE;  // 238 int32 0x10 bytes
    constexpr uint16_t S_ENTITY_REMOVE      = 0xF0;
    constexpr uint16_t S_ENTITY_CLEAR       = 0xF1;  // 241 Entity clear
    constexpr uint16_t S_ENTITY_DATA_243    = 0xF3;
    constexpr uint16_t S_ENTITY_DATA_244    = 0xF4;  // 244 Entity data
    constexpr uint16_t S_ENTITY_DATA_245    = 0xF5;
    constexpr uint16_t S_ENTITY_DATA_246    = 0xF6;  // 246 Entity data
    constexpr uint16_t S_ENTITY_DATA_247    = 0xF7;
    constexpr uint16_t S_ENTITY_DATA_248    = 0xF8;  // 248 Entity data
    constexpr uint16_t S_ENTITY_DATA_249    = 0xF9;
    constexpr uint16_t S_ENTITY_DATA_251    = 0xFB;  // 251 Entity data
    constexpr uint16_t S_ENTITY_DATA_252    = 0xFC;
    constexpr uint16_t S_ENTITY_DATA_254    = 0xFE;  // 254 Entity data
    constexpr uint16_t S_ENTITY_DATA_256    = 0x100;
    constexpr uint16_t S_ENTITY_DATA_257    = 0x101;  // 257 Entity data
    constexpr uint16_t S_ENTITY_DATA_258    = 0x102;
    constexpr uint16_t S_ENTITY_DATA_259    = 0x103;  // 259 Entity data
    constexpr uint16_t S_ENTITY_DATA_260    = 0x104;
    constexpr uint16_t S_ENTITY_DATA_263    = 0x107;  // 263 Entity data
    constexpr uint16_t S_ENTITY_DATA_264    = 0x108;
    constexpr uint16_t S_ENTITY_DATA_265    = 0x109;  // 265 Entity data
    constexpr uint16_t S_ENTITY_DATA_266    = 0x10A;
    constexpr uint16_t S_ENTITY_DATA_267    = 0x10B;  // 267 Entity data
    constexpr uint16_t S_ENTITY_DATA_268    = 0x10C;
    constexpr uint16_t S_ENTITY_DATA_269    = 0x10D;  // 269 Entity data

    // real names read off live payloads prefer these over the generic entity data labels
    constexpr uint8_t  S_GIFT_INBOX_LIST    = 0x95;
    constexpr uint8_t  S_LICENSE_CATALOG    = 0xC3;   // License 01 entries
    constexpr uint8_t  S_LICENSE_RANK       = 0xC5;
    constexpr uint16_t S_PET_CATALOG        = 0x103;  // Pet 01 and PET 10 TITLE PET 10 INFO entries
    constexpr uint16_t S_FACTORY_CATALOG    = 0x107;
    constexpr uint16_t S_SKY_CATALOG        = 0x10C;  // SKY01 and SKY 01 TITLE entries
    constexpr uint16_t S_ROOM_CRAFT_APPLY   = 0x10F;
    constexpr uint8_t  S_PLAYER_STATS       = 0x0A;   // u8 then LEVEL then gold then exp
    constexpr uint8_t  C_DECLARE_APPEARANCE = 0xD9;
    constexpr uint8_t  S_OWN_CHARACTER      = 0x1B;   // login count then key id and part slots
    constexpr uint8_t  S_OWN_KART           = 0x1C;
    constexpr uint16_t S_ACK_10E            = 0x10E;  // empty ack to C2S 0x10E
    constexpr uint16_t S_ENTITY_DATA_270    = 0x10E;
    constexpr uint16_t S_ENTITY_DATA_271    = 0x10F;  // 271 Entity data
    constexpr uint16_t S_ENTITY_DATA_274    = 0x112;
    constexpr uint16_t S_ENTITY_DATA_276    = 0x114;  // 276 Entity data
    constexpr uint16_t S_ENTITY_DATA_277    = 0x115;

    constexpr uint16_t S_PLAYER_NOTICE      = 0x116;  // handled by sub 47E6D0
    constexpr uint16_t S_ENTITY_DATA_279    = 0x117;
    constexpr uint16_t S_ENTITY_DATA_280    = 0x118;  // 280 Entity data
    constexpr uint16_t S_ENTITY_DATA_281    = 0x119;
    constexpr uint16_t S_ENTITY_DATA_282    = 0x11A;  // 282 8 bytes
    constexpr uint16_t S_ENTITY_DATA_283    = 0x11B;
    constexpr uint16_t S_ENTITY_DATA_284    = 0x11C;  // 284 Entity data
    constexpr uint16_t S_ENTITY_DATA_285    = 0x11D;
    constexpr uint16_t S_ENTITY_DATA_286    = 0x11E;  // 286 Entity data wstring
    constexpr uint16_t S_ENTITY_DATA_287    = 0x11F;
    constexpr uint16_t S_ENTITY_DATA_288    = 0x120;  // 288 Entity data
    constexpr uint16_t S_ENTITY_DATA_290    = 0x122;
    constexpr uint16_t S_ENTITY_DATA_291    = 0x123;  // 291 Entity data
    constexpr uint16_t S_ENTITY_DATA_292    = 0x124;
    constexpr uint16_t S_ENTITY_DATA_293    = 0x125;  // 293 Entity data
    constexpr uint16_t S_ENTITY_DATA_294    = 0x126;
    constexpr uint16_t S_ENTITY_DATA_302    = 0x12E;  // 302 Entity data

    constexpr uint16_t S_INVITE_POPUP_SHORT = 0x12F;  // handled by sub 47ED90
    constexpr uint16_t S_ENTITY_DATA_305    = 0x131;
    constexpr uint16_t S_USERLIST_PAGE      = 0x132;
    constexpr uint16_t S_ENTITY_DATA_306    = 0x132;  // legacy misname
    constexpr uint16_t S_ENTITY_DATA_309    = 0x135;


    constexpr uint8_t C_CLIENT_AUTH         = 0x07;  // Login request PlayerInfo
    constexpr uint8_t C_CHANNEL_SELECT      = 0x18;
    constexpr uint8_t C_SERVER_QUERY        = 0x19;  // Server query
    constexpr uint8_t C_LEAVE_ROOM          = 0x22;
    // sent empty when the client returns to lobby from the waiting room cueing a room grid resend
    constexpr uint8_t C_LOBBY_ENTER         = 0x12;
    constexpr uint8_t C_PLAYER_READY        = 0x23;  // Ready state in room
    constexpr uint8_t C_STATE_CHANGE        = 0x2C;
    // 0x2D client to server create room request via sub 480CC0 reuses the GOA chat opcode
    constexpr uint8_t C_CREATE_ROOM_REQ     = 0x2D;
    constexpr uint8_t C_CHAT_MESSAGE        = 0x2D;
    // 0x2F is room join captured live body is roomId then u16 zero
    constexpr uint8_t C_ROOM_JOIN_REAL      = 0x2F;
    constexpr uint8_t C_WHISPER             = 0xB5;  // corrected was 0x2F
    constexpr uint8_t C_ROOM_STATE_REQ      = 0x30;
    // dead client never sends it server to client only via sub 479950 see minimap handler
    constexpr uint8_t C_POSITION            = 0x31;
    constexpr uint8_t C_UNKNOWN_32          = 0x32;
    // no client ever sends this our headless does laps stay checkpoint derived server side
    constexpr uint8_t C_LAP_COMPLETE        = 0x36;
    constexpr uint8_t C_ITEM_PICKUP         = 0x37;  // Item box pickup
    constexpr uint8_t C_ITEM_HIT            = 0x38;
    constexpr uint8_t C_RACE_FINISH         = 0x39;  // Race finish
    constexpr uint8_t C_JOIN_ROOM           = 0x3F;
    constexpr uint8_t C_GAME_START          = 0x40;
    // in race motion via sub 4818A0 shares value with grid direction client to server only
    constexpr uint8_t C_MOTION              = 0x40;
    // 0x45 is the standings row not an item use item use is 0x47
    constexpr uint8_t C_ITEM_USE            = 0x47;
    constexpr uint8_t C_REQUEST_DATA        = 0x4D;
    // 0x63 server only ack 0x68 via sub 47AE30 not shop 0x6E via sub 481E20 not buy
    constexpr uint8_t C_SHOP_ENTER          = 0x10;
    constexpr uint8_t C_SELL_ITEM           = 0x6B;  // sell item

    constexpr uint8_t C_BUY_ITEM            = 0x70;  // item tab5 sub 4820F0 int32 id
    constexpr uint8_t C_BUY_ITEM_ALT        = 0x71;
    constexpr uint8_t C_BUY_KART            = 0x72;  // kart tab1 sub 4822D0 wstring name
    constexpr uint8_t C_KART_ACT            = 0x74;
    constexpr uint8_t C_KART_ALT            = 0x7A;  // kart tab1 sub 481FB0 wstring name alternate sender
    constexpr uint8_t C_BUY_PREMIUM         = 0x7B;
    // gift purchase from shop via sub 482880 payload cat name id result msg
    constexpr uint8_t C_SHOP_GIFT           = 0x98;

    constexpr uint8_t C_SHOP_POLL           = 0x73;  // shop poll 0 bytes reply S 0x73

    constexpr uint8_t C_USE_ITEM            = 0x9A;
    constexpr uint8_t C_HEARTBEAT           = 0xA6;  // Heartbeat 4 bytes every 1000ms
    constexpr uint8_t C_LOBBY_CHAT          = 0xB4;
    constexpr uint8_t C_CLIENT_INFO         = 0xD0;  // Client info
    constexpr uint8_t C_FULL_STATE          = 0xFA;
    constexpr uint8_t C_LAUNCHER_LOGIN      = 0xFE;  // Launcher auth request
    constexpr uint8_t S_LAUNCHER_RESPONSE   = 0xFE;
    
    constexpr uint8_t C_START_TUTORIAL      = 0xA9;  // Start tutorial request
    constexpr uint8_t C_TUTORIAL_COMPLETE   = 0xAA;
    constexpr uint8_t C_LICENSE_TEST        = 0xAB;  // License test request
    constexpr uint8_t C_LICENSE_RESULT      = 0xAC;
    
    // mission finish report carries i32 missionId
    constexpr uint8_t C_MISSION_COMPLETE    = 0x8C;
    // not mission start real start is 0x90 via sub 4835C0 proven by S2C 0x90 via sub 47DF30
    constexpr uint8_t C_LICENSE_PANEL_CLOSE = 0x8D;
    constexpr uint8_t C_MISSION_START       = 0x90;  // real mission start handled by sub 4835C0
    constexpr uint8_t C_SCREEN_INIT_REQ     = 0x8E;
    constexpr uint8_t C_MISSION_MENU        = 0x8E;  // legacy alias same value
    constexpr uint8_t C_MISSION_MENU_OPEN   = 0x8F;
    // 0xA1 is in race per player not mission list license complete via sub 482C40
    constexpr uint8_t C_LICENSE_COMPLETE    = 0xA3;
    // legacy alias mission claim unreversed reward auto granted in S 0x8C do not route 0xA3
    constexpr uint8_t C_CLAIM_REWARD        = 0xA3;
    
    constexpr uint8_t C_OPEN_GARAGE         = 0x0F;  // Open garage
    constexpr uint8_t C_GARAGE_VEHICLES     = 0x1B;
    constexpr uint8_t C_GARAGE_ITEMS        = 0x1C;  // Get item list
    constexpr uint8_t C_GARAGE_ACCESSORIES  = 0x1D;
    constexpr uint8_t C_UPGRADE_VEHICLE     = 0x9C;  // upgrade one i32 vehicleId handled by sub 482B00

    constexpr uint8_t C_GARAGE_BUY          = 0xB7;  // buy i32 action i32 key i32 result wstr name
    constexpr uint8_t C_GARAGE_DELETE       = 0xB8;
    constexpr uint8_t C_GARAGE_INSTALL      = 0xB9;  // install use repair i32 action i32 key i32 extra
    constexpr uint8_t C_GARAGE_REMOVE       = 0xBA;
    // 0x9D and 0x9E stay server to client only repair is 0xB9 action 2
    
    constexpr uint8_t C_QUICK_MATCH         = 0x64;
    // team change via sub 4816A0 int32 team also room update via sub 47ACD0 lobby means quick match
    constexpr uint8_t C_TEAM_CHANGE         = 0x64;
    
    // unproven labels the real messenger opcodes are the verified block below
    constexpr uint8_t C_ADD_FRIEND          = 0xB0;
    constexpr uint8_t C_REMOVE_FRIEND       = 0xB1;
    constexpr uint8_t C_BLOCK_PLAYER        = 0xB2;
    constexpr uint8_t C_PLAYER_PROFILE      = 0xB3;

    // messenger verified these used to route into the shop handler granting an item on friend click
    constexpr uint8_t C_ROOM_INVITE         = 0x6C;
    constexpr uint8_t C_ROOM_INVITE_ANSWER  = 0x6D;
    constexpr uint8_t C_FRIEND_ADD          = 0x6F;  // bare name
    constexpr uint8_t C_FRIEND_REQ_ACCEPT   = 0x70;
    constexpr uint8_t C_FRIEND_REQ_REJECT   = 0x71;  // bare id
    constexpr uint8_t C_USERINFO_BY_NAME    = 0x72;
    constexpr uint8_t C_FRIEND_STATUS_POLL  = 0x73;  // every 3000 ms keep it cheap
    constexpr uint8_t C_FRIEND_DEL          = 0x74;
    constexpr uint8_t C_BLOCK_ADD           = 0x7A;  // bare name  zero is the SUCCESS code
    constexpr uint8_t C_BLOCK_DEL           = 0x7B;
    constexpr uint8_t C_SMALLTALK_REQ       = 0x7D;  // bare name
    constexpr uint8_t C_SMALLTALK_ACCEPT    = 0x7E;
    constexpr uint8_t C_SMALLTALK_DECLINE   = 0x7F;  // bare id
    constexpr uint8_t C_SMALLTALK_CLOSE     = 0x80;
    constexpr uint8_t C_NOTE_SEND           = 0x81;
    constexpr uint8_t C_WHISPER_SEND        = 0xB5;  // real whisper send via sub 480C00 is not via 0x2F

    // 0xB7 has no failure variant refuse with server 0x0001 or 0x0002 instead
    constexpr uint8_t C_BUY                 = 0xB7;
    constexpr uint8_t C_SELL                = 0xB8;
    constexpr uint8_t C_LICENSE_SCREEN_OPEN = 0x16;  // screen ack data BEFORE it

    // 16 bit opcodes unreachable until the dispatcher switches on the full u16
    constexpr uint16_t C_CARCRAFT_OPEN      = 0x010A;
    constexpr uint16_t C_CARCRAFT_SAVE      = 0x010B;
    constexpr uint16_t C_ROOMCRAFT_OPEN     = 0x010E;
    constexpr uint16_t C_ROOMCRAFT_SAVE     = 0x010F;
    constexpr uint16_t C_EXTEND             = 0x0112;
    constexpr uint16_t C_STAGE22_OPEN       = 0x011C;
    constexpr uint16_t C_STAGE23_OPEN       = 0x011D;
    constexpr uint16_t C_USERLIST_PAGE      = 0x0132;
    constexpr uint16_t C_USERINFO_BY_ID     = 0x0133;
    
    constexpr uint8_t C_DRIFT_START         = 0xBC;
    constexpr uint8_t C_DRIFT_END           = 0xBD;  // End drift with boost level
    constexpr uint8_t C_BOOST_ACTIVATE      = 0xBE;
    constexpr uint8_t C_BOOST_END           = 0xBF;  // Boost ended
    
    constexpr uint8_t C_GHOST_MENU          = 0xC0;
    constexpr uint8_t C_GHOST_SELECT_MAP    = 0xC1;  // Select map for ghost race
    constexpr uint8_t C_GHOST_START         = 0xC2;
    constexpr uint8_t C_GHOST_COMPLETE      = 0xC3;  // Ghost race complete
    constexpr uint8_t C_GHOST_SAVE          = 0xC4;
    constexpr uint8_t C_GHOST_LIST          = 0xC5;  // Get ghost list for map
    constexpr uint8_t C_GHOST_DOWNLOAD      = 0xC6;
    
    constexpr uint8_t C_SCENARIO_MENU       = 0xC7;  // Open scenario menu state 22
    constexpr uint8_t C_SCENARIO_CHAPTER    = 0xC8;
    constexpr uint8_t C_SCENARIO_STAGE      = 0xC9;  // Select stage
    constexpr uint8_t C_SCENARIO_START      = 0xCA;
    constexpr uint8_t C_SCENARIO_COMPLETE   = 0xCB;  // Scenario complete
    constexpr uint8_t C_SCENARIO_PROGRESS   = 0xCC;
    constexpr uint8_t C_SCENARIO_CHAPTERS   = 0xCD;  // Get chapter list
    
    constexpr uint8_t I_SERVER_REGISTER     = 0xF0;  // GameServer registration to LoginServer


    // re added after a bad checkout wiped this header values proven on the wire
    constexpr uint16_t S_ROOM_TRACK_SELECT  = 0x35;
    constexpr uint16_t S_CAR_PART_CATALOG   = 0x108;
    constexpr uint8_t  C_SHOP_EXIT          = 0x71;  // shop screen leave via sub 482050
    constexpr uint16_t C_SHOP_BUY           = 0xB7;
    constexpr uint16_t C_ROOM_READY_TOGGLE  = 0x33;   // start and ready toggle
    constexpr uint16_t C_INVITE_JOIN        = 0x6E;
    constexpr uint16_t C_PROP_HIT           = 0xA1;
    constexpr uint16_t C_ROOM_PASSWORD      = 0x118;
    constexpr uint16_t C_WAYPOINT_REACHED   = 0x121;
    constexpr uint16_t C_TITLE_EQUIP        = 0x123;
    constexpr uint16_t C_OVERHEAT_STATE     = 0x125;
    constexpr uint16_t C_RANDOM_INVITE      = 0x12F;

    // recovered from the wire log both directions are 0x0062 size 0
    constexpr uint16_t C_PRACTICE_START     = 0x62;
    constexpr uint16_t S_ENTER_TUTORIAL     = 0x62;
    // 0x65 is a relay client reports a float server echoes id then float via FUN 0047AD60
    constexpr uint16_t C_RACE_GAUGE         = 0x65;
    constexpr uint16_t S_RACE_GAUGE         = 0x65;
    // fires once when LobbyInit finishes seen 80 times on the wire
    constexpr uint16_t C_LOBBY_TELEMETRY    = 0x130;
    // logged as client to server 0x00ED
    constexpr uint16_t C_GACHA_ROLL         = 0xED;

    // ten client senders never routed on our side bodies from a resync disasm

    // nick query sends wstring client reads back wstring cstring u32
    constexpr uint16_t C_NICK_QUERY         = 0x25;
    // nick select sends u32 no client handler fire and forget
    constexpr uint16_t C_NICK_SELECT        = 0x26;
    // nick action sends wstring no client handler sits beside anti chat message
    constexpr uint16_t C_NICK_ACTION        = 0x29;
    // u32 out u32 back handled by sub 47B8D0 pairs with the 0x84 catalog
    constexpr uint16_t C_EXT_DATA_132       = 0x84;
    // u32 out u32 back handled by sub 47B900 pairs with the 0x85 catalog
    constexpr uint16_t C_EXT_DATA_133       = 0x85;
    // u32 out u32 back handled by sub 47C270 shares the number with gift delete
    constexpr uint16_t C_GIFT_ACTION        = 0x9B;
    // ghost replay chunk already routed one 28 byte frame handled by sub 47CB30
    constexpr uint16_t C_ENTITY_REQ_242     = 0xF2;
    // u32 u32 out handled by sub 47DFA0 in the quest reward block
    constexpr uint16_t C_QUEST_CLAIM        = 0xF8;
    // u32 cstring out handled by sub 47E620 loops cstring rows back
    constexpr uint16_t C_ENTITY_LIST_276    = 0x114;
}  // namespace CMD

// wire structs see docs packets STRUCTURES

#pragma pack(push, 1)

/// 44 byte wire vehicle used by cmd 0x1B 0x3E 0x78 0x9D
struct VehicleData {
    int32_t uniqueId;  // 0x00 Unique instance ID DB key
    int32_t vehicleId;
    int32_t durability;  // 0x08 Current durability 0 100
    int32_t maxDurability;
    int32_t statSpeed;  // 0x10 Speed stat
    int32_t statAccel;
    int32_t statHandling;  // 0x18 Handling stat
    int32_t statDrift;
    int32_t statBoost;  // 0x20 Boost stat
    int32_t statWeight;
    int32_t statSpecial;  // 0x28 Special ability stat
};
static_assert(sizeof(VehicleData) == 44, "VehicleData must be 0x2C bytes");

/// 56 byte wire item used by cmd 0x1C 0x3E 0x9E
struct ItemData {
    int32_t uniqueId;  // 0x00 Unique instance ID DB key
    int32_t itemId;
    int32_t quantity;  // 0x08 Stack count
    int32_t slot;
    int32_t equipped;  // 0x10 0 inventory 1 equipped
    int32_t expiration;
    int32_t enhancement;  // 0x18 Enhancement level
    int32_t bound;
    int32_t reserved[6];  // 0x20 Reserved 24 bytes
};
static_assert(sizeof(ItemData) == 56, "ItemData must be 0x38 bytes");

/// 28 byte wire accessory used by cmd 0x1D 0x1E 0x9F
struct AccessoryData {
    int32_t uniqueId;  // 0x00 Unique instance ID
    int32_t accessoryId;
    int32_t slot;  // 0x08 Equipment slot 0 3
    int32_t bonus1;
    int32_t bonus2;  // 0x10 Bonus stat 2 value
    int32_t bonus3;
    int32_t equipped;  // 0x18 0 inventory 1 equipped
};
static_assert(sizeof(AccessoryData) == 28, "AccessoryData must be 0x1C bytes");

/// 32 byte compact item used by cmd 0x79 0x7A
struct SmallItem {
    int32_t itemId;  // 0x00 Item template ID
    int32_t uniqueId;
    int32_t quantity;  // 0x08 Stack count
    int32_t slot;
    int32_t flags;  // 0x10 Item flags status
    int32_t reserved[3];
};
static_assert(sizeof(SmallItem) == 32, "SmallItem must be 0x20 bytes");

/// 1224 byte PlayerInfo used by login 0x07 and 0xA7
struct PlayerInfo {
    int32_t  characterId;
    uint8_t  accountData[0x482];

    char16_t driverName[13];

    uint8_t  flag1;
    uint8_t  flag2;
    uint8_t  padding[2];

    int32_t  gold;
    int32_t  astros;
    int32_t  cash;

    int32_t  vehicleId;
    int32_t  driverId;

    uint8_t  flags[4];

    int32_t  unknown1;
    int32_t  unknown2;
    int32_t  unknown3;
};
static_assert(sizeof(PlayerInfo) == 1224, "PlayerInfo must be 0x4C8 bytes");

/// 246 byte room join payload cmd 0x3E
struct PlayerJoinData {
    int32_t      playerId;
    char16_t     playerName[35];
    int32_t      level;
    int32_t      team;
    VehicleData  vehicle;
    ItemData     item;
    int32_t      slot;
    uint8_t      extraData[60];
};
static_assert(sizeof(PlayerJoinData) == 246, "PlayerJoinData must be 246 bytes");

/// create room request from sub 480CC0 variable length wstrings name and password then four int32 fields
struct CreateRoomRequestLayout {
    // documentation only real wire is variable use Packet readWString
    static constexpr const char* FIELDS = "wstring name, wstring password, int32 mode, int32 maxPlayers, int32 mapId, int32 laps";
};

/// create room ack from sub 47AC30 payload is one int32 roomId
struct CreateRoomAck {
    int32_t roomId;
};
static_assert(sizeof(CreateRoomAck) == 4, "CreateRoomAck must be 4 bytes");

/// room info from sub 47A050 payload is one int32 roomId used as a lookup key
struct RoomInfoPacket {
    int32_t roomId;
};
static_assert(sizeof(RoomInfoPacket) == 4, "RoomInfoPacket must be 4 bytes");

/// room announce from sub 479630 lobby broadcast when any room is created variable length
struct RoomAnnounceLayout {
    static constexpr const char* FIELDS = "int32 roomId, wstring name, 7x int32 meta";
};

#pragma pack(pop)

// rate limits ms between packets the 0xA6 keepalive has no floor
namespace RateLimit {
    constexpr int CHAT_MIN_INTERVAL = 200;
    constexpr int DEFAULT_MIN_INTERVAL = 50;
    // 0x40 at 10 Hz plus 0x67 about once per client frame blow a cap of 100
    constexpr int GLOBAL_MAX_PACKETS_SEC = 400;
    // a login try runs PBKDF2 so one socket gets one a second
    constexpr int LOGIN_MIN_INTERVAL = 1000;
}

// no byte for this long drops a socket the stock client sends 0xA6 every second
constexpr int SESSION_IDLE_LIMIT_SEC = 180;

}  // namespace knc

