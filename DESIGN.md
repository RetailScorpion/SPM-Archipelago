# Super Paper Mario Archipelago: Design

Status: draft, design phase. Nothing is implemented yet.
Target: US release (1.0 for development, 1.1 and 1.2 later).

Items marked **(verify)** come from the Super Mario Wiki and still need to be confirmed against the game's own scripts.

---

## 1. Goal

- Beat the final boss (Super Dimentio, 8-4).
- Pure Heart 8 opens the Chapter 8 door, like any other heart.
- Reaching the final fight also requires holding **`pure_hearts_required`** Pure Hearts (1–8).
  The game blocks the path to the boss fight in 8-4 until you have enough, with a message saying how many you need.
- "Holding" means received from Archipelago, whether or not the heart has been placed in its pillar.

## 2. Hub and chapter structure

### Heart pillars (vanilla locations, from the wiki)
| Pillar | Hub location | Vanilla requirement to reach it |
|---|---|---|
| 1 | Flipside 3F (left) | none |
| 2 | Flipside 1F outskirts (right), high platform | Peach (float) |
| 3 | Flipside B1 outskirts (right) | Slim (get past a wall) |
| 4 | Flipside 2F outskirts (left) | Thudley (pound pegs in a pattern) |
| 5 | Flopside 3F (right) | Fleep |
| 6 | Flopside 1F outskirts (left) | Fleep + Cudge (break disguised stone, hit switch) |
| 7 | Flopside B1 outskirts (left) | Fleep + Luigi (jump over high wall) |
| 8 | Flopside 2F outskirts (right) | Fleep; invisible until after Ch7, **we make it visible from the start** |

Every Flopside pillar needs Fleep. Modelled as: the Flopside region requires `fleep`, and the extra requirements above apply on top of that.

### Rules
- Pillar N accepts **only** Pure Heart N. Interacting without it shows a message (e.g. "This pillar needs the Orange Pure Heart").
- Activating pillar N sets our own flag `PILLAR_N` and opens **only** the one door mapped to it.
  - Vanilla mapping: pillar N opens door N.
  - With `pillar_door_shuffle` on, the seed picks a random mapping: pillar 1 still opens door 1, and pillars 2–8 open doors 2–8 in shuffled order.
    The heart still has to go in its own pillar. Only which door that pillar opens changes.
  - The mapping is sent in slot data. The client writes it to the mailbox on connect, and the game reads it whenever it needs to know whether a door is open.
  - With `starting_open_doors` = X, the seed opens X random doors from 1–7 at the start, whether or not their pillars are active. Door 8 is never pre-opened.
    A door is open if its mapped pillar is active **or** it was pre-opened.
    The pillar for a pre-opened door still accepts its heart, which still counts toward `pure_hearts_required`. Examining that pillar says the door is already open.
  - The player needs to be able to see the mapping. A pillar shows "This pillar opens the door to <chapter>" when you examine it, before and after it's activated, and the PopTracker pack shows it too.
- Flipside is fully open from the start (all floors and elevators). Flopside needs Fleep, as in vanilla, unless `pillar_requirements: open`.
- Inside a chapter, levels stay sequential: N-1 → N-2 → N-3 → N-4. Each level opens when the previous one is cleared.
  This isn't an option, because each chapter's story runs on its own progress value (see §6).
- Clearing N-4 gives the chapter-reward location (vanilla: the next Pure Heart).

### Chapter-specific story problems
- **Ch1 → Peach:** in vanilla, Peach is revived in a Flipside cutscene after Ch1. That becomes a hub location: available from the start with `open_hubs`, or after clearing Ch1 without it.
- **Ch4 Helmet interlude:** partway through 4-1 you can't breathe, and you're sent back to Flipside. Merlon sends you after the boy with the Goldfish Bowl, and the goldfish trade gets you the Helmet. Then you return to 4-1. So **most of Chapter 4 requires the Helmet** (STG4_HELMET), and the Goldfish Bowl and Helmet steps are hub checks.
- **Ch6 ending:** Sammer's Kingdom is destroyed, the heart turns to stone, and you play the "???" level in the World of Nothing. After that, Dimentio defeats the party in Flipside and you play the **Underwhere intermission**. There Luigi joins (seq 297) and you get the revitalised Pure Heart (seq 300).
  We keep that whole run (World of Nothing, then the defeat, then the Underwhere intermission) as **the end of Chapter 6**. Its locations are "Luigi joins" and "Revitalised Pure Heart" (the Ch6 reward).
- **Ch6 layout (confirmed by the user from playing):** the story only uses the first map group (`wa1`, Sammer's Kingdom). Going through the Ch6 door again after that loads the white "???" map (World of Nothing). `wa2`–`wa4` (gates 26–100) are post-game only.
- **Sammer's gauntlet (always available after Ch6):** once Ch6 is cleared, Chapter 6's own saved story value is set to the post-game state. That's easy with per-chapter story values, and the hubs and other chapters aren't affected. So the Ch6 door then leads to the full 100-duel Sammer's Kingdom.
  - **Locations:** in vanilla, King Sammer hands over all seven TTYD partner Catch Cards at once after the 100th Sammer Guy: Goombella, Koops, Madame Flurrie, Yoshi, Vivian, Admiral Bobbery and Ms. Mowz (items 496–502).
    That gives **seven locations on one event** ("King Sammer Reward 1–7"). They're all checked together when he hands them over, and the game shows each item received in turn.
  - **Items:** the seven partner cards go into the item pool as filler, so they can still be found somewhere.
  - **`sammer_gauntlet` option** controls what those locations can hold:
    - `off` (default): the locations are marked **excluded** and only ever hold filler. The gauntlet is long and hard, so nothing important is stuck behind it.
    - `on`: the locations are normal and can hold progression items.
- **Ch7 entry:** door 7 leads to 7-1 with your party intact. In vanilla, Bowser rejoins in 7-2 (seq 313–314) and Peach in 7-3 (seq 330–331). Those scenes become **locations** that give whatever's placed there, whether or not character rando is on.
- **Ch8 party splits:** Bowser holds up the roof, Peach falls through the floor, Luigi gets trapped. With character randomization on, **entering Ch8 requires all three characters**. The game enforces this at door 8, and the logic does too. The alternative would be rewriting those scenes for any party, which is far more work and risk.

## 3. Item pool

### Always randomized
- **Pure Hearts 1–8** (8 items)
- **Story Pixls** (verify each chapter):

  | Pixl | Vanilla location | Ability (logic name) |
  |---|---|---|
  | Thoreau | 1-2, Yold Town chest | `throw` |
  | Boomer | Gloam Valley (Ch2) | `bomb` |
  | Slim | Merlee's Mansion (Ch2) | `thin` |
  | Thudley | 3-2 Tile Pool | `ground_pound` |
  | Carrie | 3-4 Fort Francis basement | `carry` (platform, spike crossing) |
  | Fleep | 4-2 Planet Blobule outhouse | `fleep` (flip objects between dimensions) |
  | Cudge | 5-2 Gap of Crag | `hammer` |
  | Dottie | 5-3 Floro Caverns | `shrink` |

- **Key items** (wiki's "Important Things" list, 32 entries). Known story items include:
  Ruins Key ×3 (1-4), Door Key (Gloam Valley), Door Key (Planet Blobule), Door Key (Underwhere),
  Paper / Ancient Clue (4-2, Fleep's quest), Fire / Water / Stone Tablet (5-2, Cudge's shrine),
  Fort Key, Cave Key, Card Key, Red/Yellow/Blue Orb, Floro Sprout, Diet Book, Castle Bleck Key.
  The rest (Golden Card, Crystal Ball, Cooking Disk, Training Machine, Goldfish Bowl, Random House Key, Pit Key, Autograph, Helmet, House Key, Old Key, Dimension Key, You-Know-What) still need sorting into story items vs. side-quest items while mapping locations.

### Never randomized
- **Mario:** the game assumes he's always in the party.
  He does **not** start out able to flip: Bestovius teaches flipping in 1-1 (seq 016). See `flip` in §5.
- **Tippi:** she's the pointer, needed for hidden objects and for most of the story's assumptions.
- **Return Pipe:** given at the start. It's our protection against getting stuck (softlocks).
- **Tiptron:** post-game purchase. Excluded.

### Optional (see §5)
- Peach, Bowser, Luigi
- Barry (3-1 bush), Dashell (Flipside Pit floor 100), Piccolo (end of the Flopside trading sequence)
- Overworld items, blocks, shops, Pit rewards, side quests, and so on

### How "not randomized" works in the apworld
Every optional category still exists as items and locations. When its option is off, each item is **locked to its vanilla location** (`place_locked_item`). The logic code is the same either way: "needs float" always means "has Peach". The only difference is whether Peach can be found anywhere or only after clearing Ch1.

## 4. Abilities and logic

### Abilities
| Ability | Comes from | Notes |
|---|---|---|
| `flip` | Bestovius (1-1) | Depends on the `flip` option: from the start, at Bestovius, or anywhere |
| `reveal` | Tippi | always available |
| `float` | Peach | |
| `fire` | Bowser | |
| `super_jump` | Luigi | |
| `throw`, `bomb`, `thin`, `ground_pound`, `carry`, `fleep`, `hammer`, `shrink` | story Pixls | |
| `barrier` | Barry | optional Pixl |
| `dash` | Dashell | optional Pixl |
| `music` | Piccolo | optional Pixl; cures Cursya curses |

### Region graph
```
Menu
 └─ Flipside                                  (open from the start)
     ⇄ Flopside  [needs fleep, unless pillar_requirements: open]
     ├─ Pillar N area [ability req, §2]  →  "Activate Pillar N"  [needs Pure Heart N]
     └─ Door D  [pre-opened, OR needs PILLAR_P where map[P] = D; identity unless pillar_door_shuffle]
         └─ N-1 → N-2 → N-3 → N-4 → "Chapter N Clear" location
             (each level split into sub-regions at ability gates)
 Door 8 (+ all characters if character rando) → 8-1 … 8-4
     └─ Final boss [needs pure_hearts_required hearts]  →  Victory
```

- Every level is split into sub-regions wherever a Pixl, character or key item is needed. Every location sits in the smallest sub-region that contains it.
- "Level N-k clear" is an event (no item) that opens N-(k+1).
- Story fights inside a chapter count as part of that chapter's path. If a boss needs an ability, it's a requirement on the next sub-region.
- Duplicate key items (three Ruins Keys, for example) are tracked by count. Using a key on any matching door works.

### Logic difficulty
v1 has a single "intended" logic: no tricks, no damage boosts. A `logic_difficulty` option can come later, once speedrun tricks are documented.

## 5. Options (draft)

| Option | Values | Default | Notes |
|---|---|---|---|
| `pure_hearts_required` | 1–8 | 8 | Hearts needed to fight the final boss |
| `merlon_gift` | pure_heart / random_item | pure_heart | What Merlon gives at the start. `pure_heart`: the heart picked by `starting_pure_heart`. `random_item`: Merlon is a normal location |
| `starting_pure_heart` | chapter_1 / random | chapter_1 | Which heart Merlon gives. Ignored when `merlon_gift: random_item`. See "Starting heart rule" below |
| `open_hubs` | on / off | on | On: every hub check tied to the story is available from the start. Off: each one waits for its vanilla story trigger. See "Hub checks" below |
| `randomize_characters` | on / off | off | Peach, Bowser, Luigi in the pool |
| `pillar_requirements` | vanilla / open | vanilla | `open` removes the ability gates around pillars 2–8, including the Fleep requirement for Flopside |
| `pillar_door_shuffle` | on / off | off | Pillars 2–8 open doors 2–8 in random order; pillar 1 always opens door 1 |
| `starting_open_doors` | 0–7 | 0 | X random chapter doors (from 1–7, never 8) open at the start, regardless of pillars |
| `flip` | start_with / vanilla / random | start_with | `start_with`: you can flip from the start. `vanilla`: Flip is locked to Bestovius in 1-1. `random`: Flip can be anywhere in the multiworld. Bestovius's lesson is a location in all three |
| `sammer_gauntlet` | on / off | off | The 100-duel Sammer's Kingdom always opens after Ch6, and its TTYD partner card rewards are always checks. `off`: they only hold filler. `on`: they can hold progression. See §2 |
| `optional_pixls` | on / off | off | Barry, Dashell, Piccolo and their locations |
| `overworld_items` | on / off | off | Item pickups lying in levels and hubs |
| `item_blocks` | on / off | off | ? blocks and hidden blocks that give items (not coins) |
| `shop_items` | on / off | off | Each hub shop slot sells an Archipelago item |
| `pit_rewards` | off / flipside / both | off | Pit of 100 Trials chests; Dashell needs `flipside` or higher |
| `side_quests` | on / off | off | Piccolo trading sequence, NPC rewards in the hubs |
| `catch_cards` | off / … | off | **Later.** 256 cards is a lot; maybe "every Nth card" |
| `recipes` | off / on | off | **Later.** First cook of each recipe |
| `skip_cutscenes` | on / off | on | Skips the story interludes outside the chapters: Count Bleck & minions (story values 10, 54, 96), the memory flashbacks after Chapters 1–7 (57, 98, 126, 174, 221, 287, 354), the Peach intermission (55–56) and the Luigi intermission (97). Off: they play after the chapter that triggers them. Scenes with checks (the Ch4 Helmet interlude, the Underwhere) always play |
| `traps` | 0–100% of filler | 0 | Poison Shroom, Sleepy Sheep, Mistake |
| `death_link` | on / off | off | Standard Archipelago option |

**Starting heart rule** (`merlon_gift: pure_heart`). Merlon's gift is a location like any other, with the chosen heart **locked** to it (`place_locked_item`). So the heart is handed over in-game by Merlon, not just added to your inventory. The heart has to give the player somewhere to go in sphere 1, before any items are found:
- `chapter_1`: Merlon gives Pure Heart 1. Pillar 1 needs no abilities and always opens door 1, so Chapter 1 is open from the start.
- `random`: pick from the hearts whose pillar can be reached with what's available in sphere 1.
  With `pillar_requirements: vanilla` and nothing else changed, that's only pillar 1 (every Flopside pillar needs Fleep).
  The list grows with other options: for example, with `open_hubs` on and characters not randomized, Peach is available in Flipside from the start, so pillar 2 joins the list. With `pillar_requirements: open`, all eight are in it.
  The heart opens whatever door its pillar maps to, so with `pillar_door_shuffle` on, the starting chapter is random too.
  A heart whose pillar opens door 8 is never picked. Starting in the final chapter makes no sense, and with character randomization on it would leave nothing to do in sphere 1.
- A starting heart can be the heart for a door that's already pre-opened. That's allowed: it still counts toward the boss, and the other open doors give sphere 1 somewhere to go.

**`merlon_gift: random_item`.** Merlon's gift goes into the normal fill, so the game may start with no heart at all. Sphere 1 is then the hub checks plus any `starting_open_doors`.
In a solo seed with `starting_open_doors: 0` and `open_hubs: off`, sphere 1 can be only one or two locations, and fill can fail. To prevent that, the world adds one heart whose pillar is reachable in sphere 1 to `local_early_items`, so fill places it early in your own world. With an open door or open hubs this isn't needed, and it's skipped.

**Hub checks.** Every check in Flipside and Flopside that depends on the story in vanilla. Examples: Merlon's gift, Peach's revival, Merlon handing over the Old Key, the Pit rewards, the Flopside trading sequence. The full list comes from the mapping pass.
- `open_hubs: on`: they're all there from the start. Peach has already fallen into Flipside, and you just talk to her for her check. Merlon has the Old Key ready, and so on. Flopside's checks still need Fleep, because you need him to get into Flopside.
- `open_hubs: off`: each check waits for its vanilla trigger, as a logic event. For example, the Peach check needs "Chapter 1 Clear", and the Old Key check needs whatever unlocks it in vanilla.
- Either way it's done with our own flags, not the story progress value, which the hubs keep fixed (see §6).

Filler (when the pool has more locations than progression items): coins, consumables, HP Plus / Power Plus (balance check needed).

**Pool size warning:** with only the core pool (8 hearts + 8 Pixls + about 20 story key items) there are about 36 locations and every one holds progression. That will generate, but it's tight. Enabling `overworld_items` adds most of the space for filler.

## 6. Technical approach (summary)

1. **Story value virtualization.** Our code hooks every read and write of the story progress value (GSW 0). Each chapter keeps its own saved value in unused save-file space, and that's the one the game sees while you're in that chapter's maps. The hubs see one fixed "hub value", chosen so that every hub feature is unlocked and no story cutscenes trigger. Hub events we still want (Peach's revival, side quests) are rewritten to use our own flags.
2. **Location hooks.** Every vanilla event that gives an item, Pixl, character or heart is replaced: it sets `CHECKED[loc_id]` instead and does nothing else. The item arrives from the client.
   **Rule: no missable locations.** Vanilla gates many of these events on a story-value window that's impossible to skip past in normal play. The randomizer can skip it (an item from another world can let you skip ahead), so each location must stay collectable until its own flag is set, whatever the chapter's story value. Patch the scripts to check "location not collected" instead of "story value below X".
   Example: Bestovius only teaches flipping while Chapter 1 is below 15; at 16+ he has other lines. If you can already flip (`flip: start_with`) and hit 1-1's Star Block first, his check would be lost. His higher-value dialogue needs a branch that gives the check if it hasn't been collected.
3. **Mailbox.** A fixed block of memory the client reads and writes through Dolphin memory engine: received-item index, an outgoing queue of checked locations, a seed hash (so you can't connect with the wrong save), and slot options.
4. **Party control.** Character join and leave events go through our code, which only allows a character in the party once you've received them (when character rando is on).
5. **In-world appearance.** SPM items placed in SPM show their real icon. Items for other games use one of two Archipelago icons (`assets/icons/`, built by `tools/make_ap_icons.py` from the AP logo to match the game's 40×40 icon style):
   - `ap_progression.png` (colour): **progression** and **useful** items.
   - `ap_filler.png` (greyscale): **filler** and **traps**. Traps look like junk, which is standard Archipelago practice.
   - In-game, item icons are 40×40 RGB5A3 textures in `files/wicon.tpl`, with icon definitions in `files/wicon.bin`. We add both textures there and add two item-table entries ("AP Item" and "AP Junk") that use them. Placed or received items for other games use those two IDs, with the real item name shown through our own message.

## 7. Open questions

- [ ] Confirm the level and trigger script for each Pixl, character and key item (the mapping pass).
- [ ] Sort the remaining key items into story vs. side-quest.
- [ ] Which in-chapter scenes depend on Tippi being absent (Ch3 Francis kidnapping, the Underwhere) and how they behave if the party is different.
- [ ] How the game picks a level when you go through a chapter door (level-select screen or the furthest level?).
- [ ] Is there a hub story value with everything unlocked and no pending cutscenes, or will hub NPCs need their own flags?
- [ ] Bowser's exact recruitment level (Chapter 3; verify).
- [ ] Flipside Pit: when it opens in vanilla, and what its chest rewards are.
- [ ] Full list of hub checks and the vanilla story trigger for each (used by `open_hubs: off`).
- [ ] Peach's revival scene: can the "fallen Peach" state be shown at any time, or is it built into the post-Ch1 cutscene?
  (Vanilla chain: see Peach unconscious → cook Spicy Soup → Peach tutorial → unlock Flipside 2F door, seq 061–064.)
- [x] ~~Chapter 6 level layout~~: the story uses `wa1` then "???", and `wa2`–`wa4` are post-game only (see §2).
- [x] ~~Sammer gauntlet rewards~~: King Sammer gives all seven cards after gate 100 (user confirmed). Seven locations on one event.
- [ ] Which story value (or flag) turns the post-game Sammer's Kingdom on.
- [ ] **How the game decides you can flip.** If native code compares the story value directly, story-value virtualization changes when flip is available. Flip needs its own flag either way.
- [ ] Fleep's recruitment doesn't appear in the sequence list (4-2's "bathroom" steps, seq 141–145, may be him). Slim's and Thudley's joins also seem to be flag-based, not story values.
- [ ] Find native code that reads `gp->gsw0` directly, bypassing `swByteGet` (needs disassembly).

## 8. Findings from the disc (US 1.0, R8PE01 rev 0)

Disc extracted to `disc/DATA/` (game partition only). Reference repos are in `ref/`, along with the downloaded sequence list (`ref/gsw0_values.txt`) and map-name list (`ref/map_names.txt`).

### File layout
- `files/rel/relF.bin`: LZ10-compressed REL. It holds all map code and evt scripts. `relmgr` loads it, and a decompressed `relF.rel` takes priority if present, so we can patch scripts statically. `rel.bin` next to it is an older build that the game never loads.
- `files/map/*.bin`: map models and geometry, LZ10-compressed. 382 maps.
  Prefixes: `he` Ch1, `mi` Ch2, `ta` Ch3, `sp` Ch4, `gn` Ch5, `wa` Ch6, `an` Ch7 (Underwhere/Overthere), `ls` Ch8, `mac` Flipside/Flopside, `dan` the Pits, `aa` prologue and ending, `mg` minigames.
- `files/setup/*.dat`: enemy spawns, plus **coins only**. Real item pickups come from map scripts calling `itemEntry`, not from these files.
- `files/msg/US/*.txt`: plain `key\0text\0` pairs. `global.txt` has item names and place names; `machi.txt` is the hubs; `stg1`–`stg8` are the chapters. Easy to add our own messages.

### Things that change the design
1. **Pure Hearts aren't items.** There's no Pure Heart in the item table. They exist only as story state and as icons (`ICON_PURE_HEART_RED` … `_WHITE`). So "holding heart N" is our own saved flag, and the pause menu or HUD has to show them with those icons. Upside: they don't take up key item slots.
2. **There are only 32 key item slots** (`POUCH_KEY_ITEM_MAX`). In vanilla you never carry many at once, but with randomization you could receive dozens of key items early. Over 32, items are lost. We need our own key item storage in save data, and we either show the overflow ourselves or only put a chapter's keys into the real pouch while you're in that chapter.
3. **There are only 10 consumable slots.** Filler items received while full need a queue (or go into the shop storage, which has 32 slots) instead of being lost.
4. **Characters and Pixls are items with a "selectable" flag** (`pouchMakeCharSelectable` / `pouchMakePixlSelectable`). Story scenes remove party members by clearing that flag. That's the hook for party control: we decide whether the flag is set.
5. **evt scripts read the story value through `swByteGet(0)`**, which we can hook, so virtualization works for scripts. Native C code that reads `gp->gsw0` directly gets around the hook and has to be found one by one (open question).
6. **Story value range is 0–422** (422 is the post-game). On US 1.0, `gp` (pointer at `0x8056D198`) is `0x804E2550`, so the story value is at **`0x804E2690`** (`gp + 0x140`, s32). Verified in-game. The community doc's `0x4E4092` is wrong for this version.
7. **Save space:** 8192 GSWF flags and 2048 GSW bytes in `SpmarioGlobals`. Plenty of room for our flags (pillars, hearts, checked locations, received-item index) once we find unused ranges.
8. **Hub interludes with checks** (from the sequence list): Merlon's gift and pillar 1 (seq 4–8); Peach's revival chain (61–65); the Helmet / Goldfish Bowl chain (130–135); meeting Nolrem in Flopside (175–177); Merlon after the Underwhere (301–303). All of these are covered by the `open_hubs` design.
9. **Overworld items** are spawned by scripts calling `itemEntry(name, …)`. Every spawned item has a name, so (map, item name) is a natural location ID. Hooking `itemEntry` to swap the item lets `overworld_items` work without patching each script.
10. **The mod loader is a Gecko code** (spm-rel-loader) that loads `/mod/mod.rel` from the disc. **Decision: ship a patched disc** (see "Distribution" below).

### Distribution (decided: patched disc)
- **The disc is the same for every seed.** It contains:
  - `main.dol` with the rel-loader built in (the loader's code is added to the dol as a new code section, and a call to it is patched into boot, so no Gecko code is needed);
  - `/mod/mod.rel` (our mod);
  - our extra messages;
  - a static `relF.rel` if we patch scripts that way.
- **Seed data comes from the client**, not the disc. On connect, the client writes slot data (options, pillar→door mapping, pre-opened doors, location→item display info) to the mailbox. The mod copies it into the save file, so a save stays tied to its seed. One patched disc works for every seed, and updates only change the disc when the mod itself changes.
- **Patcher:** a client component like the Archipelago Launcher's other patchers. Given the user's clean US disc (any revision once we support 1.1/1.2), it checks the disc's hash, extracts it, adds and replaces the files above, and rebuilds an ISO. Rebuilding needs a disc-image tool: Wiimms ISO Tools (`wit`), or our own Python rebuilder if bundling `wit` is a problem.
- **Where the loader lives:** `tools/patch_dol.py` puts the rel-loader's code in a new dol text section at `0x80001900`.
  `0x80001800` itself doesn't work, because Dolphin writes a `STUBHAXX` marker at `0x80001804` on boot (that crashed the first test).
  **Known limitation:** `0x80001800`–`0x80003000` is also where Dolphin installs the Gecko codehandler, so enabling Gecko cheats for the patched game would overwrite the loader. Fine for now. Before release, consider a home that can't collide: a dead function in the dol, or a section above the end of BSS with the arena start moved up.
- **Development shortcut:** Dolphin can boot an extracted disc folder directly (`disc/DATA/sys/main.dol`), so during development we just copy files into the folder, with no rebuild each time.

### Proof-of-concept results (US 1.0, Dolphin)
- **Mod loading:** the rel-loader built into `main.dol` (no Gecko) loads `/mod/mod.rel`, and the title-screen banner shows.
- **Mailbox:** the client finds it by scanning MEM1 for the magic string. Two copies exist (the rodata literal and the live struct), so the client picks the one with a valid version field.
- **Item IDs:** characters are **216–219** and Pixls **220–231** (Tippi 220, Thoreau 221 … Dashell 231). The later entries with the same names (458+, e.g. Thoreau 477) are separate copies that `pouchAddItem` silently ignores.
- **Delivery that works:** spawn the received item as a world item on Mario (`itemEntry(name, id, 0, mario pos, nullptr, 0)`). He collects it at once, and the game plays its own pickup: hold-up pose and the "You got X!" box. Pixls also get their usage tutorial box. The mod only marks an item delivered (`commitReceived`) when `pouchAddItem` sees it, one at a time. If a map change happens first, it respawns in the next map.
  - Polish needed: the `item_get` message adds an article ("You got **a** Thoreau!"). For Pixls, the item-get backdrop (the green star) shows but the Pixl's sprite that normally hovers over it is missing. The world-item path has no sprite for Pixl item IDs. Fix with our own message, plus the Pixl's sprite (from its join scene) or its icon, for Pixls and characters.
- **Drawing text:** drawing from the seq main function only shows on the title screen; in maps the world is drawn over it. Use `dispEntry` on `CAM_ID_DEBUG_3D` (as spm-practice-codes does), though in-game notices should normally go through the game's own message system.
- **Pickup reporting:** hooking `pouchAddItem` catches everything, including story events (a new file adds Mario through it). Real location checks have to be tied to specific events or world items, not to "an item was added".

### Per-chapter story value experiment (mod 0.2-story): it works
Implementation (`mod/source/storyvalue.cpp`): in the `SEQ_MAPCHANGE` init hook (`wp->p0` = destination map, before any of its scripts run), save `gp->gsw0` into the slot of the context being left, then load the slot of the context being entered.
- Contexts come from the map prefix: `mac_` = hub, `he/mi/ta/sp/gn/wa/an/ls` = chapters 1–8, anything else = carry the value over.
- The hub value is fixed, so anything a hub script writes is dropped (for example, the "entered the door" values).
- Writing the real `gsw0`, not hooking `swByteGet`, means native code sees the same value as scripts.

Tested in Dolphin, with the hub fixed at 65:
| Step | Story values (hub / ch1 / ch2) | Result |
|---|---|---|
| Load save in Flipside | 65 / 11 / 66 | Pillar 1 has its heart, doors 1 and 2 open |
| Gloam Valley first | ch2 66 → 67 → 68 → 69 | Tippi's intro, Door Key, locked door: all normal |
| Return Pipe → Lineland | ch1 11 → 16 | Chapter 1 starts fresh; no flip until Bestovius |
| Back to Gloam Valley | ch2 resumes at 69 | Door still unlocked, no intro cutscene |

**Conclusion: chapters can be played out of order, and each keeps its own progress.** The core of §6 works.

Problems found, all now design requirements:
1. **Tippi is the "guide" (`guide.h`), not a Pixl.** The jump left guide `flag0` bit `0x80` (disable) set, so Tippi was gone everywhere. Clearing it brought her back after the next map change. The mod must force Tippi on (clear `0x80`) whenever she should be present. On US 1.0: `guideGetWork` = `0x8011EFD0`, guide work = `0x804CD398`.
2. **Item 220 ("Tippi" Pixl) is an unused leftover.** Giving it adds a broken Tippi to the Pixl menu, and switching Pixls then crashes in `mot_pixl_change`. Never give it.
3. **Flipping is gated by the story value (16 = Bestovius)**, per chapter: blocked in Ch1 at 11, allowed in Ch2 at 66+ without Bestovius. The `flip` option needs a hook on that check, driven by our own flag.
4. **Chapter doors offer level select from the hub's value.** At 65, Chapter 1 counted as finished, so the door offered 1-1 to 1-4 although Chapter 1 was at 11. The mod has to decide what each door offers from that chapter's own progress.
5. **Story items aren't given by the jump.** Peach, Thoreau, the Old Key and so on come from story events. In the randomizer they come from Archipelago anyway, but the start of a file must give everything the skipped prologue would (Tippi's guide flag, the Return Pipe).
6. **Hub cutscenes keyed to earlier values don't run.** Merlon's Peach scene didn't trigger at 65. That's expected, and it's what the `open_hubs` flags are for.
7. **The slots live in mod memory.** They reset when the game restarts. Next: keep them in the save file (unused GSW space), and make sure a save made in the hub stores the hub value.
8. **Character items crash the world-item pickup.** Characters are added directly for now and need their own "joined" presentation.

### Fixes after the experiment (mod 0.3, in progress)
Found with Dolphin's debugger (memory breakpoints) and the new `tools/evtdis.py` (script disassembler that resolves `relF` relocations) and `tools/dolasm.py` (Capstone + symbol names).

- **Save data layout** (`mod/include/savedata.h`): a scan of every script in `main.dol` and `relF`, every native `swByteGet`/`swSet` call with a constant, and the coin flag table found nothing above **GSW 534** or **GSWF 1327**. Reserved for the randomizer: **GSW 1792–2047** and **GSWF 7168–8191**. Chapter slots are GSW 1792–1807 (8 × big-endian u16, 0 = not started). The current chapter's slot syncs from GSW 0 every frame, so a save made mid-chapter is current.
- **Tippi:** the save code (`nandUpdateSave`) writes **GSWF 403 = Tippi present** (guide `flag0` bit `0x80` clear). Loading a save restores it (function at `0x800E4970`, which also restores GSWF 401/402/404 states, HP and the flip timer). The test save was made at story value 7, when the hub shows Tippi as an NPC stand-in and the real guide is off, so it loaded without her. The jump wasn't the cause. Fix: on every map change, set GSWF 403 and clear `0x80`, except in the prologue (value < 9). **Decision (user): Tippi stays with Mario through the stretches where vanilla removes her** (Chapter 3 values 102–124 after Francis swallows her; 286–302, the Underwhere). Needs testing: cutscenes there assume she's gone (Barry giving hints, Francis's room, the rescue scene). Per-map guide reset: `0x8011F020` (keeps `0x80`). Per-frame update: `0x8011FAA8` region.
- **Chapter doors:** `relF` script `0xB906C` (`mac_door_setup_evt`), run from the tower init at `0xCA09C`, decides open doors and section menus from GSW 0. Every finished chapter gets the menu; the newest needs GSW 0 ≥ 54/96/126/174/221/287/354 (`relF` func `0x1308`). The menu is a fixed message (`mac_door_stgN_01`) and `mac_door_select_evt` (`0xB8DCC`) accepts choices 0–3 (Chapter 6's door: only 0). **Mod:** its own copies of both scripts. A door gets the menu once its chapter reaches its second section, and the menu lists only the reached sections (built through a `msgSearch` hook on `ap_door_sel_N` keys). Leaving mid-chapter no longer means replaying from X-1.
  Section-end values used (X-1/X-2/X-3 end): Ch1 18/29/38, Ch2 77/83/89, Ch3 106/112/118, Ch4 139/150/161, Ch5 187/195/203, Ch7 307/328/340.
- **`relF` load address on US 1.0: `0x80C16840`** (`relWork` at `0x804F1F90`). Symbols in `relF` go in `mod/symbols/spm.us0.lst` as `1,<section>,<offset>` (section 1 = file offset − `0xDC`, section 5 = file offset − `0xAF300`), merged with spm-headers' list at build time.
- **Build note:** GCC's induction variable optimisation rewrote a loop to address a `const` table from a base before its section (`.rodata - 0x60c`), and REL can't encode a negative relocation addend (`elf2rel` fails with a `struct.error`). Built with `-fno-ivopts`. To check a failing build: `powerpc-eabi-readelf -rW out/us0.elf`, look for `- <hex>` addends.
- **Flip (solved):** the ability check at `0x80147A08` switches on the character (`mario+0x38`). For Mario it calls `evtGetValue(GSW(0))` at `0x80147AB4` and refuses when the result is 1–15. The mod replaces that `bl` with its own function (0 = allowed, 1 = blocked), driven by the `flip` option (`mod/source/flip.cpp`). Earlier searches missed it because the read goes through `evtGetValue` with a constant, not `gp->gsw0`. The pause menus (`charPixlItemWin`, `pluswin`) also compare GSW(0) with 16 the same way; check what they hide.
- **Tested (mod 0.3):** fresh file through the prologue; flipping before Bestovius; 1-1 finished without Bestovius (his lesson is then lost, see the no-missable-locations rule); section menus on the Ch1 and Ch2 doors listing only reached sections; revisiting cleared sections and "None of them" without changing chapter values; save and reload mid-chapter (slots and Tippi restored).
- **Seen once:** "Invalid read from 0x81815bd2, PC = 0x80272014" (inside `OSLink`, applying a REL14 relocation) right after Dolphin's Emulation → Reset; never on a fresh start, and not reproduced with a breakpoint. Probably the OS module list surviving Dolphin's soft reset. Revisit if it comes back.
- **Watch for:** practice-codes' `mapfixes.cpp` notes crashes when entering `ta4_13` (Francis's room) below story value 124, and `bos_01` (Dimension D) between 117 and 178. Per-chapter values could hit these.

### Useful tools found
- `ref/spm-practice-codes`: warps, story-value editing and other debug features. Very useful for testing chapters out of order.
- Skawo's Setup File Editor & Randomizer (linked from spm-docs): edits enemy and coin setups. It's not a progression randomizer, but it's useful for reading setup files.
- `evt-disassembler` works on a **RAM dump**, so mapping scripts means running the game in Dolphin and dumping memory per map, or writing a REL parser of our own for `relF`.
