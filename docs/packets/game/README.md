# Game / Race Packets

Race gameplay, position sync, and results.

## Server → Client

| CMD | Name | Description |
|-----|------|-------------|
| [0x14](0x14_RACE_START.md) | Race Start | Begin race |
| [0x31](0x31_POSITION.md) | Position | Position update |
| [0x33](0x33_GAME_STATE.md) | Game State | State change |
| [0x35](0x35_SCORE.md) | Score | Score update |
| [0x36](0x36_LAP_INFO.md) | Lap Info | Lap/checkpoint |
| [0x37](0x37_ITEM_USE.md) | Item Use | Item activated |
| [0x38](0x38_ITEM_HIT.md) | Item Hit | Player hit by item |
| [0x39](0x39_FINISH.md) | Finish | Player finished race |
| [0x3A](0x3A_RESULTS.md) | Results | Race results |
| [0x3B](0x3B_COUNTDOWN.md) | Countdown | Start countdown |
| [0x3C](0x3C_RACE_END.md) | Race End | Race ended |
| [0x3D](0x3D_ROOM_STATE.md) | Room State | Room status (8 bytes) |
| [0x42](0x42_GAME_MODE.md) | Game Mode | Set mode (4 bytes) |
| [0x44](0x44_GAME_UPDATE.md) | Game Update | Update value |
| [0x45](0x45_ITEM_USAGE.md) | Item Usage | Item use notification |
| [0x46](0x46_LARGE_GAME_STATE.md) | Large State | ~1160 bytes! |
| [0x47](0x47_PLAYER_ACTION.md) | Player Action | Action (24 bytes) |
| [0x49](0x49_PLAYER_DATA.md) | Player Data | Race data (12 bytes) |
| [0x4E](0x4E_TIMESTAMP.md) | Timestamp | Time marker (0 bytes) |
| [0x54](0x54_SERVER_REDIRECT.md) | **Server Redirect** | Cross-server transfer |
| [0x57](0x57_RACE_STATUS.md) | Race Status | Status update |
| [0x58](0x58_PLAYER_STATUS.md) | Player Status | Status + flag |
| [0x64](0x64_ROOM_STATUS.md) | Room Status | Room counters |
| [0x65](0x65_SPEED_UPDATE.md) | Speed Update | Player speed |

## Client → Server

| CMD | Name | Description |
|-----|------|-------------|
| 0x4B | Position Sync | Send position |
| 0x4C | Item Request | Request item use |
| 0x4E | Checkpoint | Pass checkpoint |
| 0x4F | Finish Report | Report finish |
