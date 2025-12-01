# CMD 0x38 (56) - Item Hit

**Direction:** Server → Client

## Structure

```c
struct ItemHit {
    int32 victimId;
    int32 attackerId;
    int32 itemId;
    int32 damage;
};
```

## Fields

| Offset | Size | Type | Name |
|--------|------|------|------|
| 0x00 | 4 | int32 | victimId |
| 0x04 | 4 | int32 | attackerId |
| 0x08 | 4 | int32 | itemId |
| 0x0C | 4 | int32 | damage |

## Damage Values

| Value | Effect |
|-------|--------|
| 0 | No effect (blocked) |
| 1 | Light hit |
| 2 | Medium hit (spin) |
| 3 | Heavy hit (crash) |

## Notes

- Client plays hit animation
- May trigger slowdown/stun
- `attackerId` = -1 for environment hazards

