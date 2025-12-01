# 0x28 ENTITY_DATA (Server → Client)

**Handler:** `sub_479B20`

## Purpose

Sends entity data block.

## Payload Structure

```c
struct EntityData {
    int8 data[104];          // 0x68 bytes
};
```

## Size

**104 bytes** (0x68)

## Notes

- Fixed size entity data
- Used for room/game entity sync

