#pragma once

#include <common.h>

/*
    Keys work for any lock of their kind in their chapter (user, 2026-11-12): any Underwhere Door Key opens
    any Underwhere lock, either Cave Key either Floro Caverns lock, and so on, whatever number the key has.
    Each group's keys all become one of them (its canonical key) as they enter the inventory, and the
    group's locks (their mobj, the "have the key?" check, the item menu's list, the "right item?" test, and
    3-4's removal) are pointed at that key. Groups: 3-4's Fort Keys 19 and 21 -> 20 (the King's door
    already takes 20 or 21), 4-4's Dimension Keys 28 and 29 -> 27, Floro Caverns' Cave Key 34 -> 33, the
    Underwhere's Door Keys 37, 38 and 49 -> 40 (7-1's lock, 49's, never appears in Chapter 7, so that
    key opens one of the other four). Ruins Keys and Castle Bleck Keys are one item already;
    2-1's Door Key, 2-2's House Key and 4-2's Door Key have no others in their chapter.
*/

namespace mod::key_groups {

// The key an item becomes in the inventory: its group's canonical key, or itself.
s32 canonical(s32 itemId);

// Points the grouped locks at their canonical keys. Call once at startup.
void init();

// Turns grouped keys already held (from before this, or stored) into their canonical key. Call on every
// map change of an Archipelago file.
void onMapChange();

}
