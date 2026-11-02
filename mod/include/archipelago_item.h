#pragma once

#include <common.h>

/*
    The Archipelago item: what Mario finds where another world's item is placed.

    Collecting it plays the game's normal item-get, but Mario holds up the Archipelago icon and the
    text says "Sent <item> to <player>!" instead of "You got <item>!". It never goes into the
    inventory.

    It reuses item 63 (Cooking Disk 1, which can't be obtained in vanilla): the mod points that
    item's picture at the Archipelago icon (added to the game's icon files by the patcher) and its
    name and description at the mod's own text.

    Which item and player to show: the client writes a list of every location holding another
    world's item, keyed by the location's save flag (LocName below). When the Archipelago item is
    collected, the mod works out which location it came from:
      - pickups lying in a level carry their flag on the item itself
      - chests and Pixl joins set their flag around the time the item appears (watched with a hook)
      - chests also get matched to the nearest unopened chest the map created

    Icons: progression and useful items show the colour icon, filler (junk) the grey one.

    Traps (Cursya Traps): a location holding this world's trap looks like a random key item. Whatever
    it looks like, collecting it becomes the Archipelago item with a blank icon and "You are a FOOL!",
    and once that's over Mario gets a random Cursya curse. Traps sent from other worlds come through
    the server (TRAP_ITEM_ID) and play the same way.
*/

namespace mod::archipelago_item {

// The game item reused as the Archipelago item
constexpr s32 AP_ITEM_ID = 63;
// A Cursya Trap from the server (not a game item: the Archipelago item number minus the world's base)
constexpr s32 TRAP_ITEM_ID = 1100;

// What kind of item a listed location holds
enum LocKind : u16
{
    LOC_KIND_OTHER_WORLD = 0,         // another world's progression or useful item: colour icon
    LOC_KIND_TRAP = 1,                // this world's trap
    LOC_KIND_OTHER_WORLD_FILLER = 2,  // another world's filler (or trap): grey icon
    LOC_KIND_PURE_HEART = 16,         // + N (1-8): this world's Pure Heart N, shown with its own icon
};

// One location holding another world's item, as the client writes it (mailbox locNames)
struct LocName
{
    u16 flag;        // the location's save flag (GSWF); 0 = empty entry
    u16 kind;        // LocKind
    char item[36];   // item name, ending with a 0 byte
    char player[24]; // player name
};
static_assert(sizeof(LocName) == 0x40);

// Sets up item 63 and hooks the game functions involved. Call once at startup.
void init();

// Ends the Archipelago item-get shortly after its last frame. Call every frame in game.
void tick();

// Forgets the chests of the previous map. Call on every map change.
void onMapChange();

// Makes the next Archipelago item-get a trap (one received from the server).
void armTrap();

// The mod's text for a game message name, or nullptr to use the game's own.
const char * message(const char * msgName);

}
