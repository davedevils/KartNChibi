# 0x14 GAME_MODE (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_479CC0` @ line 220559
**Handler Ghidra:** `FUN_00479cc0`

## Purpose

Sets game mode and initializes race. Payload size varies based on mode.

## Payload Structure

### Base (4 bytes)
```c
struct GameModeBase {
    int32_t mode;    // Game mode
};
```

### Extended (24 bytes) - When mode == 3 or mode == 8
```c
struct GameModeExtended {
    int32_t mode;       // Game mode (3 or 8)
    int32_t unknown1;   // unk_B23170
    int32_t unknown2;   // dword_B23174
    int32_t unknown3;   // dword_B23178
    int32_t unknown4;   // dword_B23198
    int32_t unknown5;   // dword_B2319C
};  // Total: 24 bytes
```

## Handler Code (IDA)

```c
char __thiscall sub_479CC0(void *this, int a2)
{
  if (dword_F727F4 != 2) {
    sub_44E910(a2, &dword_B2316C, 4);  // Read mode
    
    if (dword_B2316C == 3 || dword_B2316C == 8) {
      // Extended mode - read 5 more int32
      sub_44E910(a2, &unk_B23170, 4);
      sub_44E910(a2, &dword_B23174, 4);
      sub_44E910(a2, &dword_B23178, 4);
      sub_44E910(a2, &dword_B23198, 4);
      sub_44E910(a2, &dword_B2319C, 4);
      
      sub_4538B0();
      sub_404410(dword_B23288, 11);  // Set UI state = 11 (RACING)
      *(int*)(&dword_80E1B0 + this) = -1;
    }
  }
  return result;
}
```

## Game Mode Values

| Mode | Name | Payload | UI State |
|------|------|---------|----------|
| 3 | Race (Normal) | 24 bytes | 11 (RACING) |
| 8 | Race (Special) | 24 bytes | 11 (RACING) |
| Other | Unknown | 4 bytes | No change |

## Server Implementation

```cpp
// Basic mode (non-race)
void sendGameMode(Session::Ptr session, int32_t mode) {
    Packet pkt(0x14);
    pkt.writeInt32(mode);
    session->send(pkt);
}

// Race mode (mode 3 or 8)
void sendRaceMode(Session::Ptr session, int32_t mode, 
                  int32_t u1, int32_t u2, int32_t u3, int32_t u4, int32_t u5) {
    Packet pkt(0x14);
    pkt.writeInt32(mode);  // Must be 3 or 8
    pkt.writeInt32(u1);
    pkt.writeInt32(u2);
    pkt.writeInt32(u3);
    pkt.writeInt32(u4);
    pkt.writeInt32(u5);
    session->send(pkt);
}
```

## Notes

- Requires `dword_F727F4 != 2` (connection confirmed)
- Mode 3 or 8 triggers race initialization
- Sets UI state to 11 (RACING) for race modes
