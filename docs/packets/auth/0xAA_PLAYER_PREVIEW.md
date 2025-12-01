# 0xAA PLAYER_PREVIEW (Server → Client)

**Handler:** `sub_47CBA0`

## Purpose

Sends player preview data for display.

## Payload Structure

```c
struct PlayerPreview {
    int32       playerId;
    wchar_t     name[];          // wstring
    int32       unknown1;
    int32       unknown2;
    VehicleData vehicle;         // 0x2C (44 bytes)
    ItemData    item;            // 0x38 (56 bytes)
};
```

## Size

Variable (~108+ bytes)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | wstring | var | Player name |
| var | int32 | 4 | Unknown |
| var | int32 | 4 | Unknown |
| var | VehicleData | 44 | Vehicle data |
| var | ItemData | 56 | Item data |

## Notes

- Calls `sub_4538B0` first
- Triggers UI state 15
- Used for player profile/preview screens

