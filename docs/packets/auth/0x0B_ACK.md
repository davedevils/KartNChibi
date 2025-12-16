# 0x0B ACK (Bidirectional)

**Packet ID:** 0x0B (11 decimal)  
**Handler:** `sub_479190` / `FUN_00479190`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Bidirectional acknowledgment packet - **ping-pong style**.
When the server sends 0x0B, the client automatically responds with 0x0B.

## Payload Structure

```c
// No payload - 0 bytes
```

## Size

**0 bytes** (header only)

## Handler Decompilation (IDA)

```c
void __thiscall sub_479190(_DWORD *this, int a2)
{
    sub_4803A0(this, 11);  // Send back 0x0B to server!
}
```

### sub_4803A0 - Packet Send Function

```c
void __thiscall sub_4803A0(_DWORD *this, __int16 a2)
{
    // ... stack setup ...
    sub_44ECD0(v5, a2);      // Create packet with CMD = a2 (11)
    sub_476B80(this, v5);    // Send packet to server
    sub_44E8E0(v5);          // Cleanup
}
```

## Behavior

1. **Server sends 0x0B** → Client receives it
2. **Handler called** → `sub_479190`
3. **Auto-response** → Client sends 0x0B back to server via `sub_4803A0`

## Flow Diagram

```
Server                          Client
   |                               |
   |-------- 0x0B (ACK) --------->|
   |                               | sub_479190() called
   |                               | sub_4803A0(this, 11)
   |<------- 0x0B (ACK) ----------|
   |                               |
```

## Important Functions

| Function | Purpose |
|----------|---------|
| `sub_479190` | Handler - receives 0x0B |
| `sub_4803A0` | Creates and sends packet |
| `sub_476B80` | Network send function |
| `sub_44ECD0` | Packet constructor (sets CMD) |

## Use Cases

- **Keep-alive / Heartbeat**: Verify connection is still active
- **Sync check**: Ensure client-server are in sync
- **Generic acknowledgment**: Confirm receipt of previous packet

## Server Implementation

```cpp
void sendAck(Session::Ptr session) {
    Packet pkt(0x0B);
    // No payload needed
    session->send(pkt);
}

// Handle client's ACK response
void handleAck(Session::Ptr session, Packet& packet) {
    // Client responded to our ACK
    LOG_DEBUG("ACK", "Received ACK response from " + session->remoteAddress());
}
```

## Notes

- This is NOT a one-way packet - it's bidirectional
- The client ALWAYS responds with 0x0B when receiving 0x0B
- Can be used to test if connection is alive
- No data is transmitted - pure acknowledgment
