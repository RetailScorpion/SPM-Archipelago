# Super Paper Mario Archipelago (no-logic prototype)

An [Archipelago](https://archipelago.gg) randomizer for **Super Paper Mario** (Wii, USA revision 0).

This is a **prototype with no logic**: every location counts as reachable from the start, so items can be anywhere. It exists so the community can play it and help map out the real logic. Expect rough edges.

## What's randomized
Depending on your YAML options:
- the eight story Pixls (Thoreau to Dottie), and optionally Barry, Dashell and Piccolo;
- key items whose spots the prototype handles: the Fort Keys, the Water/Stone/Fire Tablets, Cooking Disks and the Golden Card;
- items in chests and lying around in levels and the hubs;
- optionally, catch cards found in chests or lying around;
- the **Pure Hearts** of every chapter you don't start with (see Chapter doors below);
- the **Save Block** (`save_block`), a key item that saves your game from the key item menu, anywhere. By default you start with it; it can also be shuffled or left out. It doesn't work in the Pits of 100 Trials;
- optionally, **Peach, Luigi and Bowser** (`randomize_characters`): their join scenes become locations and can hold anything;
- **Cursya Traps** (`trap_percentage`, 10% of filler by default): they look like a key item you haven't found, give nothing ("You are a FOOL!") and put a random Cursya curse on Mario.

Story gifts are locations too: Peach's, Luigi's and Bowser's join scenes, and Bestovius's flip lesson in 1-1. **Peach** is there from the start: unconscious in Flipside (`mac_01`). Bring her a Spicy Soup (Saffron cooks one from a Fire Burst) and talk to her to collect her check. Bowser's is his fight in 3-1, and Luigi's is in the Underwhere (7-1). The **Helmet** isn't shuffled yet: you always get it when you first enter Chapter 4.

**Not yet:** the story keys (Ruins Keys, Door Keys, Dimension Keys, Orbs, …), ? blocks, shops and the Pit rewards. These stay vanilla.

**Chapter doors:** some chapters' doors in Flipside Tower are open from the start, picked by `starting_chapter_mode`: the ones you list (`starting_chapters`, default Chapter 1), a set number at random, or a random number at random. Every other chapter's door opens when you receive its Pure Heart. Chapter 8's door opens once Chapters 1-7 are finished, as in vanilla.

**Cutscenes:** by default (`skip_cutscenes`), a new file skips the opening and Merlon's scenes and starts in Flipside Tower with the Return Pipe. The Count Bleck scenes between chapters are skipped, along with the Peach and Luigi intermissions. Turn the option off to watch the story.

**Goal:** finish Chapter 8.

Items from other worlds show up in Super Paper Mario as the Archipelago icon (grey for filler). Items you receive from other worlds pop up on Mario with the normal item-get.

**Key item storage:** Mario can carry 32 key items. One received while all 32 slots are full goes into storage instead of being lost, with a red notice on screen. Use SPM Archipelago Tools (below) to swap it with one you carry.

## What you need
- A **Super Paper Mario (USA) revision 0** disc image (game ID `R8PE01`). The patcher checks this.
- [Dolphin](https://dolphin-emu.org), recent.
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) **0.6.7 or higher** (tested on 0.6.7).
- From this release: `spm.apworld` and `Super Paper Mario.yaml` (in `SuperPaperMario_AP_<version>.zip`).
- Optional: `SPM_helpful_tools_<version>.zip`, see Helpful tools below.

## Setup
1. **Install the APworld:** double-click `spm.apworld`, or copy it into Archipelago's `custom_worlds` folder. Restart the Archipelago Launcher.
2. **Extract the disc:** in Dolphin, right-click Super Paper Mario > **Properties** > **Filesystem** > right-click the disc > **Extract Entire Disc**, into an empty folder.
3. **Patch it:** in the Launcher, open **Super Paper Mario Patcher** and pick that folder. It patches the folder in place and keeps the originals as `.orig` files. Run it again after updating the APworld.
4. **Boot it:** in Dolphin, **File > Open**, and pick `sys/main.dol` inside the patched folder (inside `DATA/` if Dolphin made one). You can also click and drag your `main.dol` onto Dolphin's game select window.
5. **Make your YAML** from `Super Paper Mario.yaml` and generate a game as usual.
6. **Connect:** in the Launcher, open **Super Paper Mario Client**, connect to your room and enter your slot name.
7. **Load your save, then play.** Connect the client **before entering any level**, so it can place the items.

## Known issues
- No logic: you may need a late Pixl early. The **Return Pipe** (you start with it, or Merlon gives it with cutscenes on) gets you out of rooms you can't leave.
- Some story events still check the story's progress rather than whether you have the right item.
- Gecko cheat codes can't be used with the patched game.
- **Story events in Flipside and Flopside are simplified:** the hubs don't follow the story. Chapter 6 moves on to its destroyed world once the duels are done, and Castle Bleck's door (Chapter 8) opens in Flopside Tower once Chapters 1-7 are finished.
- Chapter 3 still takes Tippi away when you enter it; changing maps brings her back.

## Helpful tools
`SPM_helpful_tools_<version>.zip` has three optional windows that read the game live from Dolphin:
- **SPM Tracker** (`client/tracker.py`): every location by level and map, checked or not. Add `--missing` to hide the checked ones.
- **SPM Coords** (`client/coords.py`): Mario's position, the map, and that map's unchecked locations.
- **SPM Archipelago Tools** (`client/ap_tools.py`): add or remove characters, Pixls and the Save Block, set coins, and swap key items between your slots and storage. This one writes to the game, so use it with care.

They need [Python 3](https://www.python.org) and `py -3 -m pip install dolphin-memory-engine`. Start Dolphin with the game, then run one, for example `pyw -3 client	racker.py --missing`. Keep the zip's folders as they are. Details are in its `README.txt`.

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

## License
This project is open source under the **GNU General Public License v3** (see [`LICENSE`](LICENSE)), the same as spm-rel-loader and the `mod/` part of spm-headers, which it includes. You're free to use, change and share it, as long as what you share stays open source under the same license, with its source available.

Super Paper Mario is © Nintendo / Intelligent Systems. This project contains no game files: you need your own copy of the game.

## For developers
- `DESIGN.md`: the design, every finding about the game, and open questions.
- `mod/`: the C++ mod (`py -3 configure.py us0`, then `ninja`, with `DEVKITPPC` set).
- `apworld/spm/`: the APworld, client and patcher. `tools/build_apworld.py` regenerates its data and builds `build/spm.apworld`.
- `tools/`: research and build tools: location scan, script disassemblers, data generator, dev disc builder.
- `client/`: the helpful tools above, plus dev tools (mailbox watcher, debug client with warps).
