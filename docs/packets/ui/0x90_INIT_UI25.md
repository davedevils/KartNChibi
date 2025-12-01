# 0x90 INIT_UI25 (Server → Client)

**Handler:** `sub_47DF30`

## Purpose

Initializes UI state 25.

## Payload Structure

```c
struct InitUI25 {
    int32 param1;
    int32 param2;
};
```

## Size

**8 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Parameter 1 |
| 0x04 | int32 | 4 | Parameter 2 |

## Notes

- Calls `sub_4538B0` first
- Triggers UI state 25

