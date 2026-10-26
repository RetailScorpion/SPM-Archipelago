# Super Paper Mario prototype: setup

## You need
- A Super Paper Mario (USA) revision 0 disc image (game ID R8PE01).
- Dolphin, Archipelago 0.6.7, and this APworld installed (double-click `spm.apworld`).

## Steps
1. In Dolphin, right-click Super Paper Mario > Properties > Filesystem > right-click the disc > **Extract Entire Disc**, into an empty folder.
2. In the Archipelago Launcher, open **Super Paper Mario Patcher** and pick that folder. It patches it in place (originals are kept as `.orig`). Run it again after updating the APworld.
3. In Dolphin, File > Open > `sys/main.dol` in the patched folder (inside `DATA/` if there is one).
4. In the Launcher, open **Super Paper Mario Client** and connect to your room.
5. Load your save. **Connect before entering any level**, so the client can place the items.

## Known limits
- No logic: you may need items from late chapters early. Use the Return Pipe to leave rooms you can't get out of.
- Pure Hearts, characters, the story keys, ? blocks and shops are still vanilla.
- Gecko cheat codes can't be used with the patched game.
