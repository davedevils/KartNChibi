# CMD 0x1C (28) - Ghost Lobby / Inventory B

**Direction:** Server → Client

## Dual Purpose

This CMD has two uses depending on flags:

### Flag 0x00 - Inventory B (Item List)

```
04 00 1C 00 00 00 00 00 ...
```

See [0x1C_INVENTORY_B.md](0x1C_INVENTORY_B.md) for full structure.

### Flag 0x01 - Ghost Lobby Tab

```
04 00 1C 01 00 00 00 00 00 00 00 00
```

From capture:
```
2024-06-26 02:29:30,934 INFO: Responding with: 04001c010000000000000000
```

## Notes

- Flag byte at offset [3] determines behavior
- 0x00 = inventory data follows
- 0x01 = UI transition to ghost lobby tab

