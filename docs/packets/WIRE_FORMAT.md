# KnC wire format, generated from KnC.exe

Generated not guessed, source is the client itself. Do not hand edit, rerun the generator.

Header is 8 bytes, u16 payload size then u16 opcode then u32 reserved, body at offset 8.

## How this was extracted

```
dispatcher   0x4777E5  movzx eax ax, dec, cmp 0x134  so opcodes are 1..309
index table  0x478638  309 bytes one per opcode
case table   0x4782D8  216 targets
read  n      0x44E910(dest, n)     size is pend[-2] of the arg pushes
read  cstr   0x44EB30(dest)        NUL terminated char
read  wstr   0x44EB60(dest)        wcslen*2+2
write n      0x44E9C0(src, n)
write cstr   0x44EAD0(str)
write wstr   0x44EB00(str)
begin        0x44ECD0(opcode)      every C2S sender starts here
send         0x476B80(buf)         overlapped WriteFile
```

Marks. `~` the sweep crossed a branch so a tail field may be conditional.
`?` a size the sweep could not resolve. `bN` a fixed N byte blob.

Verified against live chibikart capture. 0x35 8 bytes, 0x33 8 bytes, 0x3C 17 bytes,
0x30 4 bytes, 0x34 zero bytes. All match.

## S2C server to client

| op | name | layout | bytes | handler |
|----|------|--------|-------|---------|
| 0x01 | S_LOGIN_RESPONSE | ~cstr i32 | var | 00478DA0 |
| 0x02 | S_DISPLAY_MESSAGE | wstr i32 | var | 00478D40 |
| 0x03 | S_SET_GAME_VAR | (empty) | 0 | 00479220 |
| 0x04 | S_REGISTER_NICK_RES | ~i32 wstr b44 b56 | var | 00479230 |
| 0x07 | C_CLIENT_AUTH | ~i32 b1224 | 1228 | 00479020 |
| 0x08 | S_NO_HANDLER_08 | (empty) | 0 | 0046F7A0 |
| 0x0A | S_CONNECTION_OK | u8 x2 i32 x5 u8 x4 i32 x3 | 38 | 0047D3B0 |
| 0x0B | S_ACK | ~i32 x3 | 12 | 00479190 |
| 0x0C | S_DATA_PAIRS | ~i32 x3 | 12 | 004791A0 |
| 0x0D | S_FLAG_SET | (empty) | 0 | 0047ADE0 |
| 0x0E | S_CHANNEL_LIST | ~i32 x2 wstr i32 x3 wstr | var | 004793F0 |
| 0x0F | S_SHOW_GARAGE | (empty) | 0 | 00479520 |
| 0x10 | S_SHOW_SHOP | (empty) | 0 | 00479550 |
| 0x11 | S_SHOW_MENU | (empty) | 0 | 004793C0 |
| 0x12 | S_SHOW_LOBBY | (empty) | 0 | 004795A0 |
| 0x13 | S_PLAYER_ROOM_DATA | ~i32 wstr i32 x9 b48 | var | 0047FC20 |
| 0x14 | S_GAME_MODE_14 | ~i32 x6 | 24 | 00479CC0 |
| 0x16 | S_UI_STATE_14 | (empty) | 0 | 0047E8B0 |
| 0x18 | S_GAME_18 | i32 cstr i32 x2 | var | 0047C7A0 |
| 0x19 | C_SERVER_QUERY | cstr i32 x2 | var | 00479340 |
| 0x1B | S_INVENTORY_VEHICLES | ~i32 b44 | 48 | 00478EC0 |
| 0x1C | S_INVENTORY_ITEMS | ~i32 b56 | 60 | 00478F20 |
| 0x1D | S_INVENTORY_ACCESSORY | ~i32 b28 | 32 | 00478F80 |
| 0x1E | S_ITEM_UPDATE | b28 | 28 | 00478FE0 |
| 0x1F |  | (empty) | 0 | 0046F7A0 |
| 0x20 |  | (empty) | 0 | 0046F7A0 |
| 0x21 | S_ROOM_MEMBER | i32 x3 wstr u8 x3 i32 b44 b56 i32 x2 b60 | var | 004797C0 |
| 0x22 | S_LEAVE_ROOM | i32 | 4 | 00479920 |
| 0x23 | S_PLAYER_UPDATE | i32 x2 | 8 | 00479BE0 |
| 0x24 |  | (empty) | 0 | 0046F7A0 |
| 0x25 | S_ROOM_STRING | wstr cstr i32 | var | 00479AB0 |
| 0x27 | S_ROOM_INFO_ALT | i32 wstr cstr | var | 00479A30 |
| 0x28 | S_ENTITY_DATA | b104 | 104 | 00479B20 |
| 0x2A | S_WHISPER_ROOM | (empty) | 0 | 00479B60 |
| 0x2B | S_WHISPER_DISABLE | (empty) | 0 | 00479BD0 |
| 0x2D | S_ROOM_ANNOUNCE | i32 wstr i32 x7 | var | 00479630 |
| 0x2E | S_PLAYER_LEFT | ~i32 | 4 | 00479710 |
| 0x30 | S_ROOM_STATE | ~i32 | 4 | 00479760 |
| 0x31 | S_POSITION | i32 x3 | 12 | 00479950 |
| 0x32 | S_ROOM_SLOT_ENABLED | i32 x2 | 8 | 00479C70 |
| 0x33 | S_GAME_STATE | ~i32 x2 | 8 | 004799B0 |
| 0x34 | S_FLAG_34 | (empty) | 0 | 00479A10 |
| 0x35 | S_SCORE | i32 x2 | 8 | 00479C20 |
| 0x36 | C_LAP_COMPLETE | (empty) | 0 | 0046F7A0 |
| 0x37 | C_ITEM_PICKUP | (empty) | 0 | 0046F7A0 |
| 0x38 | C_ITEM_HIT | (empty) | 0 | 0046F7A0 |
| 0x39 | S_FINISH | (empty) | 0 | 00479CB0 |
| 0x3A | S_RESULTS | i32 | 4 | 0047AE00 |
| 0x3C | S_RACE_END | ~i32 x2 u8 i32 x2 | 17 | 0047A5C0 |
| 0x3D | S_ROOM_STATE_3D | ~i32 x2 | 8 | 0047A6B0 |
| 0x3E | S_PLAYER_JOIN | ~i32 wstr i32 x2 b44 b56 i32 b60 | var | 00479D60 |
| 0x3F | S_ROOM_INFO | ~i32 | 4 | 0047A050 |
| 0x40 | S_GAME_STATE_40 | ~u8 i32 u16 i64 x2 u8 u16 | 26 | 0047FD30 |
| 0x42 | S_GAME_MODE | ~i32 | 4 | 0047A0A0 |
| 0x43 |  | (empty) | 0 | 0046F7A0 |
| 0x44 | S_GAME_UPDATE | i32 | 4 | 0047A0E0 |
| 0x45 | S_ITEM_USAGE | i32 x3 | 12 | 0047A560 |
| 0x46 | S_LARGE_GAME_STATE | ~i32 x5 wstr u8 i32 x5 u8 i32 x3 | var | 0047A760 |
| 0x47 | S_PLAYER_ACTION | ~i32 x6 | 24 | 0047A110 |
| 0x48 |  | (empty) | 0 | 0046F7A0 |
| 0x49 | S_PLAYER_DATA | ~i32 x3 | 12 | 0047AAD0 |
| 0x4B | S_GAME_DATA_4B | ~i32 x4 | 16 | 0047A460 |
| 0x4D | C_REQUEST_DATA | (empty) | 0 | 004790C0 |
| 0x4E | S_TIMESTAMP | (empty) | 0 | 00479160 |
| 0x54 | S_SERVER_REDIRECT | ~i32 cstr i32 | var | 0047AA00 |
| 0x57 | S_RACE_STATUS | ~i32 x3 | 12 | 0047AB40 |
| 0x58 | S_PLAYER_STATUS | ~i32 u8 | 5 | 0047ABE0 |
| 0x5C | S_ROOM_DATA_5C | ~i32 x3 | 12 | 0047A500 |
| 0x5F | S_GAME_5F | ~i32 x2 | 8 | 0047EE70 |
| 0x62 | S_TUTORIAL_FAIL | (empty) | 0 | 00479580 |
| 0x63 | S_CREATE_ROOM_ACK | i32 | 4 | 0047AC30 |
| 0x64 | S_ROOM_STATUS | ~i32 x2 | 8 | 0047ACD0 |
| 0x65 | S_SPEED_UPDATE | ~i32 x2 | 8 | 0047AD60 |
| 0x66 |  | (empty) | 0 | 0046F7A0 |
| 0x68 | S_SHOP_LOOKUP | ~i32 x5 | 20 | 0047AE30 |
| 0x69 | S_SHOP_ITEM | ~i32 u16 u8 | 7 | 0047AF00 |
| 0x6A | S_SHOP_UPDATE | ~i32 u16 | 6 | 0047AFE0 |
| 0x6C | S_SHOP_CHAT | ~i32 wstr cstr i32 x2 wstr ? | var | 0047B030 |
| 0x6D | C_ROOM_INVITE_ANSWER | (empty) | 0 | 0046F7A0 |
| 0x6E | S_SHOP_CALL | (empty) | 0 | 0047B190 |
| 0x6F | S_SHOP_RESPONSE | ~i32 b44 | 48 | 0047B3C0 |
| 0x70 | S_SHOP_EVENT | i32 x2 | 8 | 0047B300 |
| 0x71 | C_BUY_ITEM_ALT | i32 x2 | 8 | 0047B300 |
| 0x72 | S_DATA_BLOCK | b104 | 104 | 0047B550 |
| 0x73 | S_SLOT_UPDATE | ~i32 b12 | 16 | 0047B570 |
| 0x74 | C_KART_ACT | i32 x2 | 8 | 0047B620 |
| 0x76 | S_INVENTORY_LIST | ~i32 b44 | 48 | 0047B1B0 |
| 0x77 | S_UPDATE_LIST | ~i32 x3 | 12 | 0047B340 |
| 0x78 | S_ITEM_LIST | ~i32 b36 | 40 | 0047B220 |
| 0x79 | S_ITEM_LIST_B | ~i32 b32 | 36 | 0047B290 |
| 0x7A | S_ITEM_ADD | ~i32 b32 | 36 | 0047B4F0 |
| 0x7B | S_INV_SLOT | i32 x2 | 8 | 0047B670 |
| 0x7D | S_NOTIFICATION | ~i32 wstr | var | 0047B6B0 |
| 0x7E | C_SMALLTALK_ACCEPT | (empty) | 0 | 0046F7A0 |
| 0x7F | C_SMALLTALK_DECLINE | (empty) | 0 | 0046F7A0 |
| 0x80 | C_SMALLTALK_CLOSE | (empty) | 0 | 0046F7A0 |
| 0x81 | S_EXT_81 | i32 | 4 | 0047B8A0 |
| 0x82 | S_EXT_82 | b396 | 396 | 0047B710 |
| 0x83 | S_EXT_DATA_131 | b396 | 396 | 0047B7D0 |
| 0x84 | S_EXT_DATA_132 | ~i32 | 4 | 0047B8D0 |
| 0x85 | S_EXT_DATA_133 | i32 | 4 | 0047B900 |
| 0x86 |  | (empty) | 0 | 0046F7A0 |
| 0x87 | S_EXT_87 | b188 | 188 | 0047DC10 |
| 0x88 | S_EXT_88 | ~i32 i64 | 12 | 0047B930 |
| 0x8A | S_EXT_DATA_138 | i64 | 8 | 0047B990 |
| 0x8B |  | (empty) | 0 | 0046F7A0 |
| 0x8C | S_EQUIP_ITEM | ~i32 i64 i32 x4 b44 b56 b28 x3 i64 b48 b132 u8 b52 | 453 | 0047B9E0 |
| 0x8D | C_LICENSE_PANEL_CLOSE | (empty) | 0 | 0046F7A0 |
| 0x8F | S_UI_STATE_24 | (empty) | 0 | 0047E950 |
| 0x90 | S_UI_STATE_25 | i32 x2 | 8 | 0047DF30 |
| 0x91 |  | (empty) | 0 | 0046F7A0 |
| 0x92 |  | (empty) | 0 | 0046F7A0 |
| 0x93 |  | (empty) | 0 | 0046F7A0 |
| 0x94 |  | (empty) | 0 | 0046F7A0 |
| 0x95 | S_LIST_D4 | ~i32 b212 | 216 | 0047BE00 |
| 0x96 | S_GIFT_INBOX_RECV | ~i32 b212 | 216 | 0047BD80 |
| 0x97 | S_GIFT_INBOX_SENT | ~i32 b212 | 216 | 0047BE00 |
| 0x98 | S_GIFT | i32 x2 b212 | 220 | 0047BEF0 |
| 0x99 | S_SINGLE_D4 | ~b212 | 212 | 0047BE80 |
| 0x9A | S_GIFT_REWARD_GRANT | ~i32 x2 b28 | 36 | 0047BF80 |
| 0x9B | S_GIFT_DELETE | i32 | 4 | 0047C270 |
| 0x9C | S_GIFT_MARK_READ | ~i32 | 4 | 0047C2A0 |
| 0x9D | S_ADD_VEHICLE | ~b44 | 44 | 0047C2D0 |
| 0x9E | S_ADD_ITEM | ~b56 | 56 | 0047C320 |
| 0x9F | S_ADD_ACCESSORY | ~b28 | 28 | 0047C370 |
| 0xA0 |  | (empty) | 0 | 0046F7A0 |
| 0xA1 | S_MISSION_COMPLETE | i32 | 4 | 0047C930 |
| 0xA2 | S_MISSION_LIST | ~i32 b12 | 16 | 0047C960 |
| 0xA3 | S_REWARD_CLAIM | ~i32 x2 b12 i64 i32 b44 b56 b28 x3 i64 b48 b132 ? b52 | var | 0047C3C0 |
| 0xA4 |  | u8 | 1 | 0047C770 |
| 0xA5 |  | ~u8 i32 i64 | 13 | 0047C830 |
| 0xA7 | S_SESSION_CONFIRM | i32 b1224 | 1228 | 00479080 |
| 0xAA | S_PLAYER_PREVIEW | ~i32 wstr i32 x2 b44 b56 i32 b176 i32 b176 | var | 0047CBA0 |
| 0xAB | S_GACHA_BANNER_HEADER | i32 | 4 | 0047C9C0 |
| 0xAC | S_GACHA_BANNER_ENTRY | ~i32 b176 i32 b176 | 360 | 0047C9F0 |
| 0xAD |  | (empty) | 0 | 0047CAE0 |
| 0xAE |  | i32 | 4 | 0047CB00 |
| 0xAF |  | ~i32 b28 | 32 | 0047CB30 |
| 0xB0 | C_ADD_FRIEND | ~i32 b176 i32 b176 | 360 | 0047CC20 |
| 0xB4 | S_SYSTEM_MESSAGE | ~i32 wstr x2 i32 | var | 0047CD60 |
| 0xB5 | S_PLAYER_COMPARISON | ~b1224 x2 wstr | var | 0047CF80 |
| 0xB6 | S_DISPLAY_TEXT | wstr i32 | var | 0047AC60 |
| 0xB7 | S_INVENTORY_UPDATE | ~i32 x3 b44 b56 b28 x2 b48 u8 b132 b52 b28 | 429 | 00484F50 |
| 0xB8 | S_REMOVE_ITEM | ~i32 x2 | 8 | 00484690 |
| 0xB9 | S_INVENTORY_SLOT | ~i32 b44 | 48 | 00484770 |
| 0xBA | S_INVENTORY_OP | ~i32 b28 | 32 | 00484B10 |
| 0xBB |  | (empty) | 0 | 0046F7A0 |
| 0xBC | C_DRIFT_START | ~i32 x3 b48 | 60 | 00484D90 |
| 0xBE | S_RACE_INIT | (empty) | 0 | 00478B50 |
| 0xBF | S_DRIVER_CATALOG | ~i32 x5 cstr b20 cstr x2 i32 b16 | var | 0047F390 |
| 0xC0 | S_KART_CATALOG | ~i32 x3 u8 i32 x4 cstr x3 b32 b68 i64 i32 b16 | var | 0047F4F0 |
| 0xC1 | S_ITEM_CATALOG | ~i32 x5 cstr x3 i32 b16 | var | 0047F6B0 |
| 0xC2 | S_PART_CATALOG | ~i32 x4 cstr i32 x3 cstr x2 i64 i32 b16 | var | 0047F800 |
| 0xC3 | S_RACE_PLAYER_5 | i32 x3 cstr i32 x14 cstr | var | 0047F990 |
| 0xC4 | S_RACE_DATA | i32 x2 cstr x2 | var | 00478C40 |
| 0xC5 | S_RACE_PLAYER_6 | i32 x2 cstr i32 x13 cstr x2 | var | 0047FAE0 |
| 0xC6 | S_RACE_DATA_2 | i32 x7 | 28 | 00478CB0 |
| 0xC9 | C_SCENARIO_STAGE | ~i32 | 4 | 0047CD20 |
| 0xCD | S_FRIEND_ONLINE_FLIP | ~i32 x2 | 8 | 0047D1C0 |
| 0xCE | S_GAME_MESSAGE | ~cstr wstr i32 | var | 0047D250 |
| 0xCF | S_FRIEND_STATS_UPDATE | ~i32 x4 | 16 | 0047D4A0 |
| 0xD0 | C_CLIENT_INFO | i32 | 4 | 0047D520 |
| 0xD9 | S_PLAYER_FULL_UPDATE | ~i32 b44 b56 b60 | 164 | 0047D540 |
| 0xED | S_ENTITY_UPDATE | ~b20 b28 b44 b56 b28 x2 b48 b132 u8 b52 | 437 | 0047D5E0 |
| 0xEE | S_FRIEND_BLOB | ~i32 b16 | 20 | 0047D880 |
| 0xF0 | S_FRIEND_RECORD | ~i32 | 4 | 0047D930 |
| 0xF1 | S_ENTITY_CLEAR | b36 | 36 | 0047D970 |
| 0xF3 | S_BLOCK_LIST_REFRESH | b156 | 156 | 0047DAA0 |
| 0xF4 | S_ENTITY_DATA_244 | ~i32 i64 | 12 | 0047DB00 |
| 0xF5 | S_ENTITY_DATA_245 | ~i32 x2 | 8 | 0047DD10 |
| 0xF6 | S_ENTITY_DATA_246 | i32 | 4 | 0047DC70 |
| 0xF7 | S_ENTITY_DATA_247 | ~i32 b28 | 32 | 0047DCA0 |
| 0xF8 | S_ENTITY_DATA_248 | ~i32 u8 i64 i32 x4 b44 b56 b28 x3 i64 b48 b132 u8 b52 | 454 | 0047DFA0 |
| 0xF9 | S_ENTITY_DATA_249 | i64 | 8 | 0047E350 |
| 0xFB | S_BUDDY_LIST_ENTRY | b112 | 112 | 0047DB60 |
| 0xFC | S_BUDDY_STATUS_LIST | ~i32 b12 | 16 | 0047DBB0 |
| 0xFE | S_ENTITY_DATA_254 | i32 | 4 | 0047F270 |
| 0x100 | S_ENTITY_DATA_256 | i32 | 4 | 0047F2C0 |
| 0x101 | S_ENTITY_DATA_257 | i32 x2 | 8 | 0047F2F0 |
| 0x102 | S_ENTITY_DATA_258 | i32 x2 | 8 | 0047F340 |
| 0x103 | S_ENTITY_DATA_259 | ~i32 x4 cstr x3 i32 b16 | var | 004800D0 |
| 0x104 | S_ENTITY_DATA_260 | ~i32 b28 | 32 | 0047E3A0 |
| 0x107 | S_ENTITY_DATA_263 | ~i32 b52 | 56 | 0047E400 |
| 0x108 | S_ENTITY_DATA_264 | ~i32 x5 cstr x3 b68 i64 b12 i32 b16 | var | 00480210 |
| 0x109 | S_ENTITY_DATA_265 | b132 | 132 | 0047E4C0 |
| 0x10A | S_ENTITY_DATA_266 | (empty) | 0 | 0047E500 |
| 0x10B | S_ENTITY_DATA_267 | ~i32 x3 b32 i32 b132 | 180 | 0047E530 |
| 0x10C | S_ENTITY_DATA_268 | ~i32 x6 cstr x3 i32 b16 | var | 0047FF70 |
| 0x10D | S_ENTITY_DATA_269 | b48 | 48 | 0047D9A0 |
| 0x10E | S_ENTITY_DATA_270 | (empty) | 0 | 0047D9D0 |
| 0x10F | S_ENTITY_DATA_271 | ~i32 b48 | 52 | 0047DA00 |
| 0x112 | S_ENTITY_DATA_274 | ~i32 x3 b48 | 60 | 00484EB0 |
| 0x114 | S_ENTITY_DATA_276 | ~i32 cstr | var | 0047E620 |
| 0x115 | S_ENTITY_DATA_277 | i32 x2 | 8 | 0047E680 |
| 0x116 | S_ENTITY_DATA_278 | i32 wstr u8 | var | 0047E6D0 |
| 0x117 | S_ENTITY_DATA_279 | i32 | 4 | 0047E750 |
| 0x118 | S_ENTITY_DATA_280 | ~i32 x3 | 12 | 0047E780 |
| 0x119 | S_ENTITY_DATA_281 | i32 x2 cstr x3 | var | 0047E800 |
| 0x11A | S_ENTITY_DATA_282 | i64 | 8 | 0047E880 |
| 0x11B | S_ENTITY_DATA_283 | i64 | 8 | 0047E880 |
| 0x11C | S_ENTITY_DATA_284 | (empty) | 0 | 0047E8E0 |
| 0x11D | S_ENTITY_DATA_285 | (empty) | 0 | 0047E980 |
| 0x11E | S_ENTITY_DATA_286 | cstr i32 | var | 0047E9B0 |
| 0x11F | S_ENTITY_DATA_287 | ~i32 | 4 | 0047EA10 |
| 0x120 | S_ENTITY_DATA_288 | ~i32 b12 i32 | 20 | 0047EA70 |
| 0x122 | S_ENTITY_DATA_290 | (empty) | 0 | 0047EB70 |
| 0x123 | S_ENTITY_DATA_291 | i32 | 4 | 0047EAE0 |
| 0x124 | S_ENTITY_DATA_292 | ~b52 | 52 | 0047EB10 |
| 0x125 | S_ENTITY_DATA_293 | ~i32 u8 | 5 | 0047EBB0 |
| 0x126 | S_ENTITY_DATA_294 | ~i32 wstr cstr i32 | var | 0047EC00 |
| 0x12E | S_ENTITY_DATA_302 | ~i32 b16 | 20 | 0047ED40 |
| 0x12F | S_ENTITY_DATA_303 | ~i32 wstr i32 x2 wstr | var | 0047ED90 |
| 0x131 | S_ENTITY_DATA_305 | ~i32 | 4 | 00478E80 |
| 0x132 | S_ENTITY_DATA_306 | i32 x2 b288 | 296 | 0047EE30 |
| 0x135 | S_ENTITY_DATA_309 | ~i32 x7 b28 b56 b132 u8 b52 b28 b44 b48 | 417 | 0047EEF0 |

## C2S client to server

Note on 0x39. It is NOT a race finish. sub_40D9D0 is a mouse click handler on the
room player list, it walks up to 30 rows 25 px apart, requires the clicker to be the
room master or admin level 6, requires the clicked row to not be yourself, then sends
0x39 carrying the selected player id and shows MSG_WAIT. It is a moderation action on
another player, almost certainly a kick.


| op | name | layout | bytes | sender |
|----|------|--------|-------|--------|
| 0x04 | S_REGISTER_NICK_RES | i32 wstr | var | 00480630 |
| 0x07 | C_CLIENT_AUTH | cstr i32 wstr x2 cstr i32 wstr i32 | var | 00480464 |
| 0x18 | C_CHANNEL_SELECT | i32 x2 | 8 | 00480724 |
| 0x19 | C_SERVER_QUERY | cstr i32 x2 | var | 00481815 |
| 0x25 | S_ROOM_STRING | wstr | var | 004807E4 |
| 0x26 |  | i32 | 4 | 00480884 |
| 0x29 |  | wstr | var | 00480924 |
| 0x2C | C_STATE_CHANGE | i32 x2 | 8 | 00481044 |
| 0x2D | C_CREATE_ROOM_REQ | wstr x2 i32 x4 | var | 00480D23 |
| 0x2F | C_WHISPER | i32 wstr | var | 00480F96 |
| 0x33 | C_ROOM_READY_TOGGLE | i32 | 4 | 00480E14 |
| 0x35 | S_SCORE | i32 x2 | 8 | 00480EB4 |
| 0x39 | C_RACE_FINISH | i32 | 4 | 0040DABD |
| 0x40 | C_GAME_START | ~i64 x2 u8 u16 | 19 | 004818D5 |
| 0x41 |  | i32 x2 | 8 | 00481104 |
| 0x47 | S_PLAYER_ACTION | i32 x5 | 20 | 00481264 |
| 0x49 | S_PLAYER_DATA | i32 x2 | 8 | 004811B4 |
| 0x4B | MODERN_MAGIC | i32 x3 | 12 | 0048139A |
| 0x4D | C_REQUEST_DATA | b276 | 276 | 004790F4 |
| 0x57 | S_RACE_STATUS | i32 x3 | 12 | 00481554 |
| 0x58 | S_PLAYER_STATUS | u8 | 1 | 00481624 |
| 0x5C | S_ROOM_DATA_5C | i32 x2 | 8 | 004814AA |
| 0x5F | S_GAME_5F | i32 x2 | 8 | 00483E34 |
| 0x64 | C_QUICK_MATCH | i32 | 4 | 004816D4 |
| 0x65 | C_RACE_GAUGE | i32 | 4 | 00481774 |
| 0x67 |  | i32 | 4 | 00481A14 |
| 0x69 | S_SHOP_ITEM | u16 u8 | 3 | 00481B94 |
| 0x6C | C_ROOM_INVITE | wstr | var | 00481D04 |
| 0x6D | C_ROOM_INVITE_ANSWER | i32 x2 | 8 | 00481DA4 |
| 0x6E | C_INVITE_JOIN | i32 wstr | var | 00481E86 |
| 0x6F | C_FRIEND_ADD | wstr | var | 00481F40 |
| 0x70 | C_BUY_ITEM | i32 | 4 | 00482124 |
| 0x71 | C_BUY_ITEM_ALT | i32 | 4 | 00482084 |
| 0x72 | C_BUY_KART | wstr | var | 00482304 |
| 0x74 | C_KART_ACT | i32 | 4 | 004821C4 |
| 0x7A | C_KART_ALT | wstr | var | 00481FF0 |
| 0x7B | C_BUY_PREMIUM | i32 | 4 | 00482264 |
| 0x7D | C_SMALLTALK_REQ | wstr | var | 004823A4 |
| 0x7E | C_SMALLTALK_ACCEPT | i32 | 4 | 004824E4 |
| 0x7F | C_SMALLTALK_DECLINE | i32 | 4 | 00482444 |
| 0x80 | C_SMALLTALK_CLOSE | i32 | 4 | 00482587 |
| 0x81 | C_NOTE_SEND | wstr x2 | var | 00482627 |
| 0x84 | S_EXT_DATA_132 | i32 | 4 | 004826D7 |
| 0x85 | S_EXT_DATA_133 | i32 | 4 | 00482777 |
| 0x8C | C_MISSION_COMPLETE | i32 | 4 | 0043ADDD |
| 0x8D | C_LICENSE_PANEL_CLOSE | u16 | 2 | 00481C54 |
| 0x90 | C_MISSION_START | i32 | 4 | 004835F7 |
| 0x98 | C_SHOP_GIFT | i32 wstr i32 x2 wstr | var | 00482915 |
| 0x9A | C_USE_ITEM | i32 | 4 | 004829F7 |
| 0x9B | S_GIFT_DELETE | i32 | 4 | 00482A97 |
| 0x9C | C_UPGRADE_VEHICLE | i32 | 4 | 00482B37 |
| 0xA1 | C_PROP_HIT | i32 | 4 | 00482BD7 |
| 0xA3 | C_LICENSE_COMPLETE | i32 x3 | 12 | 00482C77 |
| 0xA7 | S_SESSION_CONFIRM | cstr i32 wstr i32 | var | 00480538 |
| 0xAA | C_TUTORIAL_COMPLETE | i32 | 4 | 00482D47 |
| 0xAF |  | ~i32 b28 i32 b28 | 64 | 0042570F |
| 0xB0 | C_ADD_FRIEND | i32 x3 | 12 | 00425850 |
| 0xB2 | C_BLOCK_PLAYER | (empty) | 0 | 00425515 |
| 0xB4 | C_LOBBY_CHAT | wstr i32 | var | 00480B7E |
| 0xB5 | C_WHISPER_SEND | i32 wstr | var | 00480C4A |
| 0xB7 | C_GARAGE_BUY | ~i32 x3 wstr i32 x10 | var | 00484248 |
| 0xB8 | C_GARAGE_DELETE | ~i32 x5 | 20 | 004844E5 |
| 0xB9 | C_GARAGE_INSTALL | ~i32 x10 | 40 | 00484335 |
| 0xBA | C_GARAGE_REMOVE | ~i32 x7 | 28 | 00484415 |
| 0xCB | C_SCENARIO_COMPLETE | b28 u8 | 29 | 00482E02 |
| 0xCC | C_SCENARIO_PROGRESS | b28 u8 | 29 | 00482E8B |
| 0xCD | C_SCENARIO_CHAPTERS | i32 | 4 | 004831B7 |
| 0xCF | S_FRIEND_STATS_UPDATE | i32 x3 | 12 | 00483257 |
| 0xD0 | C_CLIENT_INFO | wstr cstr | var | 00483327 |
| 0xD9 | S_PLAYER_FULL_UPDATE | i32 x2 b60 | 68 | 004714B5 |
| 0xED | C_GACHA_ROLL | b28 | 28 | 0048311E |
| 0xF2 |  | i32 | 4 | 004833D7 |
| 0xF5 | S_ENTITY_DATA_245 | i32 | 4 | 00483557 |
| 0xF8 | S_ENTITY_DATA_248 | i32 x2 | 8 | 0042E50B |
| 0xFE | C_LAUNCHER_LOGIN | i32 | 4 | 00483EE7 |
| 0x100 | S_ENTITY_DATA_256 | i32 | 4 | 00483F87 |
| 0x102 | S_ENTITY_DATA_258 | i32 | 4 | 00401F5C |
| 0x10B | C_CARCRAFT_SAVE | ~i32 b32 i32 ? | var | 004836C9 |
| 0x10F | C_ROOMCRAFT_SAVE | ~i32 ? | var | 00483491 |
| 0x112 | C_EXTEND | ~i32 x3 | 12 | 004845F7 |
| 0x114 | S_ENTITY_DATA_276 | i32 cstr | var | 004837C7 |
| 0x118 | C_ROOM_PASSWORD | i32 x2 wstr | var | 004838C2 |
| 0x121 | C_WAYPOINT_REACHED | i32 | 4 | 004839D7 |
| 0x123 | C_TITLE_EQUIP | i32 | 4 | 00483A77 |
| 0x125 | C_OVERHEAT_STATE | u8 | 1 | 00483B17 |
| 0x12F | C_RANDOM_INVITE | (empty) | 0 | 00483BB7 |
| 0x130 | C_LOBBY_TELEMETRY | i32 | 4 | 00483C57 |
| 0x132 | C_USERLIST_PAGE | i32 | 4 | 00483CF7 |
| 0x133 | C_USERINFO_BY_ID | i32 | 4 | 00483D97 |

## The packed motion vector, sub_44E610

C2S 0x40 is 19 bytes, two 64 bit packed vectors then a u8 then a u16. Real clients
UNPACK every relayed motion, so sending the raw 28 byte form makes other players draw
you far off the map. Send packed on any server with real clients.

Per axis the client takes the magnitude, keeps 12 bits of whole part clamped to 0xFFF
and two decimals, and carries the three signs together as an octant code.

```
low  dword   bits 0-3    octant, 1 to 8, from sub_44E370
             bits 4-11   C fraction, 0 to 99
             bits 12-23  C whole, 12 bits
             bits 24-31  B fraction
high dword   bits 0-11   B whole
             bits 12-19  A fraction
             bits 20-31  A whole
```

Octant, the sign combination. Product of the three components decides the half.

```
product > 0   all positive 1, else 2, then 3 if B > 0, then 4 if C > 0
product < 0   all negative 5, else 6, then 7 if B < 0, then 8 if C < 0
zero          replace each zero component with 1.0 and redo
```

Verified by rebuilding every captured word from its decoded fields, 8370 of 8370
exact. Decoding a live race gives a smooth trajectory, about 4 units per frame at
10 Hz through a corner, which is a plausible kart speed.
