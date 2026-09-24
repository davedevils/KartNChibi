# UI Reverse Engineering Guide

Goal: extract all positions, sizes, assets for each game UI from IDA Pro.

---

## Methodology

### Step 1: Find Init Function

Each UI state has an init function:

| State | Name | Init Function | Address |
|-------|------|---------------|---------|
| 0 | Logo | FUN_00411e20 | 0x00411e20 |
| 1 | Title | FUN_00411f70 | 0x00411f70 |
| 2 | Login | FUN_00427190 | 0x00427190 |
| 3 | Channel | FUN_00424b00 | 0x00424b00 |
| 4 | Menu | FUN_00412530 | 0x00412530 |
| 5 | Garage | FUN_00413340 | 0x00413340 |
| 6 | Shop | FUN_00417f40 | 0x00417f40 |
| 7 | Lobby | FUN_00408ee0 | 0x00408ee0 |
| 8 | Room | FUN_00410cb0 | 0x00410cb0 |
| 10 | Game | FUN_004028c0 | 0x004028c0 |
| 12 | Tutorial | FUN_0041b860 | 0x0041b860 |
| 13 | TutorialMenu | FUN_00437190 | 0x00437190 |
| 14 | GhostMode game | FUN_00425cb0 | 0x00425cb0 |
| 16 | Quest game | FUN_0042d690 | 0x0042d690 |
| 17 | CarFactory | FUN_00430ef0 | 0x00430ef0 |
| 18 | RoomEditor | FUN_00434c00 | 0x00434c00 |
| 21 | ScenarioMenu | FUN_00437fb0 | 0x00437fb0 |
| 22 | GhostMode | FUN_00439350 | 0x00439350 |
| 23 | MissionMenu | FUN_0043bad0 | 0x0043bad0 |
| 24 | Mission game | FUN_0043a320 | 0x0043a320 |
| 25 | QuestMenu | FUN_0043cfc0 | 0x0043cfc0 |

### Step 2: Asset Loading Pattern

```c
// texture loading
cVar = FUN_00441720("Path/To/Image.png", layer);
if (cVar == '\0') { /* failed */ }

// position
*(int *)(element_ptr + 0x10) = x_coordinate;
*(int *)(element_ptr + 0x14) = y_coordinate;

// size
*(int *)(element_ptr + 0x18) = width;
*(int *)(element_ptr + 0x1c) = height;
```

### Step 3: Record Per Element

```
Element: [Type] [Name]
Asset: [Path from FUN_00441720 call]
Position: (X, Y)
Size: (Width, Height)
Layer: [Second param of FUN_00441720]
```

---

## State 0: Logo (FUN_00411e20)

Search: `FUN_00441720("Login/Login01.png", ?)`, `FUN_00441720("Logo/Logo.png", ?)`.
Find coords from `mov dword ptr [reg+offset], immediate`.

## State 2: Login (FUN_00427190)

Search: background `Login/Login01.png`, buttons `Buttons/Common_OK_01.png` (hover `_02`, active `_03`), `Buttons/Register_*.png`.
Input fields: search for UI element creation.

## State 4: Menu (FUN_00412530)

Search: `Menu/Menu_Back.png`, `Menu/Play_01.png`, `Menu/Garage_01.png`, `Menu/Shop_01.png`, `Menu/Option_01.png`, `Menu/Exit_01.png`.
Character display 3D zone on right side.

## State 5: Garage (FUN_00413340)

Search: `Garage/Garage_Back.png`, `Garage/Vehicle_List_*.png`, `Garage/Stats_Panel_*.png`, `Garage/Color_*.png`, `Garage/Upgrade_*.png`.
Layout: vehicle list left, 3D preview center, stats right, color selector bottom.

## State 6: Shop (FUN_00417f40)

Search: `Shop/Shop_Back.png`, `Shop/Tab_*.png`, `Shop/Buy_*.png`, `Shop/Sell_*.png`.
Grid layout 4x3 items, tabs for categories.

## State 7: Lobby (FUN_00408ee0)

Search: `Lobby/Lobby_Back.png`, `Lobby/Create_*.png`, `Lobby/Refresh_*.png`, `Lobby/QuickJoin_*.png`.
Room list table, chat area bottom.

## State 8: Room (FUN_00410cb0)

Search: `Room/Room_Back.png`, `Room/Player_Slot_*.png`, `Room/Ready_*.png`, `Room/Start_*.png`.
8 player slots (2x4), track preview, chat.

## State 10: In-Game HUD (FUN_004028c0)

Search: `Game/Speedometer_*.png`, `Game/Position_*.png`, `Game/ItemSlot_*.png`.
Speedometer bottom-right, position top-left, timer top-center, minimap bottom-left, boost meter bottom-center.

---

## IDA Techniques

### Find All Texture Loads

```
Ctrl+F -> Text -> "FUN_00441720"
```

### Find Asset Strings

```
Shift+F12 -> Strings window
Search: ".png", "Image/", "Button/"
```

### Follow References

Click string -> X (cross-references).

### Assembly Patterns

Position setting:
```asm
mov dword ptr [eax+10h], 64h   ; X = 100
mov dword ptr [eax+14h], 12Ch  ; Y = 300
mov dword ptr [eax+18h], 0C8h  ; Width = 200
mov dword ptr [eax+1Ch], 3Ch   ; Height = 60
```

Texture loading:
```asm
push 1                          ; Layer
push offset aButtonsOk01        ; "Buttons/Common_OK_01.png"
call FUN_00441720
```

---

## Extraction Checklist Per UI

### Assets
- Background image path
- Button images (normal/hover/active)
- Icon images
- Panel backgrounds
- Text labels

### Positions
- X, Y, Width, Height

### Properties
- Element type (Image/Button/Text/Input/List/Panel)
- Layer/z-order
- Visibility
- Parent-child relationships

---

## Priority

| Phase | States |
|-------|--------|
| 1 Critical | [OK] Login(2), Menu(4), Lobby(7), Room(8) |
| 2 Secondary | Garage(5), Shop(6), Channel(3), Logo(0) |
| 3 Game | HUD(10), Tutorial(12/13) |
| 4 Advanced | CarFactory(17), GhostMode(22), MissionMenu(23), ScenarioMenu(21), QuestMenu(25) |

---

## Output Format

Per completed UI, create `UI_STATE_XX_Name.md`:

```
# UI State XX: [Name]
## IDA Function: 0x________ FUN_________
## Background: ./Data/Eng/Image/______/______.png (1024x768)
## Elements
### Element 1: [Name]
- Type: [Image/Button/Text/Input/List/Panel]
- Asset: ./Data/Eng/Image/______/______.png
- Position: (X, Y)
- Size: (W, H)
- Layer: ___
```
