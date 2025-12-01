# 0x04 REGISTRATION_RESPONSE (Server → Client)

**Handler:** `sub_479230`

## Purpose

Response to player registration/nickname creation.

## Payload Structure

```c
struct RegistrationResponse {
    int32       resultCode;    // 0=success, 1-3=error
    // Only if resultCode == 0:
    wchar_t     nickname[];    // New nickname (wstring)
    VehicleData vehicle;       // 0x2C (44 bytes)
    int32       itemData[7];   // 28 bytes
};
```

## Result Codes

| Code | Message | Description |
|------|---------|-------------|
| 0 | - | Success |
| 1 | MSG_INVALID_NICK | Invalid nickname |
| 2 | MSG_ALREADY_REGIST | Already registered |
| 3 | MSG_INVALID_NICK | Invalid format |

## Fields (on success)

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Result code (0) |
| 0x04 | wstring | var | Player nickname |
| var | VehicleData | 44 | Initial vehicle |
| var | int32[7] | 28 | Initial items |

## Notes

- On error, only resultCode is sent
- On success, initializes player inventory

