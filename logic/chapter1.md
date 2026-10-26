# Chapter 1 logic: draft for review

Draft from the location scan (`data/locations_us0.json`), story values (`data/story_us0.json`) and what we've found in testing. **Correct anything that's wrong**, especially lines marked **?** (my guess, not checked).

Notation: requirement names are the logic abilities from DESIGN.md §4 (`flip`, `throw` = Thoreau, …). "—" = no requirement once you can reach the level. Every level can always use Tippi's `reveal`.

## Chapter structure
- **Door 1** (Flipside Tower) → **1-1 Lineland Road** (`he1_*`) → **1-2 Mount Lineland** (`he2_*`, includes Yold Town) → **1-3 Yold Desert** (`he3_*`) → **1-4 Yold Ruins** (`he4_*`) → **Chapter 1 Clear** (Pure Heart 1).
- Each level's Star Block is a "Level 1-k Clear" event that opens 1-(k+1). With the per-chapter story values, the door's section menu lets you re-enter any section you've reached.

## Which option each location belongs to
| Kind | Count | Option |
|---|---|---|
| Story checks (Thoreau, Ruins Keys, Bestovius's lesson, Pure Heart) | see below | always |
| Item pickups lying in levels | 13 | `overworld_items` |
| ? blocks with items | 21 | `item_blocks` |
| Catch cards in chests or lying around (Squiglet, Goomba, …) | 8 | `catch_cards` (later) |
| Paper → Autograph → You-Know-What → Training Machine | 4 steps | `side_quests` / `piccolo_chain` |
| Yold Town shop (`he2_03`) | 1 shop | `shop_items` |

## Story checks
| Location | Map | Requirement | Notes |
|---|---|---|---|
| Bestovius's lesson (Flip) | `he1_06` | — | Not in the scan (it isn't an item pickup); needs its own location hook. With `flip: start_with` it's a normal location |
| Level 1-1 Clear (Star Block) | 1-1 end | `flip` | You must flip past the first wall **?** (that's why Bestovius is mid-level) |
| Thoreau's location | `he2_07` | — **?** | Yold Town basement. Trap removed, gated on its own flag (tested) |
| Level 1-2 Clear | 1-2 end | `throw` **?** | Vanilla needs Thoreau to progress up the mountain **?** |
| Level 1-3 Clear | 1-3 end | `flip` **?** | Desert pathways seen only in 3D **?** |
| Ruins Key 1 | `he4_04` | — | Story-gated in vanilla (chest empties at story value 40+): needs the "collected" gate |
| Ruins Key 2 | `he4_05` | 1 Ruins Key **?** | Behind lock 1 (`he4_03`)? Vanilla item vanishes at story value 43+ |
| Ruins Key 3 | `he4_07` | 2 Ruins Keys **?** | Behind lock 2 (`he4_05`)? Vanilla item vanishes at story value 46+ |
| Fracktail / Chapter 1 Clear (Pure Heart 1) | 1-4 end | 3 Ruins Keys, `flip` **?** | Locks at `he4_03`, `he4_05`, `he4_07`. Any key opens any lock (counted) |

## Item pickups (`overworld_items`)
| Map | Item | Requirement |
|---|---|---|
| `he1_01` | Fire Burst | — |
| `he1_06` | Shell Shock, Shroom Shake | — (inside Bestovius's house) |
| `he1_03` | Shroom Shake (chest) | — **?** (the chest condition reads a local flag, possibly a 3D-only spot: `flip`?) |
| `he2_03` | Fire Burst, POW Block, Sleepy Sheep, Shroom Shake | — (Yold Town) **?** these may be shop stock, not pickups |
| `he3_02` | Courage Shell | `flip` **?** |
| `he3_06` | Ghost Shroom | `flip` **?** |
| `he4_02` | Life Shroom (chest) | — |

## ? blocks (`item_blocks`)
21 blocks: Mushrooms, Super Mushrooms, Slow Shrooms, Happy/Speed Flowers, a Dot Star. **Assumption: none needs more than reaching the level, except hidden ones that need `reveal` (always available).** Two blocks in `he3_01`/`he3_03` hold "item 548", which isn't a valid item id; probably a coin or special block. To check.

## Catch cards (`catch_cards`, later)
Squiglet (`he1_01`), Goomba (`he1_02`), Koopa Troopa (`he1_05`), Paratroopa (`he2_01`), Boomboxer (`he2_03`), Red & Green (`he2_05`), Squig (`he3_01`), Buzzy Beetle (`he4_07`). Requirements as for their level, **?** for any in 3D-only spots.

## Side quest chain (`side_quests` / `piccolo_chain`)
1. **Paper**: `he4_11` (1-4) **?** and/or `he2_03`.
2. **Autograph**: in exchange for the Paper, Yold Town (`he2_03`).
3. **You-Know-What**: Yold Town (`he2_03`), for the Autograph.
4. **Training Machine**: Bestovius (`he1_06`), for the You-Know-What. Starts the Piccolo chain (Merluvlee → Crystal Ball → Merlee → Random House Key).

## Questions for you
1. In 1-1, is `flip` needed to reach the Star Block, and which maps (`he1_02`–`he1_05`) are past that wall?
2. In 1-2, what exactly needs Thoreau? Is everything in Yold Town reachable before him?
3. Does 1-3 need anything besides `flip`?
4. In 1-4, are the keys found in order behind the locks (key 1 opens the way to key 2, and so on)? Does Fracktail need anything specific?
5. Are the four Yold Town items (Fire Burst, POW Block, Sleepy Sheep, Shroom Shake) lying in town, or are they the shop's stock?
