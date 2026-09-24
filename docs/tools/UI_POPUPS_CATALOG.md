# UI popups

Two kinds of popup exist in the tree: JSON layouts in `Data/Public/UI/` and popup classes in
the client. They are not tied together yet.

## Popup classes in the client

These are the popups the client really shows. Each one draws itself in C++ and loads no JSON.

| Class | File | Opens on |
|-------|------|----------|
| `MessagePopup` | `client/ui/MessagePopup.cpp` | server text boxes, confirms and errors through `App::showMessage` |
| `CharacterCreatePopup` | `client/screens/CharacterCreatePopup.cpp` | the creation step after login, `App::openCharacterCreate` |
| `MenuPopup` | `client/screens/MenuPopup.cpp` | Escape in a screen |
| `GameOptionPopup`, `ControlOptionPopup` | `client/screens/OptionPopup.cpp` | the game and control buttons of the menu |
| `HelpPopup` | `client/screens/HelpPopup.cpp` | F1 |
| `InvitePopup` | `client/screens/InvitePopup.cpp` | a room invite from the server |
| `MessengerPopup` | `client/screens/MessengerPopup.cpp` | the messenger button of the lobby |
| `PendantPopup` | `client/screens/PendantPopup.cpp` | the pendant button of the character panel |
| `GachaPopup` | `client/screens/GachaPopup.cpp` | the gacha button of the lobby |

## Popup layouts in Data/Public/UI

Seventeen files. Nothing in the client loads them today, `tools/ui_editor` opens one with
`--json`. The root has
`version`, `type` "popup", `name`, `resolution` and `modal`, the elements follow
[UI_JSON_SCHEMA.md](UI_JSON_SCHEMA.md).

| File | name | Elements |
|------|------|----------|
| `popup_carname.json` | FactoryCarName | 4 |
| `popup_gacha.json` | Gacha | 8 |
| `popup_ghostmode.json` | GhostModeSetup | 4 |
| `popup_ghostresult.json` | GhostModeResult | 2 |
| `popup_giftinfo.json` | GiftInfo | 7 |
| `popup_help.json` | Help | 2 |
| `popup_info.json` | UserInfo | 3 |
| `popup_input.json` | InputConfig | 8 |
| `popup_invite.json` | Invite | 4 |
| `popup_licenseclear.json` | LicenseStepClear | 3 |
| `popup_makeroom.json` | MakeRoom | 10 |
| `popup_menu.json` | GameMenu | 5 |
| `popup_message.json` | Message | 4 |
| `popup_messenger.json` | Messenger | 10 |
| `popup_randomitem.json` | RandomItem | 3 |
| `popup_selecttrack.json` | SelectTrack | 7 |
| `popup_shopitem.json` | ShopItem | 8 |

The pendant box has a class and no layout file.
