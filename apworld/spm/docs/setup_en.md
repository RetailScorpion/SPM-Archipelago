# Super Paper Mario: setup

## You need
- A Super Paper Mario (USA) revision 0 disc image (game ID R8PE01).
- Dolphin, Archipelago 0.6.7 or newer (tested on 0.6.7), and this APworld installed (double-click `spm.apworld`).

## Steps
1. In Dolphin, right-click Super Paper Mario > Properties > Filesystem > right-click the disc > **Extract Entire Disc**, into an empty folder.
2. In the Archipelago Launcher, open **Super Paper Mario Patcher** and pick that folder. It patches it in place (the original files are kept as `.orig`). Run it again after updating the APworld.
3. In Dolphin, File > Open > `sys/main.dol` in the patched folder (inside `DATA/` if there is one).
4. In the Launcher, open **Super Paper Mario Client** and connect to your room.
5. Start or load your file. A new file asks whether it's an Archipelago or a Vanilla file; a new Archipelago file waits until the client has connected once.

## Known limits
- The logic is a work in progress (`logic: wip_logic`), so a seed will probably need something it doesn't say. Use the Return Pipe to leave rooms you can't get out of.
- The Pit Keys, the Old Key, the Random House Key, ? blocks, shops and the Pit rewards are still vanilla.
- Gecko cheat codes can't be used with the patched game.
