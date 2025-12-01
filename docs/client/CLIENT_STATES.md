# KnC Client States

## Overview
The client uses a state machine managed by `sub_404410(dword_B23288, state)`.
Each state corresponds to a different UI screen.

## State Machine
Located at: `dword_B23288` (game state manager)

## States List

| State | Name | Handler | Error Message | Trigger Packet |
|-------|------|---------|---------------|----------------|
| 1 | Logo | sub_411B90 | Stage logo initialize fail | - |
| 2 | Title | sub_411EA0 | Stage title initialize fail | - |
| 3 | Login | sub_426BB0 | Stage login initialize fail | None (initial state w/o launcher) |
| 4 | Channel | sub_4241A0 | Stage channel initialize fail | 0x0E |
| 5 | Menu | sub_412390 | Stage menu initialize fail | 0x11 |
| 6 | Garage | sub_412970 | Stage garage initialize fail | 0x0F |
| 7 | Shop | sub_417360 | Stage shop initialize fail | 0x10 |
| 8 | Lobby | sub_408BC0 | Stage lobby initialize fail | 0x12 |
| 9 | Room | sub_4101D0 | Stage room initialize fail | - |
| 11 | Racing | sub_401930 | - | - |
| 13 | Unknown | sub_41ADD0 | - | - |
| 14 | Unknown | sub_436BE0 | - | - |
| 15 | Unknown | sub_4250D0 | - | - |
| 17 | Unknown | sub_42D4A0 | - | - |
| 18 | Unknown | sub_42FA80 | - | - |
| 19 | Unknown | sub_433720 | - | - |
| 22 | Unknown | sub_437DE0 | - | - |
| 23 | Unknown | sub_438FB0 | - | - |
| 24 | Unknown | sub_43B620 | - | - |
| 25 | Unknown | sub_439CC0 | - | - |
| 26 | Unknown | sub_43C8D0 | - | - |

## Key State Transitions

### Login Flow (with Launcher)
```
[Connect] → State 4 (Channel) → State 8 (Lobby)
```

### Login Flow (without Launcher)
```
State 3 (Login) → State 4 (Channel) → State 8 (Lobby)
```

### Character Creation Flow
```
State 4 (Channel) → [No char] → Character Creation UI → State 4 → State 8
```

## State Trigger Packets

### 0x0E - Channel List
Sets state to **4** (Channel Selection)
```c
sub_404410(dword_B23288, 4);
```

### 0x11 - Show Menu
Sets state to **5** (Menu)
```c
sub_404410(dword_B23288, 5);
```

### 0x12 - Show Lobby
Sets state to **8** (Lobby)
```c
sub_404410(dword_B23288, 8);
```

## Connection State Variable
`dword_F727F4` - Connection state flag:
- **0** = Not connected
- **1** = Connected (set by packet 0x02 with code=1)
- **2** = Disconnected (set by packet 0x02 with code=2)

Most UI handlers check `dword_F727F4 != 2` before processing.

## Notes
- State 3 (Login) is for standalone client without launcher
- With OGPlanet launcher, client skips state 3 entirely
- State changes are atomic - old state is cleaned up, new state is initialized

