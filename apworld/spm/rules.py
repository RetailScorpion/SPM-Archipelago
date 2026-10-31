"""The logic: what each location needs before it can be reached.

The randomizer has no real logic yet (that's being mapped out by playing). The only rules are the
chapter doors:
  - a location in Chapters 1-7 needs that chapter's Pure Heart, unless its door is open from the start
  - a location in Chapter 8, and finishing the game, need every Pure Heart that isn't given at the start
  - locations outside the chapters (the hubs, the Pits...) need nothing
"""

from worlds.generic.Rules import add_rule

from .items import TOWER_CHAPTERS, VICTORY_ITEM
from .locations import LOCATION_LIST, chapter_of

VICTORY_LOCATION = "Chapter 8 complete"


def heart_needed(chapter: int, starting_chapters: list[int]) -> str | None:
    """The Pure Heart a chapter's locations need, or None if its door is open from the start."""
    return None if chapter in starting_chapters else f"Pure Heart {chapter}"


def hearts_for_location(location_name: str, all_hearts: list[str], starting_chapters: list[int]) -> list[str]:
    """The Pure Hearts a location needs."""
    chapter = chapter_of(location_name)
    if chapter == 8:
        return all_hearts
    if chapter is not None:
        heart = heart_needed(chapter, starting_chapters)
        return [heart] if heart else []
    return []


def set_rules(world) -> None:
    """Adds every rule to a player's world."""
    player = world.player
    all_hearts = [heart for heart in (heart_needed(chapter, world.starting_chapters) for chapter in TOWER_CHAPTERS)
                  if heart]
    for location in LOCATION_LIST:
        needed = hearts_for_location(location.name, all_hearts, world.starting_chapters)
        if needed:
            add_rule(world.get_location(location.name), lambda state, needed=needed: state.has_all(needed, player))
    add_rule(world.get_location(VICTORY_LOCATION), lambda state: state.has_all(all_hearts, player))
    world.multiworld.completion_condition[player] = lambda state: state.has(VICTORY_ITEM, player)
