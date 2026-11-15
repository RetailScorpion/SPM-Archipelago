# maprender: Super Paper Mario room renders and connection map

Renders each room of SPM (US rev 0) from the game's own files: a side view of the room's 3D model with its
textures and background layers, roughly what the 2D camera sees. It also builds a connection graph and one
overview image per area, for an Archipelago map tracker and for working out the randomizer's logic.

Everything it writes goes to `build/maps/` (gitignored). **Never commit the output**: it is Nintendo's art.

## Requirements

- The extracted disc in `disc/` and the decompressed `build/relF.rel` (as for the rest of the repo).
- Blender 5.1 (default path `C:\Program Files\Blender Foundation\Blender 5.1\blender.exe`, or pass `--blender`).
  The model and texture decoding runs inside Blender's Python, which has numpy.
- Python 3 with Pillow (`py -3`), for compositing, the graph and the overviews.

## Running

```bash
py -3 tools/maprender/render.py
```

```bash
py -3 tools/maprender/graph.py
```

```bash
py -3 tools/maprender/overview.py
```

`render.py` renders every playable room (456; skips the ones `ref/map_names.txt` marks UNUSED) by its
game name: rooms that load another room's map file (Chapter 6's gates: wa1_03 to wa1_25 all load wa1_02,
`data/maps_us0.json`) are drawn from that file with their own script, so each gate has its own doors. With 3
Blender processes; a full run takes about 10 minutes. Any room whose output can't be read back is
re-rendered once. `graph.py` also writes `build/maps/NOTES.md`. `overview.py --scale` sets the room size
in the overviews (default 0.3 of the room render). Pass room ids to render only those (`render.py he1_01 he1_02`).
Options:

| Option | Default | What |
| --- | --- | --- |
| `--pitch` | 18 | Camera tilt in degrees (rooms with a background picture) |
| `--indoor-pitch` | 8 | Camera tilt in indoor rooms (no background picture). Less tilt means the floor of an upper storey hangs less over the doors below it |
| `--cut-ceilings` | off | Leave out ceilings in indoor rooms (see below). Off because the lower indoor tilt already keeps doors visible, and cutting leaves gaps in some rooms |
| `--as-drawn PREFIX...` | `ls sp4` | Rooms drawn as-is: no ceiling cut (even with `--cut-ceilings`) and no mask handling. In Castle Bleck's line art and the Whoa Zone the `msk` nodes are the 2D view itself; removing them exposes the 3D-only layout behind |
| `--crop-below-floor PREFIX...` | `he3 he4_10 mi1_01 mi1_08 mi1_09 mi1_10 mi1_11 mi2_01 ta1_02 mac_` | Rooms whose image stops near the floor, not at the bottom of the 3D model: `--floor-margin` model units (default 12) below the floor, or exactly at the bottom of any pit lower than the main floor (where the sandfalls end). he3 (1-3), he4_10 and those Chapter 2 rooms have ground reaching far below anything in them |
| `--crop-above PREFIX...` | `ta1_03 ta3_01 ta3_04 sp1 sp3 an1_01 an1_02 an1_03 an1_04 an2_02 an2_08` | Rooms cropped at both ends: the top ends `--top-margin` model units (default 6) above the highest point of the collision model, with no extra sky; the bottom as for `--crop-below-floor`. Collision pieces narrower than 5% of the room (e.g. looping lift platforms) are ignored. For rooms whose scenery reaches far above where Mario can go |
| `--crop-top PREFIX...` | `an2_05` | The same top crop as `--crop-above`, without the bottom crop |
| `--keep-outlines PREFIX...` | `sp1` | Rooms whose untextured black parts are drawn. In Outer Space they are the asteroids' outlines; elsewhere such parts are usually masks and are skipped |
| `--ppu` | 8 | Pixels per model unit |
| `--max-px` | 8000 | Cap on either image side |
| `--jobs` | 3 | Parallel Blender processes |
| `--box` | off | Thin outline round each image |
| `--composite-only` | off | Redo the background step without Blender |

## Output

| Path | What |
| --- | --- |
| `build/maps/rooms/<room>.png` | The room: model over its background layers |
| `build/maps/raw/<room>.json` | Framing (model space to pixels), doors with pixel boxes, skipped parts |
| `build/maps/raw/<room>.png`, `<room>_bgN.png` | Model alone (transparent), decoded background layers |
| `build/maps/scripts.json` | What the init scripts say: doors, pipes, hidden objects, played animations |
| `build/maps/graph.json` | Rooms, their doors and pipes, and the blocks, chests and save blocks in them, with image positions; edges between rooms |
| `build/maps/overview/<area>.png` | One image per area, rooms laid out by their connections |
| `build/maps/overview/layout.json` | Where each room sits in its overview image |
| `build/maps/viewer.html` | Click-through viewer: overview, then room with door boxes (click a door to follow it) and chest markers |
| `build/maps/NOTES.md` | Per-room list of what didn't come out right (see below) |

Open `viewer.html` straight from disk; it has the graph built in and loads the images next to it.

**Desktop app (no server):** double-click `tools/maprender/tracker.pyw` (needs `py -3 -m pip install
pywebview` once). It opens the viewer in its own window and reads the game itself, giving the same live
panels as below; the page's settings are kept in `build/maps/.tracker_app`. A single `.exe` (PyInstaller)
is planned for when the tracker is closer to done.

**From the Archipelago Launcher:** "Super Paper Mario Tracker" (the APworld's `tracker_app.py`) asks for
the map folder once (`build/maps`) and opens the same window. The Launcher's own Python can't add pywebview,
so it runs the window with the first Python on the computer (from `py -0p`) that has `pywebview` and
`dolphin-memory-engine`. With none, it falls back to the browser (http://localhost:8765, like serve.py).

For the live side panels (Coords: Mario's position, the map, its unchecked locations; Tracker: every level's checks, Pure Hearts held; and Mario's marker on the map), serve it with `py -3 -E tools/maprender/serve.py` and open http://localhost:8765/viewer.html. The server reads the game through Dolphin (`/game.json`, read only), like `client/coords.py` and `client/tracker.py`. Items, Coords and Tracker each have a button and share the column on the right.

Door and pipe positions come from their hit objects' bounding boxes. Door hit boxes often reach far
above the door (so Mario can't jump past), so for doors only the bottom 7 model units are used.

Coordinates: script/setup positions are 10x the model's units. To put a game point into a room image:
`m = point / 10`, `px = (m.x - x0) * ppu`, `py = (y0 - (m.y*cos(pitch) - m.z*sin(pitch))) * ppu`
(x0, y0, ppu, pitch_deg are in the room's entry in `graph.json`).

## How it works

- `spmfiles.py`: unpacks `map/<room>.bin` (LZ10 + U8).
- `gxtex.py`: TPL decoder (I4, I8, IA4, IA8, RGB565, RGB5A3, RGBA8, C4, C8, C14X2, CMPR).
- `mapdat.py`: `map.dat` world parser, after noclip.website's `src/PaperMarioTTYD/world.ts`.
  Draws the `S` (visual) tree; the `A` (collision) tree is used to place door hit objects.
- `scripts.py`: reads each room's init script from `relF.rel` with `tools/evtdis.py`: the door/pipe
  descriptor tables, `evt_mapobj_flag_onoff` hides, `evt_map_playanim` calls, and the blocks, chests
  and save blocks it spawns.
- `ceilings.py` (only with `--cut-ceilings`): finds ceilings in indoor rooms. At each spot the ceiling is the highest downward-facing
  surface with a collision floor at least 3 units below it; triangles at or above it are left out, so the
  top of the room (door tops, upper walls) isn't hidden behind the ceiling's top and front faces.
- `animgroup.py`: parser for the AnimGroup models of map objects (after noclip.website's
  `src/PaperMarioTTYD/AnimGroup.ts`); rest pose plus an animation's first keyframe for visibility.
- `render_blender.py`: builds the scene in Blender (unlit: texture x vertex colour, alpha test/blend as the
  game sets them, back-face culling as the joint's cull mode), orthographic camera looking down -Z,
  tilted down by `--pitch` (`--indoor-pitch` in rooms without a background picture).
- `render.py`: runs the Blender jobs, then layers the backgrounds (each `bg/<room>_NN.tpl` scaled to the
  image height and tiled sideways, lowest number at the back) under the model.
- `graph.py`, `overview.py`: the graph, the overviews and the viewer.

## Known gaps

See `build/maps/NOTES.md` after a run for the per-room list. In general:

- **Spawned objects**: blocks (`evt_mobj_blk`), treasure chests (`evt_mobj_thako`), save blocks
  (`evt_mobj_save_blk`) and the end-of-level Star Blocks (`evt_mobj_eria_block`, model `MOBJ_eria_block`)
  that a room's init script places are drawn with the game's own models
  (`a/MOBJ_*`, parsed by `animgroup.py`; Chapter 3 rooms use the `_STG3` versions, as the game does).
  The script reader runs the init script's loops and variable maths to get their positions, and takes
  every IF branch, so blocks that only appear after some story event are drawn too (`conditional` in
  `graph.json`). Blocks that only exist in 3D are drawn see-through. Not drawn: items, NPCs, enemies,
  Cudge blocks, switches, springs, and anything spawned by scripts other than the init script (e.g. after
  a cutscene) or by the setup file. Block kinds without a known model (ladders, colour and gravity blocks)
  are listed in `mobjs_not_drawn` in each raw JSON.
- **Camera**: `camera_road.bin` (the 2D camera path) is not decoded. Every room uses the same fixed
  side view, so rooms the game shows from an angle or with a moving camera (curved paths, 3D-only areas)
  are drawn straight on. The fixed view keeps door pixel positions a simple linear projection.
- **Hidden geometry**: the game hides parts of the model at runtime. Hides the init script makes are
  applied; parts hidden in other ways are guessed (untextured black "mask" parts, untextured
  see-through sheets, flat white default-material parts and blur-textured glow planes are skipped; see `skipped_parts` in each raw
  JSON). Some rooms still show parts the game hides, or miss parts it shows.
- **Animations**: only mesh animations the init script plays are posed (mode 2 = last key, otherwise
  first key). Texture scrolling and multi-texture blend modes are ignored (first texture only).
- **Indoor rooms** are rooms without a background picture (the game gives them a tiny 8x8 black one), or
  rooms with a full-width mask node (`kabe_msk`, `msk`: a black front wall with the room cut out, which the
  game uses to hide the outdoor background, e.g. Merlee's mansion; not in `--as-drawn` rooms). Masked rooms are drawn without the mask
  and without background layers. Indoor rooms get `--indoor-pitch` (8 degrees): at 18 degrees, ledges,
  upper floors and ceilings in front hid the doors behind them (e.g. he4_05's right-hand doors).
- **Ceilings** are kept by default. With `--cut-ceilings` they are removed in indoor rooms (not in Castle
  Bleck, `ls`), which can leave black gaps (e.g. mi4_08, mi4_10) and thin leftover strips.
- **Forced parts**: `render_blender.FORCE_HIDE` leaves out the wall blocks above Flipside/Flopside's
  streets (they hid the houses behind). `render_blender.FORCE_SHOW` lists parts drawn even though the
  script hides them or they look like masks: he3_04's doors (gone during the O'Chunks fight), the Return
  Pipe and tower elevator on both towers (hidden until unlocked), and (the black backings of the tower doors in mac_02 and mac_12, without which they're see-through).
  Flipside Tower's chapter doors are drawn with their line-pattern mask inverted and their colour
  doubled, to match the game (coloured door, dark lines); Flopside's Castle Bleck door is black with
  purple lines in the game, so it is drawn as stored.
- **Far background**: parts lying entirely 150+ model units behind the walkable area are left out (they
  were placed for the game's perspective camera; e.g. mac_05 has scenery 7,000 units back).
- **Flipside/Flopside backgrounds** (`render.BG_TILE`) repeat at about one screen's size (`BG_SCREEN`,
  60 model units) instead of stretching to the image height, so the pattern keeps its in-game scale.
- **Elevators** get an up/down `direction` in `graph.json` (from `graph.FLOORS`) and are drawn as
  arrows in the overviews and as labelled boxes in the viewer. The game's own elevator (a black-outlined
  box with a small red arrow light) only partly renders: the outline doesn't show.
- **Backgrounds**: the bg layers' real placement and parallax aren't known; they're stretched to the
  image height, which looks wrong in very tall rooms.
- **Black holes** (Chapter 4) are read from the hit events that warp Mario (`kind: black_hole` in
  `graph.json`, purple lines in the overviews).
- **Connections**: apart from black holes, only the door/pipe tables are read. Warps done by scripts (Return Pipes, elevators,
  cutscene map changes, Flipside/Flopside tiles) are not edges yet. A door listed on only one side gets
  an inferred reverse edge.
- **Doors without a position**: `dokan_m` (the Return Pipe to Flipside, in about 25 rooms) has no hit
  object in the model; the script places it at runtime. These have `px: null` in `graph.json`.
- **Rooms with no door table** (33): Mario's house, Bowser's castle, the intro/ending/game-over rooms,
  the minigames, the Flipside/Flopside spring rooms, several Outer Space/Outer Limits rooms and a few
  cutscene rooms. Their exits are all done by script, so they have no edges of their own (some get
  inferred edges from the rooms that lead into them).
- **Overview layout** is automatic: a door in a room's right or left third puts the next room on that
  side, anything else goes below. Most SPM exits are doors in the middle of a room, so levels mostly run
  downwards. It's readable, not hand-tuned.
  Bands listed in `overview.ROUTES` are laid out along their main route instead: route rooms in a row in
  playing order, side rooms beside the door they hang off (currently 7-3, the Overthere Stair).
- Every room is in the graph, but only the **linked** ones (graph.py: reachable from Flipside Tower through
  doors, pipes, pits, elevators and Star Blocks) are on the area maps and can be opened by hand. The rest
  (Mario's house, Bowser's castle, the minigames, boss rooms only a fight's script goes to such as sp4_17,
  rooms entered by a script, ...) are shown only while **Follow Mario** is on, and their checks are listed
  under "not on the map"

Spot-checked renders: he1_01, he1_02, he1_03, he2_04, mac_02, ta1_01, gn3_05, an1_01, ls1_01, dan_01.
In mac_02 (Flipside 2nd floor) the street's roof block is drawn over the top of the street. In very tall
rooms (e.g. ls1_01, mac_02) the background is stretched. The full set hasn't been checked room by room.
