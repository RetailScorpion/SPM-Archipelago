"""Super Paper Mario (US 1.0) for Archipelago.

This is the "world": what Archipelago calls to make a seed for Super Paper Mario. It
  - lists every item and location (items.py, locations.py, from data.py)
  - picks which chapters are open at the start
  - builds the item pool: the shuffled items, the Pure Hearts, the Save Block and traps
  - adds the rules (rules.py)
  - adds the Super Paper Mario Client and Patcher to the Archipelago Launcher

The goal is finishing Chapter 8.
"""

from BaseClasses import ItemClassification, Location, Region, Tutorial
from Options import OptionError
from worlds.AutoWorld import WebWorld, World
from worlds.LauncherComponents import Component, Type, components, launch_subprocess

from . import rules
from .data import ID_BASE
from .items import (FILLER_ITEM, GAME, ITEM_NAME_TO_ID, SAVE_BLOCK_ITEM, TOWER_CHAPTERS, TRAP_ITEM, VICTORY_ITEM,
                    SPMItem, create_item, vanilla_item_name)
from .locations import LOCATION_LIST, location_id
from .options import SPMOptions

# Each location category, and the option that shuffles it
CATEGORY_OPTIONS = {
    "pixl": "shuffle_pixls",
    "optional_pixl": "shuffle_optional_pixls",
    "key_item": "shuffle_key_items",
    "overworld_item": "shuffle_overworld_items",
    "catch_card": "shuffle_catch_cards",
    "character": "randomize_characters",
}
# Categories that are never shuffled. Only the Helmet's location for now: Outer Space can't be played
# without the Helmet, and nothing in the logic keeps you out of Chapter 4 without it yet
FIXED_CATEGORIES = {"fixed"}


# ---- Launcher entries ----

def launch_client(*args):
    from .client import main
    launch_subprocess(main, name="SPMClient", args=args)


def launch_patcher(*args):
    from .patcher import launch_patcher as run
    launch_subprocess(run, name="SPMPatcher", args=args)


components.append(Component("Super Paper Mario Client", func=launch_client, component_type=Type.CLIENT))
components.append(Component("Super Paper Mario Patcher", func=launch_patcher, component_type=Type.TOOL))


# ---- The world ----

class SPMLocation(Location):
    game = GAME


class SPMWeb(WebWorld):
    theme = "partyTime"
    tutorials = [Tutorial("Setup Guide", "Setting up the Super Paper Mario prototype.", "English",
                          "setup_en.md", "setup/en", ["RetailScorpion"])]


class SPMWorld(World):
    """Super Paper Mario: flip between dimensions to stop Count Bleck. No-logic prototype."""

    game = GAME
    web = SPMWeb()
    options_dataclass = SPMOptions
    options: SPMOptions
    topology_present = False

    item_name_to_id = ITEM_NAME_TO_ID
    location_name_to_id = {location.name: location_id(i) for i, location in enumerate(LOCATION_LIST)}

    def create_item(self, name: str) -> SPMItem:
        return create_item(name, self.player)

    def shuffled(self, category: str) -> bool:
        """Whether the YAML shuffles this location category."""
        if category in FIXED_CATEGORIES:
            return False
        return bool(getattr(self.options, CATEGORY_OPTIONS[category]).value)

    def generate_early(self) -> None:
        """Picks the chapters whose doors are open at the start (starting_chapter_mode)."""
        mode = self.options.starting_chapter_mode
        if mode == mode.option_chosen:
            chapters = {int(key.split()[1]) for key in self.options.starting_chapters.value}
            if not chapters:
                raise OptionError(f"{self.player_name}: Starting Chapters needs at least one chapter")
        else:
            if mode == mode.option_random_chapters:
                count = self.options.starting_chapter_count.value
            else:
                count = self.random.randint(1, self.options.starting_chapter_max.value)
            chapters = set(self.random.sample(list(TOWER_CHAPTERS), count))
        self.starting_chapters = sorted(chapters)

    def create_regions(self) -> None:
        """One region holds every location (there's no map logic yet), plus the goal."""
        menu = Region("Menu", self.player, self.multiworld)
        for i, location in enumerate(LOCATION_LIST):
            menu.locations.append(SPMLocation(self.player, location.name, location_id(i), menu))
        victory = SPMLocation(self.player, rules.VICTORY_LOCATION, None, menu)
        victory.place_locked_item(self.create_item(VICTORY_ITEM))
        menu.locations.append(victory)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        """Builds the item pool."""
        # Each location's vanilla item: into the pool if its category is shuffled, else kept in place
        pool, kept = [], []
        for location in LOCATION_LIST:
            item = self.create_item(vanilla_item_name(location.vanilla_item))
            if self.shuffled(location.category):
                pool.append(item)
            else:
                kept.append((location.name, location.category, item))

        # Items added to the pool: the Pure Hearts of the chapters that start closed, and the Save Block
        # if it's shuffled
        added = [self.create_item(f"Pure Heart {n}") for n in TOWER_CHAPTERS if n not in self.starting_chapters]
        save_block = self.options.save_block
        if save_block == save_block.option_shuffle:
            added.append(self.create_item(SAVE_BLOCK_ITEM))
        elif save_block == save_block.option_start_with:
            self.multiworld.push_precollected(self.create_item(SAVE_BLOCK_ITEM))

        self.make_room(pool, kept, len(added))

        # Traps replace a share of the remaining shuffled filler
        filler = [item for item in pool if item.classification == ItemClassification.filler]
        traps = round(len(filler) * self.options.trap_percentage.value / 100)
        for item in self.random.sample(filler, traps):
            pool.remove(item)
        pool += [self.create_item(TRAP_ITEM) for _ in range(traps)]
        self.multiworld.itempool += pool + added

    def make_room(self, pool: list, kept: list, needed: int) -> None:
        """Makes room for `needed` added items, which take the place of filler: first by taking filler
        out of the pool, then by freeing locations that would have kept their filler. Every location
        not freed gets its vanilla item placed."""
        for item in [item for item in pool if item.classification == ItemClassification.filler][:needed]:
            pool.remove(item)
            needed -= 1
        freeable = [entry for entry in kept
                    if entry[2].classification == ItemClassification.filler and entry[1] not in FIXED_CATEGORIES]
        freed = set(name for name, _, _ in self.random.sample(freeable, min(needed, len(freeable))))
        if len(freed) < needed:
            raise OptionError(f"{self.player_name}: not enough locations for the added items")
        for name, _, item in kept:
            if name not in freed:
                self.get_location(name).place_locked_item(item)

    def set_rules(self) -> None:
        rules.set_rules(self)

    def get_filler_item_name(self) -> str:
        return FILLER_ITEM

    def fill_slot_data(self) -> dict:
        """What the client gets when it connects: some options, and the starting chapters."""
        return {"options": self.options.as_dict(*CATEGORY_OPTIONS.values(), "skip_cutscenes"), "id_base": ID_BASE,
                "starting_chapters": self.starting_chapters}
