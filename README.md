# Super Paper Mario Archipelago (no-logic prototype)

An [Archipelago](https://archipelago.gg) randomizer for **Super Paper Mario** (Wii, USA revision 0).

This is a **prototype with no logic**: every location counts as reachable from the start, so items can be anywhere. It exists so the community can play it and help map out the real logic. Expect rough edges.

## What's randomized
Depending on your YAML options:
- the eight story Pixls (Thoreau to Dottie), and optionally Barry, Dashell and Piccolo;
- key items whose spots the prototype handles: the Fort Keys, the Water/Stone/Fire Tablets, Cooking Disks and the Golden Card;
- items in chests and lying around in levels and the hubs;
- optionally, catch cards found in chests or lying around.

**Not yet:** Pure Hearts, characters (Peach, Bowser, Luigi), the story keys (Ruins Keys, Door Keys, Dimension Keys, Orbs, …), ? blocks, shops and the Pit rewards. These stay vanilla.

**Goal:** finish Chapter 8.

Items from other worlds show up in Super Paper Mario as the Archipelago icon. Items you receive from other worlds pop up on Mario with the normal item-get.

## What you need
- A **Super Paper Mario (USA) revision 0** disc image (game ID `R8PE01`). The patcher checks this.
- [Dolphin](https://dolphin-emu.org), recent.
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) **0.6.7**.
- From this release: `spm.apworld` and `Super Paper Mario.yaml`.

## Setup
1. **Install the APworld:** double-click `spm.apworld`, or copy it into Archipelago's `custom_worlds` folder. Restart the Archipelago Launcher.
2. **Extract the disc:** in Dolphin, right-click Super Paper Mario > **Properties** > **Filesystem** > right-click the disc > **Extract Entire Disc**, into an empty folder.
3. **Patch it:** in the Launcher, open **Super Paper Mario Patcher** and pick that folder. It patches the folder in place and keeps the originals as `.orig` files. Run it again after updating the APworld.
4. **Boot it:** in Dolphin, **File > Open**, and pick `sys/main.dol` inside the patched folder (inside `DATA/` if Dolphin made one).
5. **Make your YAML** from `Super Paper Mario.yaml` and generate a game as usual.
6. **Connect:** in the Launcher, open **Super Paper Mario Client**, connect to your room and enter your slot name.
7. **Load your save, then play.** Connect the client **before entering any level**, so it can place the items.

## Known issues
- No logic: you may need a late Pixl early. The **Return Pipe** (from Merlon at the start) gets you out of rooms you can't leave.
- "Sent \<item\> to \<player\>!" may sometimes show "Placeholder Item". The client's log always shows the real item.
- Some story events still check the story's progress rather than whether you have the right item. For example, Old Man Watchitt in Yold Town calls the bridge builder based on the story, not on whether you have Thoreau.
- Gecko cheat codes can't be used with the patched game.

Please report what you find, especially **what each location needs to reach**. That's the logic this prototype is meant to map out. The full list of checks is in [`logic/checks.md`](logic/checks.md).

## Credits
- **RetailScorpion**: design, testing and direction.
- **Claude** (Anthropic): code.
- **[SeekyCt](https://github.com/SeekyCt)**, whose tools and research this is built on:
  - [spm-rel-loader](https://github.com/SeekyCt/spm-rel-loader) (GPLv3): loads the mod into the game. Its loader code is built into the patched `main.dol`.
  - [spm-headers](https://github.com/SeekyCt/spm-headers) (MIT, and GPLv3 for its `mod/` part): the game's functions, structures and symbols the mod is written against.
  - [pyelf2rel](https://github.com/SeekyCt/pyelf2rel): builds the mod's REL file.
  - [spm-decomp](https://github.com/SeekyCt/spm-decomp), [spm-docs](https://github.com/SeekyCt/spm-docs), [evt-disassembler](https://github.com/SeekyCt/evt-disassembler) and [spm-practice-codes](https://github.com/SeekyCt/spm-practice-codes): symbols, documentation (including the item list) and reference for the research tools.
- **PistonMiner** and **Zephiles**: the original Paper Mario: The Thousand-Year Door rel loader and tools that spm-rel-loader is based on. The mod's function-hooking approach follows theirs.
- **[Archipelago](https://github.com/ArchipelagoMW/Archipelago)** (MIT): the multiworld framework and client library. The in-game Archipelago icons are based on its logo.
- **[py-dolphin-memory-engine](https://github.com/henriquegemignani/py-dolphin-memory-engine)** (MIT, Henrique Gemignani), from Dolphin Memory Engine by aldelaro5: the client's connection to Dolphin.
- **[Ninja](https://github.com/ninja-build/ninja)** (Apache 2.0): `ninja_syntax.py`, bundled in the mod's build script.
- **[devkitPro](https://devkitpro.org)**: the PowerPC toolchain the mod is built with.

Super Paper Mario is © Nintendo / Intelligent Systems. This project contains no game files: you need your own copy of the game.

## For developers
- `DESIGN.md`: the design, every finding about the game, and open questions.
- `mod/`: the C++ mod (`py -3 configure.py us0`, then `ninja`, with `DEVKITPPC` set).
- `apworld/spm/`: the APworld, client and patcher. `tools/build_apworld.py` regenerates its data and builds `build/spm.apworld`.
- `tools/`: research and build tools: location scan, script disassemblers, data generator, dev disc builder.
- `client/`: dev tools (mailbox watcher, debug client with warps).
