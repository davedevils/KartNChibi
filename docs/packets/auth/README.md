# Auth Packets

Authentication, session management, and heartbeat.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x01](0x01_LOGIN_RESPONSE.md) | Login Response | Login result |
| [0x02](0x02_DISPLAY_MESSAGE.md) | Display Message | Show message |
| [0x03](0x03_TRIGGER.md) | Trigger | State trigger (0 bytes) |
| [0x04](0x04_REGISTRATION_RESPONSE.md) | Registration | Account creation response |
| [0x0A](0x0A_CONNECTION_OK.md) | Connection OK | Initial handshake (38 bytes) |
| [0x0B](0x0B_ACK.md) | ACK | Simple acknowledgment |
| [0x0C](0x0C_DATA_PAIRS.md) | Data Pairs | Key-value list |
| [0x0D](0x0D_FLAG_SET.md) | Flag Set | Set internal flag |
| [0x12](0x12_HEARTBEAT_RESPONSE.md) | Heartbeat Response | Ack heartbeat |
| [0x8E-0x90](0x8E_INIT_RESPONSE.md) | Init Responses | Connection init acks |
| [0xAA](0xAA_PLAYER_PREVIEW.md) | Player Preview | Player profile data |

## Client → Server

| CMD | Name | Description |
|-----|------|-------------|
| [0x07](../client/0x07_CLIENT_AUTH.md) | Client Auth | Auth info |
| [0xA6](0xA6_HEARTBEAT.md) | Heartbeat | Keep-alive |

## Connection Flow (from captures)

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
   |<-- 0x0E flag=0x01 (Menu) -----|
   |                               |
   |--- 0xA6 (Heartbeat) --------->|  (every 1000ms)
   |<-- 0x12 (Heartbeat Resp) -----|
```
