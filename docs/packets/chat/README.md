# Chat Packets

Chat messaging and player list.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x2A](0x2A_WHISPER_ENABLE.md) | Whisper Enable | Enable whisper (0 bytes) |
| [0x2B](0x2B_WHISPER_DISABLE.md) | Whisper Disable | Disable whisper (0 bytes) |
| [0x2D](0x2D_CHAT_MESSAGE.md) | Chat Message | Receive chat |
| [0x2E](0x2E_PLAYER_LEFT.md) | Player Left | Player left chat |

## Client → Server

| CMD | Name | Description |
|-----|------|-------------|
| 0x2D | Chat Message | Send chat |
| 0x2F | Whisper | Private message |

## String Encoding

Chat uses **UTF-16LE** (wstring) for message content.
Player names are typically ASCII.
