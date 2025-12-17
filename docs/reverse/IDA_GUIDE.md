# KnC UI - Complete Reverse Engineering Guide

## Objective

Extract **all positions, sizes, and assets** for each game UI from IDA Pro.

---

## General Methodology

### Step 1: Identify Init Function

Each UI state has an initialization function:

| State | Name | Init Function | IDA Address |
|-------|------|---------------|-------------|
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

In IDA, search for this pattern:

```c
// Pattern 1: Texture loading
cVar = FUN_00441720("Path/To/Image.png", layer);
if (cVar == '\0') {
    // Failed to load
}

// Pattern 2: Position setting
*(int *)(element_ptr + 0x10) = x_coordinate;  // X position
*(int *)(element_ptr + 0x14) = y_coordinate;  // Y position

// Pattern 3: Size setting
*(int *)(element_ptr + 0x18) = width;
*(int *)(element_ptr + 0x1c) = height;
```

### Step 3: Extraction Template

For each found element:

```
Element: [Type] [Name]
Asset: [Path from FUN_00441720 call]
Position: (X, Y) from memory writes
Size: (Width, Height) from memory writes
Layer: [Second param of FUN_00441720]
```

---

## UI State 0: Logo Screen

### IDA Function: FUN_00411e20

### Search for:

1. **Background Image**:
   ```c
   FUN_00441720("Login/Login01.png", ?)
   FUN_00441720("Logo/Background.png", ?)
   ```

2. **Logo Image**:
   ```c
   FUN_00441720("Logo/Logo.png", ?)
   FUN_00441720("Logo/Logo_Main.png", ?)
   ```

3. **Positions**:
   - Search for `mov dword ptr [reg+offset], immediate`
   - Immediate values = coordinates

### Template to Fill:

```cpp
void InitScreen_Logo() {
    // Background
    background = "./Data/Eng/Image/______/______.png";
    
    // Logo
    Element logo;
    logo.asset = "./Data/Eng/Image/______/______.png";
    logo.bounds = {___, ___, ___, ___}; // x, y, w, h
    logo.type = ELEM_IMAGE;
}
```

---

## UI State 2: Login Screen

### IDA Function: FUN_00427190

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Login/Login01.png", ?)
   ```

2. **Input Fields**:
   - Username input box
   - Password input box
   - Search for UI element creation (new, malloc)

3. **Buttons**:
   ```c
   FUN_00441720("Buttons/Common_OK_01.png", ?)
   FUN_00441720("Buttons/Common_OK_02.png", ?)  // hover
   FUN_00441720("Buttons/Register_*.png", ?)
   ```

4. **Text Labels**:
   - "Username:", "Password:"
   - Version text

### Key Points:

- Input fields probably centered horizontally
- Buttons below inputs
- Text next to inputs

### Template:

```cpp
void InitScreen_Login() {
    background = "./Data/Eng/Image/Login/Login01.png";
    
    // Username
    Element usernameLabel;
    usernameLabel.text = "Username:";
    usernameLabel.bounds = {___, ___, ___, ___};
    
    Element usernameInput;
    usernameInput.type = ELEM_INPUT;
    usernameInput.bounds = {___, ___, ___, ___};
    
    // Password
    Element passwordLabel;
    passwordLabel.text = "Password:";
    passwordLabel.bounds = {___, ___, ___, ___};
    
    Element passwordInput;
    passwordInput.type = ELEM_INPUT;
    passwordInput.bounds = {___, ___, ___, ___};
    
    // Login Button
    Element loginBtn;
    loginBtn.asset = "./Data/Eng/Image/Buttons/Common_OK_01.png";
    loginBtn.hoverAsset = "./Data/Eng/Image/Buttons/Common_OK_02.png";
    loginBtn.bounds = {___, ___, ___, ___};
}
```

---

## UI State 4: Main Menu

### IDA Function: FUN_00412530

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Menu/Menu_Back.png", ?)
   FUN_00441720("Menu/Background.png", ?)
   ```

2. **Vertical Buttons**:
   ```c
   FUN_00441720("Menu/Play_01.png", ?)
   FUN_00441720("Menu/Garage_01.png", ?)
   FUN_00441720("Menu/Shop_01.png", ?)
   FUN_00441720("Menu/Option_01.png", ?)
   FUN_00441720("Menu/Exit_01.png", ?)
   ```

3. **Character Display**:
   - 3D zone to display character
   - Probably on the right side of screen

### Pattern Layout:

```
┌────────────────────────────────────┐
│ Background                         │
│                                    │
│  [Play]         [Character]        │
│  [Garage]        Display           │
│  [Shop]           3D               │
│  [Option]                          │
│  [Exit]                            │
│                                    │
│  [Player Name]                     │
└────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_Menu() {
    background = "./Data/Eng/Image/Menu/_______.png";
    
    // Play Button
    Element playBtn;
    playBtn.asset = "./Data/Eng/Image/Menu/Play_01.png";
    playBtn.hoverAsset = "./Data/Eng/Image/Menu/Play_02.png";
    playBtn.bounds = {___, ___, ___, ___};
    
    // Garage Button
    Element garageBtn;
    garageBtn.asset = "./Data/Eng/Image/Menu/Garage_01.png";
    garageBtn.bounds = {___, ___, ___, ___};
    
    // Shop Button
    Element shopBtn;
    shopBtn.asset = "./Data/Eng/Image/Menu/Shop_01.png";
    shopBtn.bounds = {___, ___, ___, ___};
    
    // Option Button
    Element optionBtn;
    optionBtn.asset = "./Data/Eng/Image/Menu/Option_01.png";
    optionBtn.bounds = {___, ___, ___, ___};
    
    // Exit Button
    Element exitBtn;
    exitBtn.asset = "./Data/Eng/Image/Menu/Exit_01.png";
    exitBtn.bounds = {___, ___, ___, ___};
    
    // Character Display Area
    Element charDisplay;
    charDisplay.type = ELEM_PANEL;
    charDisplay.bounds = {___, ___, ___, ___};
}
```

---

## UI State 5: Garage

### IDA Function: FUN_00413340

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Garage/Garage_Back.png", ?)
   ```

2. **UI Elements**:
   ```c
   FUN_00441720("Garage/Vehicle_List_*.png", ?)
   FUN_00441720("Garage/Stats_Panel_*.png", ?)
   FUN_00441720("Garage/Color_*.png", ?)
   FUN_00441720("Garage/Upgrade_*.png", ?)
   ```

3. **Specific Zones**:
   - Vehicle list (left)
   - 3D preview (center)
   - Stats panel (right)
   - Color selector (bottom)

### Layout:

```
┌──────────┬──────────────┬──────────┐
│ Vehicle  │              │  Stats   │
│  List    │   3D View    │  Panel   │
│          │              │          │
│  [Item1] │   [Kart]     │  Speed:  │
│  [Item2] │              │  Accel:  │
│  [Item3] │              │  Handle: │
│          │              │          │
├──────────┴──────────────┴──────────┤
│        [Color Selector]             │
│  ⬛⬜🟥🟦🟩🟨                        │
└─────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_Garage() {
    background = "./Data/Eng/Image/Garage/_______.png";
    
    // Vehicle List
    Element vehicleList;
    vehicleList.type = ELEM_LIST;
    vehicleList.bounds = {___, ___, ___, ___};
    
    // 3D Preview Area
    Element preview3D;
    preview3D.type = ELEM_PANEL;
    preview3D.bounds = {___, ___, ___, ___};
    
    // Stats Panel
    Element statsPanel;
    statsPanel.type = ELEM_PANEL;
    statsPanel.bounds = {___, ___, ___, ___};
    
    // Color Selector
    Element colorSelector;
    colorSelector.bounds = {___, ___, ___, ___};
    
    // Upgrade Button
    Element upgradeBtn;
    upgradeBtn.asset = "./Data/Eng/Image/Garage/Upgrade_01.png";
    upgradeBtn.bounds = {___, ___, ___, ___};
    
    // Back Button
    Element backBtn;
    backBtn.asset = "./Data/Eng/Image/Buttons/Common_Back_01.png";
    backBtn.bounds = {___, ___, ___, ___};
}
```

---

## UI State 6: Shop

### IDA Function: FUN_00417f40

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Shop/Shop_Back.png", ?)
   ```

2. **Tabs**:
   ```c
   FUN_00441720("Shop/Tab_*.png", ?)
   // Karts | Items | Parts | Special
   ```

3. **Item Grid**:
   - Grid layout (4x3 or 5x3)
   - Item icons
   - Prices

4. **Buttons**:
   ```c
   FUN_00441720("Shop/Buy_*.png", ?)
   FUN_00441720("Shop/Sell_*.png", ?)
   ```

### Layout:

```
┌─────────────────────────────────────┐
│ [Karts][Items][Parts][Special]      │
├─────────────────────────────┬───────┤
│ [Item] [Item] [Item] [Item] │ Info  │
│ [Item] [Item] [Item] [Item] │ Panel │
│ [Item] [Item] [Item] [Item] │       │
│                             │ [Buy] │
│ [← Page →]                  │       │
├─────────────────────────────┴───────┤
│ Gold: 9999  Astros: 100             │
└─────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_Shop() {
    background = "./Data/Eng/Image/Shop/_______.png";
    
    // Category Tabs
    Element tabKarts, tabItems, tabParts, tabSpecial;
    tabKarts.bounds = {___, ___, ___, ___};
    tabItems.bounds = {___, ___, ___, ___};
    tabParts.bounds = {___, ___, ___, ___};
    tabSpecial.bounds = {___, ___, ___, ___};
    
    // Item Grid (4x3 = 12 items)
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 4; col++) {
            Element itemSlot;
            itemSlot.bounds = {
                ___ + col * ___,  // x + spacing
                ___ + row * ___,  // y + spacing
                ___,              // width
                ___               // height
            };
        }
    }
    
    // Info Panel
    Element infoPanel;
    infoPanel.type = ELEM_PANEL;
    infoPanel.bounds = {___, ___, ___, ___};
    
    // Buy Button
    Element buyBtn;
    buyBtn.asset = "./Data/Eng/Image/Shop/Buy_01.png";
    buyBtn.bounds = {___, ___, ___, ___};
    
    // Currency Display
    Element goldDisplay, astrosDisplay;
    goldDisplay.bounds = {___, ___, ___, ___};
    astrosDisplay.bounds = {___, ___, ___, ___};
}
```

---

## UI State 7: Lobby

### IDA Function: FUN_00408ee0

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Lobby/Lobby_Back.png", ?)
   ```

2. **Room List Table**:
   - Headers: Room Name | Map | Mode | Players | Status
   - Scrollable rows

3. **Buttons**:
   ```c
   FUN_00441720("Lobby/Create_*.png", ?)
   FUN_00441720("Lobby/Refresh_*.png", ?)
   FUN_00441720("Lobby/QuickJoin_*.png", ?)
   ```

4. **Chat Area** (bottom of screen)

### Layout:

```
┌───────────────────────────┬─────────┐
│ Room List                 │ Create  │
│ ┌────┬────┬────┬────┬────┐│ Refresh │
│ │Name│Map │Mode│Play│Stat││ Quick   │
│ ├────┼────┼────┼────┼────┤│ Join    │
│ │Room│Map1│Race│4/8 │Open││         │
│ │...                      ││         │
│ └────┴────┴────┴────┴────┘│ Players │
│                           │ List    │
├───────────────────────────┴─────────┤
│ Chat: [___________________] [Send]  │
└─────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_Lobby() {
    background = "./Data/Eng/Image/Lobby/_______.png";
    
    // Room List Table
    Element roomList;
    roomList.type = ELEM_LIST;
    roomList.bounds = {___, ___, ___, ___};
    
    // Create Room Button
    Element createBtn;
    createBtn.asset = "./Data/Eng/Image/Lobby/Create_01.png";
    createBtn.bounds = {___, ___, ___, ___};
    
    // Refresh Button
    Element refreshBtn;
    refreshBtn.asset = "./Data/Eng/Image/Lobby/Refresh_01.png";
    refreshBtn.bounds = {___, ___, ___, ___};
    
    // Quick Join Button
    Element quickJoinBtn;
    quickJoinBtn.asset = "./Data/Eng/Image/Lobby/QuickJoin_01.png";
    quickJoinBtn.bounds = {___, ___, ___, ___};
    
    // Player List
    Element playerList;
    playerList.type = ELEM_LIST;
    playerList.bounds = {___, ___, ___, ___};
    
    // Chat Area
    Element chatInput;
    chatInput.type = ELEM_INPUT;
    chatInput.bounds = {___, ___, ___, ___};
    
    Element chatSendBtn;
    chatSendBtn.asset = "./Data/Eng/Image/Buttons/Send_01.png";
    chatSendBtn.bounds = {___, ___, ___, ___};
}
```

---

## UI State 8: Room (Waiting Room)

### IDA Function: FUN_00410cb0

### Search for:

1. **Background**:
   ```c
   FUN_00441720("Room/Room_Back.png", ?)
   ```

2. **Player Slots** (8 slots):
   ```c
   FUN_00441720("Room/Player_Slot_*.png", ?)
   FUN_00441720("Room/Slot_Empty_*.png", ?)
   ```

3. **Track Preview**:
   - Track minimap

4. **Buttons**:
   ```c
   FUN_00441720("Room/Ready_*.png", ?)
   FUN_00441720("Room/Start_*.png", ?)  // Host only
   ```

### Layout:

```
┌──────────┬──────────┬──────────┬──────────┐
│ [Slot 1] │ [Slot 2] │ [Slot 3] │ [Slot 4] │
│  Player  │  Player  │  Empty   │  Empty   │
│  Ready   │  Not     │          │          │
├──────────┼──────────┼──────────┼──────────┤
│ [Slot 5] │ [Slot 6] │ [Slot 7] │ [Slot 8] │
│  Empty   │  Empty   │  Empty   │  Empty   │
│          │          │          │          │
├──────────┴──────────┴──────────┴──────────┤
│ [Track    │ Track: Desert Circuit         │
│  Preview] │ Mode: Race                     │
│  [Mini]   │ Laps: 3          [READY]      │
└───────────┴────────────────────────────────┘
│ Chat: [_________________]                  │
└────────────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_Room() {
    background = "./Data/Eng/Image/Room/_______.png";
    
    // 8 Player Slots (2 rows x 4 cols)
    int slotW = 150, slotH = 150;
    int spacing = 25;
    int startX = 100, startY = 150;
    
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 4; col++) {
            Element slot;
            slot.type = ELEM_PANEL;
            slot.bounds = {
                startX + col * (slotW + spacing),
                startY + row * (slotH + spacing),
                slotW,
                slotH
            };
            // Extract exact values from IDA
        }
    }
    
    // Track Preview
    Element trackPreview;
    trackPreview.type = ELEM_IMAGE;
    trackPreview.bounds = {___, ___, ___, ___};
    
    // Room Info Panel
    Element roomInfo;
    roomInfo.type = ELEM_PANEL;
    roomInfo.bounds = {___, ___, ___, ___};
    
    // Ready Button
    Element readyBtn;
    readyBtn.asset = "./Data/Eng/Image/Room/Ready_01.png";
    readyBtn.bounds = {___, ___, ___, ___};
    
    // Start Button (host)
    Element startBtn;
    startBtn.asset = "./Data/Eng/Image/Room/Start_01.png";
    startBtn.bounds = {___, ___, ___, ___};
    
    // Chat
    Element chatArea;
    chatArea.type = ELEM_INPUT;
    chatArea.bounds = {___, ___, ___, ___};
}
```

---

## UI State 10: In-Game HUD

### IDA Function: FUN_004028c0

### Search for:

1. **Speedometer**:
   ```c
   FUN_00441720("Game/Speedometer_*.png", ?)
   FUN_00441720("HUD/Speed_*.png", ?)
   ```

2. **Position Display**:
   ```c
   FUN_00441720("Game/Position_*.png", ?)  // 1st, 2nd, 3rd...
   ```

3. **Lap Counter**

4. **Item Slot**:
   ```c
   FUN_00441720("Game/ItemSlot_*.png", ?)
   ```

5. **Minimap**

6. **Timer**

### Layout:

```
┌────────────────────────────────────┐
│ [1st]       Timer: 01:23.456       │
│ Lap: 1/3                           │
│                                    │
│             [Racing Area]          │
│                                    │
│                                    │
│ [Minimap]         [Item]  [Speed] │
│                    🎁      120    │
│                           ┌───┐   │
│                           │ ● │   │
│                           └───┘   │
│ [━━━━━━━━━] Boost                 │
└────────────────────────────────────┘
```

### Template:

```cpp
void InitScreen_InGame() {
    // Position Display
    Element positionDisplay;
    positionDisplay.bounds = {___, ___, ___, ___};
    
    // Timer
    Element timer;
    timer.text = "00:00.000";
    timer.bounds = {___, ___, ___, ___};
    timer.fontSize = 32;
    
    // Lap Counter
    Element lapCounter;
    lapCounter.text = "Lap 1/3";
    lapCounter.bounds = {___, ___, ___, ___};
    lapCounter.fontSize = 24;
    
    // Speedometer (bottom right)
    Element speedometer;
    speedometer.asset = "./Data/Eng/Image/Game/Speedometer_Back.png";
    speedometer.bounds = {___, ___, ___, ___};
    
    Element speedNeedle;
    speedNeedle.asset = "./Data/Eng/Image/Game/Speedometer_Needle.png";
    speedNeedle.bounds = {___, ___, ___, ___};
    
    // Item Slot
    Element itemSlot;
    itemSlot.asset = "./Data/Eng/Image/Game/ItemSlot_Empty.png";
    itemSlot.bounds = {___, ___, ___, ___};
    
    // Minimap (bottom left)
    Element minimap;
    minimap.type = ELEM_PANEL;
    minimap.bounds = {___, ___, ___, ___};
    
    // Boost Meter
    Element boostMeter;
    boostMeter.type = ELEM_PANEL;
    boostMeter.bounds = {___, ___, ___, ___};
}
```

---

## Extraction Checklist

For **EACH** UI, you must extract:

### Assets
- [ ] Background image path
- [ ] Button images (normal/hover/active)
- [ ] Icon images
- [ ] Panel backgrounds
- [ ] Text labels

### Positions
- [ ] X coordinate
- [ ] Y coordinate
- [ ] Width
- [ ] Height

### Properties
- [ ] Element type (Image/Button/Text/Input/List/Panel)
- [ ] Layer/z-order
- [ ] Visibility
- [ ] Parent-child relationships

---

## IDA Tools

### Useful Scripts

**1. Search for all FUN_00441720 calls**:
```
Ctrl+F → Text → "FUN_00441720"
```

**2. Find asset strings**:
```
Shift+F12 → Strings window
Search: ".png", "Image/", "Button/"
```

**3. Follow references**:
```
Click on string → X (cross-references)
```

**4. Extract immediate values**:
```
Look for: mov dword ptr [reg+offset], immediate
immediate = coordinate value
```

### Pattern Recognition

**Position Setting**:
```asm
mov     dword ptr [eax+10h], 64h    ; X = 100
mov     dword ptr [eax+14h], 12Ch   ; Y = 300
mov     dword ptr [eax+18h], 0C8h   ; Width = 200
mov     dword ptr [eax+1Ch], 3Ch    ; Height = 60
```

**Texture Loading**:
```asm
push    1                           ; Layer
push    offset aButtonsOk01         ; "Buttons/Common_OK_01.png"
call    FUN_00441720
```

---

## Priorities

### Phase 1: Critical UI (URGENT)
1. ✅ Login (State 2)
2. ✅ Menu (State 4)
3. ✅ Lobby (State 7)
4. ✅ Room (State 8)

### Phase 2: Secondary UI
5. ⏳ Garage (State 5)
6. ⏳ Shop (State 6)
7. ⏳ Channel (State 3)
8. ⏳ Logo (State 0)

### Phase 3: Game UI
9. ⏳ In-Game HUD (State 10)
10. ⏳ Tutorial (State 12/13)

### Phase 4: Advanced UI
11. ⏳ CarFactory (State 17)
12. ⏳ GhostMode (State 22)
13. ⏳ MissionMenu (State 23)
14. ⏳ ScenarioMenu (State 21)
15. ⏳ QuestMenu (State 25)

---

## Documentation Format

For each completed UI, create a file:

**Filename**: `UI_STATE_XX_Name.md`

**Template**:
```markdown
# UI State XX: [Name]

## IDA Function
- Address: 0x________
- Name: FUN_________

## Background
- Path: ./Data/Eng/Image/______/______.png
- Size: 1024x768

## Elements

### Element 1: [Name]
- Type: [Image/Button/Text/Input/List/Panel]
- Asset: ./Data/Eng/Image/______/______.png
- Position: (X, Y) = (___, ___)
- Size: (W, H) = (___, ___)
- Layer: ___

### Element 2: [Name]
...

## IDA Notes
[Specific observations]

## Screenshots
[Reference screenshot if available]
```