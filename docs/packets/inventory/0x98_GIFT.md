# 0x98 GIFT (Server → Client)

**Handler:** `sub_47BEF0`

## Purpose

Notifies about a gift received.

## Payload Structure

```c
struct Gift {
    int32 giftData[55];      // 220 bytes total
};
```

## Size

**220 bytes**

## Notes

- Fixed size gift data
- Triggers `MSG_GIFT_SEND` message
- Contains sender info and item details

