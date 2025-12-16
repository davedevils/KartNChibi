# 0x0E CHANNEL_LIST / SHOW_MENU (Server → Client)

**Status:** ✅ CERTIFIÉ (IDA + Ghidra)

**Handler IDA:** `sub_4793F0` @ line 220146
**Handler Ghidra:** `FUN_004793f0` @ line 84677

## Purpose

Sends channel/server list for selection. Sets UI state to 4 (CHANNEL_SELECT).

## Payload Structure

```c
struct ChannelList {
    int32_t count;                  // Number of channels
    ChannelInfo channels[count];    // Channel entries
    wchar_t footer[];               // Footer wstring (can be empty)
};

struct ChannelInfo {
    int32_t channelId;              // Channel ID
    wchar_t name[];                 // Channel name (wstring, null-terminated)
    int32_t currentPlayers;         // Current player count
    int32_t maxPlayers;             // Maximum players
    int32_t type;                   // Channel type (0=Rookie, 1=Advanced, 2=Master)
};
```

## Field Details

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Channel count |
| **For each channel:** | | | |
| +0x00 | int32 | 4 | Channel ID |
| +0x04 | wstring | var | Channel name (UTF-16LE null-term) |
| +var | int32 | 4 | Current players |
| +var | int32 | 4 | Max players |
| +var | int32 | 4 | Channel type |
| **After all channels:** | | | |
| +var | wstring | var | Footer (empty wstring for just null) |

## Handler Code (IDA)

```c
char __stdcall sub_4793F0(const wchar_t **a1)
{
  int count;      // v4
  int i;          // v1
  int channelId;  // v7
  int current;    // v6
  int max;        // v5
  int type;       // v8
  wchar_t name[256];

  sub_44E910((int)a1, &count, (const char *)4);  // Read count
  sub_424210(dword_BFE6B8);                       // Clear channel list

  if (count <= 0) goto DONE;
  
  for (i = 0; i < count; i++) {
    sub_44E910((int)a1, &channelId, (const char *)4);  // Channel ID
    sub_44EB60(a1, name);                               // Name (wstring)
    sub_44E910((int)a1, &current, (const char *)4);    // Current players
    sub_44E910((int)a1, &max, (const char *)4);        // Max players
    sub_44E910((int)a1, &type, (const char *)4);       // Type
    
    if (!sub_424250(dword_BFE6B8, channelId, name, current, max, type))
      return sub_4641E0(byte_F727E8, "MSG_UNKNOWN_ERROR", 2);
  }

DONE:
  sub_44EB60(a1, &unk_C1A718);    // Read footer wstring
  sub_424320((int)dword_BFE6B8); // Finalize list
  sub_404410(dword_B23288, 4);   // Set UI state = 4
  return sub_4538B0();           // UI transition
}
```

## Handler Code (Ghidra)

```c
void FUN_004793f0(void)
{
  int count;
  int i;
  int channelId;
  int current, max, type;
  char name[512];
  
  FUN_0044e910(&count, 4);
  FUN_00424210();
  
  if (count > 0) {
    i = 0;
    do {
      FUN_0044e910(&channelId, 4);
      FUN_0044eb60(name);
      FUN_0044e910(&current, 4);
      FUN_0044e910(&max, 4);
      FUN_0044e910(&type, 4);
      
      if (FUN_00424250(channelId, name, current, max, type) == '\0') {
        FUN_004641e0("MSG_UNKNOWN_ERROR", 2);
        return;
      }
      i++;
    } while (i < count);
  }
  
  FUN_0044eb60(&DAT_00c1a718);  // Footer
  FUN_00424320();
  FUN_00404410(4);              // UI state = 4
  FUN_004538b0();
}
```

## Channel Types

| Type | Name | Description |
|------|------|-------------|
| 0 | Rookie | Beginner channel |
| 1 | Advanced | Intermediate channel |
| 2 | Master | Expert channel |

## Server Implementation

```cpp
void sendChannelList(Session::Ptr session, const std::vector<ChannelInfo>& channels) {
    Packet pkt(0x0E);
    
    // Channel count
    pkt.writeInt32(channels.size());
    
    // Each channel
    for (const auto& ch : channels) {
        pkt.writeInt32(ch.id);
        pkt.writeWString(ch.name);
        pkt.writeInt32(ch.currentPlayers);
        pkt.writeInt32(ch.maxPlayers);
        pkt.writeInt32(ch.type);
    }
    
    // Footer (empty wstring)
    pkt.writeUInt16(0);  // null terminator only
    
    session->send(pkt);
}

// Example usage:
std::vector<ChannelInfo> channels = {
    {1, u"Rookie Channel", 50, 100, 0},
    {2, u"Advanced Channel", 30, 100, 1},
    {3, u"Master Channel", 10, 50, 2}
};
sendChannelList(session, channels);
```

## Sequence

```
Client                              Server
  │                                   │
  │  ◄════ CMD 0x07 ═══════════════  │  Session info
  │  ◄════ CMD 0x02 ═══════════════  │  Connection confirm
  │  ◄════ CMD 0x8F ═══════════════  │  UI state 24
  │                                   │
  │  ◄════ CMD 0x0E ═══════════════  │  ← THIS PACKET
  │       [count:4]                   │
  │       FOR EACH:                   │
  │         [id:4][name:ws]           │
  │         [cur:4][max:4][type:4]    │
  │       [footer:ws]                 │
  │                                   │
  │  [UI state = 4 (CHANNEL_SELECT)]  │
  │  [Shows channel selection UI]     │
  │                                   │
```

## Notes

- Sets UI state to **4** (CHANNEL_SELECT)
- Footer wstring stored at address 0xC1A718
- Channel list stored in dword_BFE6B8 structure
- On error, shows "MSG_UNKNOWN_ERROR" popup with disconnect (code=2)

## Cross-Reference

- User selects channel → [0x19_SERVER_QUERY.md](../client/0x19_SERVER_QUERY.md)
- Server redirects → [0x54_SERVER_REDIRECT.md](../game/0x54_SERVER_REDIRECT.md)
