# 0x04 REGISTRATION_RESPONSE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479230` @ line 220068
**Handler Ghidra:** `FUN_00479230` @ line 84605

## Purpose

Response to character creation request. Contains result code and starting equipment on success.

## Payload Structure

### On Error (result != 0)

```c
struct RegistrationError {
    int32_t result;    // Error code (1, 2, or 3)
};  // Total: 4 bytes
```

### On Success (result == 0)

```c
struct RegistrationSuccess {
    int32_t  result;              // 0 = success
    wchar_t  nickname[];          // Confirmed nickname (UTF-16LE null-term)
    uint8_t  vehicleData[44];     // Starting vehicle (0x2C bytes)
    uint8_t  itemData[56];        // Starting item (0x38 bytes)
};  // Total: 4 + var + 44 + 56 = 104+ bytes
```

## Field Details

| Field | Type | Size | Description |
|-------|------|------|-------------|
| result | int32 | 4 | 0=success, 1/3=invalid nick, 2=name taken |
| nickname | wstring | var | Confirmed nickname (success only) |
| vehicleData | struct | 44 | VehicleData structure (success only) |
| itemData | struct | 56 | ItemData structure (success only) |

## Result Codes

| Code | Message | Description |
|------|---------|-------------|
| **0** | MSG_SAVE_DONE | Success - character created |
| **1** | MSG_INVALID_NICK | Invalid characters in nickname |
| **2** | MSG_ALREADY_REGIST | Nickname already taken |
| **3** | MSG_INVALID_NICK | Nickname too short/long |

## Handler Code (IDA)

```c
char __thiscall sub_479230(void *this, const wchar_t **a2)
{
  int result;           // v5
  int vehicleData[11];  // v6 - 44 bytes
  int itemData[14];     // v7 - 56 bytes

  sub_4538B0();
  sub_44E910((int)a2, &result, (const char *)4);  // Read result code
  
  if (result != 0) {
    // ERROR CASES
    switch (result) {
      case 1: return sub_4707B0(byte_118C6F8, "MSG_INVALID_NICK", 2, 0);
      case 2: return sub_4707B0(byte_118C6F8, "MSG_ALREADY_REGIST", 2, 0);
      case 3: return sub_4707B0(byte_118C6F8, "MSG_INVALID_NICK", 2, 0);
    }
  }
  else {
    // SUCCESS
    sub_44EB60(a2, (char *)&unk_80E63E + (_DWORD)this);  // Read nickname to PlayerInfo
    sub_44E910((int)a2, vehicleData, (const char *)0x2C); // Read VehicleData (44 bytes)
    
    if (sub_44FCB0(&unk_844720 + this, vehicleData) >= 0) {
      sub_44E910((int)a2, itemData, (const char *)0x38);  // Read ItemData (56 bytes)
      
      if (sub_44F2D0(&unk_845228 + this, itemData) >= 0) {
        // Set equipped vehicle and item
        *(int *)(dword_80E66C + this) = itemData[0];   // Item ID
        *(int *)(dword_80E668 + this) = vehicleData[0]; // Vehicle ID
        return sub_4641E0(byte_F727E8, "MSG_SAVE_DONE", 1);
      }
    }
  }
  return sub_4641E0(byte_F727E8, "MSG_UNKNOWN_ERROR", 2);
}
```

## Handler Code (Ghidra)

```c
void __fastcall FUN_00479230(int param_1)
{
  int iVar1;
  int result;              // iStack_68
  undefined4 vehicleData[11];  // auStack_64
  undefined4 itemData[14];     // auStack_38
  
  FUN_004538b0();
  FUN_0044e910(&result, 4);  // Read result
  
  if (result == 0) {
    FUN_0044eb60(&DAT_0080e63e + param_1);  // Read nickname
    FUN_0044e910(vehicleData, 0x2c);        // Read VehicleData
    iVar1 = FUN_0044fcb0(vehicleData);
    if (-1 < iVar1) {
      FUN_0044e910(itemData, 0x38);         // Read ItemData
      iVar1 = FUN_0044f2d0(itemData);
      if (-1 < iVar1) {
        *(undefined4*)(&DAT_0080e66c + param_1) = itemData[0];
        *(undefined4*)(&DAT_0080e668 + param_1) = vehicleData[0];
        FUN_004641e0("MSG_SAVE_DONE", 1);
        return;
      }
    }
    FUN_004641e0("MSG_UNKNOWN_ERROR", 2);
    return;
  }
  
  // Error handling
  if (result == 1 || result == 3) {
    FUN_004707b0("MSG_INVALID_NICK", 2, 0);
  } else if (result == 2) {
    FUN_004707b0("MSG_ALREADY_REGIST", 2, 0);
  }
}
```

## Memory Addresses

| Address | Field | Description |
|---------|-------|-------------|
| 0x80E63E | unk_80E63E | Nickname in PlayerInfo (offset 0x486) |
| 0x80E668 | dword_80E668 | Current vehicle ID |
| 0x80E66C | dword_80E66C | Current item/driver ID |
| 0x844720 | unk_844720 | Vehicle inventory |
| 0x845228 | unk_845228 | Item inventory |

## Server Implementation

```cpp
// Error response
void sendRegistrationError(Session::Ptr session, int32_t errorCode) {
    Packet pkt(0x04);
    pkt.writeInt32(errorCode);  // 1=invalid, 2=taken, 3=invalid
    session->send(pkt);
}

// Success response
void sendRegistrationSuccess(Session::Ptr session, 
                             const std::u16string& nickname,
                             const VehicleData& vehicle,
                             const ItemData& item) {
    Packet pkt(0x04);
    
    // Result = 0 (success)
    pkt.writeInt32(0);
    
    // Confirmed nickname
    pkt.writeWString(nickname);
    
    // Starting vehicle (44 bytes)
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&vehicle), 44);
    
    // Starting item (56 bytes)
    pkt.writeBytes(reinterpret_cast<const uint8_t*>(&item), 56);
    
    session->send(pkt);
}
```

## Sequence

```
Client                              Server
  │                                   │
  │  [Character creation UI open]     │
  │                                   │
  │  ════ CMD 0x04 (create) ══════►  │
  │       [driverType:4]              │
  │       [nickname:wstring]          │
  │                                   │
  │           ┌─── SUCCESS ───┐       │
  │           │               │       │
  │  ◄════ CMD 0x04 ═════════════    │  ← THIS PACKET
  │       [result:0]                  │
  │       [nickname:wstring]          │
  │       [VehicleData:44]            │
  │       [ItemData:56]               │
  │                                   │
  │           └─── ERROR ───┘         │
  │                                   │
  │  ◄════ CMD 0x04 ═════════════    │
  │       [result:1/2/3]              │
  │                                   │
```

## Notes

- On success, client stores vehicle ID at 0x80E668 and item ID at 0x80E66C
- Nickname is stored at offset 0x486 (0x80E63E) in PlayerInfo
- VehicleData first 4 bytes = vehicle template ID
- ItemData first 4 bytes = item template ID

## Cross-Reference

- Client sends: [0x04_CLIENT_CREATE.md](../client/0x04_CLIENT_CREATE.md) (TODO)
- Related: [0x03_CHARACTER_CREATION_TRIGGER.md](0x03_CHARACTER_CREATION_TRIGGER.md)
- See: [STRUCTURES.md](../STRUCTURES.md) for VehicleData/ItemData
