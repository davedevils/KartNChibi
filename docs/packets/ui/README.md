# UI Packets

Screen transitions and UI state management.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x0E](0x0E_SHOW_MENU.md) | Show Menu | Main menu |
| [0x0F](0x0F_SHOW_GARAGE.md) | Show Garage | Garage screen |
| [0x10](0x10_SHOW_SHOP.md) | Show Shop | Shop screen |
| [0x11](0x11_SHOW_LOBBY.md) | Show Lobby | Game lobby |
| [0x12](0x12_SHOW_ROOM.md) | Show Room | Room screen |
| [0x13](0x13_PLAYER_ROOM_DATA.md) | Player/Room Data | Data for lists |
| 0x15 | Show Loading | Loading screen |
| [0x16](0x16_UI_STATE_14.md) | UI State 14 | State transition |
| [0x18](0x18_STRING_PARAMS.md) | String Params | Parameterized text |
| [0x8F](0x8F_ACK_UI24.md) | ACK UI24 | Trigger UI 24 |
| [0x90](0x90_INIT_UI25.md) | Init UI25 | Trigger UI 25 |
| [0xB4](0xB4_SYSTEM_MESSAGE.md) | System Message | Formatted message |
| [0xB5](0xB5_PLAYER_COMPARISON.md) | **Player Compare** | 2×PlayerInfo (2456+ bytes!) |
| [0xB6](0xB6_DISPLAY_TEXT.md) | Display Text | Show text |

## Screen State Machine

```
       ┌─────────────────────────────────────────┐
       │                                         │
       ▼                                         │
  ┌─────────┐    0x0F    ┌─────────┐           │
  │  MENU   │───────────>│ GARAGE  │           │
  │  0x0E   │<───────────│  0x0F   │           │
  └────┬────┘            └─────────┘           │
       │                                         │
       │ 0x10                                    │
       ▼                                         │
  ┌─────────┐                                   │
  │  SHOP   │                                   │
  │  0x10   │                                   │
  └────┬────┘                                   │
       │                                         │
       │ 0x11                                    │
       ▼                                         │
  ┌─────────┐    0x12    ┌─────────┐   0x14    │
  │  LOBBY  │───────────>│  ROOM   │──────────│
  │  0x11   │            │  0x12   │  (race)  │
  └─────────┘            └─────────┘           │
                              │                 │
                              └─────────────────┘
                              (after race)
```
