# Client → Server Packets

Packets sent by the client to the server.

| CMD | Name | Description |
|-----|------|-------------|
| [0x07](0x07_CLIENT_AUTH.md) | Client Auth | Auth info after connect |
| [0x19](0x19_SERVER_QUERY.md) | Server Query | Request server info |
| [0x2C](0x2C_STATE_CHANGE.md) | State Change | Change game state |
| [0x2D](../chat/0x2D_CHAT_MESSAGE.md) | Chat Message | Send chat |
| [0x32](0x32_UNKNOWN.md) | Unknown | Unknown request |
| [0x4D](0x4D_REQUEST_DATA.md) | Request Data | Request 276-byte data |
| [0x73](0x73_DISCONNECT.md) | Disconnect | Client disconnect |
| [0xA6](../auth/0xA6_HEARTBEAT.md) | Heartbeat | Keep-alive |
| [0xD0](0xD0_CLIENT_INFO.md) | Client Info | Client config |
| [0xFA](0xFA_FULL_STATE.md) | Full State | Full client state |

