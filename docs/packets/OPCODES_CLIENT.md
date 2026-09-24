# KnC opcode map

Generated from KnC.exe by walking the dispatcher double table then
disassembling every handler. Read primitives:

```
sub_44E910(dst, N)   read N raw bytes
sub_44EB60(dst)      read NUL terminated utf16 string
sub_44EB30(dst)      read NUL terminated ascii string
```

Dispatcher sub_4777C0:

```
EAX = opcode - 1
EAX = byte [0x478638 + EAX]     index table 0x135 entries, 215 means default
JMP [0x4782D8 + EAX*4]          each target is a 13 byte thunk
handler = thunk + 8 + rel32
```

215 of 309 opcodes have a real handler. The other 94 fall to the default
and are discarded silently.

## Legend

| Mark | Meaning |
|------|---------|
| SENT | server already builds this opcode |
| ---- | client handles it, server never sends it |
| loop | block repeats count times, count is the int32 above it |

## Opcodes

### 0x01 S_LOGIN_RESPONSE

handler `sub_478DA0`  SENT

```
string    NUL terminated ascii           -> [esp + 0xc]
int32                                    -> [esp + 0xc]
```

calls `sub_4641E0` `sub_43D7E0` `sub_590342`

### 0x02 S_DISPLAY_MESSAGE

handler `sub_478D40`  SENT

```
wstring   NUL terminated utf16           -> [esp + 4]
int32                                    -> [esp + 4]
```

calls `sub_464030` `sub_590342`

### 0x03 S_SET_GAME_VAR, S_TRIGGER

handler `sub_479220`  SENT

payload: none, pure trigger

calls `sub_473730`

### 0x04 S_REGISTER_NICK_RES, S_REGISTRATION_RESP

handler `sub_479230`  SENT

```
int32                                    -> [esp + 0xc]
wstring   NUL terminated utf16           -> [esi + 0x80e63e]
blob 44   CHAR_REC                       -> [esp + 0x10]
blob 56   KART_REC                       -> [esp + 0x3c]
```

calls `sub_4538B0` `sub_44FCB0` `sub_44F2D0` `sub_4641E0` `sub_4707B0` `sub_4707B0` `sub_4641E0`

### 0x07 C_CLIENT_AUTH

handler `sub_479020`  SENT

```
int32                                    -> [esi + 0x80e1a8]
blob 1224 REC_1224                       -> [esi + 0x80e1b8]
```

### 0x08 S_NO_HANDLER_08

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x0A S_CONNECTION_OK

handler `sub_47D3B0`  SENT

```
int8                                     -> [esi + 0x80e658]
int8                                     -> [esi + 0x80e659]
int32                                    -> [esi + 0x80e65c]
int32                                    -> [esi + 0x80e660]
int32                                    -> [esi + 0x80e664]
int32                                    -> [esi + 0x80e668]
int32                                    -> [esi + 0x80e66c]
int8                                     -> [esi + 0x80e670]
int8                                     -> [esi + 0x80e671]
int8                                     -> [esi + 0x80e672]
int8                                     -> [esi + 0x80e673]
int32                                    -> [esi + 0x80e674]
int32                                    -> [esi + 0x80e678]
int32                                    -> 0x80e67c
```

### 0x0B S_ACK

handler `sub_479190`  SENT

payload: none, pure trigger

calls `sub_590350` `sub_44ECD0` `sub_476B80` `sub_44E8E0` `sub_590342`

### 0x0C S_DATA_PAIRS

handler `sub_4791A0`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  int32                                  -> [esp + 0x14]
  int32                                  -> [esp + 0x1c]
}
```

calls `sub_40D4E0`

### 0x0D S_FLAG_SET

handler `sub_47ADE0`  SENT

payload: none, pure trigger

calls `sub_4B5160`

### 0x0E S_CHANNEL_LIST

handler `sub_4793F0`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  int32                                  -> [esp + 0x1c]
  wstring   NUL terminated utf16         -> [esp + 0x20]
  int32                                  -> [esp + 0x18]
  int32                                  -> [esp + 0x14]
  int32                                  -> [esp + 0x20]
}
wstring   NUL terminated utf16           -> 0xc1a718
```

calls `sub_424210` `sub_424250` `sub_424320` `sub_404410` `sub_4538B0` `sub_590342` `sub_4641E0` `sub_590342`

### 0x0F S_SHOW_GARAGE, C_OPEN_GARAGE

handler `sub_479520`  SENT

payload: none, pure trigger

calls `sub_484010` `sub_404410` `sub_4538B0`

### 0x10 S_SHOW_SHOP, C_SHOP_ENTER

handler `sub_479550`  SENT

payload: none, pure trigger

calls `sub_484010` `sub_404410` `sub_4538B0`

### 0x11 S_SHOW_MENU

handler `sub_4793C0`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0`

### 0x12 S_SHOW_LOBBY, S_HEARTBEAT_RESP

handler `sub_4795A0`  SENT

payload: none, pure trigger

calls `sub_484010` `sub_404410` `sub_4538B0` `sub_44F450` `sub_4641E0` `sub_4641E0`

### 0x13 S_PLAYER_ROOM_DATA

handler `sub_47FC20`  SENT

```
int32                                    -> 0xbce1b0
wstring   NUL terminated utf16           -> 0xbce1b4
int32                                    -> 0xbce22c
int32                                    -> 0xbce210
int32                                    -> 0xbce224
int32                                    -> 0xbce214
int32                                    -> 0xbce218
int32                                    -> 0xbce21c
int32                                    -> 0xbce220
int32                                    -> 0xbce244
int32                                    -> [esp + 0x40]
loop count times {
  blob 48   ROOM_OBJ                     -> [esp + 0xc]
}
```

calls `sub_452380` `sub_4523A0` `sub_404410` `sub_4538B0`

### 0x14 S_GAME_MODE_14, S_GAME_MODE_SETUP

handler `sub_479CC0`  SENT

```
int32                                    -> 0xb2316c
int32                                    -> 0xb23170
int32                                    -> 0xb23174
int32                                    -> 0xb23178
int32                                    -> 0xb23198
int32                                    -> 0xb2319c
```

calls `sub_4538B0` `sub_404410`

### 0x16 S_UI_STATE_14, C_LICENSE_SCREEN_OPEN

handler `sub_47E8B0`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0`

### 0x18 S_GAME_18, C_CHANNEL_SELECT

handler `sub_47C7A0`  SENT

```
int32                                    -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x10]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0xc]
```

calls `sub_405EB0` `sub_590342`

### 0x19 C_SERVER_QUERY

handler `sub_479340`  SENT

```
string    NUL terminated ascii           -> [esp + 0xc]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0xc]
```

calls `sub_405EB0` `sub_590342`

### 0x1B S_INVENTORY_VEHICLES, C_GARAGE_VEHICLES

handler `sub_478EC0`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 44   CHAR_REC                     -> [esp + 0x14]
}
```

calls `sub_44FC90` `sub_44FCB0`

### 0x1C S_INVENTORY_ITEMS, C_GARAGE_ITEMS

handler `sub_478F20`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 56   KART_REC                     -> [esp + 0x14]
}
```

calls `sub_44F2B0` `sub_44F2D0`

### 0x1D S_INVENTORY_ACCESSORY, C_GARAGE_ACCESSORIES

handler `sub_478F80`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 28   ITEM_REC                     -> [esp + 0x14]
}
```

calls `sub_451510` `sub_451530`

### 0x1E S_ITEM_UPDATE

handler `sub_478FE0`  SENT

```
blob 28   ITEM_REC                       -> [esp + 8]
```

calls `sub_450E30` `sub_450D20`

### 0x1F 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x20 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x21 S_ROOM_MEMBER, S_ROOM_FULL

handler `sub_4797C0`  SENT

```
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
wstring   NUL terminated utf16           -> [esp + 0x53e]
int8                                     -> [esp + 0x55d]
int8                                     -> [esp + 0x575]
int8                                     -> [esp + 0x576]
int32                                    -> [esp + 0x580]
blob 44   CHAR_REC                       -> [esp + 0x1c]
blob 56   KART_REC                       -> [esp + 0x48]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x18]
blob 60   CUSTOM_REC                     -> [esp + 0x80]
```

calls `sub_40D650` `sub_590342`

### 0x22 S_LEAVE_ROOM, C_LEAVE_ROOM

handler `sub_479920`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_40D880`

### 0x23 S_PLAYER_UPDATE, C_PLAYER_READY

handler `sub_479BE0`  SENT

```
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x10]
```

calls `sub_407660`

### 0x24 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x25 S_ROOM_STRING

handler `sub_479AB0`  ----

```
wstring   NUL terminated utf16           -> [esp + 8]
string    NUL terminated ascii           -> ?
int32                                    -> [esp + 4]
```

calls `sub_463CD0` `sub_590342`

### 0x27 S_ROOM_INFO_ALT

handler `sub_479A30`  ----

```
int32                                    -> [esp + 8]
wstring   NUL terminated utf16           -> [esp + 0xc]
string    NUL terminated ascii           -> ?
```

calls `sub_463CD0` `sub_590342`

### 0x28 S_ENTITY_DATA

handler `sub_479B20`  SENT

```
blob 104                                 -> [esp + 4]
```

calls `sub_45AAF0` `sub_590342`

### 0x2A S_WHISPER_ROOM, S_WHISPER_ENABLE

handler `sub_479B60`  SENT

payload: none, pure trigger

calls `sub_4465E0` `sub_4538B0` `sub_4538B0`

### 0x2B S_WHISPER_DISABLE

handler `sub_479BD0`  SENT

payload: none, pure trigger

### 0x2D S_ROOM_ANNOUNCE, S_CHAT_MESSAGE, C_CREATE_ROOM_REQ, C_CHAT_MESSAGE

handler `sub_479630`  SENT

```
int32                                    -> [esp + 0x20]
wstring   NUL terminated utf16           -> [esp + 0x24]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x1c]
int32                                    -> [esp + 0xc]
```

calls `sub_408360` `sub_590342`

### 0x2E S_PLAYER_LEFT

handler `sub_479710`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4084B0`

### 0x30 S_ROOM_STATE, C_ROOM_STATE_REQ

handler `sub_479760`  SENT

```
int32                                    -> 0xbce220
```

### 0x31 S_POSITION, C_POSITION

handler `sub_479950`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 8]
```

calls `sub_407600`

### 0x32 S_ROOM_SLOT_ENABLED, S_POSITION_32, C_UNKNOWN_32

handler `sub_479C70`  SENT

```
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x10]
```

calls `sub_409F90`

### 0x33 S_GAME_STATE, C_ROOM_READY_TOGGLE

handler `sub_4799B0`  SENT

```
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x10]
```

calls `sub_40A040` `sub_409FB0`

### 0x34 S_FLAG_34

handler `sub_479A10`  SENT

payload: none, pure trigger

calls `sub_40A040`

### 0x35 S_SCORE, S_ROOM_TRACK_SELECT

handler `sub_479C20`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
```

calls `sub_4538B0` `sub_410360`

### 0x36 C_LAP_COMPLETE

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x37 C_ITEM_PICKUP

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x38 C_ITEM_HIT

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x39 S_FINISH, C_RACE_FINISH

handler `sub_479CB0`  SENT

payload: none, pure trigger

calls `sub_4538B0`

### 0x3A S_RESULTS

handler `sub_47AE00`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_402400`

### 0x3C S_RACE_END

handler `sub_47A5C0`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esi + 0x80e664]
int8                                     -> [esi + 0x80e659]
int32                                    -> [esi + 0x80e65c]
int32                                    -> [esi + 0x80e1b0]
```

calls `sub_48DEA0` `sub_499030` `sub_48B460` `sub_4815F0` `sub_448C90` `sub_43ED70`

### 0x3D S_ROOM_STATE_3D

handler `sub_47A6B0`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_498F60` `sub_4B0CD0` `sub_48B460` `sub_43ED70` `sub_4A9AC0`

### 0x3E S_PLAYER_JOIN

handler `sub_479D60`  SENT

```
int32                                    -> [esp + 0x14]
wstring   NUL terminated utf16           -> [esp + 0x766]
int32                                    -> [esp + 0x20]
int32                                    -> [esp + 0x1c]
blob 44   CHAR_REC                       -> [esp + 0x2c]
blob 56   KART_REC                       -> [esp + 0x58]
int32                                    -> [esp + 0x28]
blob 60   CUSTOM_REC                     -> [esp + 0x90]
```

calls `sub_450060` `sub_44F6F0` `sub_4641E0` `sub_451D50` `sub_48CBB0` `sub_4641E0` `sub_451D50` `sub_495280`

### 0x3F S_ROOM_INFO, C_JOIN_ROOM

handler `sub_47A050`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4952F0` `sub_48D010`

### 0x40 S_GAME_STATE_40, C_GAME_START, C_MOTION

handler `sub_47FD30`  SENT

```
int8                                     -> [esp + 0x68]
loop count times {
  int32                                  -> [esp + 0x1c]
  int16                                  -> [esp + 0x14]
  blob 8                                 -> [esp + 0x20]
  blob 8                                 -> [esp + 0x28]
  int8                                   -> [esp + 0x13]
  int16                                  -> [esp + 0x18]
}
loop count times {
  int32                                  -> [esp + 0x1c]
  int16                                  -> [esp + 0x14]
  blob 28   ITEM_REC                     -> [esp + 0x48]
}
```

calls `sub_487230` `sub_48DEA0` `sub_44E7F0` `sub_44E7F0` `sub_4A41F0` `sub_4A4260` `sub_48DEA0` `sub_4A41F0`

### 0x42 S_GAME_MODE

handler `sub_47A0A0`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4B6220` `sub_4B23C0`

### 0x43 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x44 S_GAME_UPDATE

handler `sub_47A0E0`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4B0CF0`

### 0x45 S_ITEM_USAGE, C_ITEM_USE

handler `sub_47A560`  ----

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x14]
```

calls `sub_4B4B00`

### 0x46 S_LARGE_GAME_STATE

handler `sub_47A760`  SENT

```
int32                                    -> 0x80e1b4
int32                                    -> [esp + 0x14]
loop count times {
  int32                                  -> [esp + 0x10]
  int32                                  -> [esp + 0x38]
  int32                                  -> [esp + 0x14]
  wstring   NUL terminated utf16         -> [esp + 0x4c2]
  int8                                   -> [esp + 0x4e1]
  int32                                  -> [esp + 0x30]
  int32                                  -> [esp + 0x28]
  int32                                  -> [esp + 0x20]
  int32                                  -> [esp + 0x3c]
  int32                                  -> [esp + 0x1c]
  int8                                   -> [esp + 0x4fa]
  int32                                  -> [esp + 0x34]
  int32                                  -> [esp + 0x24]
  int32                                  -> [esp + 0x2c]
}
```

calls `sub_4B62A0` `sub_4B6830` `sub_48DEA0` `sub_498F60` `sub_4B6850` `sub_4489F0` `sub_4489A0` `sub_4489F0`

### 0x47 S_PLAYER_ACTION

handler `sub_47A110`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x1c]
int32                                    -> [esp + 0x24]
```

calls `sub_48DEA0`

### 0x48 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x49 S_PLAYER_DATA

handler `sub_47AAD0`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
```

calls `sub_48DEA0`

### 0x4B S_GAME_DATA_4B

handler `sub_47A460`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x18]
```

calls `sub_48DEA0` `sub_4C6910` `sub_4CA1A0`

### 0x4D C_REQUEST_DATA

handler `sub_4790C0`  SENT

payload: none, pure trigger

calls `sub_590350` `sub_44ECD0` `sub_44E9C0` `sub_476B80` `sub_44E8E0` `sub_590342`

### 0x4E S_TIMESTAMP

handler `sub_479160`  SENT

payload: none, pure trigger

calls `sub_44ED50`

### 0x54 S_SERVER_REDIRECT

handler `sub_47AA00`  SENT

```
int32                                    -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x10]
int32                                    -> [esp + 0xc]
```

calls `sub_4774C0` `sub_43D7E0` `sub_4641E0` `sub_590342` `sub_483FF0` `sub_590342`

### 0x57 S_RACE_STATUS

handler `sub_47AB40`  SENT

```
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
```

calls `sub_48DEA0` `sub_4C59F0` `sub_4C8F60`

### 0x58 S_PLAYER_STATUS

handler `sub_47ABE0`  SENT

```
int32                                    -> [esp + 8]
int8                                     -> [esp + 0x10]
```

calls `sub_48B430` `sub_48B460`

### 0x5C S_ROOM_DATA_5C

handler `sub_47A500`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_4D1A10`

### 0x5F S_GAME_5F

handler `sub_47EE70`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
```

calls `sub_4B9F80` `sub_4B9F80`

### 0x62 S_TUTORIAL_FAIL, C_PRACTICE_START, S_ENTER_TUTORIAL

handler `sub_479580`  SENT

payload: none, pure trigger

calls `sub_4538B0`

### 0x63 S_CREATE_ROOM_ACK, S_CREATE_ROOM

handler `sub_47AC30`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4634C0`

### 0x64 S_ROOM_STATUS, C_QUICK_MATCH, C_TEAM_CHANGE

handler `sub_47ACD0`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
```

calls `sub_409EC0`

### 0x65 S_SPEED_UPDATE, C_RACE_GAUGE, S_RACE_GAUGE

handler `sub_47AD60`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_4ADB90`

### 0x66 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x68 S_SHOP_LOOKUP

handler `sub_47AE30`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esi + 0x1b1c2d4]
int32                                    -> [esi + 0x1b1c2d8]
int32                                    -> [esi + 0x1b1c2dc]
int32                                    -> [esi + 0x1b1c2b0]
```

calls `sub_48DEA0` `sub_4A41F0`

### 0x69 S_SHOP_ITEM

handler `sub_47AF00`  SENT

```
int32                                    -> [esp + 0x10]
int16                                    -> [esp + 0xc]
int8                                     -> [esp + 0x18]
```

calls `sub_48DEA0` `sub_495C30` `sub_495C30` `sub_495C30` `sub_4C4A10` `sub_4C3A60`

### 0x6A S_SHOP_UPDATE

handler `sub_47AFE0`  SENT

```
int32                                    -> [esp + 0xc]
int16                                    -> [esp + 0x14]
```

calls `sub_48DEA0` `sub_49E8A0`

### 0x6C S_SHOP_CHAT, C_ROOM_INVITE

handler `sub_47B030`  SENT

```
int32                                    -> 0xf33a18
wstring   NUL terminated utf16           -> 0xf33a1c
string    NUL terminated ascii           -> 0xf33a36
int32                                    -> 0xf33a58
int32                                    -> 0xf33a5c
wstring   NUL terminated utf16           -> 0xf33a60
blob ?                                   -> 0xf33a72
```

calls `sub_481D70` `sub_481D70` `sub_45BF60` `sub_481D70` `sub_481D70` `sub_481D70` `sub_481D70`

### 0x6D C_ROOM_INVITE_ANSWER

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x6E S_SHOP_CALL, C_INVITE_JOIN

handler `sub_47B190`  SENT

payload: none, pure trigger

calls `sub_481E20`

### 0x6F S_SHOP_RESPONSE, C_FRIEND_ADD

handler `sub_47B3C0`  SENT

```
int32                                    -> [esp + 0x64]
blob 44   CHAR_REC                       -> [esp + 0x30]
blob 36                                  -> [esp + 0xc]
blob ?                                   -> ?
blob 44   CHAR_REC                       -> [esp + 0x30]
blob 36                                  -> [esp + 0xc]
```

calls `sub_44EF90` `sub_590342` `sub_590342` `sub_44EF90` `sub_590342` `sub_44EE10` `sub_590342`

### 0x70 S_SHOP_EVENT, C_BUY_ITEM, C_FRIEND_REQ_ACCEPT

handler `sub_47B300`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x14]
```

calls `sub_44EF20`

### 0x71 C_BUY_ITEM_ALT, C_FRIEND_REQ_REJECT, C_SHOP_EXIT

handler `sub_47B300`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x14]
```

calls `sub_44EF20`

### 0x72 S_DATA_BLOCK, C_BUY_KART, C_USERINFO_BY_NAME

handler `sub_47B550`  SENT

```
blob 104                                 -> 0xf78f68
```

### 0x73 S_SLOT_UPDATE, C_SHOP_POLL, C_FRIEND_STATUS_POLL

handler `sub_47B570`  SENT

```
int32                                    -> [esp + 0x1c]
loop count times {
  blob 12                                -> [esp + 0xc]
}
```

calls `sub_44F030` `sub_44F030` `sub_44F050` `sub_44F050`

### 0x74 C_KART_ACT, C_FRIEND_DEL

handler `sub_47B620`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_44F0E0`

### 0x76 S_INVENTORY_LIST

handler `sub_47B1B0`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 44   CHAR_REC                     -> [esp + 0x14]
}
```

calls `sub_44EF70` `sub_44EF90` `sub_590342`

### 0x77 S_UPDATE_LIST

handler `sub_47B340`  ----

```
int32                                    -> [esp + 0x1c]
loop count times {
  int32                                  -> [esp + 0x10]
  int32                                  -> [esp + 0x14]
}
```

calls `sub_44F050`

### 0x78 S_ITEM_LIST

handler `sub_47B220`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 36                                -> [esp + 0x14]
}
```

calls `sub_44EDF0` `sub_44EE10` `sub_590342`

### 0x79 S_ITEM_LIST_B

handler `sub_47B290`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 32                                -> [esp + 0x14]
}
```

calls `sub_44F130` `sub_44F150` `sub_590342`

### 0x7A S_ITEM_ADD, C_KART_ALT, C_BLOCK_ADD

handler `sub_47B4F0`  SENT

```
int32                                    -> [esp + 0x34]
blob 32                                  -> [esp + 8]
```

calls `sub_44F150` `sub_590342`

### 0x7B S_INV_SLOT, C_BUY_PREMIUM, C_BLOCK_DEL

handler `sub_47B670`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_44F260`

### 0x7D S_NOTIFICATION, C_SMALLTALK_REQ

handler `sub_47B6B0`  SENT

```
int32                                    -> 0x1211a68
wstring   NUL terminated utf16           -> 0x1211a6c
```

calls `sub_476330`

### 0x7E C_SMALLTALK_ACCEPT

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x7F C_SMALLTALK_DECLINE

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x80 C_SMALLTALK_CLOSE

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x81 S_EXT_81, C_NOTE_SEND

handler `sub_47B8A0`  SENT

```
int32                                    -> [esp + 8]
```

### 0x82 S_EXT_82, S_EXT_DATA_130

handler `sub_47B710`  SENT

```
blob 396  REC_396                        -> [esp + 0xc]
```

calls `sub_451B70` `sub_590342`

### 0x83 S_EXT_DATA_131

handler `sub_47B7D0`  SENT

```
blob 396  REC_396                        -> [esp + 0xc]
```

calls `sub_451B70` `sub_451A20` `sub_590342`

### 0x84 S_EXT_DATA_132

handler `sub_47B8D0`  ----

```
int32                                    -> [esp + 8]
```

calls `sub_4519E0`

### 0x85 S_EXT_DATA_133

handler `sub_47B900`  ----

```
int32                                    -> [esp + 8]
```

calls `sub_451BC0`

### 0x86 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x87 S_EXT_87

handler `sub_47DC10`  ----

```
blob 188  MISSION_DEF                    -> [esp + 8]
```

calls `sub_450C10` `sub_590342`

### 0x88 S_EXT_88, S_EXT_DATA_136

handler `sub_47B930`  ----

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 8                                 -> [esp + 0x14]
}
```

calls `sub_450A20` `sub_450AB0`

### 0x8A S_EXT_DATA_138

handler `sub_47B990`  ----

```
blob 8                                   -> [esp + 8]
```

calls `sub_450AB0` `sub_44ED50`

### 0x8B 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x8C S_EQUIP_ITEM, S_EXT_DATA_140, C_MISSION_COMPLETE

handler `sub_47B9E0`  SENT

```
int32                                    -> [esp + 0x18]
blob 8                                   -> [esp + 0x1c]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
```

calls `sub_450BA0` `sub_450BA0` `sub_450CA0` `sub_43A1B0` `sub_590342`

### 0x8D C_LICENSE_PANEL_CLOSE

handler `sub_46F7A0`  SENT

payload: none, pure trigger

### 0x8F S_UI_STATE_24, C_MISSION_MENU_OPEN

handler `sub_47E950`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0`

### 0x90 S_UI_STATE_25, C_MISSION_START

handler `sub_47DF30`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_450CA0` `sub_4538B0` `sub_404410`

### 0x91 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x92 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x93 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x94 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0x95 S_LIST_D4

handler `sub_47BE00`  ----

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 212  REC_212                      -> [esp + 0x14]
}
```

calls `sub_450220` `sub_450240` `sub_590342`

### 0x96 S_GIFT_INBOX_RECV

handler `sub_47BD80`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 212  REC_212                      -> [esp + 0x14]
}
```

calls `sub_450220` `sub_450240` `sub_590342`

### 0x97 S_GIFT_INBOX_SENT

handler `sub_47BE00`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 212  REC_212                      -> [esp + 0x14]
}
```

calls `sub_450220` `sub_450240` `sub_590342`

### 0x98 S_GIFT, C_SHOP_GIFT

handler `sub_47BEF0`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0xc]
blob 212  REC_212                        -> [esp + 0x14]
```

calls `sub_4641E0` `sub_590342`

### 0x99 S_SINGLE_D4

handler `sub_47BE80`  ----

```
blob 212  REC_212                        -> [esp + 8]
```

calls `sub_450240` `sub_4641E0` `sub_590342`

### 0x9A S_GIFT_REWARD_GRANT, S_ITEM_SWITCH, C_USE_ITEM

handler `sub_47BF80`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x10]
```

calls `sub_450430` `sub_590342`

### 0x9B S_GIFT_DELETE, S_REMOVE_ITEM_9B

handler `sub_47C270`  ----

```
int32                                    -> [esp + 0xc]
```

calls `sub_4503E0`

### 0x9C S_GIFT_MARK_READ, C_UPGRADE_VEHICLE

handler `sub_47C2A0`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_450320`

### 0x9D S_ADD_VEHICLE

handler `sub_47C2D0`  SENT

```
blob 44   CHAR_REC                       -> [esp + 8]
```

calls `sub_44FCB0` `sub_4641E0`

### 0x9E S_ADD_ITEM

handler `sub_47C320`  SENT

```
blob 56   KART_REC                       -> [esp + 8]
```

calls `sub_44F2D0` `sub_4641E0`

### 0x9F S_ADD_ACCESSORY

handler `sub_47C370`  SENT

```
blob 28   ITEM_REC                       -> [esp + 8]
```

calls `sub_450D20` `sub_4641E0`

### 0xA0 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0xA1 S_MISSION_COMPLETE, C_PROP_HIT

handler `sub_47C930`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_4BEC80`

### 0xA2 S_MISSION_LIST

handler `sub_47C960`  SENT

```
int32                                    -> [esp + 0x20]
loop count times {
  blob 12                                -> [esp + 0x10]
}
```

calls `sub_450920`

### 0xA3 S_REWARD_CLAIM, C_LICENSE_COMPLETE, C_CLAIM_REWARD

handler `sub_47C3C0`  SENT

```
int32                                    -> [esp + 0x20]
int32                                    -> [esp + 0x24]
blob 12                                  -> [esp + 0x30]
blob 8                                   -> [esp + 0x3c]
int32                                    -> [esp + 0x1c]
```

calls `sub_4509B0` `sub_450920` `sub_4532B0` `sub_451140` `sub_462580` `sub_590342`

### 0xA4 

handler `sub_47C770`  ----

```
int8                                     -> [esp + 0xc]
```

### 0xA5 

handler `sub_47C830`  ----

```
int8                                     -> [esp + 0x44]
loop count times {
  int32                                  -> [esp + 0x10]
  blob 8                                 -> [esp + 0x14]
}
```

calls `sub_48DEA0` `sub_44E7F0` `sub_4A41F0` `sub_4A4260`

### 0xA7 S_SESSION_CONFIRM

handler `sub_479080`  SENT

```
int32                                    -> [esi + 0x80e1a8]
blob 1224 REC_1224                       -> [esi + 0x80e1b8]
```

### 0xAA S_PLAYER_PREVIEW, S_GACHA_ROLL_RESULT, C_TUTORIAL_COMPLETE

handler `sub_47CBA0`  SENT

```
loop count times {
  int32                                  -> 0xc1a9c8
  wstring   NUL terminated utf16         -> 0xc1a940
  int32                                  -> 0xc1a938
  int32                                  -> 0xc1a93c
  blob 44   CHAR_REC                     -> 0xc1a95c
  blob 56   KART_REC                     -> 0xc1a988
}
```

calls `sub_4538B0`

### 0xAB S_GACHA_BANNER_HEADER, C_LICENSE_TEST

handler `sub_47C9C0`  SENT

```
int32                                    -> [esi + 0x8ccdbc]
```

### 0xAC S_GACHA_BANNER_ENTRY, C_LICENSE_RESULT

handler `sub_47C9F0`  SENT

```
int32                                    -> [esp + 0x14]
blob 176  REC_176                        -> [edx + esi + 0x882f9c]
int32                                    -> [esp + 0x10]
loop count times {
  blob 176  REC_176                      -> [esp + 0x18]
}
```

calls `sub_452290` `sub_4522B0` `sub_590342`

### 0xAD 

handler `sub_47CAE0`  ----

payload: none, pure trigger

calls `sub_4538B0` `sub_458510`

### 0xAE 

handler `sub_47CB00`  ----

```
int32                                    -> [esi + 0x1bc08e8]
```

### 0xAF 

handler `sub_47CB30`  ----

```
int32                                    -> [esp + 0x14]
loop count times {
  blob 28   ITEM_REC                     -> [ecx + esi + 0x1b1c7e4]
}
```

### 0xB0 C_ADD_FRIEND

handler `sub_47CC20`  SENT

```
int32                                    -> [esp + 0xc]
blob 176  REC_176                        -> 0xef1b48
int32                                    -> [esp + 0x14]
loop count times {
  blob 176  REC_176                      -> [ebx]
}
```

### 0xB4 S_SYSTEM_MESSAGE, C_LOBBY_CHAT

handler `sub_47CD60`  SENT

```
int32                                    -> [esp + 0xc]
wstring   NUL terminated utf16           -> [esp + 0x10]
wstring   NUL terminated utf16           -> ?
int32                                    -> [esp + 8]
```

calls `sub_590350` `sub_590342`

### 0xB5 S_PLAYER_COMPARISON, C_WHISPER_SEND

handler `sub_47CF80`  SENT

```
blob 1224 REC_1224                       -> [esp + 0x10]
blob 1224 REC_1224                       -> [esp + 0x8d8]
wstring   NUL terminated utf16           -> [esp + 0x6d4]
```

calls `sub_466950` `sub_467790` `sub_445C30` `sub_4476C0` `sub_46AFA0` `sub_590342`

### 0xB6 S_DISPLAY_TEXT

handler `sub_47AC60`  SENT

```
wstring   NUL terminated utf16           -> [esp + 4]
int32                                    -> [esp + 4]
```

calls `sub_590350` `sub_43D690` `sub_406FF0` `sub_590342`

### 0xB7 S_INVENTORY_UPDATE, C_GARAGE_BUY, C_BUY, C_SHOP_BUY

handler `sub_484F50`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x10]
```

calls `sub_4538B0` `sub_4641E0` `sub_590342`

### 0xB8 S_REMOVE_ITEM, S_INVENTORY_REMOVE, C_GARAGE_DELETE, C_SELL

handler `sub_484690`  SENT

```
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0xc]
```

calls `sub_4538B0`

### 0xB9 S_INVENTORY_SLOT, C_GARAGE_INSTALL

handler `sub_484770`  SENT

```
int32                                    -> [esp + 0xc]
```

calls `sub_4538B0`

### 0xBA S_INVENTORY_OP, C_GARAGE_REMOVE

handler `sub_484B10`  SENT

```
int32                                    -> [esp + 0x10]
blob 28   ITEM_REC                       -> [esp + 0x14]
blob 28   ITEM_REC                       -> [esp + 0x14]
blob 28   ITEM_REC                       -> [esp + 0x14]
```

calls `sub_4538B0` `sub_451690` `sub_42A050` `sub_4510C0` `sub_44FE30` `sub_450060` `sub_44F450` `sub_44F6F0`

### 0xBB 

handler `sub_46F7A0`  ----

payload: none, pure trigger

### 0xBC C_DRIFT_START

handler `sub_484D90`  ----

```
int32                                    -> [esi + 0x80e668]
int32                                    -> [esi + 0x80e66c]
int32                                    -> [esp + 0x10]
loop count times {
  blob 48   ROOM_OBJ                     -> [esp + 0x18]
}
blob 44   CHAR_REC                       -> [esp + 0x14]
blob 56   KART_REC                       -> [esp + 0x14]
```

calls `sub_452830` `sub_44FE30` `sub_44F450`

### 0xBE S_RACE_INIT, C_BOOST_ACTIVATE

handler `sub_478B50`  SENT

payload: none, pure trigger

calls `sub_4512E0` `sub_44F4F0` `sub_450730` `sub_450F20` `sub_453000` `sub_452E90`

### 0xBF S_DRIVER_CATALOG, S_RACE_PLAYER_1, C_BOOST_END

handler `sub_47F390`  SENT

```
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int32                                    -> [esp + 0x34]
string    NUL terminated ascii           -> [esp + 0x34]
blob 20                                  -> [esp + 0x5c]
string    NUL terminated ascii           -> [esp + 0x6c]
string    NUL terminated ascii           -> [esp + 0x8d]
int32                                    -> [esp + 0x10]
loop count times {
  blob 16                                -> [esp + 0x14]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_44FED0` `sub_451CC0` `sub_590342`

### 0xC0 S_KART_CATALOG, S_RACE_PLAYER_2, C_GHOST_MENU

handler `sub_47F4F0`  SENT

```
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int8                                     -> [esp + 0x34]
int32                                    -> [esp + 0x38]
int32                                    -> [esp + 0x3c]
int32                                    -> [esp + 0x40]
int32                                    -> [esp + 0x44]
string    NUL terminated ascii           -> [esp + 0x44]
string    NUL terminated ascii           -> [esp + 0x65]
string    NUL terminated ascii           -> [esp + 0x86]
blob 32                                  -> [esp + 0xac]
blob 68                                  -> [esp + 0xcc]
loop count times {
  blob 8                                 -> [esp + 0x154]
}
int32                                    -> [esp + 0x14]
loop count times {
  blob 16                                -> [esp + 0x18]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_44F510` `sub_451CC0` `sub_590342`

### 0xC1 S_ITEM_CATALOG, S_RACE_PLAYER_3, C_GHOST_SELECT_MAP

handler `sub_47F6B0`  ----

```
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int32                                    -> [esp + 0x34]
string    NUL terminated ascii           -> [esp + 0x34]
string    NUL terminated ascii           -> [esp + 0x55]
string    NUL terminated ascii           -> [esp + 0x76]
int32                                    -> [esp + 0x10]
loop count times {
  blob 16                                -> [esp + 0x14]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_450750` `sub_451CC0` `sub_590342`

### 0xC2 S_PART_CATALOG, S_RACE_PLAYER_4, C_GHOST_START

handler `sub_47F800`  SENT

```
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int32                                    -> [esp + 0x34]
string    NUL terminated ascii           -> [esp + 0x34]
int32                                    -> [esp + 0x5c]
int32                                    -> [esp + 0x60]
int32                                    -> [esp + 0x64]
string    NUL terminated ascii           -> [esp + 0x64]
string    NUL terminated ascii           -> [esp + 0x85]
loop count times {
  blob 8                                 -> [esp + 0xa8]
}
int32                                    -> [esp + 0x14]
loop count times {
  blob 16                                -> [esp + 0x18]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_450F40` `sub_451CC0` `sub_590342`

### 0xC3 S_RACE_PLAYER_5, C_GHOST_COMPLETE

handler `sub_47F990`  ----

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
string    NUL terminated ascii           -> [esp + 0x14]
int32                                    -> [esp + 0x3c]
int32                                    -> [esp + 0x40]
int32                                    -> [esp + 0x44]
int32                                    -> [esp + 0x48]
int32                                    -> [esp + 0x4c]
int32                                    -> [esp + 0x50]
int32                                    -> [esp + 0x54]
int32                                    -> [esp + 0x58]
int32                                    -> [esp + 0x5c]
int32                                    -> [esp + 0x60]
int32                                    -> [esp + 0x64]
int32                                    -> [esp + 0x68]
int32                                    -> [esp + 0x6c]
int32                                    -> [esp + 0x70]
string    NUL terminated ascii           -> [esp + 0x70]
```

calls `sub_453020` `sub_590342`

### 0xC4 S_RACE_DATA, C_GHOST_SAVE

handler `sub_478C40`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x31]
```

calls `sub_452EB0` `sub_590342`

### 0xC5 S_RACE_PLAYER_6, C_GHOST_LIST

handler `sub_47FAE0`  ----

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x10]
int32                                    -> [esp + 0x38]
int32                                    -> [esp + 0x3c]
int32                                    -> [esp + 0x40]
int32                                    -> [esp + 0x44]
int32                                    -> [esp + 0x48]
int32                                    -> [esp + 0x4c]
int32                                    -> [esp + 0x50]
int32                                    -> [esp + 0x54]
int32                                    -> [esp + 0x58]
int32                                    -> [esp + 0x5c]
int32                                    -> [esp + 0x60]
int32                                    -> [esp + 0x64]
int32                                    -> [esp + 0x68]
string    NUL terminated ascii           -> [esp + 0x68]
string    NUL terminated ascii           -> [esp + 0x89]
```

calls `sub_453270` `sub_590342`

### 0xC6 S_RACE_DATA_2, C_GHOST_DOWNLOAD

handler `sub_478CB0`  ----

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x1c]
int32                                    -> [esp + 0x20]
int32                                    -> [esp + 0x24]
```

calls `sub_451DC0`

### 0xC9 C_SCENARIO_STAGE

handler `sub_47CD20`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_49A730`

### 0xCD S_FRIEND_ONLINE_FLIP, C_SCENARIO_CHAPTERS

handler `sub_47D1C0`  SENT

```
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x10]
```

calls `sub_48DEA0` `sub_4CB160` `sub_4CACE0`

### 0xCE S_GAME_MESSAGE

handler `sub_47D250`  SENT

```
string    NUL terminated ascii           -> [esp + 0x10]
wstring   NUL terminated utf16           -> ?
int32                                    -> [esp + 0xc]
```

calls `sub_4E1B70` `sub_43D690` `sub_406FF0` `sub_590342`

### 0xCF S_FRIEND_STATS_UPDATE, S_PLAYER_UPDATE_CF

handler `sub_47D4A0`  SENT

```
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_49A860`

### 0xD0 C_CLIENT_INFO

handler `sub_47D520`  SENT

```
int32                                    -> 0x80e660
```

### 0xD9 S_PLAYER_FULL_UPDATE

handler `sub_47D540`  SENT

```
int32                                    -> [esp + 8]
blob 44   CHAR_REC                       -> [esp + 0xc]
blob 56   KART_REC                       -> [esp + 0x38]
blob 60   CUSTOM_REC                     -> [esp + 0x70]
```

calls `sub_40CC90`

### 0xED S_ENTITY_UPDATE, C_GACHA_ROLL

handler `sub_47D5E0`  SENT

```
blob 20                                  -> [esp + 0x18]
blob 28   ITEM_REC                       -> [esp + 0x2c]
```

calls `sub_456A40` `sub_4516D0` `sub_590342`

### 0xEE S_FRIEND_BLOB, S_ENTITY_POSITION

handler `sub_47D880`  SENT

```
int32                                    -> [esp + 0x20]
blob 16                                  -> [esp + 0xc]
```

calls `sub_48DEA0` `sub_451D30` `sub_44F450`

### 0xF0 S_FRIEND_RECORD, S_ENTITY_REMOVE

handler `sub_47D930`  SENT

```
int32                                    -> [esp + 8]
```

calls `sub_48DEA0` `sub_498F40`

### 0xF1 S_ENTITY_CLEAR

handler `sub_47D970`  SENT

```
blob 36                                  -> [esp + 4]
```

calls `sub_451C10`

### 0xF3 S_BLOCK_LIST_REFRESH, S_ENTITY_DATA_243

handler `sub_47DAA0`  SENT

```
blob 156                                 -> [esp + 8]
```

calls `sub_452DA0` `sub_590342`

### 0xF4 S_ENTITY_DATA_244

handler `sub_47DB00`  ----

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 8                                 -> [esp + 0x14]
}
```

calls `sub_452BB0` `sub_452C40`

### 0xF5 S_ENTITY_DATA_245

handler `sub_47DD10`  ----

```
int32                                    -> [esp + 0x74]
int32                                    -> [esp + 0x14]
```

calls `sub_452E30` `sub_450060` `sub_44FFA0` `sub_450060` `sub_4E1B70` `sub_451D30` `sub_451C30` `sub_44F6F0`

### 0xF6 S_ENTITY_DATA_246

handler `sub_47DC70`  ----

```
int32                                    -> [esi + 0x1bc08e8]
```

### 0xF7 S_ENTITY_DATA_247

handler `sub_47DCA0`  ----

```
int32                                    -> [esp + 0x14]
loop count times {
  blob 28   ITEM_REC                     -> [ecx + esi + 0x1b1c7e4]
}
```

### 0xF8 S_ENTITY_DATA_248

handler `sub_47DFA0`  ----

```
int32                                    -> [esp + 0x18]
int8                                     -> 0xc70a34
blob 8                                   -> [esp + 0x1c]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x10]
int32                                    -> [esp + 0x14]
```

calls `sub_452D30` `sub_452D30` `sub_452E30` `sub_590342`

### 0xF9 S_ENTITY_DATA_249

handler `sub_47E350`  ----

```
blob 8                                   -> [esp + 8]
```

calls `sub_452C40` `sub_44ED50`

### 0xFB S_BUDDY_LIST_ENTRY, S_ENTITY_DATA_251

handler `sub_47DB60`  SENT

```
blob 112                                 -> [esp + 8]
```

calls `sub_452180` `sub_590342`

### 0xFC S_BUDDY_STATUS_LIST, S_ENTITY_DATA_252

handler `sub_47DBB0`  SENT

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 12                                -> [esp + 0x14]
}
```

calls `sub_451EA0` `sub_451FB0`

### 0xFE S_ENTITY_DATA_254, C_LAUNCHER_LOGIN, S_LAUNCHER_RESPONSE

handler `sub_47F270`  SENT

```
int32                                    -> [esp + 0x18]
```

calls `sub_451FB0`

### 0x100 S_ENTITY_DATA_256

handler `sub_47F2C0`  ----

```
int32                                    -> [esp + 0xc]
```

calls `sub_4520D0`

### 0x101 S_ENTITY_DATA_257

handler `sub_47F2F0`  ----

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_452110`

### 0x102 S_ENTITY_DATA_258

handler `sub_47F340`  ----

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_452110`

### 0x103 S_ENTITY_DATA_259

handler `sub_4800D0`  ----

```
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
string    NUL terminated ascii           -> [esp + 0x30]
string    NUL terminated ascii           -> [esp + 0x51]
string    NUL terminated ascii           -> [esp + 0x72]
int32                                    -> [esp + 0x10]
loop count times {
  blob 16                                -> [esp + 0x14]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_451750` `sub_451CC0` `sub_590342`

### 0x104 S_ENTITY_DATA_260

handler `sub_47E3A0`  ----

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 28   ITEM_REC                     -> [esp + 0x14]
}
```

calls `sub_451510` `sub_451530`

### 0x107 S_ENTITY_DATA_263

handler `sub_47E400`  ----

```
int32                                    -> [esp + 0x10]
loop count times {
  blob 52                                -> [esp + 0x18]
}
```

calls `sub_4500C0` `sub_4500E0` `sub_44F490` `sub_44F490` `sub_590342`

### 0x108 S_ENTITY_DATA_264, S_CAR_PART_CATALOG

handler `sub_480210`  SENT

```
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int32                                    -> [esp + 0x34]
int32                                    -> [esp + 0x38]
string    NUL terminated ascii           -> [esp + 0x38]
string    NUL terminated ascii           -> [esp + 0x59]
string    NUL terminated ascii           -> [esp + 0x7a]
blob 68                                  -> [esp + 0xa0]
loop count times {
  blob 8                                 -> [esp + 0xe0]
}
blob 12                                  -> [esp + 0xf4]
int32                                    -> [esp + 0x14]
loop count times {
  blob 16                                -> [esp + 0x18]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_44F9F0` `sub_451CC0` `sub_590342`

### 0x109 S_ENTITY_DATA_265

handler `sub_47E4C0`  ----

```
blob 132                                 -> [esp + 8]
```

calls `sub_44F760`

### 0x10A S_ENTITY_DATA_266, C_CARCRAFT_OPEN

handler `sub_47E500`  SENT

payload: none, pure trigger

calls `sub_484010` `sub_404410` `sub_4538B0`

### 0x10B S_ENTITY_DATA_267, C_CARCRAFT_SAVE

handler `sub_47E530`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x1c]
blob 32                                  -> [esp + 0x20]
int32                                    -> [esp + 0x10]
loop count times {
  blob 132                               -> [esp + 0x40]
}
```

calls `sub_4501D0` `sub_44F940` `sub_4641E0`

### 0x10C S_ENTITY_DATA_268

handler `sub_47FF70`  ----

```
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x28]
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x30]
int32                                    -> [esp + 0x34]
int32                                    -> [esp + 0x38]
string    NUL terminated ascii           -> [esp + 0x38]
string    NUL terminated ascii           -> [esp + 0x59]
string    NUL terminated ascii           -> [esp + 0x7a]
int32                                    -> [esp + 0x10]
loop count times {
  blob 16                                -> [esp + 0x14]
}
```

calls `sub_451D50` `sub_451CD0` `sub_451CF0` `sub_4528E0` `sub_451CC0` `sub_590342`

### 0x10D S_ENTITY_DATA_269

handler `sub_47D9A0`  ----

```
blob 48   ROOM_OBJ                       -> [esp + 8]
```

calls `sub_4523A0`

### 0x10E S_ENTITY_DATA_270, C_ROOMCRAFT_OPEN

handler `sub_47D9D0`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0`

### 0x10F S_ENTITY_DATA_271, C_ROOMCRAFT_SAVE

handler `sub_47DA00`  SENT

```
int32                                    -> [esp + 0x44]
loop count times {
  blob 48   ROOM_OBJ                     -> [esp + 0x10]
}
```

calls `sub_452830` `sub_4641E0`

### 0x112 S_ENTITY_DATA_274, C_EXTEND

handler `sub_484EB0`  SENT

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
blob 48   ROOM_OBJ                       -> [esp + 0x18]
```

calls `sub_4538B0` `sub_452830` `sub_4641E0`

### 0x114 S_ENTITY_DATA_276

handler `sub_47E620`  ----

```
int32                                    -> [esp + 0x1c]
string    NUL terminated ascii           -> [esp + 4]
```

calls `sub_4501D0` `sub_590342`

### 0x115 S_ENTITY_DATA_277

handler `sub_47E680`  ----

```
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0xc]
```

calls `sub_451690`

### 0x116 S_ENTITY_DATA_278

handler `sub_47E6D0`  ----

```
int32                                    -> [esp + 8]
wstring   NUL terminated utf16           -> [esp + 0x48e]
int8                                     -> [esp + 0x4c5]
```

calls `sub_40F550` `sub_590342`

### 0x117 S_ENTITY_DATA_279

handler `sub_47E750`  ----

```
int32                                    -> [esp + 8]
```

calls `sub_40F5F0`

### 0x118 S_ENTITY_DATA_280, C_ROOM_PASSWORD

handler `sub_47E780`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 8]
int32                                    -> [esp + 0x14]
```

calls `sub_4076B0`

### 0x119 S_ENTITY_DATA_281

handler `sub_47E800`  SENT

```
int32                                    -> [esp + 0xc]
int32                                    -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x10]
string    NUL terminated ascii           -> [esp + 0x31]
string    NUL terminated ascii           -> [esp + 0x52]
```

calls `sub_451300` `sub_590342`

### 0x11A S_ENTITY_DATA_282

handler `sub_47E880`  SENT

```
blob 8                                   -> [esp + 8]
```

calls `sub_451140`

### 0x11B S_ENTITY_DATA_283

handler `sub_47E880`  SENT

```
blob 8                                   -> [esp + 8]
```

calls `sub_451140`

### 0x11C S_ENTITY_DATA_284, C_STAGE22_OPEN

handler `sub_47E8E0`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0` `sub_45CC70`

### 0x11D S_ENTITY_DATA_285, C_STAGE23_OPEN

handler `sub_47E980`  SENT

payload: none, pure trigger

calls `sub_404410` `sub_4538B0`

### 0x11E S_ENTITY_DATA_286

handler `sub_47E9B0`  ----

```
string    NUL terminated ascii           -> [esp + 4]
int32                                    -> [esp + 4]
```

calls `sub_405EB0` `sub_590342`

### 0x11F S_ENTITY_DATA_287

handler `sub_47EA10`  ----

```
int32                                    -> [esp + 8]
```

calls `sub_4B1190` `sub_4B0C70` `sub_49A920` `sub_43ED70`

### 0x120 S_ENTITY_DATA_288

handler `sub_47EA70`  ----

```
int32                                    -> [ebx + 0x8cceac]
loop count times {
  blob 12                                -> 0x8cceb0
}
```

### 0x122 S_ENTITY_DATA_290

handler `sub_47EB70`  ----

payload: none, pure trigger

calls `sub_472770` `sub_4A05C0` `sub_43ED70`

### 0x123 S_ENTITY_DATA_291, C_TITLE_EQUIP

handler `sub_47EAE0`  SENT

```
int32                                    -> [esp + 8]
```

### 0x124 S_ENTITY_DATA_292

handler `sub_47EB10`  ----

```
blob 52                                  -> [esp + 8]
```

calls `sub_4501D0` `sub_590342`

### 0x125 S_ENTITY_DATA_293, C_OVERHEAT_STATE

handler `sub_47EBB0`  SENT

```
int32                                    -> [esp + 8]
int8                                     -> [esp + 0x10]
```

calls `sub_48DEA0`

### 0x126 S_ENTITY_DATA_294

handler `sub_47EC00`  ----

```
int32                                    -> [esp + 0x14]
wstring   NUL terminated utf16           -> [esp + 0x18]
string    NUL terminated ascii           -> ?
int32                                    -> [esp + 0x10]
```

calls `sub_4E1B70` `sub_4016E0` `sub_590342` `sub_4088C0` `sub_590342` `sub_4088C0` `sub_590342`

### 0x12E S_ENTITY_DATA_302

handler `sub_47ED40`  ----

```
int32                                    -> 0xb23398
loop count times {
  blob 16                                -> 0xb2339c
}
```

calls `sub_403DC0`

### 0x12F S_ENTITY_DATA_303, C_RANDOM_INVITE

handler `sub_47ED90`  SENT

```
int32                                    -> [esp + 8]
wstring   NUL terminated utf16           -> 0xf33a1c
int32                                    -> 0xf33a5c
int32                                    -> [esp + 0xc]
wstring   NUL terminated utf16           -> 0xf33a60
```

calls `sub_46C1E0` `sub_45BF60`

### 0x131 S_ENTITY_DATA_305

handler `sub_478E80`  ----

```
loop count times {
  int32                                  -> [ecx + 0x8ccf40]
}
```

### 0x132 S_ENTITY_DATA_306, C_USERLIST_PAGE

handler `sub_47EE30`  SENT

```
int32                                    -> 0xc23808
int32                                    -> 0xc2380c
blob 288                                 -> 0xc236e8
```

calls `sub_453390`

### 0x135 S_ENTITY_DATA_309

handler `sub_47EEF0`  ----

```
int32                                    -> [esp + 0x2c]
int32                                    -> [esp + 0x1c]
int32                                    -> [esp + 0x14]
int32                                    -> [esp + 0x18]
int32                                    -> [esp + 0x20]
int32                                    -> [esp + 0x24]
int32                                    -> [esp + 0x28]
```

calls `sub_473390` `sub_590342` `sub_473390` `sub_590342` `sub_590342` `sub_590342`

