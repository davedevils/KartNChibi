# CMD 0x6F (111) - Shop Item Response

**Direction:** Server → Client  
**Handler:** `sub_47B3C0`

## Structure

```c
struct ShopItemResponse {
    int32 resultCode;
    // Payload varies by resultCode
};

// If resultCode == 1 (success)
struct ShopSuccess {
    int32 resultCode;     // = 1
    uint8 itemData[0x2C]; // 44 bytes - item added
};

// If resultCode == 12 (special)
struct ShopSpecial {
    int32 resultCode;     // = 12
    uint8 data[0x24];     // 36 bytes - special data
};
```

## Result Codes

| Code | Meaning | Payload |
|------|---------|---------|
| 1 | Success | 44 bytes item data |
| 12 | Special | 36 bytes special data |
| Other | Error | No additional payload |

## Fields (Success)

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | resultCode |
| 0x04 | 44 | struct | itemData |

## Behavior

- Code 1: Item added to inventory, UI updated
- Code 12: Special handling (gift box?)
- Other: Sets error state flag

## Notes

- Sent after purchase request
- Contains the purchased item data
- Client adds item to local inventory on success

