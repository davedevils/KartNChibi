# 0x64 ROOM_STATUS_B (Server → Client)

**Packet ID:** 0x64 (100 decimal)  
**Handler:** `sub_47ACD0` / `FUN_0047acd0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Updates player slot status in room.

## Payload Structure

```c
struct RoomStatusB {
    int32 slotId;       // Slot index
    int32 status;       // 0 = empty, 1 = ready, etc.
};
```

## Size

**8 bytes** (2 × int32)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Slot ID |
| 0x04 | int32 | 4 | Status value |

## Status Values

| Value | Meaning | Effect |
|-------|---------|--------|
| 0 | Empty/Left | Increment BCE238, decrement BCE23C |
| 1 | Occupied | Increment BCE23C, decrement BCE238 |
| Other | In-game | Different handling |

## Handler Logic

```c
// IDA: sub_47ACD0 / Ghidra: FUN_0047acd0
read(&slotId, 4);
read(&status, 4);

int idx = lookupSlot(slotId);
if (idx >= 0) {
    slotData[idx * 3024].status = status;
    
    if (status == 0) {
        readyCount++;    // BCE238
        totalCount--;    // BCE23C
    } else if (status == 1) {
        totalCount++;
        readyCount--;
    }
}
```

## Server Implementation

```cpp
void sendRoomStatusB(Session::Ptr session, int slotId, int status) {
    Packet pkt(0x64);
    pkt.writeInt32(slotId);
    pkt.writeInt32(status);
    session->send(pkt);
}
```

## Notes

- Updates slot status in room
- Affects ready/total counters
- Slot data entry size: 3024 bytes (0xBD0)

