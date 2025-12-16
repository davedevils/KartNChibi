# 0xB4 - SYSTEM_MESSAGE

**CMD**: `0xB4` (180 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47CD60`  
**Handler Ghidra**: `FUN_0047cd60`

## Description

Displays a system message to the player. Supports different message types and formatting with player name substitution.

## Payload Structure

| Offset | Type    | Size     | Description                    |
|--------|---------|----------|--------------------------------|
| 0x00   | int32   | 4        | Player/sender ID               |
| 0x04   | wstring | variable | Format string (UTF-16LE, null-term) |
| ?      | wstring | variable | Message content (UTF-16LE, null-term) |
| ?      | int32   | 4        | Message type                   |

**Total Size**: Variable (12 + format length + message length)

## C Structure

```c
struct SystemMessagePacket {
    int32_t senderId;           // +0x00 - Player ID or 0 for system
    wchar_t format[];           // +0x04 - Format string
    // After format string:
    wchar_t message[];          // Message content
    // After message string:
    int32_t messageType;        // Message type/destination
};
```

## Message Types

| Type | Context | Behavior                                    |
|------|---------|---------------------------------------------|
| 0    | UI 8    | Display formatted message in lobby chat     |
| 0    | UI 9    | Display in room + update player data        |
| 0    | UI 11   | Display in game overlay                     |
| 2    | UI 8    | Display message directly in lobby           |
| 2    | UI 9    | Display message directly in room            |
| 2    | UI 11   | Display message directly in game            |

## Handler Logic (IDA)

```c
// sub_47CD60
char __stdcall sub_47CD60(const wchar_t **a1)
{
    int senderId;
    wchar_t format[14];
    wchar_t message[8192];
    wchar_t buffer[256];
    int messageType;
    
    sub_44E910(a1, &senderId, 4);       // Read sender ID
    sub_44EB60(a1, format);             // Read format wstring
    sub_44EB60(a1, message);            // Read message wstring
    sub_44E910(a1, &messageType, 4);    // Read message type
    
    switch (messageType) {
        case 0:
            // Format: swprintf(buffer, format, message)
            swprintf(buffer, format, message);
            // Display based on current UI state
            if (dword_B2360C == 8)  // Lobby
                sub_4088C0(&unk_B71FC8, buffer, messageType);
            else if (dword_B2360C == 9)  // Room
                sub_4088C0(&unk_BB7BB0, buffer, messageType);
                sub_40D540(dword_B9ADD0, senderId, message);
            else if (dword_B2360C == 11)  // Game
                sub_4016E0(&unk_B0C8B0, buffer, messageType);
            break;
            
        case 2:
            // Direct message (no formatting)
            // Similar routing based on UI state
            break;
    }
}
```

## UI State Reference

| UI State | Description |
|----------|-------------|
| 8        | Lobby       |
| 9        | Room        |
| 11       | In-Game     |

## Cross-Validation

| Source | Function       | Payload Read                      |
|--------|----------------|-----------------------------------|
| IDA    | sub_47CD60     | 4 + wstring + wstring + 4         |
| Ghidra | FUN_0047cd60   | 4 + wstring + wstring + 4         |

**Status**: ✅ CERTIFIED

