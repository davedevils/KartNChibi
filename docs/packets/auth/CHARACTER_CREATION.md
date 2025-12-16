# Character Creation Flow

**Source**: `DevClient/KnC-new.exe.c` lines 216711, 220061, 220068, 225111

## Workflow Diagram

```
┌─────────────────────────────────────────────────────────────────────┐
│                    CHARACTER CREATION FLOW                          │
└─────────────────────────────────────────────────────────────────────┘

   Client                                    Server
     │                                         │
     │                                         │
     │   ◄──── CMD 0x03 (empty) ───────────   │  Trigger char creation
     │                                         │
     │   [Shows nickname input UI]             │
     │   [User types name: "MyName"]           │
     │   [User clicks Submit]                  │
     │                                         │
     │   ──── CMD 0x04 ────────────────────►   │  Create request
     │        [driverType:4]                   │
     │        [nickname:wstring]               │
     │                                         │
     │                                         │  Server validates:
     │                                         │  - Nickname length
     │                                         │  - Invalid chars
     │                                         │  - Name taken?
     │                                         │
     │   ◄──── CMD 0x04 ───────────────────   │  Response
     │        [result:4]                       │
     │        [nickname:wstring]               │  (if success)
     │        [VehicleData:44]                 │  (if success)
     │        [ItemData:56]                    │  (if success)
     │                                         │
     │   [If result=0: MSG_SAVE_DONE]          │
     │   [Proceeds to channel list]            │
     │                                         │
```

## CMD 0x03: Trigger Character Creation

**Direction**: Server → Client  
**Handler**: `sub_479220` @ line 220061  
**Payload**: Empty (0 bytes)

When server detects no character exists for the account, it sends this packet to trigger the character creation UI.

### Client Response
- Calls `sub_473730` (ShowCharacterCreationUI)
- Displays nickname input field (max 10 characters)
- Selects random starting driver

## CMD 0x04: Create Character Request

**Direction**: Client → Server  
**Handler**: `sub_4805D0` @ line 225111

### Packet Format

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0 | int32 | 4 | Driver type (selected character model) |
| 4 | wstring | var | Nickname (UTF-16LE null-terminated) |

### Nickname Validation (Client-side)
- Max 10 characters
- Filtered through `sub_4E15E0` for invalid characters
- If invalid: shows `MSG_INVALID_NICK`, doesn't send packet

## CMD 0x04: Create Character Response

**Direction**: Server → Client  
**Handler**: `sub_479230` @ line 220068

### Packet Format

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0 | int32 | 4 | Result code |
| 4+ | wstring | var | Nickname (only if result=0) |
| N | VehicleData | 44 | Starting vehicle (only if result=0) |
| N+44 | ItemData | 56 | Starting item (only if result=0) |

### Result Codes

| Code | Message | Description |
|------|---------|-------------|
| 0 | MSG_SAVE_DONE | Success - character created |
| 1 | MSG_INVALID_NICK | Invalid characters in nickname |
| 2 | MSG_ALREADY_REGIST | Nickname already taken |
| 3 | MSG_INVALID_NICK | Nickname too short/long |

## Data Structures

### VehicleData (44 bytes = 0x2C)

```c
struct VehicleData {
    int32_t vehicleId;      // +0x00
    int32_t ownerId;        // +0x04
    int32_t durability;     // +0x08
    int32_t maxDurability;  // +0x0C
    int32_t stats[7];       // +0x10 (28 bytes)
};
```

### ItemData (56 bytes = 0x38)

```c
struct ItemData {
    int32_t itemId;         // +0x00
    int32_t ownerId;        // +0x04
    int32_t quantity;       // +0x08
    int32_t unknown[11];    // +0x0C (44 bytes)
};
```

## Server Implementation Notes

### On receiving CMD 0x04 from client:

1. Read `driverType` (4 bytes)
2. Read `nickname` (wstring)
3. Validate nickname:
   - Length: 2-10 characters
   - No special characters
   - Not already taken in DB
4. If valid:
   - Create character in `characters` table
   - Create starter vehicle in `vehicles` table
   - Create starter item in `items` table
   - Send response with result=0 + data
5. If invalid:
   - Send response with result=1/2/3

### Example Server Response (Success)

```
[00 00 00 00]                    // result = 0 (success)
[4D 00 79 00 4E 00 61 00 6D 00   // "MyName" in UTF-16LE
 65 00 00 00]                    // null terminator
[01 00 00 00 ...]                // VehicleData (44 bytes)
[01 00 00 00 ...]                // ItemData (56 bytes)
```

### Example Server Response (Error - Name Taken)

```
[02 00 00 00]                    // result = 2 (already registered)
// No additional data for error responses
```

## Relevant Functions

| Address | Name | Purpose |
|---------|------|---------|
| 0x479220 | HandleCmd03 | Receive trigger, show UI |
| 0x479230 | HandleCmd04Response | Receive creation result |
| 0x4805D0 | SendCmd04Request | Send creation request |
| 0x473730 | ShowCharCreationUI | Display nickname input |
| 0x4E15E0 | ValidateNickname | Check for invalid chars |

