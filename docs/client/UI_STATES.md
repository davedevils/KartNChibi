# Client UI States

From reverse engineering.

## Full State List

| State | Hex | Stage Name | Init Function | Desc |
|-------|-----|------------|---------------|------|
| 0 | 0x00 | Logo | FUN_00411e20 | Splash/logo |
| 1 | 0x01 | Title | FUN_00411f70 | Title screen |
| 2 | 0x02 | Login | FUN_00427190 | Login screen |
| 3 | 0x03 | Channel | FUN_00424b00 | Channel/server select |
| 4 | 0x04 | Menu | FUN_00412530 | Main menu |
| 5 | 0x05 | Garage | FUN_00413340 | Garage |
| 6 | 0x06 | Shop | FUN_00417f40 | Shop |
| 7 | 0x07 | Lobby | FUN_00408ee0 | Game lobby |
| 8 | 0x08 | Room | FUN_00410cb0 | Room |
| 10 | 0x0A | Game | FUN_004028c0 | Main game. CHECKS DriverID==-1 |
| 12 | 0x0C | Tutorial | FUN_0041b860 | Tutorial game |
| 13 | 0x0D | TutorialMenu | FUN_00437190 | Tutorial menu |
| 14 | 0x0E | GhostMode game | FUN_00425cb0 | Ghost mode game |
| 16 | 0x10 | Quest game | FUN_0042d690 | Quest game mode |
| 17 | 0x11 | CarFactory | FUN_00430ef0 | Car factory/customization |
| 18 | 0x12 | RoomEditor | FUN_00434c00 | Room editor |
| 21 | 0x15 | ScenarioMenu | FUN_00437fb0 | Scenario menu |
| 22 | 0x16 | GhostMode | FUN_00439350 | Ghost mode menu |
| 23 | 0x17 | MissionMenu | FUN_0043bad0 | Mission menu |
| 24 | 0x18 | Mission game | FUN_0043a320 | Mission game mode |
| 25 | 0x19 | QuestMenu | FUN_0043cfc0 | Quest menu |

## Packets Triggering State Changes

| Packet | Hex | Target State | Desc |
|--------|-----|--------------|------|
| 14 | 0x0E | 4 (Menu) | Channel list triggers state 4 |
| 15 | 0x0F | 6 (Shop) | |
| 16 | 0x10 | 7 (Lobby) | |
| 17 | 0x11 | 5 (Garage?) | Show Menu packet |
| 18 | 0x12 | 8 (Room) | Show Lobby/Room |
| 22 | 0x16 | 14 | |
| 143 | 0x8F | 24 | |
| 144 | 0x90 | 25 | |

## Character Creation Check

State 10 (Game) is ONLY state checking `DriverID == -1`:

```c
// FUN_004028c0 (Stage game init):
if (DAT_012124b8 == -1) {
    iVar5 = FUN_00451690(0);
    *(iVar5 + 0xc) = 2; // action=2 triggers creation
}
```

When action=2, pressing OK sends Windows message `0x7e8` with `param=2`, triggers `FUN_00473730` (character creation).

### Required Assets

```c
// FUN_00473650:
cVar2 = FUN_00441720("Popup/RegistDriver/Driver_Back.png", 1);
if (cVar2 == '\0') return 0; // FAIL -> "Failed to connect!"
```

Files needed: `Popup/RegistDriver/Driver_Back.png`, `Buttons/Common_OK_*.png`

## Global Variables

| Address | Name | Desc |
|---------|------|------|
| 0x012124B8 | DriverID | -1 = no character, else character ID |
| 0x00F727F4 | ConnectionState | 0=disc, 1=connected, 2=in-game |
| 0x00B2360C | CurrentUIState | Current state number |
| 0x00B23150 | ResetFlag | Set by 0x0D packet |

## Flow

1. Normal: Logo -> Title -> Login -> Channel(3) -> Menu(4) -> Lobby(7) -> Room(8) -> Game(10)
2. State 10 is where character creation check happens
3. Problem: cant reach state 10 without character (need to join game)
