# CMD 0x8E-0x90 - Init Sequence Responses

**Direction:** Server → Client

## Overview

These packets are part of the initial connection sequence.

## 0x8E - Response to 0xFA

```
04 00 8E 00 00 00 00 00 00 00 00 00
```

Acknowledges the full client state packet (0xFA).

## 0x8F - Response to 0x07

```
04 00 8F 00 00 00 00 00 00 00 00 00
```

Acknowledges the client auth info packet (0x07).

## 0x90 - Unknown

```
04 00 90 00 00 00 00 00 00 00 00 00
```

Part of init sequence.

## Connection Sequence

```
Client                          Server
   |                               |
   |--- 0xA6 (Heartbeat) --------->|
   |<-- 0x12 (Heartbeat Resp) -----|
   |                               |
   |--- 0xFA (Full State) -------->|
   |<-- 0x8E (Ack) ----------------|
   |                               |
   |--- 0x07 (Auth Info) --------->|
   |<-- 0x8F (Ack) ----------------|
   |                               |
   |<-- 0x0E (Show Menu) ----------|  // Flag=0x01
```

## Notes

- All responses have no payload (size=4, just the int32 field)
- Order may vary slightly
- Missing ack may cause client timeout

