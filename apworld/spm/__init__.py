"""Super Paper Mario (US 1.0) for Archipelago.

This is the "world": what Archipelago calls to make a seed for Super Paper Mario. It
  - lists every item and location (items.py, locations.py, from data.py)
  - picks which chapters are open at the start
  - builds the item pool: the shuffled items, the Pure Hearts, Tatl, wallets, the Save Block and traps
  - adds the rules (rules.py)
  - adds the Super Paper Mario Client, Patcher and Tracker to the Archipelago Launcher

The goal is finishing Chapter 8.
"""

from BaseClasses import CollectionState, ItemClassification, Location, LocationProgressType, Region, Tutorial
from Options import OptionError
from Fill import fill_restrictive
from worlds.AutoWorld import WebWorld, World
from worlds.LauncherComponents import Component, Type, components, launch_subprocess

from . import rules
from .data import ID_BASE
from .items import (CHARACTERS, FILLER_WEIGHTS, GAME, HEART_8, ITEM_NAME_TO_ID, MARIO_ITEM, SAVE_BLOCK_ITEM, TATL_ITEM, TOWER_CHAPTERS, TRAP_ITEM, VICTORY_ITEM, WALLET_ITEM, FIRE_BURST_ITEM,
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
    "pure_heart": "shuffle_pure_hearts",
    "tattle": "tattle_checks",
}
# Categories that can be shuffled only among their own spots (the options' among_* choice), and the
# group each joins: a group's items go only to its spots, and its spots hold only its items
GROUP_OF = {"pixl": "pixls", "optional_pixl": "pixls", "character": "characters"}
AMONG_CHOICE = 2  # option_among_pixls / option_among_characters

# Categories that are never shuffled. Only the Helmet's location for now: Outer Space can't be played
# without the Helmet, and nothing in the logic keeps you out of Chapter 4 without it yet
FIXED_CATEGORIES = {"fixed"}
# Maps whose locations are post-game Sammer's Kingdom's rewards (the sammer_gauntlet option)
SAMMER_GAUNTLET_MAPS = ("(wa4_26)",)

# Sammer's Kingdom's groups of duels, for sammer_duels: (how many gates, whether its first gate always
# plays). The story's duels are gates 1-20 (21-30 are rooms whose Sammer Guys give up); the post-game
# has four groups of 25. Each group's last gate always plays.
SAMMER_GROUPS = [(20, True), (25, True), (25, False), (25, False), (25, False)]


def sammer_gates(random, duels: int) -> list[int]:
    """The Sammer Guy gates that play in each group (story, then post-game groups 1-4), as bitmasks of
    the group's gates (bit 0 = its first gate): the last gate, the first one where it always plays, and
    random ones up to `duels` in all."""
    masks = []
    for count, first_fixed in SAMMER_GROUPS:
        fixed = {count - 1} | ({0} if first_fixed else set())
        others = [gate for gate in range(count) if gate not in fixed]
        chosen = fixed | set(random.sample(others, max(0, min(duels, count) - len(fixed))))
        masks.append(sum(1 << gate for gate in chosen))
    return masks


# ---- Launcher entries ----

def launch_client(*args):
    from .client import main
    launch_subprocess(main, name="SPMClient", args=args)


def launch_patcher(*args):
    from .patcher import launch_patcher as run
    launch_subprocess(run, name="SPMPatcher", args=args)


def launch_tracker(*args):
    from .tracker_app import launch
    launch_subprocess(launch, name="SPMTracker", args=args)


components.append(Component("Super Paper Mario Client", func=launch_client, component_type=Type.CLIENT))
components.append(Component("Super Paper Mario Patcher", func=launch_patcher, component_type=Type.TOOL))
components.append(Component("Super Paper Mario Tracker", func=launch_tracker, component_type=Type.TOOL))


# ---- The world ----

class SPMLocation(Location):
    game = GAME


class SPMWeb(WebWorld):
    theme = "partyTime"
    tutorials = [Tutorial("Setup Guide", "Setting up Super Paper Mario.", "English",
                          "setup_en.md", "setup/en", ["RetailScorpion"])]


# How many checks a starting character needs in one of the open chapters (generate_early)
MIN_OPEN_CHECKS = 3


class SPMWorld(World):
    """Super Paper Mario: flip between dimensions to stop Count Bleck."""

    game = GAME
    web = SPMWeb()
    options_dataclass = SPMOptions
    options: SPMOptions
    topology_present = False

    item_name_to_id = ITEM_NAME_TO_ID
    location_name_to_id = {location.name: location_id(i) for i, location in enumerate(LOCATION_LIST)}

    def create_item(self, name: str) -> SPMItem:
        item = create_item(name, self.player)
        # Starting as someone else, a Fire Burst can be what Peach's check needs (rules.py PEACH_JOIN)
        if name == FIRE_BURST_ITEM and self.starting_character != 0:
            item.classification = ItemClassification.progression
        return item

    def shuffled(self, category: str) -> bool:
        """Whether the YAML shuffles this location category."""
        if category in FIXED_CATEGORIES:
            return False
        return bool(getattr(self.options, CATEGORY_OPTIONS[category]).value)

    def in_group(self, category: str) -> bool:
        """Whether the YAML shuffles this category only among its own group's spots."""
        return category in GROUP_OF and getattr(self.options, CATEGORY_OPTIONS[category]).value == AMONG_CHOICE

    def pre_fill(self) -> None:
        """Places the among_* groups' items on their own spots, with the logic (everything else in the pool
        counts as collectable, so a spot can need items from anywhere). All groups go in one fill, each
        spot taking only its own group's items: placed one group at a time, each assuming the other
        already collected, Pixl spots that need a character and character spots that need a Pixl could
        lock each other out (2026-11-08: about 1 seed in 8 once every X-2 to X-4 needed the section
        before it)."""
        if not self.groups:
            return
        state = CollectionState(self.multiworld)
        for item in self.multiworld.itempool:
            if item.player == self.player:
                state.collect(item, True)
        state.sweep_for_advancements()
        locations, items = [], []
        for spots, group_items in self.groups.values():
            names = {item.name for item in group_items}
            for name in spots:
                location = self.get_location(name)
                location.item_rule = lambda item, names=names: item.player == self.player and item.name in names
                locations.append(location)
            items += group_items
        self.random.shuffle(locations)
        fill_restrictive(self.multiworld, state, locations, items, single_player_placement=True,
                         lock=True, allow_excluded=True)

    def generate_early(self) -> None:
        """Picks the chapters whose doors are open at the start (starting_chapter_mode)."""
        start = self.options.starting_character.value
        self.starting_character = self.random.randrange(4) if start == self.options.starting_character.option_random else start
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
            # Starting as someone else, Mario's flip is an item to find, and much of the game needs it
            # (all of Chapter 1 and Flipside, user 2026-11-08): at least one open chapter must have
            # checks the starting character can do alone, or the seed has nowhere to begin
            if self.starting_character != 0:
                held = {CHARACTERS[self.starting_character]}
                # (not Chapter 6: every check there is behind Brobot L-type, who needs a Pixl)
                playable = [n for n in TOWER_CHAPTERS if n != 6 and rules.open_checks(n, held) >= MIN_OPEN_CHECKS]
                if playable and not chapters & set(playable):
                    chapters.remove(self.random.choice(sorted(chapters)))
                    chapters.add(self.random.choice(playable))
            # Chapter 6 alone: its only check is Brobot L-type, who needs Boomer (or Fleep). That's only
            # fair starting as Mario with the Pixls shuffled anywhere, so Flipside's checks can hold them
            # (user, 2026-11-08); otherwise another chapter opens in its place
            pixls_anywhere = self.options.shuffle_pixls == self.options.shuffle_pixls.option_anywhere
            if chapters == {6} and (self.starting_character != 0 or not pixls_anywhere):
                held = {CHARACTERS[self.starting_character]}
                others = [n for n in TOWER_CHAPTERS if n != 6 and rules.open_checks(n, held) >= MIN_OPEN_CHECKS]
                chapters = {self.random.choice(others or [n for n in TOWER_CHAPTERS if n != 6])}
        # With the hearts at their vanilla spots, each chapter's ending opens the next, so the chain
        # has to start at Chapter 1 (with only later doors open, the earlier chapters could never open).
        # A random pick keeps its size: Chapter 1 takes the place of one of the others
        if not self.options.shuffle_pure_hearts and 1 not in chapters:
            if mode != mode.option_chosen:
                chapters.remove(self.random.choice(sorted(chapters)))
            chapters.add(1)
        self.starting_chapters = sorted(chapters)
        # When a requirement counts Pure Hearts, the starting doors' hearts are items to find too (with
        # hearts shuffled; vanilla hearts have no spot for them). Those doors still start open
        options = self.options
        counts_hearts = (options.castle_requirement == options.castle_requirement.option_pure_hearts
                         or options.final_requirement == options.final_requirement.option_pure_hearts)
        self.starting_hearts_in_pool = bool(options.shuffle_pure_hearts) and counts_hearts
        self.sammer_gates = sammer_gates(self.random, self.options.sammer_duels.value)

    def has_location(self, location) -> bool:
        """Whether a location is in this seed: tattles only with tattle_checks, and only where the logic
        counts a room for them (rules.tattle_rooms)."""
        if location.kind != "tattle":
            return True
        return bool(self.options.tattle_checks) and bool(rules.tattle_rooms(self.options, location.name))

    def create_regions(self) -> None:
        """One region holds every location, plus the goal (the logic is all in location rules, rules.py)."""
        menu = Region("Menu", self.player, self.multiworld)
        for i, location in enumerate(LOCATION_LIST):
            if not self.has_location(location):
                continue
            spm_location = SPMLocation(self.player, location.name, location_id(i), menu)
            # Without sammer_gauntlet, the 100 duels' rewards only hold filler
            if location.name.endswith(SAMMER_GAUNTLET_MAPS) and not self.options.sammer_gauntlet:
                spm_location.progress_type = LocationProgressType.EXCLUDED
            menu.locations.append(spm_location)
        victory = SPMLocation(self.player, rules.VICTORY_LOCATION, None, menu)
        victory.place_locked_item(self.create_item(VICTORY_ITEM))
        menu.locations.append(victory)
        self.multiworld.regions.append(menu)

    def create_items(self) -> None:
        """Builds the item pool."""
        # Each location's vanilla item: into the pool if its category is shuffled, else kept in place.
        # A chapter's Pure Heart location adds filler instead: which hearts are needed depends on the
        # starting chapters, so those are added below
        # A starting character other than Mario is yours from the start, and Mario takes their place
        # (in the pool, or at their join scene)
        start = CHARACTERS[self.starting_character]
        if start != MARIO_ITEM:
            self.multiworld.push_precollected(self.create_item(start))
        pool, kept = [], []
        self.groups = {}  # group -> (its spots' names, its items), for pre_fill
        for location in LOCATION_LIST:
            if not self.has_location(location):
                continue
            name = vanilla_item_name(location.vanilla_item)
            if location.kind == "tattle":
                name = self.get_filler_item_name()  # a tattle's vanilla "item" is only a stand-in
            if name == start and start != MARIO_ITEM:
                name = MARIO_ITEM
            item = self.create_item(name)
            if self.in_group(location.category):
                spots, items = self.groups.setdefault(GROUP_OF[location.category], ([], []))
                spots.append(location.name)
                items.append(item)
            elif self.shuffled(location.category):
                pool.append(self.create_filler() if location.category == "pure_heart" else item)
            else:
                kept.append((location.name, location.category, item))

        # Items added to the pool: the Pure Hearts of the chapters that start closed (unless they stay at
        # their chapters' endings), and the Save Block if it's shuffled
        added = []
        if self.options.shuffle_pure_hearts:
            added += [self.create_item(f"Pure Heart {n}") for n in TOWER_CHAPTERS
                      if n not in self.starting_chapters or self.starting_hearts_in_pool]
            added.append(self.create_item(HEART_8))  # Chapter 7's reward: for the castle and final requirements
        if self.options.tattle_checks:
            added.append(self.create_item(TATL_ITEM))
        wallet = self.options.progressive_wallet
        if wallet != wallet.option_off:
            added += [self.create_item(WALLET_ITEM) for _ in range(4 if wallet == wallet.option_with_infinite else 3)]
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
        return self.random.choices(list(FILLER_WEIGHTS), weights=list(FILLER_WEIGHTS.values()))[0]

    def write_spoiler_header(self, spoiler_handle) -> None:
        """What the seed picked, in the spoiler (its option lines only show the YAML's choices: Starting
        Chapters there is the list for "chosen", not the random pick; user, 2026-11-08)."""
        spoiler_handle.write(f"Open chapters at the start:      {', '.join(map(str, self.starting_chapters))}\n")
        spoiler_handle.write(f"Starting character (picked):     {CHARACTERS[self.starting_character]}\n")

    def fill_slot_data(self) -> dict:
        """What the client gets when it connects: some options, and the starting chapters."""
        return {"options": self.options.as_dict(*CATEGORY_OPTIONS.values()), "id_base": ID_BASE,
                "sammer_gates": self.sammer_gates,
                "starting_hearts_in_pool": self.starting_hearts_in_pool,
                "big_cloud_requires_peach_reunion": bool(self.options.big_cloud_requires_peach_reunion.value),
                "castle_requirement": rules.requirement(self.options.castle_requirement, self.options.castle_chapters,
                                                        self.options.castle_hearts),
                "final_requirement": rules.requirement(self.options.final_requirement, self.options.final_chapters,
                                                       self.options.final_hearts),
                "starting_chapters": self.starting_chapters,
                "starting_character": self.starting_character,
                "tattle_checks": bool(self.options.tattle_checks.value),
                "pit_tattles": bool(self.options.pit_tattles.value),
                "afk_tattles": bool(self.options.afk_tattles.value),
                "logic": self.options.logic.value,
                "progressive_wallet": self.options.progressive_wallet.value,
                # for the map tracker (tracker_logic.py, and its items tab)
                "brobot_with_fleep": bool(self.options.brobot_with_fleep.value),
                "bowser_with_items": bool(self.options.bowser_with_items.value),
                "save_block": self.options.save_block.value}
