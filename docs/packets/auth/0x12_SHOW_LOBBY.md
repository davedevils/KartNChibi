# 0x12 SHOW_LOBBY (Server → Client)

**Packet ID:** 0x12 (18 decimal)
**Handler:** `sub_4795A0` @ 0x004795A0

## Purpose

Triggers transition to **UI State 8** (Main Lobby).

##  CRITICAL WARNING

**DO NOT use as heartbeat response!** This packet changes the UI state!

## Payload Structure

```c
// No payload required - 0 bytes
// Client ignores any payload sent
```

## Handler Decompilation (IDA)

```c
void __thiscall sub_4795A0(void *this, int a2)
{
    if (dword_F727F4 != 2)  // Not disconnecting
    {
        sub_484010(this);                    // Some initialization
        sub_404410(dword_B23288, 8);         // *** SET UI STATE TO 8 (LOBBY) ***
        sub_4538B0();                        // Sync/ack
        
        // Check vehicle durability for warnings
        if (dword_B23610 == 9 || dword_B23610 == 11)
        {
            v2 = sub_44F450(dword_1A576D8, dword_1A20B1C);
            if (v2 && *(_DWORD *)(v2 + 44) == 3)
            {
                v3 = *(_DWORD *)(v2 + 48);  // Durability
                if (v3 > 0 && v3 <= 10)
                    sub_4641E0(byte_F727E8, "MSG_DURABILITY_LOW", 1);
                else if (v3 <= 0)
                    sub_4641E0(byte_F727E8, "MSG_DURABILITY_ZERO", 1);
            }
        }
    }
}
```

## Behavior

1. **Connection Check**: Verifies `dword_F727F4 != 2`
2. **Initialize**: Calls `sub_484010` for setup
3. **State Change**: `sub_404410(state, 8)` → **LOBBY**
4. **Durability Check**: Shows warnings if vehicle durability is low

## Important Variables

| Variable | Description |
|----------|-------------|
| `dword_F727F4` | Connection state (2 = disconnecting) |
| `dword_B23288` | UI manager object |
| `dword_B23610` | Current game mode (9, 11 = racing modes?) |

## Messages

| Message | Condition |
|---------|-----------|
| MSG_DURABILITY_LOW | Vehicle durability 1-10% |
| MSG_DURABILITY_ZERO | Vehicle durability 0% |

## ❌ Wrong Usage (Causes Issues!)

```cpp
// DON'T DO THIS on LoginServer!
void handleHeartbeat(Session::Ptr session) {
    Packet pkt(0x12);  // WRONG! This sends player to lobby!
    session->send(pkt);
}
```

This was the cause of the bug where players skipped character creation!

## ✅ Correct Usage

```cpp
// Only send when you WANT player to go to lobby
void sendShowLobby(Session::Ptr session) {
    Packet pkt(0x12);
    // No payload needed
    session->send(pkt);
}

// For heartbeats on LoginServer:
void handleHeartbeat(Session::Ptr session) {
    // Just ignore it! No response needed!
    LOG_DEBUG("AUTH", "Heartbeat ignored");
}
```

## When to Use

- After successful character selection/creation
- After completing a race
- Returning from garage/shop
- **NOT** as heartbeat response!

