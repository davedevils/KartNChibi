# Client scripts

Proof scripts for the clone, one flow each, run against the package server with a test account:

```
release\knc_client.exe --game <client> --host 127.0.0.1 --port 50017 --user hltest --pass hltestpw --no-focus --mute --debug --stop-at lobby --script tools/client_scripts/create.txt --screenshot out.png
```

The exit code is 1 when an `expect` fails. Verbs: `wait N`, `waitfor SCREEN [N]`, `click X Y`, `rclick X Y`, `drag X1 Y1 X2 Y2 [left|right|middle] [N]`, `key NAME`, `hold NAME N`, `text WORDS`, `shot NAME`, `expect SCREEN`, `quit`. Coordinates are the stock 1024 by 768 ones. `hold` keeps a key down N frames while the next lines run, the race keys read it. In `text` the token `{nick}` stands for the `--auto-create-character` nickname, the one the gate makes for a fresh account.

| Script | Flow |
|---|---|
| intro.txt | the two logo plates then the intro nif of the stock title stage then the login, no host needed |
| login.txt | run with `--stop-at login`, the login box art of `Login02.png` then the OK sends `0x0007` and the channel list lands |
| create.txt | CREATE then the make room box then OK lands in a room |
| create_private.txt | the same with the Private row and a password |
| gear.txt | the gear opens the Menu box then Help Game Setting Control Setting |
| escape.txt | Escape opens the Menu box End The Game asks then Cancel returns |
| channel.txt | the Channel button leaves to the channel list a pick returns |
| quest.txt | the Quest image is dead as in the stock |
| shop_tabs.txt | every shop category tab switches its tiles |
| gacha.txt | the shop sells a Gacha Coin for 1000 gold then the Gacha box over the shop rolls it with `0x00ED`, the 3D machine plays its camera path, the prize shows at 9.75 s, Close returns to the shop with its preview back |
| gacha_close.txt | the Gacha box over the lobby then Close, the kart preview comes back and the User List tab still fills from `0x0132` |
| gift.txt | the Gift button of the Gacha Coin tile, a friend picked in the drop, a note typed, `0x0098` out, the package server answers the wallet and the gold drops on the card |
| delete.txt | run with `--state garage` and no host, the sample server carries a dead Gold Up row, Delete sends `0x00B8` and the ack drops the row through the session |
| ghost.txt | run with `KNC_AUTO_GHOST=90`, the top bar Ghost button opens the ghost menu through the frame action, the auto run races the track and comes back on the `0x011D` ack, the run ends by itself |
| licence_test.txt | run with `KNC_AUTO_LICENCE=<test>` and `--stop-at licence`, the test opens on the `0x0062` ack, the `0x00A3` answer lands on the session and the menu comes back |
| tutorial1.txt | run with `KNC_AUTO_LICENCE=0` and `--stop-at licence`, the driving practice of the rookie tier, two shots of the run with the minimap of `License_01`, the speed gauge board and the item slot window, the panel set stage 13 draws |
| licence_grade.txt | the same on the third rookie test of a fresh account, the `0x00A4` grade lands and the Mission button opens the mission menu with no relogin, hltest and hltest2 hold the grade already so this one needs a fresh account |
| frame_buttons.txt | the top bar Mission button from the garage and the Ghost button from the shop open their menu through the frame action |
| options.txt | the Sound tab lowers the effect bar twice, OK writes Option2.ini and the log prints the new group gain, the Control tab binds the up row to W, OK writes Input.ini, the next start prints `race keys up W` |
| options_default.txt | Default then OK on both panels, the two ini files go back to the stock values |
| room_drive.txt | run with `KNC_ROOM_DRIVE=1`, the room seat drives the floor of `Floor01` facing the eye, the mode 13 camera follows it so the kart stays in the frame, and the Quick Garage button of the top row opens the garage on its `0x000F` ack |
| race_items.txt | run with `--auto-race 90 --wait 20`, the line follower crosses a `[?]` box, the slot fills with its icon and the Ctrl press sends the item use. `waitfor race` holds till the GO so the shots keep their place in the countdown |
| race_start.txt | run with `--state race`, the gas is held through the three two one, the kart stays on its grid row, the GO shot shows it on the start checkers beside its neighbour and the log prints `[race] green light car at` with the row position |
| race_boxes.txt | run with `--state race`, the sample hands a rabbit at the green light, the gas down the first straight takes a box of the row into the second slot, the log reads `item box 2 gives 3 into slot 1` |
| race_speed_boxes.txt | run with `--state race` and `KNC_RACE_MODE=2`, the same straight in the speed single mode shows no box, the log reads `0 item boxes on the track mode 2 items 0` |
| race_wall.txt | run with `--state race`, the gas then a right turn into the yellow barrier of Race 01, the kart bounces off with the impact sprite and runs on along the road |
| race_item_*.txt | run with `--state race`, one item kind each on the sample race, their `# gate: env` lines set the three aids below. The capture lands as `out_item_NNNNN.png` at the race clock milliseconds after the green light, so the shots sit on the item whatever the frame pace. `turtle` `rocket` `bomb` `thunder` `hammer` `booster` `rabbit` `hive` `storm` `flash` `devil` `handle` `smoke` `magnet` use the own item at the leader or on the road, `spike` `dung` `turtle_hit` `hit` have the leader throw at the own kart, `shield` and `angel` pop on a rocket of the leader, `ice` and `sting` land the `0x0069` 1000 and 700 of a hive and an ice on the own kart |
| team.txt | run with `--mode 1`, the item team room draws the two plates on their own columns and a click on each plate moves the seat with `0x0064` |
| weather.txt | run with `--auto-race 90 --wait 18`, the track box opens, the moon of the weather radio is picked, OK sends it on `0x0035` and the race runs under the night sky of the track |
| parts.txt | the garage kart preview wears the antenna on O_ANT and the plate on O_NAME, the right arrow turns it half round so the `NAMEBOX_NORMAL` plate shows on the tail |
| equip.txt | the garage Character tab installs Moriko with `0x00B9`, the ack rebuilds the driver and she comes back with her hat her face and her dress |
| buy.txt | run on hltest2, the shop Car tab Antenna sub tab, the item box of a bottom row tile stays open and Buy takes 500 gold with `0x00B7` |
| pets.txt | run on hltest, the shop Character tab Pet sub tab buys Rosie for 1800 gold, the garage Pet sub tab lists the owned row and Install sends `0x00B9` category 4, the pet model shows beside the driver in the preview |
| paint.txt | run on hltest, the shop Car tab Paint sub tab buys Black Paint for 500 gold, the garage Paint sub tab takes the old one off with Backspace and Enter installs the new one, the 3D body turns black at once |
| roomcraft.txt | run on hltest2, the shop Room Craft Object sub tab buys an Aged Tree for 300 gold, the room editor opens with `0x010E`, a strip tile dragged onto the floor places it, a click on it picks it, five E presses turn it ten degrees and Save sends `0x010F` |
| carcraft.txt | run on hltest2, the shop Car Craft Tire and Cover sub tabs buy one part each for 1000 gold, the factory installs both on the Factory Car preset and Save sends `0x010B` |
| block.txt | run on hltest, a click on the HlTestTwo friend row opens the stock six item menu, its Block sends `0x007A`, the Block tab lists the name and its bin sends `0x007B` |
| repair.txt | run with `--state garage` and no host, the sample server carries a durability kart and a Repair Scroll Half, the Use button asks `MSG_REPAIR_USE` and the `0x00B9` ack raises the durability through its 16 byte kart period tail |
| roomcraft_markers.txt | run with `--state roomcraft` and no host, the Object tab tile dragged onto the field lands with nothing picked, a click on it puts the red ring and arrow of `lobby_image_red` on it, a right drag of 120 px turns it 24 degrees, a middle drag of 200 px pans the eye 14 5 along y |
| carcraft_slots.txt | run on dock2 of a server with migration 072, the shop Car Craft Chassis tab buys a second chassis for gold, the `0x00B7` ack then the new `0x0109` BASIC set and the full `0x0107` give it its own top row slot, the second slot lists its tires with the grade the period and the plus count, Equip then Save sends `0x010B`, the garage Car tab lists the two crafted karts first and their detail box lists the chassis and the seven parts with the wrench bar |
| carcraft_factory.txt | run with `--state carcraft` and no host, the sample preset seats the Circler chassis with its seven parts, the stage draws the built factory car, the Cover tab Remove takes the hood off and Equip puts it back, the right arrow held turns it |
| licence_pick.txt | run on uitest4 (`uitest4pw`, a fresh account that lands on the licence stage), the first rookie tile opens the picked state with the sheet slice and the own kart, the log shows one `[view] kart` build before the first frame |
| userinfo.txt | run on hltest, the friend row menu User Info item sends `0x0072` and the 104 byte blob comes back on the card under the rows, the Invite item sends `0x006C` with the name. From the lobby the server of 2026-09-23 answers `MSG_UNSUPPORT` on `0x0126`, the package image `knc-server:20260919a` still drops it |
| invite_lobby_sample.txt | run with `--state lobby` and no host, the Invite item of the friend row menu from the lobby sends `0x006C`, the sample server answers the refusal on `0x0126` as our server does and the lobby chat prints Not available once the messenger closes |
| shop_noprice.txt | run on hltest, the shop Item tab lists 29 tiles and the log prints the skipped key 1000, the slot changer with a shop definition and no price option, the stock tile draw rule |
| create_driver.txt | a fresh account, the RegistDriver popup: an empty name gives `MSG_NEED_NAME`, `ab` and a `def_taboo` word give `MSG_INVALID_NICK` with no packet, `HlTester` comes back as `0x0004` result 2 `MSG_ALREADY_REGIST`, each OK opens a fresh popup, the gate nickname creates the character, `MSG_SAVE_DONE` then the licence stage of `0x0016` |
| pendant.txt | run on hltest, the pendant button of the char panel opens the box of the `0x0119` rows, a cell opens the detail, Install sends `0x0123` and the ack puts the worn icon over the button, then the User List tab and a room with the name plate. It needs an owned pendant: the image of 2026-09-23 grants PENDANT 01 at login to a character with a licence grade, an older image needs a row in `owned_pendant` |
| missions_menu.txt | run on hltest, the top bar Mission button, the five slots of the `0x0088` rows with the lock the clear and the NEW plate, a locked slot hides Start, Start asks the fee question and its Cancel stays |
| mission_kind0.txt | run on hltest with `--auto-mission 0`, the kind 0 run of Mission_01 with the time items, the FINISH faces end it, `0x008C` once, the finish board then the menu |
| mission_kind1.txt | run on hltest with `--auto-mission 1`, the kind 1 run of Mission_02 with the boxes on the `Mission/Num` counter, the lap under the goal of 50 fails it and the fail board slides in |
| mission_kind1_sample.txt | the sample server with `--state mission`, row 1 of kind 1 wants 5 boxes, the lap over the goal wins it |
| mission_kind2_sample.txt | the sample server with `KNC_SAMPLE_CLEARED=4`, row 4 of kind 2 on Mission_05 with no gimmick, the lap wins it and the type 1 kart reward rides `0x008C` |
| options_graphic.txt | the Graphic tab, the window check, the wide arrow, the quality arrows and the two checks, OK writes `Option2.ini` and the window takes 1440 by 900 on a stretched canvas at once, Default then OK put the stock rows back |
| options_keys_race.txt | run with `--auto-race 90 --wait 20`, the room Menu box Control Setting binds the back row to B, the race reads it and a held B turns the eye to the rear. `Input.ini` keeps B until `options_default.txt` runs |

The item proofs lean on three capture aids of the sample race. `KNC_ITEM_TEST=<kind>` hands that kind at the green light instead of the rabbit, puts the own kart on start row 9 behind the leader on row 1, leaves out the sample hit of 13.5 s and presses the item key 0.3 s after the green light (a rocket or a magnet holds it 0.8 s, the release fires), the boxes roll that kind too. `KNC_ITEM_BOT=<kind>` has the leading bot use that kind on the own kart one second after the green light, 700 and 1000 land the hive and the ice code on it. `KNC_ITEM_SHOTS=<s,s,...>` takes a capture at each race clock second after the green light.

`intro.txt` runs with no host: `release\knc_client.exe --game <client> --mute --no-focus --debug --script tools/client_scripts/intro.txt --screenshot out.png`. It writes `intro_ogp`, `intro_rnr`, four intro frames and `intro_login`, the whole way in takes about eleven seconds at the stock timings.

`parts.txt` and `equip.txt` read the owned rows of `hltest`, which wears Pumpkin and owns Wolf and Moriko plus one Sushi Antenna. `buy.txt` runs on `hltest2` and spends gold only. A Character tab part cannot be bought on the package server, the answer is `MSG_UNKNOWN_ERROR` and the game log prints `why=UnknownDefinition`, the `shop_definition` table has no row for those keys.

`room_drive.txt` and `weather.txt` need the account free, the package server holds one game per account so two runs at once take the second one back to the login box. `gacha.txt` needs 1000 gold on the account, the coin costs it, `hltest` had 6294 after the runs of 2026-09-18. The ini files of `options.txt` land in the working folder, run it from a scratch folder or run `options_default.txt` after it. `delete.txt` and `repair.txt` are the two scripts of the sample server, the package server has no dead row on the test accounts and sells no kart with a unit type 3 price row, so neither the live Delete button nor a live repair can run there.

`pets.txt`, `paint.txt`, `roomcraft.txt` and `carcraft.txt` spend gold on every run: 1800 for the pet, 500 for the paint, 300 for the room object and 2000 for the two craft parts. `roomcraft.txt` and `carcraft.txt` run on `hltest2` whose password is `hltest2pw`, the other two on `hltest`. A rerun of `pets.txt` buys a second pet, the others are idempotent on the wire but not on the wallet.
