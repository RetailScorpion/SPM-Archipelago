"""The randomizer's locations, with names for each of their details.

The list itself is in data.py, which tools/gen_ap_data.py generates from the game's files (don't edit
it by hand). There each location is a row of six values; this file gives those values names, so the
rest of the code can say `location.flag` instead of `row[4]`.
"""

from typing import NamedTuple

from .data import ID_BASE, LOCATIONS as LOCATION_ROWS


class Location(NamedTuple):
    name: str          # e.g. "1-2 Mount Lineland - Thoreau joins (he2_07)"
    kind: str          # how it gives its item: "item" (lying in a level), "chest", "join" (a Pixl
                       # joining) or "gift" (a story gift)
    category: str      # which option shuffles it: "pixl", "key_item", "overworld_item", ...
    vanilla_item: int  # the game's item id of what it holds in the unrandomized game
    flag: int          # the save flag (GSWF) the game sets when it's collected
    patches: list      # where its item is written in the game's map scripts:
                       # [(relF file offset, the vanilla word there)]


# Every location, in order. A location's Archipelago id is ID_BASE + its position in this list.
LOCATION_LIST = [Location(*row) for row in LOCATION_ROWS]


def location_id(index: int) -> int:
    """The Archipelago id of the location at this position in LOCATION_LIST."""
    return ID_BASE + index


def location_index(location_id_: int) -> int:
    """The position in LOCATION_LIST of an Archipelago location id (can be out of range for ids that
    aren't this world's)."""
    return location_id_ - ID_BASE


def is_location_index(index: int) -> bool:
    """Whether a position is inside LOCATION_LIST."""
    return 0 <= index < len(LOCATION_LIST)


def chapter_of(location_name: str) -> int | None:
    """The chapter a location is in, from its name ("3-1 The Bitlands - ..." is in Chapter 3), or None
    for the hubs and other places outside the chapters."""
    if location_name[0].isdigit() and location_name[1] == "-":
        return int(location_name[0])
    return None
