# 0x18 STRING_PARAMS (Server → Client)

**Packet ID:** 0x18 (24 decimal)  
**Handler:** `sub_47C7A0` / `FUN_0047c7a0`  
**Status:** ✅ CERTIFIÉ IDA+Ghidra

## Purpose

Sends string parameters for UI display/configuration.

## Payload Structure

```c
struct StringParams {
    int32  unused;      // Read but stored locally (not used)
    char   text[256];   // Null-terminated ASCII string
    int32  param1;      // Parameter 1
    int32  param2;      // Parameter 2
};
```

## Size

**Variable:** 4 + string + 4 + 4

Minimum: ~10 bytes (empty string)

## Fields

| Offset | Type | Size | Description |
|--------|------|------|-------------|
| 0x00 | int32 | 4 | Unused field |
| 0x04 | string | var | ASCII text (null-terminated) |
| +0x00 | int32 | 4 | Parameter 1 |
| +0x04 | int32 | 4 | Parameter 2 |

## Handler Logic

```c
// IDA: sub_47C7A0 / Ghidra: FUN_0047c7a0
read(&unused, 4);
read_string(&text);   // ASCII string
read(&param1, 4);
read(&param2, 4);
updateUI(text, param1, param2);
```

## Server Implementation

```cpp
void sendStringParams(Session::Ptr session, 
                      int unused,
                      const std::string& text,
                      int param1, int param2) {
    Packet pkt(0x18);
    pkt.writeInt32(unused);
    pkt.writeString(text);
    pkt.writeInt32(param1);
    pkt.writeInt32(param2);
    session->send(pkt);
}
```

## Notes

- Uses ASCII string (not wstring)
- First int32 is read but not used directly
- Used for UI text/parameter configuration
