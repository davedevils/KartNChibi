# CMD 0x0F (15) - Show Garage

**Direction:** Server → Client  
**Handler:** `sub_479520`

## Structure

```c
// No payload
```

## Behavior

1. Calls `sub_484010()` - Initialize garage data
2. Sets game state to **6 (GARAGE)**
3. Calls `sub_4538B0()` - Refresh UI

## Raw Packet

```
0F 00 00 00 00 00 00 00
```

## Notes

- Simple state transition, no data required
- Garage shows player's vehicles
- Player can equip/repair vehicles here

