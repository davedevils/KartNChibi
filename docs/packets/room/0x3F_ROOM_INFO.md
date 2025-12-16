# 0x3F ROOM_INFO (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47A050` @ line 220687
**Handler Ghidra:** `FUN_0047a050`

## Purpose

Room information update. Simple packet with room ID.

## Payload Structure (4 bytes)

```c
struct RoomInfo {
    int32_t roomId;    // Room ID
};  // Total: 4 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_47A050(int a1)
{
  int roomId;
  
  sub_44E910(a1, &roomId, 4);
  sub_4952F0(byte_1B19090, roomId);      // Update some state
  sub_48D010(dword_1AF2BB0, roomId);     // Update room list
  
  // Decrement player count if not in certain states
  if (dword_B23178 != 3 && dword_B23178 != 1)
    --dword_B23198;
    
  return dword_B23178;
}
```

## Server Implementation

```cpp
void sendRoomInfo(Session::Ptr session, int32_t roomId) {
    Packet pkt(0x3F);
    pkt.writeInt32(roomId);
    session->send(pkt);
}
```

## Notes

- Used for room list updates
- Decrements player count in certain conditions
- dword_B23178 appears to be room/game state
