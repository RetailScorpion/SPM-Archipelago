# Super Paper Mario Archipelago

An [Archipelago](https://archipelago.gg) randomizer for **Super Paper Mario** (Wii, USA revision 0).

**Status:** work in progress. The logic is being mapped out by playing, so you choose how items are placed with the `logic` option:
- **`wip_logic`** (default): the logic mapped out so far. It covers a lot of the game but isn't complete, so a seed will probably need something it doesn't say.
- **`no_logic`**: only the rules the game itself enforces (chapter doors, Flopside, Chapter 8's party). Inside a chapter, items can be anywhere.

Either way, the Return Pipe gets you out of rooms you can't leave. Expect rough edges, and please report what you find.

## What you need
- A **Super Paper Mario (USA) revision 0** disc image (game ID `R8PE01`). The patcher checks this.
- [Dolphin](https://dolphin-emu.org), a recent version.
- [Archipelago](https://github.com/ArchipelagoMW/Archipelago/releases) **0.6.7 or newer** (tested on 0.6.7).
- From this release: `spm.apworld` and `Super Paper Mario.yaml` (in `SuperPaperMario_AP_<version>.zip`).
- Optional: `SPM_helpful_tools_<version>.zip` (see Helpful tools below).

## Setup
1. **Install the APworld:** double-click `spm.apworld`, or copy it into Archipelago's `custom_worlds` folder. Restart the Archipelago Launcher.
2. **Extract the disc:** in Dolphin, right-click Super Paper Mario > **Properties** > **Filesystem** > right-click the disc > **Extract Entire Disc**, into an empty folder.
3. **Patch it:** in the Launcher, open **Super Paper Mario Patcher** and pick that folder. It patches the folder in place and keeps the original files as `.orig`. Run it again whenever you update the APworld.
4. **Boot it:** in Dolphin, **File > Open** and pick `sys/main.dol` in the patched folder (inside `DATA/` if Dolphin made one). You can also drag `main.dol` onto Dolphin's game list.
5. **Make your YAML** from `Super Paper Mario.yaml` and generate a game as usual.
6. **Connect:** in the Launcher, open **Super Paper Mario Client**, connect to your room and enter your slot name.
7. **Start or load your file, then play.** When you make a new file, the game asks whether it's an **Archipelago** or a **Vanilla** file. A new Archipelago file waits at "Please connect to an Archipelago server." until the client has connected once.

## What's randomized
Depending on your YAML options:
- **Pixls:** the eight story Pixls, and optionally Barry, Dashell and Piccolo. They can go anywhere, or only on Pixl spots (`among_pixls`).
- **Key items:** every chapter's keys, 7-4's orbs, the Water, Stone and Fire Tablets, the Cooking Disks and the Golden Card. A chapter's keys open any lock of their kind in that chapter.
- **Items** in chests and lying around in levels and the towns, and optionally **catch cards**.
- **Pure Hearts** (`shuffle_pure_hearts`): the heart at the end of each of Chapters 1-7 is a location, and the hearts that open closed doors are in the item pool.
- **Characters** (`randomize_characters`): Peach, Luigi and Bowser. Their join scenes are locations either way. You can also start as someone other than Mario (`starting_character`); Mario then becomes an item to find.
- **Story moments** that are locations: Bestovius's flip lesson (1-1), Peach in Flipside (bring her a Spicy Soup: Saffron cooks one from a Fire Burst), Bowser after his fight in 3-1, Luigi in the Underwhere (7-1) and beating Brobot L-type (end of Chapter 6).
- **Tattle checks** (`tattle_checks`): adds Tatl, a Pixl to find, and one check per kind of enemy. With Tatl out, press 1, move her over an enemy with the arrow buttons, and press 1 again.
- **Progressive Wallet** (`progressive_wallet`): you start able to carry 99 coins, and each wallet raises the limit (200, 500, 999, and optionally no limit).
- **The Save Block** (`save_block`): a key item that saves your game from the key item menu, anywhere except the Pits. You start with it by default.
- **Cursya Traps** (`trap_percentage`): they look like a key item you haven't found, give nothing ("You are a FOOL!") and put a random Cursya curse on Mario.
- **Sammer's Kingdom:** `sammer_duels` sets how many duels play in each part (the seed picks which). King Sammer's seven Catch Card rewards after the 100th duel are locations; `sammer_gauntlet` decides whether they can hold anything important.

**Not randomized yet:** the Pit Keys, the Old Key, the Random House Key, the Helmet (you get it when you first enter Chapter 4), ? blocks, shops and the Pit rewards.

## Doors and the goal
- **Chapter doors:** some doors in Flipside Tower are open from the start (`starting_chapter_mode`: the ones you list, a set number at random, or a random number). Every other door opens when you receive its Pure Heart.
- **Flopside:** the blue pipe between the towers works once you have Boomer, Fleep and Mario.
- **Chapter 8:** its door in Flopside Tower follows `castle_requirement`: Pure Heart 8 (`pure_heart_8`), a number of chapters finished (the default: all of 1-7), a number of Pure Hearts, or open from the start. Chapter 8 also needs Peach and Luigi, and Bowser past 8-1.
- **The final battle:** Count Bleck's throne room door in 8-4 is padlocked, with a sign saying what's left. Once you meet `final_requirement` (default: all 8 Pure Hearts) you get the Throne Room Key.
- **Goal:** finish Chapter 8.

Items for other worlds show up as the Archipelago icon (grey for filler). Items you receive pop up on Mario with the normal item-get.

## In the game
- **Options:** the pause menu has an Options page (below Recipes). Each file keeps its own settings, and the file select screen's Options button (between Copy Data and Back) sets them for the next new file.

  | Setting | Default | What it does |
  |---|---|---|
  | Music shuffle | off | Songs play in place of each other |
  | Shuffle fanfares | off | The same for jingles that play once |
  | Shuffle sound effects | off | Player and menu sounds swap among themselves |
  | Low health beeping | off | The warning beep at low HP |
  | Hold 1 to skip text | on | Holding 1 speeds through text |
  | Fast climbing | on | Ladders and vines twice as fast |
  | Fast elevators | on | Town elevators skip their waits; tower elevators go straight to the other floor |
  | Skip cutscenes | on | Skips the opening, Merlon's scenes, the Count Bleck scenes between chapters and other story scenes without checks |
  | Cull far enemies | on | Pauses and hides enemies far off screen, so crowded rooms run smoothly |
  | Remove Squirps projectile limit | on | Squirps can fire more than 4 shots at once |
  | Hold 2 for rapid fire Squirps | on | Holding 2 keeps firing |
  | No ninjas in Sammer duels | on | Hides the ninjas in Sammer's Kingdom |
  | Highlight Dorguy answers | on | The right answers in 7-2's quizzes show in red |

- **Fast travel:** in the pause menu's Chapters tab, pick a section you've reached and press 2 to go there.
- **Connection status:** while paused, the bottom left shows whether the client is connected.
- **Key item storage:** Mario can carry 32 key items. Any more go into storage (with a red notice on screen); use SPM Archipelago Tools to swap them.
- **Tippi on an Archipelago item:** tattling one says what it is and who it's for, and sends that as a hint to everyone.
- **Files belong to a seed:** a file remembers the first seed and slot it plays. Loading it while connected to a different one shows a message and returns to the title screen, so a check from the wrong file never gets sent.
- **Vanilla files** play the normal game, keeping the Options settings (except the cutscene skips) and the block hints in 5-1.

## Known issues
- The WIP logic is incomplete: a seed will probably need something it doesn't list. Please report it.
- Some story events still check the story's progress rather than whether you have the right item.
- The towns don't follow the story: their story scenes are simplified.
- Chapter 3 still takes Tippi away when you enter it; changing rooms brings her back.
- Gecko cheat codes can't be used with the patched game.

## Helpful tools
- **Super Paper Mario Tracker** (in the Launcher): a map tracker. It needs pictures of the game's rooms, which aren't included (they're Nintendo's art); developers can render them from their own disc with `tools/maprender`.
- `SPM_helpful_tools_<version>.zip` has three small windows that read the game live from Dolphin:
  - **SPM Tracker** (`client/tracker.py`): every location by level and room, checked or not. Add `--missing` to hide the checked ones.
  - **SPM Coords** (`client/coords.py`): Mario's position, the room, and its unchecked locations.
  - **SPM Archipelago Tools** (`client/ap_tools.py`): add or remove characters, Pixls and the Save Block, set coins, and swap key items between your slots and storage. This one writes to the game, so use it with care.

  They need [Python 3](https://www.python.org) and `py -3 -m pip install dolphin-memory-engine`. Start Dolphin with the game, then run one, for example `pyw -3 client\tracker.py --missing`. Keep the zip's folders as they are. Details are in its `README.txt`.

## Reporting
Please report what each location needs to reach, especially anything the logic gets wrong. Every check, with what the WIP logic says it needs, is in [`logic/checks.md`](logic/checks.md).

## Credits
- **RetailScorpion**: design, testing and direction.
- **Claude** (Anthropic): code. See the [AI usage disclosure](AI_DISCLOSURE.md).
- **[Super Mario Wiki](https://www.mariowiki.com/Coin_Bag_(Mario_Party:_Star_Rush))**: the Progressive Wallet's icon, the Coin Bag from *Mario Party: Star Rush* ([File:CoinBag_MPSR.png](https://www.mariowiki.com/File:CoinBag_MPSR.png)), shrunk to 40x40.
- **Gamtex font by FG Studios**: the "Options" lettering on the pause menu and file select buttons.
- **[SeekyCt](https://github.com/SeekyCt)**, whose tools and research this is built on:
  - [spm-rel-loader](https://github.com/SeekyCt/spm-rel-loader) (GPLv3): loads the mod into the game. Its loader code is built into the patched `main.dol`.
  - [spm-headers](https://github.com/SeekyCt/spm-headers) (MIT, and GPLv3 for its `mod/` part): the game's functions, structures and symbols the mod is written against.
  - [pyelf2rel](https://github.com/SeekyCt/pyelf2rel): builds the mod's REL file.
  - [spm-decomp](https://github.com/SeekyCt/spm-decomp), [spm-docs](https://github.com/SeekyCt/spm-docs), [evt-disassembler](https://github.com/SeekyCt/evt-disassembler) and [spm-practice-codes](https://github.com/SeekyCt/spm-practice-codes): symbols, documentation and reference for the research tools.
- **PistonMiner** and **Zephiles**: the original Paper Mario: The Thousand-Year Door rel loader and tools that spm-rel-loader is based on. The mod's function-hooking approach follows theirs.
- **[Archipelago](https://github.com/ArchipelagoMW/Archipelago)** (MIT): the multiworld framework and client library. The in-game Archipelago icons are based on its logo.
- **[py-dolphin-memory-engine](https://github.com/henriquegemignani/py-dolphin-memory-engine)** (MIT, Henrique Gemignani), from Dolphin Memory Engine by aldelaro5: the client's connection to Dolphin.
- **[Ninja](https://github.com/ninja-build/ninja)** (Apache 2.0): `ninja_syntax.py`, bundled in the mod's build script.
- **[devkitPro](https://devkitpro.org)**: the PowerPC toolchain the mod is built with.

## License
This project is open source under the **GNU General Public License v3** (see [`LICENSE`](LICENSE)), the same as spm-rel-loader and the `mod/` part of spm-headers, which it includes. You're free to use, change and share it, as long as what you share stays open source under the same license, with its source available.

Super Paper Mario is © Nintendo / Intelligent Systems. This project contains no game files: you need your own copy of the game.

## For developers
- [`DESIGN.md`](DESIGN.md): how the randomizer works, feature by feature, and what we know about the game.
- `mod/`: the C++ mod, one file per feature. Build: `py -3 configure.py`, then `ninja`, with `DEVKITPPC` set.
- `apworld/spm/`: the APworld, client, patcher and tracker. `tools/build_apworld.py` regenerates its data and builds `build/spm.apworld`.
- `tools/`: build and research tools (location scan, script disassemblers, data generator, dev disc builder, map renderer).
- `client/`: the helpful tools above, plus dev tools (mailbox watcher, debug client with warps).
