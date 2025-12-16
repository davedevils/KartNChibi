# 0x9D - ADD_VEHICLE

**CMD**: `0x9D` (157 decimal)  
**Direction**: Server → Client  
**Handler IDA**: `sub_47C2D0`  
**Handler Ghidra**: `FUN_0047c2d0`

## Description

Adds a new vehicle to the player's garage. Contains a full VehicleData structure.

## Payload Structure

| Offset | Type        | Size | Description           |
|--------|-------------|------|-----------------------|
| 0x00   | VehicleData | 44   | Vehicle data          |

**Total Size**: 44 bytes (0x2C)

## C Structure

```c
struct AddVehiclePacket {
    VehicleData vehicle;        // +0x00 - Full vehicle data (44 bytes)
};
```

## Handler Logic (IDA)

```c
// sub_47C2D0
char __thiscall sub_47C2D0(void *this, int a2)
{
    int v5[11];  // VehicleData buffer (44 bytes)
    
    sub_44E910(a2, v5, 0x2C);   // Read 44 bytes
    
    int result = sub_44FCB0((int*)((char*)&unk_844720 + this), v5);
    if (result < 0)
        sub_4641E0(byte_F727E8, "MSG_UNKNOWN_ERROR", 2);
    
    return result;
}
```

## Notes

- If addition fails (result < 0), displays "MSG_UNKNOWN_ERROR"
- Vehicle is added to internal list at offset `unk_844720`

## Cross-Validation

| Source | Function       | Payload Read |
|--------|----------------|--------------|
| IDA    | sub_47C2D0     | 44 bytes     |
| Ghidra | FUN_0047c2d0   | 44 bytes     |

**Status**: ✅ CERTIFIED
