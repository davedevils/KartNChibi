# UI layout JSON

The screen layouts of the client live as JSON files in `Data/Public/UI/`. Two programs read
them:

| Program | File | Role |
|---------|------|------|
| client runtime | `client/ui/UiJson.cpp` `parseScreenLayout` | builds the widgets of a screen |
| layout editor | `tools/ui_editor/ui_json.cpp` `load_screen_from_json` | opens, edits and saves a layout |

The client looks for the folder in this order: `<exe dir>/clone/Data/Public/UI`, the game
folder given with the options, the source tree, then `Data/Public/UI` in the working folder
(`client/app/App.cpp`).

## Files

`ui_state_NN_name.json` holds one screen, `NN` is the client stage number. `popup_name.json`
holds one popup, see [UI_POPUPS_CATALOG.md](UI_POPUPS_CATALOG.md).

Seven screens load a layout today, the others are drawn in C++:

| Screen class | Layout |
|--------------|--------|
| `LoginScreen` | `ui_state_02_login.json` |
| `ChannelScreen` | `ui_state_03_channel.json` |
| `MenuScreen` | `ui_state_04_menu.json` |
| `GarageScreen` | `ui_state_06_garage.json` |
| `LobbyScreen` | `ui_state_07_lobby.json` |
| `RoomScreen` | `ui_state_08_room.json` |
| `ShopScreen` | `ui_state_13_shop.json` |

The folder also holds older files with doubled stage numbers (`ui_state_10_game.json` and
`ui_state_10_roomeditor.json`, `ui_state_13_tutorial.json` next to the shop, and so on) and
a `ui_state_02_login_FIXED.json`. Nothing loads them. For the stage numbers see
`docs/client/UI_STATES.md`.

## Root

```json
{
  "state": 2,
  "name": "Login",
  "resolution": [1024, 768],
  "elements": [ ... ]
}
```

| Key | Type | Read by | Note |
|-----|------|---------|------|
| `state` | int | client, editor | stage number |
| `name` | string | client, editor | |
| `elements` | array | client, editor | drawn in array order, the client adds its own z guess |
| `resolution` | [w, h] | client | optional, the design size |
| `version`, `type`, `modal` | | nobody | present in some files, ignored |

## Element

```json
{
  "id": "input_password",
  "type": "input",
  "position": [450, 339],
  "size": [184, 20],
  "visible": true,
  "enabled": true,
  "properties": { "maxLength": 16, "password": true, "placeholder": "PW" }
}
```

| Key | Type | Note |
|-----|------|------|
| `id` | string | the screen code finds widgets by id |
| `type` | string | `image`, `button`, `text`, `input`, `list`, `panel`, `checkbox`, anything else reads as `image` |
| `position` | [x, y] | pixels from the top left of the design size |
| `size` | [w, h] | optional, an image without it takes the texture size |
| `asset` | string | path under the client image folders |
| `visible`, `enabled` | bool | default true |
| `action` | string | name the screen handles when the widget is clicked |
| `text`, `fontSize` | string, int | label text and size |
| `hoverAsset`, `pressedAsset`, `disabledAsset` | string | buttons only |
| `properties` | object | see below |

A button `asset` is a prefix. When the three state keys are missing the client appends
`01.png`, `02.png` and `03.png` for hover, pressed and disabled, so `Login/confirm_` gives
`Login/confirm_01.png` and the next two.

`properties` keys the client reads: `maxLength`, `placeholder`, `password`, `checked`,
`content` (same as `text`), `fontSize` or `size`, and `color` as three or four ints. The
editor reads `maxLength`, `placeholder`, `password` and `checked`. Other keys in the files,
such as `align` or `wordWrap`, are not read.

## Actions

An action is a plain string. Each screen handles its own list in its `onAction`, for example
`login` and `cancel` in `LoginScreen`, `ready`, `start`, `pick_track`, `team_red` in
`RoomScreen`, `ui_close` in `LogoScreen` and `MenuScreen`. There is no global action table.

## Editing

`tools/ui_editor` is a bgfx and Dear ImGui editor. It reads textures from the game
`pak001.dat` and `Data/<lang>/Image`, loads a layout, lets you drag, resize and edit
elements, and saves `ui_state_<state>_<Name>.json` with Ctrl+S.

```
release/ui_editor.exe --game <client>
release/ui_editor.exe --game <client> --json Data/Public/UI/ui_state_07_lobby.json
release/ui_editor.exe --game <client> --state 2 --screenshot out.png --frames 5
```

Options: `--game`, `--ui-dir`, `--json`, `--export <file>` (write and exit), `--screenshot
<png>`, `--frames`, `--lang`, `--state`, `--size WxH`. Build it with the engine solution,
target `ui_editor`.
