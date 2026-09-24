# 0x18 STRING_PARAMS

`0x18` (24). Server -> Client. Variable size. Sends string params for UI config.

**Status:** CERTIFIED (IDA + Ghidra)

**Handler:** `sub_47C7A0` / `FUN_0047c7a0`

## Payload

```c
struct StringParams {
  int32 unused;
  char text[256];
  int32 param1;
  int32 param2;
};
```

## Size

Variable: 4 + string + 4 + 4. Min ~10 bytes.

## Fields

| Offset | Type | Size | Field |
|--------|------|------|-------|
| 0x00 | int32 | 4 | Unused (read but discarded) |
| 0x04 | string | var | ASCII text null-terminated |
| +0x00 | int32 | 4 | Param 1 |
| +0x04 | int32 | 4 | Param 2 |

## Handler

```c
read(&unused, 4);
read_string(&text);
read(&param1, 4);
read(&param2, 4);
updateUI(text, param1, param2);
```

## Server Impl

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

- ASCII string not wstring
- First int32 read but not used
