# 0x6C PLAYER_MESSAGE (Server → Client)

**Packet ID:** 0x6C (108 decimal)  
**Handler:** `sub_47B030` / `FUN_0047b030`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sends a message with player information (whisper/private message with UI context).

## Payload Structure

```c
struct PlayerMessage {
    int32   userId;           // Sender user ID
    wchar_t nickname[13];     // wstring (null-term UTF-16LE)
    char    asciiData[34];    // ASCII string (null-term)
    int32   field1;           // Unknown field
    int32   roomId;           // Room ID for context
    wchar_t message[9];       // wstring message
    byte    flag;             // Display flag
};
```

## Size

**Variable:** ~17 + wstring + string + wstring bytes

Minimum: ~25 bytes

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | User ID (F33A18) |
| 0x04 | wstring | var | Nickname (F33A1C) |
| +0x00 | string | var | ASCII data (F33A36) |
| +0x00 | int32 | 4 | Field (F33A58) |
| +0x04 | int32 | 4 | Room ID (F33A5C) |
| +0x08 | wstring | var | Message (F33A60) |
| +0x00 | byte | 1 | Flag (F33A72) |

## Handler Logic

```c
// IDA: sub_47B030 / Ghidra: FUN_0047b030
read(&userId, 4);
read_wstring(&nickname);
read_string(&asciiData);
read(&field1, 4);
read(&roomId, 4);
read_wstring(&message);
read(&flag, 1);

if (uiState == 11 || uiState == 15) {
    showMessage(userId, 6);
} else if (!flag) {
    if (uiState == 9 && roomId == currentRoomId) {
        showMessage(userId, 7);
    } else if (someFlag1) {
        showMessage(userId, 4);
    } else if (someFlag2 || someFlag3) {
        showMessage(userId, 5);
    } else {
        playSoundEffect();
    }
} else {
    // flag set - different handling
    if (someFlag1) showMessage(userId, 4);
    else if (someFlag2 || someFlag3) showMessage(userId, 5);
    else playSoundEffect();
}
```

## Message Types

| Type | Context |
|------|---------|
| 4 | Friend request/notification |
| 5 | General notification |
| 6 | Lobby message (UI state 11/15) |
| 7 | Room message (same room) |

## Server Implementation

```cpp
void sendPlayerMessage(Session::Ptr session, 
                       int userId, 
                       const std::wstring& nick,
                       const std::string& data,
                       int field1, int roomId,
                       const std::wstring& msg,
                       bool flag) {
    Packet pkt(0x6C);
    pkt.writeInt32(userId);
    pkt.writeWString(nick);
    pkt.writeString(data);
    pkt.writeInt32(field1);
    pkt.writeInt32(roomId);
    pkt.writeWString(msg);
    pkt.writeUInt8(flag ? 1 : 0);
    session->send(pkt);
}
```

## Notes

- Complex message packet with context-aware display
- UI state determines how message is shown
- flag byte affects handling logic
- Used for whispers, notifications, friend requests

