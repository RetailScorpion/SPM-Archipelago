# Hub story value tests, sorted (US 1.0)

Every test of the story value (GSW(0)) in the Flipside and Flopside scripts, sorted for the plan in DESIGN.md §7: lower the hub value and make every hub check depend on its own location flag. Scanned 2026-11-01 from `build/relF.rel` (every script reachable from the `mac_` map inits, 637 branches) and from `main.dol`'s own scripts (the hub NPC systems). Addresses are relF file offsets unless marked `dol@`.

## Checks: tied to the story value in vanilla

| Check | Where | Vanilla gate | Status in the mod |
|---|---|---|---|
| Return Pipe (Merlon) | `mac_02`, scene `0xc7ed8`, item at `0xb62a4` | story value 8 | Given at the start of a new file; not a location |
| Peach joins | `mac_02`, scene `0xbbd08` (started from the init's switch, `CASE_SMALL(63)`, when entering from `dearu_ie`) | below 63 | Gift location; the test reads `AP_GSW_PEACH_SCENE` |
| Old Key (Merlon) | the same scene, `0xbbd08`, item at `0xbc54c`; the scene ends with story value 63 | below 63 | Not a location yet. It comes with Peach's scene, so it needs its own flag before it can be shuffled |
| Spicy Soup, Peach chain | `mac_01` (60-62), Saffron's cooking script (`dol@0x803e2710` onwards, 61 and 62) | 60-62 | Runs on the mod's own value, `AP_GSW_PEACH_CHAIN` |
| Goldfish Bowl | `mac_01`, item at `0xb26f0` (Gills' owner's talk at `0xb0074` sets 133) | exactly 132 | Not a location. The Helmet is given on entering Chapter 4 instead |
| Helmet (goldfish trade) | `mac_04`, `0xce100` runs `0xcce30` (gives the Helmet, takes the Goldfish Bowl, sets 134) | exactly 133 | Same as above |

None of the other hub spots depends on the story value. That covers the flagged chests, Cooking Disks 2-5, the catch-card chests, Piccolo's join, the Nolrem card, the HP Plus and Power Plus, and the Merluvlee and Merlee trades (those check for the item you hand over).

## Access: what a low value closes, or what has to stay closed

| What | Where | Vanilla | Plan |
|---|---|---|---|
| Flipside 2F elevator (`elv1`) | `mac_02`, `0xca8d0` | off below 53 | Patched to always work |
| Flipside 1F elevator (`elv2`, to B1 and B2) | `mac_09`, `0xd3534` | off below 73 | Patched to always work |
| Flopside 1F elevator (`elv2`) | `mac_19`, `0xe2f5c` | off below 192 | Patched to always work |
| Flopside house door (`ie_02_doa`) | `mac_12`, `0xdbd2c` | locked below 174 | Patched open (whose house is still to check) |
| Old Key gate (`mon_lock`) | `mac_02`, `0xc83c4` | locked below 64; from 64 the gate is gone | Patched open for now (the Old Key comes with Peach's join scene, which may not play). Once the Old Key is a location: keep the lock, so it opens only with the key |
| Pillar 8 (hidden under rock) | `mac_12`, `0xdc0d0` (`heart_01`, `before_iwa`) | hidden below 355 | Make it visible from the start (§2) |
| Recipe terminal ("Awaiting data upgrade...") | `mac_09`, `mac_19` (`foodnavi_001`, shared script `0xd2f10`) | off below 64 | Patched to always work |
| Door 1's destination | `mac_02`, `0xc9fec` | below 11 it leads to `ls4_11` (prologue), from 11 to `he1_01` | The hub value must be 11 or more |
| Door 6's destination | `mac_02`, `0xca034` | `wa1_27` (World of Nothing) for 280-420, else `wa1_01` | Read Chapter 6's slot instead (next build) |
| Castle Bleck door | `mac_12`, `0xdbac8` onwards | closed below 356 | Already handled: the mod sets 357 while Mario is in Flopside Tower once Chapters 1-7 are finished |
| Chapter door scenes and looks | `mac_02` (`evt_door_set_event` at 98, 126, 174; `tt_doa_08`-`12` below 66-225) | story value | Already replaced by the mod's own door scripts (`tower_doors.cpp`) |

## Scenes: hub scenes without a check

They all test an exact value or a window, so none plays with the hub fixed at 11. The mod also discards story values written in the hub. One exception had to be patched: Flipside Tower's scene SWITCH (`0xcafe8`) checks its cases in order, and its cases below 54 and below 59 (Peach chain steps before 60) would catch 11 before Peach's join scene (below 63, compared with `AP_GSW_PEACH_SCENE`). Both now never match. The soup talk's "before Saffron" test (`mac_01`, `0xaf734`, at most 61) is patched the same way.

| Story value | Where | Scene (sets) |
|---|---|---|
| below 5, below 7 | `mac_01` | opening (5, 6, 7) |
| below 4 | `mac_02` | opening (4) |
| 57-59 | `mac_02` | Peach chain start (58, 59, 60) |
| 64 | `mac_06` | pillar 2 (65) |
| 98 | `mac_02` | after Chapter 2 (99) |
| 99 | `mac_07` | pillar 3 (100) |
| 126 | `mac_02` | after Chapter 3 (127), then pillar 4 (128) |
| 130 | `mac_02` | Helmet interlude start (131) |
| 174, 177 | `mac_02` | after Chapter 4 (175), Merlon (178) |
| 175 | `mac_12` | Nolrem (176) |
| 176 | `mac_11` | pillar 5 (177) |
| 221, 222 | `mac_02` | after Chapter 5 (222, 223) |
| 223 | `mac_16` | pillar 6 (224) |
| 280 | `mac_02` | the world ends (281); the mod sets 281 itself |
| 287 | `mac_02` | Dimentio's attack (288) |
| 301 | `mac_02` | Merlon after the Underwhere (302) |
| 302 | `mac_17` | pillar 7 (303) |
| 354 | `mac_02` | after Chapter 7 (355) |
| 356 | `mac_12` | pillar 8 and the Castle Bleck door (357) |
| 421 | `mac_02` | post-game (422) |

**Pillar hearts:** each pillar shows its heart from the value its scene sets. Pillar 1 shows it from 11 (shared script `0xe694c`), pillar 2 from 65, pillar 3 from 100, pillar 4 from 128, pillar 5 from 177, pillar 6 from 224, pillar 7 from 303 and pillar 8 from 356. At 11 only pillar 1 shows a heart, so the pillar system has to drive these displays from its own flags.

## The rest: stays on the low value

- **Dialogue (about 400 branches):** NPC lines that change with the story value. At 11, NPCs say their early-game lines.
- **Decorations and NPCs:** the bones in `mac_04` (221-421), the bookends in `mac_02` (174-178), the goldfish in `mac_04` (133+), Peach and Merlon in `mac_01` during the chain, and the post-game NPCs (421+).
- **Merluvlee** (`dol@0x803dd3e0`) stops telling fortunes from 422. **Merlee** (`dol@0x803e1830` onwards) stands somewhere else from 126 and has a test at 179. Neither gates an item.

## Native code

Main-program code that reads the story value through `evtGetValue(GSW(0))` (found by the code that builds the GSW(0) reference; code reading `gp->gsw0` directly isn't covered). The thresholds are what each one compares with.

| Where (nearest symbol) | Compares with | What it does at 11 |
|---|---|---|
| Shop code (`0x8010e784`, `0x8010f218`; nearest symbol `evt_shop_wait_coin_sfx`) | 11, 18, 29, 38, ... (Chapter 1's section ends onwards) | Picks a number by story value band. May be shop stock: check in game |
| Shop code (`0x8010fc40`) | 11, 66, 101, 129, ... (the chapter starts) | Same |
| Pause menu, character and Pixl descriptions (`0x801949d8`-`0x8019787c`, 14 places) | 16 (Bestovius's lesson) | Shows the "before flipping" description text. Text only |
| Pause menu, chapter window (`0x801897e0`, `0x8018a4e4`) | 9, 177 | Text only |
| Tippi's descriptions (`searchGetNpcMsg`, `0x802438e4` onwards) | various | Text that changes with the story |
| Mario's flip check (`0x80147aa8`) | 1-15 | Already replaced by the mod (`flip_ability.cpp`) |
| A door check for `sp1` and `sp3` maps (`0x800e4b10`) | 135 | Chapter 4 only |
| Pause check in `he3_08` (`0x801849f8`) | 35 | Chapter 1 only |
| Others (`0x8003181c`, `0x8011f070`, `0x80243960`) | 421 | Post-game only |

## Not covered by this scan

- Native code that reads `gp->gsw0` directly, as in §8 "Find native code that reads `gp->gsw0`".
- `main.dol` scripts the scanner doesn't decode (one test at `dol@0x803e1f18`, story value 179 or more, in Merlee's script).
