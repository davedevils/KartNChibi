# CMD 0x01 - Login Response

**Direction:** Server → Client  
**Handler:** `sub_478DA0`

## Structure

```c
struct LoginResponse {
    char message[256];    // Null-terminated ASCII string
    int32 serverCode;     // Status code for UI
};
```

## Fields

| Offset | Type | Name | Description |
|--------|------|------|-------------|
| 0x00 | string | message | Error message or empty for success |
| 0x100 | int32 | serverCode | Numeric status code |

## Messages

| Message | Meaning | Client Action |
|---------|---------|---------------|
| `""` (empty) | Success | Continue to lobby |
| `MSG_SERVER_NOT_READY` | Server starting | Show wait dialog, retry |
| `MSG_DB_ACCESS_FAIL` | Database error | Show error, return to login |
| `MSG_REINPUT_IDPASS` | Wrong password | Show error, retry login |
| `MSG_INVALID_ID` | User not found | Show error, retry login |

## Example

**Success:**
```
01 00 00 00 00 00 00 00  // CMD 0x01, empty payload
00                        // Empty message (just null byte)
00 00 00 00              // serverCode = 0
```

**Error (wrong password):**
```
01 XX XX 00 00 00 00 00  // CMD 0x01
4D 53 47 5F 52 45 49 4E  // "MSG_REIN"
50 55 54 5F 49 44 50 41  // "PUT_IDPA"
53 53 00                  // "SS\0"
01 00 00 00              // serverCode = 1
```

## Notes

- If message contains error string, client shows dialog and triggers error callback
- `MSG_SERVER_NOT_READY` sets a global flag and starts a retry timer
- Empty message = success, proceed to next state

