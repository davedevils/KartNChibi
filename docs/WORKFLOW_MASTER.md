# KnC Complete Game Workflow

**Source**: `DevClient/KnC-new.exe.c` - Reverse engineered from client

## Table of Contents

1. [Game States](#game-states)
2. [Startup & Connection](#1-startup--connection)
3. [Authentication](#2-authentication)
4. [Character Creation](#3-character-creation)
5. [Channel Selection](#4-channel-selection)
6. [Main Menu](#5-main-menu)
7. [Garage](#6-garage)
8. [Shop](#7-shop)
9. [Lobby](#8-lobby)
10. [Room](#9-room)
11. [Race](#10-race)
12. [Tutorial/License](#11-tutoriallicense)
13. [Packet Reference](#packet-reference)

---

## Game States

Variable: `dword_B23610` (set by `sub_404410`)

| State | Name | Description | Triggered By |
|-------|------|-------------|--------------|
| 0 | DISCONNECTED | Not connected | - |
| 1 | CONNECTING | Connecting to server | Client startup |
| 2 | CONNECTED | Socket connected | Connection success |
| 3 | AUTHENTICATING | Sending login | User login |
| 4 | CHANNEL_SELECT | Channel list | CMD 0x0E |
| 5 | MAIN_MENU | Blue menu | CMD 0x11 |
| 6 | GARAGE | Vehicle garage | CMD 0x0F |
| 7 | SHOP | Item shop | CMD 0x10 |
| 8 | LOBBY | Room list/lobby | CMD 0x12 (flag=0x00) |
| 9 | ROOM | Inside room | CMD 0x12 (flag=0x01) |
| 10 | LOADING | Loading race | Race start |
| 11 | RACING | In race | Race loaded |
| 12 | RESULTS | Race results | Race end |
| 13 | TUTORIAL | Tutorial mode | CMD 0x62 fail |
| 14 | TUTORIAL_MENU | Tutorial selection | CMD 0x0E special |

### Critical State Variable

`dword_F727F4` (offset +12 from `byte_F727E8`)

| Value | Meaning | Behavior |
|-------|---------|----------|
| 0 | Not confirmed | Client shows MSG_CONNECT_FAIL |
| 1 | Connection OK | UI can proceed |
| 2 | Force disconnect | All UI handlers return immediately |

**CRITICAL**: Server MUST send CMD 0x02 with code=1 to set this to 1!

---

## 1. Startup & Connection

```
┌─────────────────────────────────────────────────────────────────────┐
│                         STARTUP FLOW                                │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ═══ TCP Connect to port 50017 ═══►    │
     │                                         │
     │  ◄════ CMD 0x0A (38 bytes) ════════    │  Connection OK
     │                                         │
     │  ════ CMD 0xA6 (heartbeat) ════════►   │  Every ~1000ms
     │  ◄════ CMD 0x12 (heartbeat resp) ══    │
     │                                         │
     │  ════ CMD 0xFA (full state) ═══════►   │  Initial state
     │  ◄════ CMD 0x8E (ack) ═════════════    │
     │                                         │
```

### CMD 0x0A - Connection OK (Server → Client)
- **Handler**: `sub_47D3B0` @ line 219074
- **Payload**: 38 bytes (content unknown, possibly server info)
- **Purpose**: First packet after TCP connect, confirms server is ready

### CMD 0xA6 - Heartbeat (Client → Server)
- **Payload**: 4 bytes (timestamp)
- **Interval**: ~1000ms minimum
- **Purpose**: Keep connection alive

### CMD 0x12 - Heartbeat Response (Server → Client)
- **Handler**: `sub_4795A0` @ line 220232
- **Flag**: 0x01 for heartbeat (0x00 for lobby transition)
- **Payload**: Variable

### CMD 0xFA - Full State (Client → Server)
- **Payload**: Complex state dump
- **Purpose**: Sync client state with server

### CMD 0x8E - Init Response/Ack (Server → Client)
- **Handler**: `sub_47DF30` @ line 219334
- **Payload**: Variable
- **Purpose**: Acknowledge full state

---

## 2. Authentication

```
┌─────────────────────────────────────────────────────────────────────┐
│                      AUTHENTICATION FLOW                            │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ════ CMD 0x07 (login request) ════►   │
     │       [version:string "178"]            │
     │       [action:4 = 4]                    │
     │       [username:wstring]                │
     │       [password:wstring]                │
     │                                         │
     │           ┌─── SUCCESS ───┐             │
     │           │               │             │
     │  ◄════ CMD 0x07 ═════════════════════  │  Session info
     │       [accountId:4]                     │
     │       [PlayerInfo:1224]                 │
     │                                         │
     │  ◄════ CMD 0x8F (ack) ═══════════════  │
     │                                         │
     │  ◄════ CMD 0x02 ═════════════════════  │  CRITICAL!
     │       [message:wstring ""]              │
     │       [code:4 = 1]  ──► dword_F727F4=1 │
     │                                         │
     │           └─── FAILURE ───┘             │
     │                                         │
     │  ◄════ CMD 0x01 ═════════════════════  │  Login error
     │       [message:string]                  │
     │       [code:4]                          │
     │                                         │
```

### CMD 0x07 - Login Request (Client → Server)
- **Payload**:
  - `[version:string]` - "178\0" (4 bytes)
  - `[action:int32]` - 4 = login
  - `[username:wstring]` - UTF-16LE null-terminated
  - `[password:wstring]` - UTF-16LE null-terminated

### CMD 0x07 - Session Info (Server → Client)
- **Handler**: `sub_479020` @ line 219945
- **Payload**:
  - `[accountId:int32]` - 4 bytes
  - `[PlayerInfo:1224]` - 0x4C8 bytes

### CMD 0x01 - Login Error (Server → Client)
- **Handler**: `sub_478DA0` @ line 219814
- **Payload**:
  - `[message:string]` - ASCII null-terminated
  - `[code:int32]` - Error code

### Error Messages
| Message | Description |
|---------|-------------|
| MSG_SERVER_NOT_READY | Server starting up |
| MSG_DB_ACCESS_FAIL | Database error |
| MSG_REINPUT_IDPASS | Wrong credentials |
| MSG_INVALID_ID | User not found |

### CMD 0x02 - Display Message / Connection Confirm (Server → Client)
- **Handler**: `sub_478D40`
- **Payload**:
  - `[message:wstring]` - Can be empty
  - `[code:int32]` - **MUST BE 1** to confirm connection!

---

## 3. Character Creation

```
┌─────────────────────────────────────────────────────────────────────┐
│                   CHARACTER CREATION FLOW                           │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  [If no character exists for account]   │
     │                                         │
     │  ◄════ CMD 0x03 (empty) ══════════════  │  Trigger UI
     │                                         │
     │  [Shows nickname input dialog]          │
     │  [User enters: "MyName"]                │
     │  [User clicks OK]                       │
     │                                         │
     │  ════ CMD 0x04 ═══════════════════════► │  Create request
     │       [driverType:4]                    │
     │       [nickname:wstring]                │
     │                                         │
     │           ┌─── SUCCESS ───┐             │
     │           │               │             │
     │  ◄════ CMD 0x04 ═════════════════════   │  Created!
     │       [result:4 = 0]                    │
     │       [nickname:wstring]                │
     │       [VehicleData:44]                  │
     │       [ItemData:56]                     │
     │                                         │
     │           └─── FAILURE ───┘             │
     │                                         │
     │  ◄════ CMD 0x04 ═════════════════════   │  Error
     │       [result:4 = 1/2/3]                │
     │                                         │
```

### CMD 0x03 - Trigger Character Creation (Server → Client)
- **Handler**: `sub_479220` @ line 220061
- **Payload**: Empty (0 bytes)
- **Purpose**: Opens character creation UI

### CMD 0x04 - Create Character Request (Client → Server)
- **Sender**: `sub_4805D0` @ line 225111
- **Payload**:
  - `[driverType:int32]` - Selected character model
  - `[nickname:wstring]` - UTF-16LE, max 10 chars

### CMD 0x04 - Create Character Response (Server → Client)
- **Handler**: `sub_479230` @ line 220068
- **Payload** (success):
  - `[result:int32]` - 0 = success
  - `[nickname:wstring]` - Confirmed name
  - `[VehicleData:44]` - Starting vehicle
  - `[ItemData:56]` - Starting item
- **Payload** (error):
  - `[result:int32]` - 1/2/3 = error

### Result Codes
| Code | Message | Description |
|------|---------|-------------|
| 0 | MSG_SAVE_DONE | Success |
| 1 | MSG_INVALID_NICK | Invalid characters |
| 2 | MSG_ALREADY_REGIST | Name taken |
| 3 | MSG_INVALID_NICK | Too short/long |

---

## 4. Channel Selection

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CHANNEL SELECTION FLOW                           │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x0E ══════════════════════ │  Channel list
     │       [count:4]                         │
     │       FOR EACH channel:                 │
     │         [channelId:4]                   │
     │         [name:wstring]                  │
     │         [currentPlayers:4]              │
     │         [maxPlayers:4]                  │
     │         [status:4]                      │
     │                                         │
     │  [Displays channel selection UI]        │
     │  [User clicks channel]                  │
     │                                         │
     │  ════ CMD 0x19 (server query) ════════► │  Select channel
     │       [channelId:4]                     │
     │                                         │
     │  ◄════ CMD 0x54 ══════════════════════  │  Redirect!
     │       [ip:string]                       │
     │       [port:4]                          │
     │       [token:string]                    │
     │                                         │
     │  [Disconnects from LoginServer]         │
     │  [Connects to GameServer]               │
     │                                         │
```

### CMD 0x0E - Channel List (Server → Client)
- **Handler**: `sub_4793F0` @ line 220146
- **Payload**:
  - `[count:int32]` - Number of channels
  - For each channel:
    - `[channelId:int32]`
    - `[name:wstring]`
    - `[currentPlayers:int32]`
    - `[maxPlayers:int32]`
    - `[status:int32]`
- **Effect**: Sets game state to 4 (CHANNEL_SELECT)

### CMD 0x19 - Server Query (Client → Server)
- **Handler**: `sub_479340` @ line 220113
- **Payload**: Variable, depends on query type

### CMD 0x54 - Server Redirect (Server → Client)
- **Handler**: `sub_47AA00` @ line 219225
- **Payload**: ~264 bytes
  - `[ip:string]` - GameServer IP
  - `[port:int32]` - GameServer port
  - `[token:string]` - Session token
- **Purpose**: Redirect client to GameServer

---

## 5. Main Menu

```
┌─────────────────────────────────────────────────────────────────────┐
│                        MAIN MENU FLOW                               │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x11 ══════════════════════ │  Show menu
     │                                         │
     │  [Displays main menu with options:]     │
     │    - Play (Lobby)                       │
     │    - Garage                             │
     │    - Shop                               │
     │    - Options                            │
     │                                         │
     │  [User clicks "Play"]                   │
     │                                         │
     │  ════ Request lobby ══════════════════► │
     │                                         │
     │  ◄════ CMD 0x12 (flag=0x00) ══════════ │  Show lobby
     │                                         │
```

### CMD 0x11 - Show Menu (Server → Client)
- **Handler**: `sub_4793C0` @ line 220131
- **Payload**: Empty or minimal
- **Condition**: Only works if `dword_F727F4 != 2`
- **Effect**: Sets game state to 5 (MAIN_MENU)

---

## 6. Garage

```
┌─────────────────────────────────────────────────────────────────────┐
│                          GARAGE FLOW                                │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x0F ══════════════════════ │  Show garage
     │                                         │
     │  [Displays vehicle selection]           │
     │                                         │
     │  ◄════ CMD 0x1B ══════════════════════ │  Vehicle list
     │       [count:4]                         │
     │       [VehicleData:44] × count          │
     │                                         │
     │  ◄════ CMD 0x1C ══════════════════════ │  Item list
     │       [count:4]                         │
     │       [ItemData:56] × count             │
     │                                         │
     │  ◄════ CMD 0x1D ══════════════════════ │  Accessory list
     │       [count:4]                         │
     │       [AccessoryData:28] × count        │
     │                                         │
     │  [User selects/equips vehicle]          │
     │                                         │
     │  ════ CMD 0x8C (equip) ═══════════════► │
     │       [vehicleId:4]                     │
     │                                         │
     │  ◄════ CMD 0x1E ══════════════════════ │  Item update
     │       [AccessoryData:28]                │
     │                                         │
```

### CMD 0x0F - Show Garage (Server → Client)
- **Handler**: `sub_479520` @ line 220192
- **Payload**: Minimal
- **Effect**: Sets game state to 6 (GARAGE)

### CMD 0x1B - Inventory Vehicles (Server → Client)
- **Handler**: `sub_478EC0` @ line 219866
- **Payload**:
  - `[count:int32]`
  - `[VehicleData:44]` × count

### CMD 0x1C - Inventory Items (Server → Client)
- **Handler**: `sub_478F20` @ line 219888
- **Payload**:
  - `[count:int32]`
  - `[ItemData:56]` × count

### CMD 0x1D - Inventory Accessories (Server → Client)
- **Handler**: `sub_478F80`
- **Payload**:
  - `[count:int32]`
  - `[AccessoryData:28]` × count

### CMD 0x1E - Item Update (Server → Client)
- **Handler**: `sub_478FE0`
- **Payload**: `[AccessoryData:28]` (single item update)

---

## 7. Shop

```
┌─────────────────────────────────────────────────────────────────────┐
│                           SHOP FLOW                                 │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x10 ══════════════════════ │  Show shop
     │                                         │
     │  [Displays shop UI]                     │
     │                                         │
     │  ════ CMD 0x70 (enter shop) ══════════► │
     │                                         │
     │  ◄════ CMD 0x68 ══════════════════════ │  Shop lookup data
     │                                         │
     │  ◄════ CMD 0x69 ══════════════════════ │  Shop items
     │       [itemId:4]                        │
     │       [price:2]                         │
     │       [flag:1]                          │
     │                                         │
     │  [User clicks Buy]                      │
     │                                         │
     │  ════ CMD 0x6E (purchase) ════════════► │
     │       [itemId:4]                        │
     │       [quantity:4]                      │
     │                                         │
     │  ◄════ CMD 0x6F ══════════════════════ │  Purchase result
     │                                         │
     │  ◄════ CMD 0x7A ══════════════════════ │  Item added
     │       [SmallItem:32]                    │
     │                                         │
     │  ════ CMD 0x71 (exit shop) ═══════════► │
     │                                         │
```

### CMD 0x10 - Show Shop (Server → Client)
- **Handler**: `sub_479550` @ line 220208
- **Effect**: Sets game state to 7 (SHOP)

### CMD 0x68 - Shop Lookup (Server → Client)
- **Handler**: `sub_47AE30` @ line 219252

### CMD 0x69 - Shop Item (Server → Client)
- **Handler**: `sub_47AF00` @ line 219255
- **Payload**: 7 bytes
  - `[itemId:int32]`
  - `[price:int16]`
  - `[flag:int8]`

### CMD 0x6F - Shop Response (Server → Client)
- **Handler**: `sub_47B3C0` @ line 219267

### CMD 0x70/0x71 - Shop Enter/Exit (Client → Server)
- **Handler**: `sub_47B300` @ line 219271 (same handler)

### CMD 0x7A - Item Add (Server → Client)
- **Handler**: `sub_47B4F0` @ line 219295
- **Payload**: `[SmallItem:32]`

---

## 8. Lobby

```
┌─────────────────────────────────────────────────────────────────────┐
│                          LOBBY FLOW                                 │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x12 (flag=0x00) ══════════ │  Show lobby
     │                                         │
     │  [Displays room list]                   │
     │                                         │
     │  ◄════ CMD 0x3F ══════════════════════ │  Room list
     │       [rooms...]                        │
     │                                         │
     │  [User clicks "Create Room"]            │
     │                                         │
     │  ════ CMD 0x63 (create room) ═════════► │
     │       [roomName:wstring]                │
     │       [password:wstring]                │
     │       [maxPlayers:4]                    │
     │       [mapId:4]                         │
     │       [gameMode:4]                      │
     │                                         │
     │  ◄════ CMD 0x63 ══════════════════════ │  Room created
     │       [roomId:4]                        │
     │       [roomInfo...]                     │
     │                                         │
     │  [OR User clicks existing room]         │
     │                                         │
     │  ════ CMD 0x3F (join room) ═══════════► │
     │       [roomId:4]                        │
     │       [password:wstring]                │
     │                                         │
     │  ◄════ CMD 0x12 (flag=0x01) ══════════ │  Enter room
     │                                         │
```

### CMD 0x12 - Show Lobby (Server → Client)
- **Handler**: `sub_4795A0` @ line 220232
- **Flag**: 0x00 for lobby view
- **Effect**: Sets game state to 8 (LOBBY)

### CMD 0x3F - Room Info/List (Server → Client)
- **Handler**: `sub_47A050` @ line 219192

### CMD 0x63 - Create Room (Client ↔ Server)
- **Handler**: `sub_47AC30` @ line 219243

### CMD 0x21 - Room Full (Server → Client)
- **Handler**: `sub_4797C0` @ line 219129
- **Purpose**: Cannot join, room is full

---

## 9. Room

```
┌─────────────────────────────────────────────────────────────────────┐
│                           ROOM FLOW                                 │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x12 (flag=0x01) ══════════ │  Enter room
     │                                         │
     │  [Displays room with players]           │
     │                                         │
     │  ◄════ CMD 0x3E ══════════════════════ │  Player joined
     │       [playerId:4]                      │
     │       [playerName:wstring 35]           │
     │       [unknown:4]                       │
     │       [unknown:4]                       │
     │       [VehicleData:44]                  │
     │       [ItemData:56]                     │
     │                                         │
     │  ◄════ CMD 0x23 ══════════════════════ │  Player update
     │       [playerId:4]                      │
     │       [ready:1]                         │
     │       [team:1]                          │
     │                                         │
     │  [User clicks Ready]                    │
     │                                         │
     │  ════ CMD 0x23 (ready) ═══════════════► │
     │       [ready:1]                         │
     │                                         │
     │  [Host clicks Start]                    │
     │                                         │
     │  ════ CMD 0x40 (start game) ══════════► │
     │                                         │
     │  ◄════ CMD 0x14 ══════════════════════ │  Game mode
     │       [mode:4]                          │
     │                                         │
     │  ◄════ CMD 0x30 ══════════════════════ │  Room state
     │       [state:4]                         │
     │                                         │
     │  [User clicks Leave]                    │
     │                                         │
     │  ════ CMD 0x22 (leave) ═══════════════► │
     │                                         │
     │  ◄════ CMD 0x22 ══════════════════════ │  Left room
     │                                         │
     │  ◄════ CMD 0x2E ══════════════════════ │  Player left (others)
     │                                         │
```

### CMD 0x12 - Enter Room (Server → Client)
- **Flag**: 0x01 for room view
- **Effect**: Sets game state to 9 (ROOM)

### CMD 0x3E - Player Join (Server → Client)
- **Handler**: `sub_479D60` @ line 219189
- **Payload**: ~180 bytes
  - `[unknown:int32]`
  - `[playerName:wchar_t[35]]` (70 bytes)
  - `[unknown:int32]`
  - `[unknown:int32]`
  - `[VehicleData:44]`
  - `[ItemData:56]`

### CMD 0x23 - Player Update (Server ↔ Client)
- **Handler**: `sub_479BE0` @ line 219135
- **Payload**: Ready state, team, etc.

### CMD 0x30 - Room State (Server → Client)
- **Handler**: `sub_479760` @ line 219159

### CMD 0x22 - Leave Room (Client ↔ Server)
- **Handler**: `sub_479920` @ line 219132

### CMD 0x2E - Player Left (Server → Client)
- **Handler**: `sub_479710` @ line 219156

---

## 10. Race

```
┌─────────────────────────────────────────────────────────────────────┐
│                           RACE FLOW                                 │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  ◄════ CMD 0x14 ══════════════════════ │  Game mode set
     │       [mode:4]                          │
     │                                         │
     │  [Loading screen]                       │
     │                                         │
     │  ◄════ CMD 0x3B ══════════════════════ │  Countdown start
     │       [seconds:4]                       │
     │                                         │
     │  [3... 2... 1... GO!]                   │
     │                                         │
     │  ════ CMD 0x31 (position) ════════════► │  Every frame
     │       [x:float][y:float][z:float]       │
     │       [rotation:float]                  │
     │       [speed:float]                     │
     │                                         │
     │  ◄════ CMD 0x31 ══════════════════════ │  Other players pos
     │       [playerId:4]                      │
     │       [x:float][y:float][z:float]       │
     │                                         │
     │  ◄════ CMD 0x33 ══════════════════════ │  Game state update
     │                                         │
     │  ◄════ CMD 0x35 ══════════════════════ │  Score/lap update
     │       [playerId:4]                      │
     │       [score:4]                         │
     │                                         │
     │  ════ CMD 0x45 (use item) ════════════► │
     │       [itemId:4]                        │
     │       [targetId:4]                      │
     │                                         │
     │  ◄════ CMD 0x45 ══════════════════════ │  Item used
     │       [playerId:4]                      │
     │       [itemId:4]                        │
     │       [targetId:4]                      │
     │                                         │
     │  ════ CMD 0x39 (finish) ══════════════► │  Crossed finish line
     │       [time:4]                          │
     │                                         │
     │  ◄════ CMD 0x39 ══════════════════════ │  Player finished
     │       [playerId:4]                      │
     │       [position:4]                      │
     │       [time:4]                          │
     │                                         │
     │  ◄════ CMD 0x3A ══════════════════════ │  Race results
     │       [results...]                      │
     │                                         │
     │  ◄════ CMD 0x3C ══════════════════════ │  Race end
     │                                         │
```

### CMD 0x14 - Game Mode (Server → Client)
- **Handler**: `sub_479CC0` @ line 219105
- **Payload**: 4 bytes
  - `[gameMode:int32]`

### CMD 0x3B - Countdown (Server → Client)
- **Payload**: Start countdown

### CMD 0x31 - Position (Client ↔ Server)
- **Handler**: `sub_479950` @ line 219162
- **Purpose**: Position synchronization

### CMD 0x33 - Game State (Server → Client)
- **Handler**: `sub_4799B0` @ line 219168

### CMD 0x35 - Score (Server → Client)
- **Handler**: `sub_479C20` @ line 219174
- **Payload**:
  - `[playerId:int32]`
  - `[score:int32]`

### CMD 0x45 - Item Usage (Client ↔ Server)
- **Handler**: `sub_47A560` @ line 219204
- **Payload**: 12 bytes
  - `[playerId:int32]`
  - `[itemId:int32]`
  - `[targetId:int32]`

### CMD 0x39 - Finish (Client ↔ Server)
- **Handler**: `sub_479CB0` @ line 219177

### CMD 0x3A - Results (Server → Client)
- **Handler**: `sub_47AE00` @ line 219180

### CMD 0x3C - Race End (Server → Client)
- **Handler**: `sub_47A5C0` @ line 219183

---

## 11. Tutorial/License

```
┌─────────────────────────────────────────────────────────────────────┐
│                       TUTORIAL FLOW                                 │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │  [First-time player or license test]    │
     │                                         │
     │  ◄════ CMD 0x0E (special) ════════════ │  Tutorial menu
     │                                         │
     │  [Shows license selection:]             │
     │    - License A (beginner)               │
     │    - License B (intermediate)           │
     │    - License C (advanced)               │
     │                                         │
     │  [User selects license test]            │
     │                                         │
     │  ════ Start tutorial ═════════════════► │
     │                                         │
     │  [Same as race flow but solo]           │
     │                                         │
     │  ◄════ CMD 0x62 ══════════════════════ │  Tutorial result
     │       [passed:1]                        │
     │                                         │
     │  [If failed: MSG_TUTORIAL_FAIL]         │
     │  [If passed: License unlocked]          │
     │                                         │
```

### CMD 0x62 - Tutorial Fail (Server → Client)
- **Handler**: `sub_479580` @ line 220224
- **Effect**: Sets game state to 13 (TUTORIAL)

---

## Packet Reference

### Quick CMD Lookup Table

| CMD | Hex | Direction | Name | Handler |
|-----|-----|-----------|------|---------|
| 1 | 0x01 | S→C | Login Error | sub_478DA0 |
| 2 | 0x02 | S→C | Display Message | sub_478D40 |
| 3 | 0x03 | S→C | Char Creation Trigger | sub_479220 |
| 4 | 0x04 | Both | Char Creation | sub_479230 |
| 7 | 0x07 | Both | Login/Session | sub_479020 |
| 10 | 0x0A | S→C | Connection OK | sub_47D3B0 |
| 14 | 0x0E | S→C | Channel List | sub_4793F0 |
| 15 | 0x0F | S→C | Show Garage | sub_479520 |
| 16 | 0x10 | S→C | Show Shop | sub_479550 |
| 17 | 0x11 | S→C | Show Menu | sub_4793C0 |
| 18 | 0x12 | S→C | Show Lobby/Room | sub_4795A0 |
| 20 | 0x14 | S→C | Game Mode | sub_479CC0 |
| 27 | 0x1B | S→C | Inventory Vehicles | sub_478EC0 |
| 28 | 0x1C | S→C | Inventory Items | sub_478F20 |
| 29 | 0x1D | S→C | Inventory Accessories | sub_478F80 |
| 30 | 0x1E | S→C | Item Update | sub_478FE0 |
| 33 | 0x21 | S→C | Room Full | sub_4797C0 |
| 34 | 0x22 | Both | Leave Room | sub_479920 |
| 35 | 0x23 | Both | Player Update | sub_479BE0 |
| 45 | 0x2D | Both | Chat Message | sub_479630 |
| 46 | 0x2E | S→C | Player Left | sub_479710 |
| 48 | 0x30 | S→C | Room State | sub_479760 |
| 49 | 0x31 | Both | Position | sub_479950 |
| 53 | 0x35 | S→C | Score | sub_479C20 |
| 57 | 0x39 | Both | Finish | sub_479CB0 |
| 58 | 0x3A | S→C | Results | sub_47AE00 |
| 60 | 0x3C | S→C | Race End | sub_47A5C0 |
| 62 | 0x3E | S→C | Player Join | sub_479D60 |
| 63 | 0x3F | Both | Room Info | sub_47A050 |
| 69 | 0x45 | Both | Item Usage | sub_47A560 |
| 84 | 0x54 | S→C | Server Redirect | sub_47AA00 |
| 99 | 0x63 | Both | Create Room | sub_47AC30 |
| 111 | 0x6F | S→C | Shop Response | sub_47B3C0 |
| 122 | 0x7A | S→C | Item Add | sub_47B4F0 |
| 166 | 0xA6 | C→S | Heartbeat | - |
| 250 | 0xFA | C→S | Full State | - |

### NULLSUB (Ignored) CMDs
```
8, 31, 32, 36, 54, 55, 56, 67, 72, 102, 109, 126, 127, 128, 134, 139, 141, 145, 146, 147, 148, 160, 187
```

