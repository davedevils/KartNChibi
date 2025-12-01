# 0x3D ROOM_STATE (Server → Client)

**Handler:** `sub_47A6B0`

## Purpose

Updates room state for a specific player.

## Payload Structure

```c
struct RoomState {
    int32 playerId;
    int32 state;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Player ID |
| 0x04 | int32 | 4 | New state value |

## Notes

- Uses `sub_48DEA0` to find player index
- Updates `sub_498F60` with new state
- Triggers sound effect on state change

