# 0x63 CREATE_ROOM (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_47AC30` @ line 221178
**Handler Ghidra:** `FUN_0047ac30`

## Purpose

Response to room creation request. Contains the new room ID.

## Payload Structure (4 bytes)

```c
struct CreateRoomResponse {
    int32_t roomId;    // New room ID
};  // Total: 4 bytes
```

## Handler Code (IDA)

```c
int __stdcall sub_47AC30(int a1)
{
  int roomId;
  sub_44E910(a1, &roomId, 4);
  dword_F6C938 = roomId;  // Store room ID
  return sub_4634C0(&unk_F661F8);  // Initialize room UI
}
```

## Server Implementation

```cpp
void sendCreateRoomResponse(Session::Ptr session, int32_t roomId) {
    Packet pkt(0x63);
    pkt.writeInt32(roomId);
    session->send(pkt);
}
```

## Notes

- Room ID stored globally at dword_F6C938
- Triggers room UI initialization
