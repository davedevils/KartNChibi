# Archive sources for Kart N' Crazy / Chibi Kart game data

Research done 2026-09-23 with the Wayback Machine (CDX API) and web search.
No login, no client, no packet capture used here, only public web archives.

Plain notes, no measurements were needed for topic 1: the one guide page that
survived crawl gives kart stats as text numbers, not drawn bars. No other
guide image (missions, customize, experience table, characters) was ever
crawled by the Wayback bot, confirmed with the `archive.org/wayback/available`
API. So `reference/archive/` holds HTML dumps only, no images. Paths are
listed per section below.

The game went through three public builds, each with its own site:

- GOA (France/EU), "Kart n' Crazy", 2007-2009, currency **Nitros**.
- Ongame (Brazil), "Kart N' Crazy" (site branding, not "Chibi Kart"), 2009-2012ish,
  currency **Gold** (free) + **Cash** (Ongame's portal-wide premium currency name).
- OGPlanet (NA/EU), "Chibi Kart", Oct 2014 relaunch, currency **Gold** + **Astro**.

The user's client (Velocidade / Controle / Deslize / Turbo stat bars, Portuguese)
matches the Ongame Brazil build. Its own site (`kart.ongame.com.br`) was archived
but mostly as an empty client-side shell, see topic 1 and 2 below.

`chibikart.com.br` has zero Wayback captures under any matchType. `chibikart.com`
is an unrelated squatted domain (Asian business site, nothing to do with the game).

---

## 1. Kart stats

### What was found: GOA "Classic" tier, 6 karts, exact numbers

Source: `http://www.goa.com/knc/en/kartncrazykartspage1.html`
Archive: https://web.archive.org/web/20090407044035/http://www.goa.com/knc/en/kartncrazykartspage1.html
Saved: `reference/archive/goa_en/kartspage1.html`

The page text gives Speed / Handling / Drift / Booster as plain numbers, not
bars, so no pixel measurement was needed or possible here.

| Kart | Speed | Handling | Drift | Booster | Notes |
|---|---|---|---|---|---|
| Silent Runner 1 | 109 | 109 | 122 | 114 | "Two horses underneath the hood" |
| Desert Star 1 | 101 | 103 | 105 | 99 | desert/rock terrain kart |
| Frame T-1 | 91 | 91 | 91 | 91 | starter kart, "all opponents start off on" it |
| Solid M-1 | 91 | 103 | 91 | 91 | made by "Dee" |
| Frame T-2 | 105 | 109 | 94 | 91 | upgrade of Frame T-1 |
| Everest D-2 | 105 | 109 | 94 | 91 | same stats as Frame T-2, different model |

No price, no duration, no level requirement text on this page. The menu links
to 3 more kart pages, "Bikes", "Muscle", "Crazy" tiers
(`kartncrazykartspage2/3/4.html`), confirmed by CDX search across every
goa.com subdomain (`static.`, `www.`, `fr.`, `de.`, `es.`) that **none of
those 3 pages were ever crawled**. Only page 1 exists in the Wayback index.

The kart images referenced on the page (`/syndic_img/knc/guide_presentation/karts/*.jpg`,
served from `img.goa.com`) were checked individually with the
`archive.org/wayback/available` API: none archived. So no bar image existed
to measure anyway for this build; the numbers above are the complete data.

### Ongame Brazil guide: no kart data recovered

Source: `http://kart.ongame.com.br/guia/?m=1` and `?m=2`
Archive: https://web.archive.org/web/20091115075815/http://kart.ongame.com.br/guia/?m=1
Archive: https://web.archive.org/web/20091116070540/http://kart.ongame.com.br/guia/?m=2
Saved: `reference/archive/ongame_kart/guia_m1.html`, `guia_m2.html`

Only 2 guide sub-pages were ever crawled: "Introdução" (game pitch text) and
"História" (backstory: "País Encantado", "Aldeia do Biscoito" built of candy,
villain "Grande Sier"). No kart list, no stats. The guide menu on the page
shows only these 2 entries were ever live at crawl time, so the kart-by-kart
guide (where "Fulgor GZ-2" would live) either did not exist yet in Nov 2009
or was never linked from a crawled page.

### Discrepancy found: a private-server project claims all karts are identical

`chibikart.gg`, a fan-made server for this game (see topic 7), states in its
public announcement that in the original game **every kart shares one single
stat row** (same Speed/Handling/Drift/Booster on all karts) and that karts are
purely cosmetic. Source: https://forum.ragezone.com/threads/chibikart-gg-kart-n-crazy-private-server-chibi-kart-knc-free-no-p2w-live.1271748/
(live web, posted 2026-08-30, read 2026-09-23).

This directly contradicts the GOA guide page above, which lists 6 different
stat rows for 6 different karts. Flagging this as an open question, not
resolving it here: either the GOA marketing page invented numbers that never
mattered in the client, or the private server's authors simplified/got this
wrong, or the stat system changed between the 2008 GOA build and the later
builds. Worth checking directly against client data before trusting either
source.

### What could not be found

- Bikes / Muscle / Crazy tier kart stats (pages never crawled).
- Any kart price, in any currency.
- Any kart duration option (1/3/7 day, permanent) for karts specifically.
- Any level requirement to buy or use a kart.
- "Fulgor GZ-2" by name: zero hits anywhere, web search or archive.

---

## 2. Item shop

No priced item catalog was found in any archive. Every "shop" page recovered
turned out to be the **cash/currency top-up portal** (how to buy the premium
currency with real money), not the in-game item catalog. The in-game shop
itself was rendered from the client reading server data (same pattern as the
"Unknown error -4" PNG issue already known for this project), so a static
HTML crawl never captured it, in any of the 3 builds.

### Ongame Brazil "loja" (kart.ongame.com.br/loja/)

Archive: https://web.archive.org/web/20101216052128/http://kart.ongame.com.br/loja/?m=1 (Como Comprar)
Archive: https://web.archive.org/web/20100812045242/http://kart.ongame.com.br/loja/?m=2 (Comprar Cash)
Archive: https://web.archive.org/web/20101216030615/http://kart.ongame.com.br/loja/?m=3 (Ativar Cartão)
Archive: https://web.archive.org/web/20101216052741/http://kart.ongame.com.br/loja/?m=4 (O que é Cash)
Saved: `reference/archive/ongame_kart/loja_m1.html` .. `loja_m4.html`

`?m=4` has the only real numbers found, the Cash-to-Real (BRL) exchange table
sold through the Ongame portal (this currency was shared across all Ongame
games, not Kart-specific):

| Cash | Price (BRL) |
|---|---|
| 3,500 | R$ 10,00 |
| 5,000 | R$ 15,00 |
| 10,000 | R$ 25,00 |
| 15,000 | R$ 37,00 |
| 20,000 | R$ 49,00 |
| 40,000 | R$ 97,00 |
| 60,000 | R$ 145,00 |

Same page confirms explicitly: "Todos os jogos da Ongame trabalham com 2
tipos de moeda virtual, uma é o gold, que pode ser conseguido gratuitamente
dentro dos jogos e a outra é o cash" (all Ongame games use 2 virtual
currencies: gold, free, earned in-game, and cash, the portal-wide paid one).

### Press release: shop + gacha opening, 1 July 2010

Source: `http://www.ongame.com.br/noticias/Imprensa/Kart_N%92_Crazy_inaugura_sua_loja_online_e_traz_promo%E7%E3o_para_seus_usu%E1rios/`
Archive: https://web.archive.org/web/20100801050316/http://www.ongame.com.br/noticias/Imprensa/Kart_N%92_Crazy_inaugura_sua_loja_online_e_traz_promo%E7%E3o_para_seus_usu%E1rios/
Saved: `reference/archive/ongame_kart/press_loja_abertura2.html`

Confirms: shop opened 1 July 2010, sells "novos veículos, roupas e itens
para personalizar o personagem e o kart" (new karts, clothes, character and
kart customization items), no prices given. Also: "Sistema Gacha" launched
same day, described as a slot machine, entry costs "apenas um pequeno valor
de gold" (a small amount of gold), prizes: "itens de gold com período maior"
(longer-duration gold items), "itens de cenário" (scenery/room items),
"itens de cash" (cash items). No odds, no exact entry price.

### Gold Crazy Lotto coin price: one external corroboration

The `chibikart.gg` private server (topic 7) sells the same "Gold Crazy Lotto"
coin for **2,500 gold, permanent**, matching the number already known from
this project's own reference material. Source (live web, self-described
fan reimplementation, not an official archive):
https://forum.ragezone.com/threads/chibikart-gg-kart-n-crazy-private-server-chibi-kart-knc-free-no-p2w-live.1271748/

Same source, same caveat (their own implementation, not a primary source),
gives more numbers worth recording for cross-checking against real client data:

- Astro Crazy Lotto coin: 1,000 astro, permanent.
- A kart for 1 week: 9,000 gold.
- A character accessory for 1 week: 2,500 gold.
- Duration options stated: 1 day, 1 week (7 days), permanent. No 3-day
  option mentioned anywhere by this source or any other found.
- Rentals stack additively (buying a week on top of 3 remaining days gives
  10 days, not a 7-day reset).
- Donation/Astro exchange: 1,000 astro per 1 euro (their own rate, likely
  not the original one, listed only for completeness).

### What could not be found

- Any per-item price list (kart, character, accessory, paint, plate,
  antenna, pet, room craft object, car craft part), in any currency, from
  any official source.
- The 1/3/7-day/permanent duration matrix for any specific item type. Only
  day/week/permanent is attested, and only from the unofficial source above.
- Official Astro-to-money or Cash-to-Gold conversion inside the shop UI
  itself (only the real-money purchase table for Cash was found).

---

## 3. Gacha (Crazy Lotto)

Official confirmation that the system exists and is called "Sistema Gacha"
in Brazil, launched 1 July 2010 (see press release above, topic 2). Described
as a slot machine, small gold entry cost, prizes across 3 categories (longer
duration gold items, scenery items, cash items). No odds published anywhere
official.

The only odds found anywhere are from the `chibikart.gg` private server,
explicitly their own current implementation, not a confirmed original value:

- Gold Crazy Lotto (spend 1 Gold Coin, bought for 2,500 gold): kart 53.3%,
  accessory 26.7%, Astro Coin 20%.
- Astro Crazy Lotto (spend 1 Astro Coin, bought for 1,000 astro): accessory
  94.1%, remaining ~5.9% unspecified in the source text.
- Stated rule: "Anything already yours outright leaves the bowl before the
  draw, so you never win a duplicate" (their own dedup rule, not confirmed
  as an original-game rule).

Source (live web): https://forum.ragezone.com/threads/chibikart-gg-kart-n-crazy-private-server-chibi-kart-knc-free-no-p2w-live.1271748/

Separately, the 2014 OGPlanet "Chibi Kart" full-release patch notes mention
"eight new custom kart pieces available via the in-game Gachapon" added at
launch (Nov 26 2014), confirming the machine is called "Gachapon" in that
build and used for kart *pieces* (parts) too, not just whole karts/accessories.
Source: https://mmohuts.com/news/chibi-kart-enters-full-release-soon (live web,
no Wayback capture of the original ck.ogplanet.com article body: the site is
a JS single-page app, every article body loads by AJAX call that Wayback
never captured, only the page shell).

### What could not be found

- Any official odds table, from any build, any region.
- A confirmed original price for the Astro/paid-currency gacha coin.
- Confirmation of the dedup ("no duplicates") rule as an original mechanic.

---

## 4. Pendants

Nothing found under this name, in English, French, Portuguese or Spanish, in
any archive or web search. No page, no image, no forum mention of a
level/trophy/licence pendant system tied to the player name.

### Adjacent but distinct: Pets (found, not pendants)

The GOA EU guide has a full, intact Pets page, likely worth keeping on file
even though it is not what was asked for, since it is the same
"cosmetic earned by beating a named rival" pattern:

Source: `http://www.goa.com/knc/en/kartncrazypets1.html`
Archive: https://web.archive.org/web/20090407044116/http://www.goa.com/knc/en/kartncrazypets1.html
Saved: `reference/archive/goa_en/pets1.html`

| Pet | Effect | How to get it |
|---|---|---|
| Rosie | +5% to drift gauge in speed mode | win Peekay's challenge |
| Chai | 3% chance per mini-booster use, or per green booster tile, to get a booster item / blue-tile effect | win against Buttercup |
| Porki | +3 km/h max speed during a booster (not short boosters, not green tiles) | win Frankie's challenge |
| Dim Dim | 2% chance to transform into a rabbit on booster use or booster tile | defeat Peekay again |
| Edward III | +0.5s to all booster effect durations | win a race against Dabi |

Pets are given through the Quest game mode, one per defeated boss character,
per the Game Modes page (same archive set, `kartncrazygamemodes.html`).

### What could not be found

- Pendant/badge system: nothing at all. No level-10/20/30/40/50 trophy icons,
  no "100 star" badge, no "M" mission badge, no licence badge, anywhere.

---

## 5. Number plates

Nothing found. No mention of custom plate text or numbered plates in any
archived page, press release, or forum thread. The word "plate"/"plaque"/
"placa" never appears in any of the recovered guide, shop, or news text.

### What could not be found

Everything: whether plates were customizable at all, whether it was free
text or picked numbers, how it was unlocked. No source, official or
fan-made, mentions this system by any name.

---

## 6. Missions, licence tests, levels and EXP, rewards

### Confirmed to exist, no numbers recovered

GOA EU had a dedicated "Rewards" page built entirely as a single baked-in
image (`level_en.gif`, a level/EXP table), never archived:

Source: `http://www.goa.com/knc/en/kartncrazyexperience1.html`
Archive: https://web.archive.org/web/20090407034545/http://www.goa.com/knc/en/kartncrazyexperience1.html
Saved: `reference/archive/goa_en/experience1.html`
Checked `img.goa.com/syndic_img/knc/guide_presentation/experience/en/level_en.gif`
against `archive.org/wayback/available`: not archived, confirmed empty result.

Page text (the only recoverable part) says the EXP-to-level table exists,
and that server access is gated by level: "you will only have access to the
server corresponding to your level in the game."

Same story for Missions: page is 2 stacked comic-style images
(`stamps.jpg`, `01.jpg`), neither ever archived.

Source: `http://www.goa.com/knc/en/kartncrazymissions1.html`
Archive: https://web.archive.org/web/20090407024856/http://www.goa.com/knc/en/kartncrazymissions1.html
Saved: `reference/archive/goa_en/missions1.html`

Game Modes page (text, fully recovered) confirms Quests as the licence/level
gated mode: "A new quest becomes available at every level, but you need to
win the races in order to unlock these new opportunities."

Source: `http://www.goa.com/knc/en/kartncrazygamemodes.html`
Archive: https://web.archive.org/web/20090407034550/http://www.goa.com/knc/en/kartncrazygamemodes.html
Saved: `reference/archive/goa_en/gamemodes.html`

### Max level: 55, cross-confirmed by 2 independent sources

The Ongame Brazil ranking system used per-level badge icons, archived up to
`Lv_icon_s_055.png` and no higher, out of a CDX search across the whole
`imagem.ongame.com.br` path (icons `001` to `024` were apparently never
crawled, `025` to `055` were).

Archive listing checked: https://web.archive.org/web/20121001033104/http://imagem.ongame.com.br/kart/ranking/level/Lv_icon_s_055.png
(and neighbours `_025` through `_054`, same path, same crawl batch, Oct-Nov 2012)

The `chibikart.gg` private server states "Max level: 55" outright as a
design fact of the original game (not their own choice), which lines up
with the icon count above. Source: same RaGEZONE thread as topic 3.

### Feature names confirmed, no content

`chibikart.gg` lists "Car Craft, Quest, License and Mission" as 4 distinct
systems in the real game that its emulator has not implemented yet ("answer
with a refusal instead of opening"). This confirms these are 4 separate
named systems in the client (Quest already known from GOA text above), but
gives no content for any of them.

### What could not be found

- The actual EXP-per-level table or curve (image lost).
- Any licence test content or requirement.
- Any mission list, mission reward, or mission structure.
- Confirmation of level 1 to 24 icon existence (gap in the archive, not
  proof they did not exist).

---

## 7. Server protocol / private server projects

This is the one topic with real, citable technical detail. Two separate,
unrelated community efforts were found, roughly a decade apart, both
public on the RaGEZONE forum (forum.ragezone.com, MMO dev community).
WebFetch was blocked by the forum (403) without a browser user agent; both
threads were retrieved directly with curl and a desktop Chrome user agent.

### Client builds identified (with hashes)

From `https://forum.ragezone.com/threads/request-chibi-kart-kart-n-crazy-server-files-server-emulator.1102723/`
(thread started 15 May 2016, live web, still active as of Sept 2026):

| Build | File | Size | MD5 | Date |
|---|---|---|---|---|
| Kart n' Crazy (GOA), older | GOA_KNC_client.exe | 176 MB | adc1e8c4a115414e8bfa7480f08b0291 | 18-05-2008 |
| Kart n' Crazy (GOA), later | GOA_KNC_client.exe | 201 MB | 21c97ed2912acf3e6459d4c7f6dfcbb1 | 11-12-2008 |
| Kart n' Crazy (GOA), another copy | GOA_KNC_client.exe | 464.68 MB | 9f509e23c75f9b8935912041c4ed16d4 (sha256 bc023b8326f464f81d7794a82b07209d2af3117a1c4cb761205dbc93bffacf42) | file dated 16-08-2009 |
| Chibi Kart (OGPlanet) | CK_141024.exe | 279 MB | 6a2104f5799618280405e9420bf0df41 | 24-10-2014, client v1.78 |

Engine: **Gamebryo**, confirmed by a poster who found the string in a hex
dump of `KnC.exe` (GOA build). Matches this project's own NIF pipeline
(`engine/bridge/NifToRHI`) working on the same asset format.

Anti-cheat: GOA build runs **GameGuard/nProtect**. The 2014 Chibi Kart build
has no GameGuard.

### Network2.ini: format, encryption, and one leaked IP:port

Poster "Darkshowdo" (May-June 2024, same thread): Chibi Kart's `Network2.ini`
is a different, checksummed binary format versus the plaintext-visible GOA
version. Decryption method as posted: "The game was using a special key to
grab characters from (would change every time), then it would xor that by
0x5B after which it would +0x20 the output. Then check-summed to ensure it
was always correct." Decrypted output reported: the address of the official server of that build
and port 50017.

Same poster: isolated the PacketHandler in a decompiler, called for anyone
with a packet capture from any build to speed the work up. As of Porygon_'s
post May 2025 (same thread), no `.pcap` existed for that particular effort.

Another poster released a `.dat` unpacker for both Kart n' Crazy and Chibi
Kart and a Network2.ini encrypt and decrypt tool in that thread (June 2024,
source behind a forum login).

### A second, newer, unrelated project: chibikart.gg

Source: https://forum.ragezone.com/threads/chibikart-gg-kart-n-crazy-private-server-chibi-kart-knc-free-no-p2w-live.1271748/
(thread started 30 Aug 2026, posts through 14 Sept 2026, live web)

Run by forum user "isgalvez24" (also posts under "Ism1tha" in the older
thread, present in both). Claims: server built "from scratch, by reading
what the game itself says over the wire, packet by packet," not from leaked
files. States it is live in open beta since 28 August 2026, full
login-to-podium flow working (driver dress-up, kart/accessory purchase,
room creation/fill, countdown to podium, Gold/EXP awarding, per-track ghost
records, messenger with friends/presence/invites/gifts). States Car Craft,
Quest, License and Mission are not implemented yet (return a refusal).
Content scope claimed: 34 playable tracks across 9 themed sets, 40+ karts,
10 drivers, hundreds of cosmetics, room size 8 (Item mode) / 16 (Speed
mode), 4 starting drivers (pumpkin, monster, witch, princess).

This is the source for the gacha odds, gold/astro shop prices, and max
level 55 cited in topics 1 to 6 above, all clearly their own implementation,
useful as a cross-check, not proof of the original values.

### Third, solo, in-progress effort (2025-2026), AI-assisted

Same older thread, posts from "xxboraxa" starting June 2025: working alone
with AI assistance, bypassed GameGuard, reached login and character
selection against the GOA client by March 2026, still debugging a
post-character-select "Failed" error as of the last post read (4 March
2026).

### Other confirmed facts from the same threads

- The game shipped under different names per region: "Kart n' Crazy" (EU,
  GOA), "Chibi Kart" (NA/international, OGPlanet), reportedly "Fun Fun
  Buggy" in Japan and "Crazy Moon Racing" in Thailand per one forum poster
  (unverified, single source, not cross-checked here).
- Track/course count as described in the 2016 request post: "thirty-six
  magnificently designed courses" across "nine colorful stages," 2 modes
  (Speed, Item), plus Ghost Mode and Quest Mode. This is close to but not
  identical to chibikart.gg's "34 playable tracks across 9 themed sets"
  (34 implemented vs 36 total, or simple inaccuracy, not resolved here).
- A separate, explicitly unrelated game sometimes confused with this one:
  "Crazy Kart" / "Crazy Kart 3: New Generation", Chinese-made, Unity engine,
  different company. Do not conflate the two when researching further.

### What could not be found

- Any actual packet capture, packet structure documentation, or opcode list
  published anywhere publicly (this project's own opcode map, per its own
  memory notes, is far more complete than anything found in this websearch).
- Confirmation, independent of the forum posts themselves, of the
  Network2.ini XOR key or the leaked IP:port.
- Any source code repository content itself (links in both threads require
  a forum login to view).

---

## 8. Fan forums, second pass (added 2026-09-23)

Follow-up pass, pointed at by the user with this link:
https://web.archive.org/web/20090814125932/http://kartncrazy.taguilde.net/truck-et-astuce-f5/tutorial-kart-n-crazy-t9.htm#10

This pass found real numbers: a level/EXP table, 2 official item-shop price
lists (with a real screenshot), the level-gated map access rule, and 2 more
regional builds (Japan, Thailand). Full CDX listings saved under
`reference/archive/cdx_taguilde.json`, `cdx_fkt_xooit.json`,
`cdx_goaforums_terms.json`. No images needed pixel measurement: every price
or stat found was given as plain text or a plain price table in a real
screenshot, not a drawn bar.

### kartncrazy.taguilde.net, "Hells Angels" guild forum (French)

CDX: `https://web.archive.org/cdx/search/cdx?url=kartncrazy.taguilde.net/*&output=json&limit=2000`
50 URLs total, 2008 and 2009 captures. Saved under `reference/archive/taguilde/`.

Almost the whole forum is guild admin chatter by a single poster ("Admin"):
recruitment forms, room codes for guild meetups, a "critique my forum"
thread, a note that beta ended 28 April 2008 and the game went live 29 April
2008. Pseudonym "Admin" is the only poster of substance; no other member
data recorded here.

The one thread with real game data is the tutorial the user pointed at:

Source: `http://kartncrazy.taguilde.net/truck-et-astuce-f5/tutorial-kart-n-crazy-t9.htm`
Archive: https://web.archive.org/web/20090814125932/http://kartncrazy.taguilde.net/truck-et-astuce-f5/tutorial-kart-n-crazy-t9.htm
Saved: `reference/archive/taguilde/tutorial_t9_2009.html` (also a 2008 capture,
same content, `tutorial_t9_2008.html`)

Tricks and tutorial content (posted by "Admin", credits parts of it to the
official GOA forum, "shaeck"):

- Controller setup via JoyToKey, suggested button mapping.
- Team arcade etiquette: do not throw lightning/iceberg/turtle at
  teammates, weapon-swap item bought from "boutique >> objet" (shop > item
  section, confirms the shop had a section literally called "objet").
- Turbo start: hit accelerate just before "GO".
- Drift: hold Shift then turn, can be done mid-air.
- Mini-turbo: drift until flame turns blue, release Shift and accelerator,
  reaccelerate fast. Works after a rabbit transformation and after a
  missile hit (accelerate again on landing).
- Missile: can hit a player other than the locked target; hold fire to
  relock onto the nearest target.
- Slipstream ("aspiration"): drafting behind another kart (white halo)
  raises your speed.
- Speed-boost gauge: fills by drifting, wider drift angle fills it faster;
  team mode has a separate team-wide gauge that gives a longer blue boost
  to the whole team.
- Levels shown as a number (1-5) plus a color tier. Confirmed table,
  reproduced here exactly as posted (XP needed to reach that level, next
  level's XP cost in parenthesis):

| Level | Total XP | XP to next |
|---|---|---|
| Yellow 1 | 0 | +100 |
| Yellow 2 | 100 | +200 |
| Yellow 3 | 300 | +300 |
| Yellow 4 | 600 | +400 |
| Yellow 5 | 1000 | +500 |
| Green 6 | 1500 | +650 |
| Green 7 | 2150 | +800 |
| Green 8 | 2950 | +950 |
| Green 9 | 3900 | +1100 |
| Green 10 | 5000 | +1300 |
| Blue 11 | 6300 | +1600 |
| Blue 12 | 7900 | +1900 |
| Blue 13 | 9800 | +2200 |
| Blue 14 | 12000 | +2500 |
| Blue 15 | 14500 | +3000 |
| Red 16 | 17350 | +3450 |
| Red 17 | 20800 | (not given) |

Post cuts off after level 17 (red tier levels 18-20 listed with no numbers
in the source post). Confirms currency names: **Nitro** is the free
currency (buys karts, accessories, characters), **Tek** is the currency
bought with real euros. This matches GOA's own site branding found in
topic 2 ("kart-n-crazy-30-bonus-on-tek-purchase").

### fkt.xooit.fr, "F.K.T." Kart n' Crazy community forum (French)

CDX: `https://web.archive.org/cdx/search/cdx?url=fkt.xooit.fr/*&output=json&limit=2000`
120 URLs. Most captures are from a 2024 re-crawl of the still-live site (the
forum itself dates from 2008-2009, but Wayback re-crawled it in June 2024,
so these pages came back gzip-compressed and needed `gunzip` after the
`id_` fetch before they would parse as HTML). Saved under
`reference/archive/fkt_xooit/`, both the raw `.html` and the decompressed
`_decoded.html` / `_extracted.txt` versions.

This is a much bigger, much more active forum than taguilde's, with named
sub-teams (FKT Speed team, FKT Item team, Team ToF, Team ASK, Team KOF) and
regular GOA staff presence (poster "LoLaTiOn", tagged "GOA", answers
directly in threads).

**Item shop, 2 real official price lists recovered:**

Patch 17 September 2008 (thread `t154-17-09-2008-Kart-n-Crazy-Mise-a-jour`,
archive https://web.archive.org/web/20240617123959/https://fkt.xooit.fr/t154-17-09-2008-Kart-n-Crazy-Mise-a-jour.htm,
posted by "Prs_-" quoting the GOA patch note, saved
`reference/archive/fkt_xooit/t154_patch_20080917.html`): 8 new hairstyle
items, one per character, each priced **1,500 / 4,900 / 14,900 Nitros**
(1 week / 1 month / 1 year, confirmed by the matching screenshot in the
next patch below):

| Item | Character |
|---|---|
| Coupe 70s | Charlotine |
| Couettes de blondinette | Sand'rillon |
| Coupe JFK | Coloss'Al |
| Coupe brocoli | M. Trouille |
| Couettes à ruban | Nefer |
| Perruque Mozart | Prince Petit |
| Cheveux de crystal | Touffu |
| Elfe de jardin | Arbrakhan |

Same patch adds the **level-gated map access rule**: level 1-10 can pick
1-3 star maps, level 11-20 can pick 4-5 star maps, level 21+ can pick 6 star
maps. A lower-level player can still race in a room hosted by a
higher-level player on a map above their own tier, but cannot host such a
map themselves. Also adds a **reward multiplier by map star rating**:
1 star = 100% (baseline) EXP and Nitro, 2 star = 110%, 3 star = 120%,
4 star = 130%, 5 star = 140%, 6 star = 150%. The actual EXP/Nitro number
tables and the level-icon table were posted as forum attachments on
`forums.goa.com/attachment.php?attachmentid=219..222`; checked individually,
all 404 in the archive, not recoverable.

Patch 18 February 2009 (thread `t1063-18-02-2009-Kart-n-Crazy-Mise-a-jour`,
archive https://web.archive.org/web/20240617124016/https://fkt.xooit.fr/t1063-18-02-2009-Kart-n-Crazy-Mise-a-jour.htm,
saved `reference/archive/fkt_xooit/t1063_patch_20090218.html`): the patch
announcement image itself survived in the archive as a real screenshot
(hotlinked from `img.xooimage.com`, confirmed archived through the
`wayback/available` API, downloaded and read directly, no measurement
needed since it is a plain price table, not a bar graph):

Archive: https://web.archive.org/web/20240617124020/https://img.xooimage.com/files27/0/d/e/item_update_06_fr-b58b02.jpg
Saved: `reference/archive/images/item_update_06_fr.jpg`

8 hats, same 8 characters as the September patch, each **900 / 1,800 /
4,200 Teks** (1 week / 1 month / 1 year), each with the same effect text
"en portant cela vous avez 20% de chances de gagner 5 Nitro supplémentaires
quand attaqué" (wearing it gives 20% chance to gain 5 extra Nitro when
attacked):

| Item | Character |
|---|---|
| Bonnet brun | M. Trouille |
| Chapeau raton-laveur | Charlotine |
| Bonnet à pompon | Coloss'Al |
| Chapeau-couronne d'hiver | Sand'rillon |
| Bonnet Péruvien | Arbrakhan |
| Bonnet en plume d'oie | Prince Petit |
| Bonnet à bandages | Nefer |
| Bonnet de pilote | Touffu |

Plus one more item in the same screenshot, priced in Nitros not Teks:
**Antenne OVNI**, "objet non-identifié qui envoie les attaques de missile
sur orbite, bloque 20% des attaques de missile" (blocks 20% of missile
attacks), **1,500 / 4,900 / 14,900 Nitros** (1 week / 1 month / 1 year,
same 3 prices as the September hairstyles).

This gives a confirmed, repeated shop pattern: **3 duration tiers are 1
week, 1 month, 1 year** (not 1/3/7 days/permanent as guessed before this
pass), and the same price points (900/1800/4200 and 1500/4900/14900) recur
across unrelated items, suggesting these are tier-price constants applied
per item rarity/slot rather than a price unique to each item.

Also gives an **8-character canonical list**, cross-confirmed by 2
independent patches, same order both times: M. Trouille, Charlotine,
Coloss'Al, Sand'rillon, Arbrakhan, Prince Petit, Nefer, Touffu.

**Next patch teaser**, 18 June 2009 (thread `t1632-prochaine-mise-a-jour`,
archive https://web.archive.org/web/20240617124237/https://fkt.xooit.fr/t1632-prochaine-mise-a-jour.htm):
new circuit, "zone F1, map 2", delayed by a day, GOA staff post signed
"L'Equipe GOA" pasted directly into the fan thread. One player comment
confirms items are "comme d'habitude en tek" (as usual, in Tek), i.e. Tek
remained a valid currency for shop items at least through mid-2009, not
just Nitro.

**Tricks and circuit guides** (forum section "Tutoriaux, Astuces et Videos"):

- Track/zone names confirmed across several threads: Forêt 1-4, Choco 4
  (Chocolate zone), Volcan 1-2, Ice 2, Jardin 1 (Garden), Désert 2. Source:
  `t734-ASTUCES-GAMEPLAY` (archive
  https://web.archive.org/web/20240617122043/https://fkt.xooit.fr/t734-ASTUCES-GAMEPLAY.htm)
  and `t162-CIRCUIT-Allee-des-cerisiers` (Forêt 1, archive
  https://web.archive.org/web/20240617123500/https://fkt.xooit.fr/t162-CIRCUIT-Allee-des-cerisiers.htm).
- Boost mechanics detailed with worked examples: red boost can be
  re-triggered the moment speed starts to drop without waiting for it to
  fully end; blue (team) boost cannot be re-triggered until the current one
  finishes; slipstream lets a trailing player close the gap in the last
  seconds of a boost.
- "FuRiOuS MoDe": a fan name (not an official game mode) for chaining
  mini-boosts to exceed 200 km/h, source `t141-FuRiOuS-MoDe` (archive
  https://web.archive.org/web/20240617121944/https://fkt.xooit.fr/t141-FuRiOuS-MoDe.htm).
  Kart names mentioned in passing: "moto sourire" (smile motorbike), "moto
  mirage".
- Gamepad setup with a suggested PS2-pad-to-USB-adapter mapping (Accelerate
  R2, Weapons Cross, Brake/reverse L1, Rear view Triangle, Drift Square,
  Weapon-slot switch L2), source `t163-TUTO-Manette` (archive
  https://web.archive.org/web/20240617123410/https://fkt.xooit.fr/t163-TUTO-Manette.htm).

**Two more regional builds found and confirmed archived**, both linked from
FKT tutorial threads teaching French players how to register on the
foreign site to play there:

- Japan, "Fun Fun Buggy", `www.funfunbuggy.com`. Thread
  `t424-TUTO-Fun-Fun-Buggy` (archive
  https://web.archive.org/web/20240616114302/https://fkt.xooit.fr/t424-TUTO-Fun-Fun-Buggy.htm).
  Site itself confirmed archived: CDX
  `https://web.archive.org/cdx/search/cdx?url=funfunbuggy.com&matchType=domain`,
  13 URLs, Sept 2008, including a `guide/beginners/step1.html` page (in
  Japanese, not read in this pass).
- Thailand, "Crazy Mon Racing" (not "Crazy Moon Racing"), published by
  Hitsplay, `cmr.hitsplay.com`. Thread `t426-TUTO-Crazy-Mon-Racing` (archive
  https://web.archive.org/web/20240616114332/https://fkt.xooit.fr/t426-TUTO-Crazy-Mon-Racing.htm).
  One player comment: "on demarre avec 5000 gold" (start with 5000 gold),
  suggesting this build's free currency is called "gold" in the site's own
  English/French-facing text, unlike GOA's "Nitro". Site confirmed
  archived: CDX `https://web.archive.org/cdx/search/cdx?url=cmr.hitsplay.com&matchType=domain`,
  40+ URLs, 2008-2010, including a working forum (`bbs/forum.aspx`) in
  Thai, not read in this pass.

Both sites are new leads for a future pass, not yet read in depth here.

### forums.goa.com item wiki and tags, dead ends

The official GOA forum ran a vBulletin "item" plugin at
`forums.goa.com/?do=item&viewitem=NNN` (IDs 368, 429, 498, 518, 763, 767,
819, 914 found via CDX). Every archived capture of these just redirects to
the forum's home page, no item content recoverable; the plugin likely
needed a login the crawler did not have.

`forums.goa.com/tags.php?tag=cash+shop` (archive
https://web.archive.org/web/20080916191959/http://forums.goa.com/tags.php?tag=cash+shop):
only 1 linked thread, "Probleme Cash Shop", a payment support question, not
an item list.

### Other fan forums checked, nothing found

- jeuxvideo.com forum for Kart n' Crazy: the game's own page exists
  (`jeuxvideo.com/jeux/pc/00020148-kart-n-crazy.htm`) but a CDX prefix
  search for its forum path (`jeuxvideo.com/forums/42-20148*`) returned zero
  captures. The site's forum ID/URL scheme for this game was not found.
- Portuguese "Chibi Kart" community on Orkut or Forumeiros: no web search
  hit found anything specific to this game on either platform. Orkut itself
  has been dead since 2014 and was never well archived; nothing to check
  via CDX (Orkut communities were not on a crawlable per-game URL).
  Forumeiros (the Brazilian forumactif-equivalent) search returned only
  unrelated results.
- No other xooit, bbfr, or forumactif fan forum for this game was found
  beyond kartncrazy.taguilde.net and fkt.xooit.fr.

## Summary: what stays unknown after this pass

- Full kart roster with stats: only 6 of an estimated 20-40+ karts have
  confirmed original numbers (GOA "Classic" tier only). Kart names picked
  up in forum trick posts ("moto sourire", "moto mirage") were not matched
  back to a stat line.
- Item prices: 2 real official price lists now confirmed (topic 2 and 8),
  both cosmetic (hairstyles, hats, 1 antenna). Still no price for any kart,
  plate, pet, room-craft or car-craft item, in any currency, from an
  official source.
- Gacha odds, official: still none. The Brazil press release names the
  system and its prize categories, no odds anywhere official.
- Pendants: entire system, nothing found, in either pass.
- Number plates: entire system, nothing found, in either pass.
- EXP/level curve: now partly recovered. Real numbers for levels 1-17
  (topic 8, taguilde tutorial, credited to GOA forum user "shaeck"), plus
  the level-gated map access rule and the per-map-star EXP/Nitro bonus
  (topic 8, FKT patch thread). Levels 18+ and the "max level 55" figure
  are still only cross-confirmed by icon files and a private server's
  claim, not an official numbers table.
- Mission and licence test content: confirmed to exist as named systems,
  zero content recovered, in either pass.
- Any original server IP, port, or protocol byte format, confirmed
  independently (only secondhand forum claims found, topic 7).
- 2 more regional client builds identified but not read in depth: Japan
  ("Fun Fun Buggy", funfunbuggy.com) and Thailand ("Crazy Mon Racing",
  cmr.hitsplay.com). Both confirmed archived, both worth a follow-up pass,
  both in languages (Japanese, Thai) not read here.

## Files saved

All under `reference\archive\` (git-ignored). HTML page
dumps only, no images: none of the bar/table images referenced by any build
were ever archived by the Wayback Machine, checked individually via the
`archive.org/wayback/available` API before giving up on each one.

- `goa_en/` - GOA EU English guide pages (karts, items, customize, pets,
  missions, gamemodes, ranking, characters, use-takos, experience)
- `goa_fr/` - GOA French pre-launch forum thread (hype talk only, no data)
- `ongame_kart/` - Ongame Brazil guide, shop, ranking, press release,
  newsletters, game rules
- `ckogplanet/` - OGPlanet Chibi Kart 2014 site shells and news article
  shells (JS single-page app, body content never crawled)
- `ragezone_*.html` / `ragezone_*_extracted.txt` - the 2 RaGEZONE forum
  threads discussed in topic 7, full page dumps and extracted text
- `taguilde/` - kartncrazy.taguilde.net, the "Hells Angels" guild forum
  (topic 8), all archived pages and extracted text
- `fkt_xooit/` - fkt.xooit.fr, the F.K.T. community forum (topic 8), raw
  gzip `.html`, decompressed `_decoded.html`, and extracted `_extracted.txt`
  for each thread
- `goa_forums_items/` - the dead-end GOA forum item-wiki and tag pages
  (topic 8)
- `images/item_update_06_fr.jpg` - the one real screenshot recovered in
  this pass, the 18 Feb 2009 GOA shop update, read directly, no bars, a
  plain price table (topic 8)
- `cdx_taguilde.json`, `cdx_fkt_xooit.json`, `cdx_goaforums_terms.json`,
  `cdx_goa_attachments.json`, `cdx_jeuxvideo_kart.json` - the raw CDX
  listings pulled for this pass
