"""Writing each location's item into the game (part of the client).

The game decides what a location gives from its own map scripts: for an item lying in a level or in a
chest, the item number is written right in the script command that creates it. The map scripts (relF)
stay loaded the whole game, so the client overwrites those numbers in memory with the seed's items.
Pixl joins, story gifts and the chapters' Pure Hearts work differently: the mod asks the mailbox what
they give.

What a location shows:
  - one of this world's items: that item
  - another world's item: the Archipelago item (63), and its item and player names go in the mailbox
  - this world's Cursya Trap: a disguise (a key item not received yet)
  - an item this save already got delivered: a coin (a Mistake in a spot), so collecting it again
    doesn't give it twice

Loading a save reloads the map scripts with their vanilla items, so the client writes everything again
whenever a save is loaded.
"""

import random

from BaseClasses import ItemClassification
from CommonClient import logger

from .data import ID_BASE
from .game_link import (GIFT_FLAG_BASE, HEART_LOC_FLAG_BASE, JOIN_ITEM_OTHER_WORLD, LOC_KIND_OTHER_WORLD, LOC_KIND_OTHER_WORLD_FILLER,
                        LOC_KIND_PURE_HEART, LOC_KIND_TRAP, Mailbox)
from .locations import LOCATION_LIST, is_location_index, location_id, location_index

AP_ITEM = 63               # the mod's stand-in for other worlds' items (archipelago_item.h AP_ITEM_ID)
COIN = 1                   # what a join, gift or heart gives once the save already got its item delivered
# What a spot (an item lying around, a chest) holds instead. Not a coin: collecting a coin uses the spot's
# arguments as a coin number, and a spot with a pickup script then reads memory far out of range (user,
# 2026-11-02: a warning in Dolphin, a crash on a Wii). A Mistake is an ordinary item
DELIVERED_SPOT = 175
SERVER_ONLY_FIRST = 1001   # item numbers from here (Pure Hearts, traps) aren't game items
HEARTS = range(1001, 1009) # the Pure Hearts (pure_hearts.h PURE_HEART_ITEM_FIRST), always delivered
TRAP = 1100                # the Cursya Trap (archipelago_item.h TRAP_ITEM_ID)

# What a trap can look like: one of the key items placed in the game's own spots. (Pixls can't be shown
# as world items, and characters crash the pickup.)
DISGUISES = sorted({location.vanilla_item for location in LOCATION_LIST if location.category == "key_item"})


def item_shown(ctx, network_item) -> tuple[int, bool]:
    """(the game item shown at a location, whether it belongs to another world)."""
    if network_item.player == ctx.slot and network_item.item - ID_BASE < SERVER_ONLY_FIRST:
        return network_item.item - ID_BASE, False
    return AP_ITEM, True


def disguise(ctx, location: int) -> int:
    """A trap's look at a location: a key item not received yet, always the same one for that location
    in that seed."""
    received = {item.item - ID_BASE for item in ctx.items_received if item.player == ctx.slot}
    choices = [item for item in DISGUISES if item not in received] or DISGUISES
    return random.Random(f"{ctx.seed_name}-{location}").choice(choices)


def delivered_not_found(ctx, flags) -> set[int]:
    """Locations (positions in LOCATION_LIST) holding this world's items that the loaded save got
    delivered instead of finding them there (found after the save was made, or sent by mistake).
    They hold a coin instead. Pure Hearts never come from their spot."""
    found = set()
    for item in ctx.items_received[:ctx.queued_upto or 0]:
        index = location_index(item.location)
        if (item.player == ctx.slot and is_location_index(index) and item.item - ID_BASE not in HEARTS
                and not Mailbox.flag_set(flags, LOCATION_LIST[index].flag)):
            found.add(index)
    return found


def other_world_names(ctx, network_item, flag: int) -> tuple:
    """The mailbox entry naming another world's item: (flag, item name, player name, icon kind)."""
    important = network_item.flags & (ItemClassification.progression | ItemClassification.useful)
    return (flag, ctx.item_names.lookup_in_slot(network_item.item, network_item.player),
            ctx.player_names.get(network_item.player, "someone"),
            LOC_KIND_OTHER_WORLD if important else LOC_KIND_OTHER_WORLD_FILLER)


def write_location(mb, location, shown: int, foreign: bool, previous: tuple) -> int:
    """Writes what one location gives. `previous` lists other values the spot may hold from an earlier
    write. Returns how many script words didn't hold what was expected (wrong game version)."""
    if location.kind == "join":
        mb.set_join_item(location.vanilla_item, JOIN_ITEM_OTHER_WORLD if foreign else shown)
    elif location.kind == "gift":
        mb.set_gift_item(location.flag - GIFT_FLAG_BASE, JOIN_ITEM_OTHER_WORLD if foreign else shown)
    elif location.kind == "heart":
        mb.set_heart_item(location.flag - HEART_LOC_FLAG_BASE, JOIN_ITEM_OTHER_WORLD if foreign else shown)
    failed = 0
    for offset, original in location.patches:
        if not mb.patch_relf(offset, original, shown, previous):
            failed += 1
    return failed


def write_placements(ctx):
    """Writes every location's item into the game, and the names of other worlds' items."""
    mb = ctx.mailbox
    names, failed = [], 0
    for i, location in enumerate(LOCATION_LIST):
        network_item = ctx.scouted.get(location_id(i))
        if network_item is None:
            continue
        shown, foreign = item_shown(ctx, network_item)
        placed = shown

        if i in ctx.coin_locations:
            # A coin, or a Mistake in a spot (which may still hold the item, a disguise or an old coin)
            if location.kind == "join":
                mb.set_join_item(location.vanilla_item, COIN)
            elif location.kind == "gift":
                mb.set_gift_item(location.flag - GIFT_FLAG_BASE, COIN)
            elif location.kind == "heart":
                mb.set_heart_item(location.flag - HEART_LOC_FLAG_BASE, COIN)
            for offset, original in location.patches:
                if not mb.patch_relf(offset, original, DELIVERED_SPOT, (placed, disguise(ctx, location_id(i)), COIN)):
                    failed += 1
            continue

        if network_item.player == ctx.slot and network_item.item - ID_BASE == TRAP:
            # Spots show a disguise; joins and gifts give the Archipelago item. Either way the mod
            # turns it into the trap's item-get
            names.append((location.flag, "Cursya Trap", "", LOC_KIND_TRAP))
            shown = disguise(ctx, location_id(i))
        elif network_item.player == ctx.slot and network_item.item - ID_BASE in HEARTS:
            # One of this world's Pure Hearts: the Archipelago item, shown with the heart's own icon
            heart = network_item.item - ID_BASE - HEARTS.start + 1
            names.append((location.flag, f"Pure Heart {heart}", "", LOC_KIND_PURE_HEART + heart))
        elif foreign:
            names.append(other_world_names(ctx, network_item, location.flag))
        failed += write_location(mb, location, shown, foreign, (placed, COIN, DELIVERED_SPOT))

    mb.write_loc_names(names)
    if failed:
        logger.warning(f"{failed} item placements didn't match the game; are you on US 1.0 with the matching mod?")
    logger.info(f"Placed items at {len(ctx.scouted)} locations")
