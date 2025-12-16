# 📡 KnC Protocol - Complete Overview

**Version**: 1.0  
**Status**: ✅ CERTIFIÉ IDA + Ghidra  
**Date**: Décembre 2025  
**Source**: Reverse engineered from KnC.exe (Chibi Kart / Kart n' Crazy)

> **Note**: Ce document fusionne PROTOCOL_MASTER.md et PROTOCOL.md pour une référence complète.

---

## 📦 Packet Structure

```
┌──────────────────────────────────────────────────────────┐
│ HEADER (8 bytes)                                         │
├──────────────────────────────────────────────────────────┤
│ [0-1]  Size     uint16_t  Payload size (little-endian)   │
│ [2]    CMD      uint8_t   Command/Opcode                 │
│ [3]    Flag     uint8_t   Sub-command / variant          │
│ [4-7]  Reserved 4 bytes   Usually 0x00                   │
├──────────────────────────────────────────────────────────┤
│ PAYLOAD (variable)                                       │
└──────────────────────────────────────────────────────────┘
```

### Flag Byte [3]
- `0x00` = Default behavior
- `0x01` = UI transition / alternate mode
- Other values = Packet variants

### Total Packet Size
**Total** = Header (8) + Payload (Size field value - 4)

---

## 📊 Data Types

| Type | Size | Description |
|------|------|-------------|
| `int8` | 1 | Signed 8-bit |
| `uint8` / `byte` | 1 | Unsigned 8-bit |
| `int16` | 2 | Signed 16-bit LE |
| `uint16` | 2 | Unsigned 16-bit LE |
| `int32` | 4 | Signed 32-bit LE |
| `uint32` | 4 | Unsigned 32-bit LE |
| `float` | 4 | IEEE 754 single |
| `string` | N+1 | Null-terminated ASCII |
| `wstring` | 2N+2 | Null-terminated UTF-16LE |

---

## 🏗️ Core Structures (CERTIFIÉES)

| Structure | Size | Hex | Verification |
|-----------|------|-----|--------------|
| **PlayerInfo** | 1224 | 0x4C8 | ✅ IDA+Ghidra |
| **VehicleData** | 44 | 0x2C | ✅ IDA+Ghidra |
| **ItemData** | 56 | 0x38 | ✅ IDA+Ghidra |
| **AccessoryData** | 28 | 0x1C | ✅ IDA+Ghidra |
| **SmallItem** | 32 | 0x20 | ✅ IDA+Ghidra |
| **ItemData36** | 36 | 0x24 | ✅ IDA+Ghidra |
| **EntityData** | 104 | 0x68 | ✅ IDA+Ghidra |
| **ShopData** | 396 | 0x18C | ✅ IDA+Ghidra |
| **TrackData** | 156 | 0x9C | ✅ IDA+Ghidra |
| **LapData** | 112 | 0x70 | ✅ IDA+Ghidra |
| **WaypointEntry** | 28 | 0x1C | ✅ IDA+Ghidra |
| **ChatMessage** | 116 | 0x74 | ✅ IDA+Ghidra |
| **GiftData** | 212 | 0xD4 | ✅ IDA+Ghidra |
| **LargeData** | 132 | 0x84 | ✅ IDA+Ghidra |
| **ExtendedDataEntry** | 52 | 0x34 | ✅ IDA+Ghidra |

📄 **Voir [STRUCTURES.md](STRUCTURES.md) pour les définitions C++ complètes.**

---

## 🔑 Critical Variables (Memory Addresses)

| Variable | Address | Purpose |
|----------|---------|---------|
| `dword_F727F4` | 0xF727F4 | UI State Gate (1=OK, 2=blocked) |
| `dword_80E1A8` | 0x80E1A8 | Driver ID |
| `dword_80E1B8` | 0x80E1B8 | PlayerInfo base |
| `dword_80E668` | 0x80E668 | Current Vehicle ID |
| `dword_80E66C` | 0x80E66C | Current Driver Outfit ID |
| `dword_B23288` | 0xB23288 | UI State Manager |

---

## 🎮 Game States

| Value | State | Description |
|-------|-------|-------------|
| 0 | DISCONNECTED | Not connected |
| 1 | CONNECTING | Connection in progress |
| 2 | CONNECTED | Connected, waiting |
| 3 | AUTHENTICATING | Login in progress |
| 4 | AUTHENTICATED | Login success |
| 5 | MENU | Main menu |
| 6 | GARAGE | Garage screen |
| 7 | SHOP | Shop screen |
| 8 | LOBBY | Game lobby |
| 9 | ROOM | In a room |
| 10 | IN_RACE | Racing |
| 11 | RESULTS | Race results |
| 12 | TUTORIAL | Tutorial mode |

---

## 📡 Packet Categories (150+ Packets)

### 🔐 Authentication & Session
- **0x01-0x12**: Login, character creation, session init
- **0xA6, 0xA7**: Heartbeat, session confirm
- **0x8E-0x90**: Connection acknowledgments

📂 **Détails**: [auth/](auth/)

### 🎨 UI & Transitions
- **0x0E-0x12 (Flag=0x01)**: Screen transitions
- **0xB6**: Display text
- **0x62**: Tutorial triggers

📂 **Détails**: [ui/](ui/)

### 📦 Inventory & Items
- **0x1B-0x1E**: Character items
- **0x78-0x7A**: Item lists
- **0x81-0x85**: Shop data
- **0x88-0x8C**: Slot management
- **0x98-0x9F**: Add items (vehicle, accessory, gift)

📂 **Détails**: [inventory/](inventory/)

### 🏠 Room Management
- **0x21-0x23**: Join/leave room, player updates
- **0x30**: Room state
- **0x3E-0x3F**: Player join, room info
- **0x62-0x63**: Tutorial, create room
- **0xBF-0xC5**: Room lists, details, extended info

📂 **Détails**: [room/](room/)

### 💬 Chat & Messaging
- **0x2A-0x2B**: Whisper enable/disable
- **0x2D, 0x2E**: Chat messages, player left
- **0x6C, 0x6E**: Player messages, display
- **0x116-0x117**: Player name, remove

📂 **Détails**: [chat/](chat/)

### 🏁 Game & Racing
- **0x14**: Game mode
- **0x30-0x40**: Room/game state, ready, position, lap, items
- **0x44-0x4E**: Game updates, effects, timestamps
- **0x54-0x65**: Race status, speed, entity states
- **0xF3-0xFE**: Track data, checkpoints, race init, positions

📂 **Détails**: [game/](game/)

### 🛒 Shop
- **0x6F**: Shop action
- **0x72-0x74**: Shop data blocks

📂 **Détails**: [shop/](shop/)

### ⚙️ System
- **0xAA, 0xB5**: Player data, dual player info
- **0xCD**: Audio control
- **0xEE, 0xF0-0xF1**: Entity updates/remove/data

📂 **Détails**: [system/](system/)

### 📤 Client → Server
- **0x07**: Client auth
- **0x19**: Server query
- **0x2C**: State change
- **0x4D**: Request data
- **0x73**: Disconnect
- **0xA6**: Heartbeat
- **0xD0**: Client info
- **0xFA**: Full state

📂 **Détails**: [client/](client/)

---

## 🔄 Connection Flow

```
1. TCP Connect (port 50017 - Login Server)
   ↓
2. Send 0xD0 (Client Info)
   ↓
3. Receive 0x0A (Connection OK)
   ↓
4. Send 0x07 (Client Auth)
   ↓
5. Receive 0x01 (Login Response)
   ↓
6. If success:
   - Receive 0x12 (Show Lobby)
   - Send 0xA6 (Heartbeat) every 30s
   ↓
7. TCP Connect (port 50018 - Game Server)
   ↓
8. Game session begins
```

---

## 📚 Documentation Structure

```
docs/protocol/
├── PROTOCOL_OVERVIEW.md     (ce fichier)
├── STRUCTURES.md            (structures C++ complètes)
├── MESSAGES.md              (messages types)
├── README.md                (index navigation)
│
├── auth/                    (packets authentification)
│   ├── 0x01_LOGIN_RESPONSE.md
│   ├── 0xA6_HEARTBEAT.md
│   └── ...
│
├── game/                    (packets jeu/course)
│   ├── 0x31_POSITION.md
│   ├── 0x36_LAP_INFO.md
│   └── ...
│
├── chat/                    (packets chat)
├── inventory/               (packets inventaire)
├── room/                    (packets room)
├── shop/                    (packets shop)
├── system/                  (packets système)
└── ui/                      (packets UI)
```

---

## 🔗 References

- **Structures détaillées**: [STRUCTURES.md](STRUCTURES.md)
- **Messages types**: [MESSAGES.md](MESSAGES.md)
- **Client Architecture**: [../architecture/CLIENT_ARCHITECTURE.md](../architecture/CLIENT_ARCHITECTURE.md)
- **Network Protocol Impl**: [../client-clone/05_network_protocol.md](../client-clone/05_network_protocol.md)

---

## ✅ Certification

- ✅ Vérifié avec IDA Pro 7.x
- ✅ Vérifié avec Ghidra 10.x
- ✅ Testé avec DevClient (client original)
- ✅ Compatible avec serveur émulé

**Dernière mise à jour:** Décembre 2025

