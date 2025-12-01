# CMD 0x12 (18) - SHOW_ROOM / Heartbeat Response

**Direction:** Server → Client

## ⚠️ CRITICAL WARNING

**DO NOT use 0x12 as heartbeat response on LoginServer!**

In the client's packet handler switch (case 18 = 0x12), this CMD calls:
```c
sub_4795A0() → sub_404410(state, 8)  // Goes to LOBBY!
```

Sending 0x12 will **immediately transition the client to the Lobby state**, regardless of the current flow!

## Correct Behavior

| Server | Heartbeat (0xA6) Response |
|--------|---------------------------|
| **LoginServer** | ❌ DO NOT RESPOND - just ignore heartbeats |
| **GameServer** | May need response during gameplay (TBD) |

## Original Documentation (from captures)

The original server may have used flag differentiation:

| Packet | Header Flag | Purpose |
|--------|-------------|---------|
| 0x12 flag=0x00 | Heartbeat response? | Keep-alive (unconfirmed) |
| 0x12 flag=0x01 | UI transition | Show Room screen |

However, the decompiled client code shows **case 18 always calls the UI transition function**, suggesting the flag differentiation may not work as expected.

## Structure (if used for UI)

```c
struct ShowRoom {
    // Triggers state change to Room/Lobby
    // Payload content TBD
};
```

## Lesson Learned

During development, responding to heartbeats with 0x12 caused the client to jump directly to Lobby, skipping character creation and channel selection entirely.

**Solution**: LoginServer ignores heartbeats (no response). The heartbeat is just a keep-alive signal that doesn't require acknowledgment during login flow.

