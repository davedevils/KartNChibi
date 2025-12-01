# 0x18 STRING_PARAMS (Server → Client)

**Handler:** `sub_47C7A0`

## Purpose

Sends a string with additional integer parameters.

## Payload Structure

```c
struct StringParams {
    int32 param1;
    char  string[256];
    int32 param2;
    int32 param3;
};
```

## Size

**~268 bytes**

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Parameter 1 |
| 0x04 | string | 256 | ASCII string |
| 0x104 | int32 | 4 | Parameter 2 |
| 0x108 | int32 | 4 | Parameter 3 |

## Notes

- Used for parameterized messages
- Calls `sub_405EB0` to display

