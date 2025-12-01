# CMD 0x62 (98) - Tutorial Initialize Fail

**Direction:** Server → Client

## Structure

```c
struct TutorialFail {
    // No payload
};
```

## Raw Packet

```
04 00 62 00 00 00 00 00 00 00 00 00
```

## Behavior

Client shows "stage tutorial initialize fail" message.

## Notes

- Sent when tutorial mode cannot start
- May indicate missing assets or invalid state

