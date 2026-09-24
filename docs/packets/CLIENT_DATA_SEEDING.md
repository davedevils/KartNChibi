# Client data seeded by hand

`DevClient/` is git ignored so these are NOT in the repository. Redo them on a
fresh checkout or the client fails in ways that look like server bugs.

## Why

The client asks for file names the shipped data does not carry. A miss is not
logged anywhere, the screen just fails, and the error is usually a win32
MessageBox that no screenshot shows. Read those with `Get-Popup` from
`DevClient/drive.ps1`.

## What was added

| Path | From | Why |
|------|------|-----|
| `Data/Public/Image/Popup/GhostMode/UI_Ghost mode_back.png` | `Popup/GhostModeResult/UI_Ghost_result_back.png` | `sub_458310` loads it first, a miss makes the board quit with -4 |
| `Data/Public/Image/Popup/SelectTrack/UI_system_arrow_left_00..02.png` | `CarFactory/UI_system_arrow_left_*` | same screen, same quit |
| `Data/Public/Image/Popup/SelectTrack/UI_system_arrow_right_00..02.png` | `CarFactory/UI_system_arrow_right_*` | same |
| `Data/Public/World/Room/Floor/*/track1.col` | `track.COL` in the same folder | `sub_485580` asks for track1.col, without it the room has no collision and every car build answers "Set body fail 1" |
| `Data/Public/World/*/*/track1.COL` | `track.COL` in the same folder | same rule for every race, licence and mission world, 29 folders |

## Rule

Real race tracks ship `track.COL` AND `track1.COL` and sometimes `track2.COL`.
Any world that carries only `track.COL` needs a `track1.COL` copy. Check with

    find Data/Public/World -iname "track.COL" -execdir sh -c '[ -f track1.COL ] || pwd' ;

## Data/Eng junctions, 2026-08-22

The model loaders only ever probe the locale root. `FUN_00443F50` and its two
siblings build `./Data/%s/%s.nif` from `FUN_0044D3E0(-1)`, which is `Eng` here.
The shipped tree puts every model under `Data/Public` and `Data/Eng` carried only
Effect GhostMode Image License. So every `.nif` and `.kfm` missed.

Measured before: 3 successful opens against 2246 misses for a whole boot.
The character body face and head, the kart body and its four wheels, every
animation, all missing.

Fix, junctions not copies, `Data/Eng/<d>` -> `Data/Public/<d>` for:

```
Car  Driver  Item  Pet  Sound  Title  UI  World
```

plus `cam.nif` copied, it is a file not a directory.

Recreate them with:

```powershell
$base = 'DevClient\Data'
foreach ($d in @('Car','Driver','Item','Pet','Sound','Title','UI','World')) {
    $link = Join-Path $base ('Eng\' + $d)
    $tgt  = Join-Path $base ('Public\' + $d)
    if (Test-Path -LiteralPath $link) { continue }
    New-Item -ItemType Junction -Path $link -Target $tgt | Out-Null
}
```

Measured after: 226 opens. `Pumpkin_char_body_001.nif`, `_face_`, `_head_`,
`Car/Body/High/basic_1/body.nif` and `wheel1..4.nif` all load, plus the whole
`.kf` animation set and the `.dds` textures.

STILL OPEN. The meshes load and the car object is live, it takes input and
moves, and its nameplate tracks the right screen position, but no mesh draws in
the lobby stand or in the waiting room. So this was a real and necessary fix but
it is not the whole story, the remaining fault is in the draw not the data.
