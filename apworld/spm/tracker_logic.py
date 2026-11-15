"""The logic for the map tracker: which locations can be reached with the items received so far.

It runs the APworld's own rules (rules.set_rules) against a stand-in world and state, so the tracker always
agrees with the generator, outside Archipelago too (the tracker's window runs on a Python of its own, which
has no BaseClasses or worlds packages: small stand-ins are put in their place then).

    reachable(received_ids, slot_data) -> [Archipelago ids of the reachable locations]

received_ids: the Archipelago item ids the server sent this slot (copies repeated); slot_data: what the
server gave on connecting (__init__.py fill_slot_data). Options missing from older seeds' slot data are
taken as off, which only makes the logic stricter.
"""

import sys
import types


def _stand_ins():
    """BaseClasses and worlds.generic.Rules, when Archipelago isn't there: just what rules.py and items.py use."""
    try:
        import BaseClasses  # noqa: F401
        from worlds.generic.Rules import add_rule  # noqa: F401
        return
    except ImportError:
        pass
    import enum
    base = types.ModuleType("BaseClasses")

    class ItemClassification(enum.IntFlag):
        filler = 0
        progression = 1
        useful = 2
        trap = 4
        skip_balancing = 8
        progression_skip_balancing = 9

    class Item:
        def __init__(self, name, classification, code, player):
            self.name, self.classification, self.code, self.player = name, classification, code, player

    class _Anything:  # names rules.py / items.py import but the tracker never uses
        def __init__(self, *args, **kwargs):
            pass
    base.Item, base.ItemClassification = Item, ItemClassification
    for name in ("CollectionState", "Location", "LocationProgressType", "Region", "Tutorial", "MultiWorld"):
        setattr(base, name, _Anything)
    sys.modules.setdefault("BaseClasses", base)
    rules_mod = types.ModuleType("worlds.generic.Rules")
    rules_mod.add_rule = lambda spot, rule, combine="and": spot.rules.append(rule)
    worlds = sys.modules.setdefault("worlds", types.ModuleType("worlds"))
    generic = sys.modules.setdefault("worlds.generic", types.ModuleType("worlds.generic"))
    worlds.generic, generic.Rules = generic, rules_mod
    sys.modules.setdefault("worlds.generic.Rules", rules_mod)


_stand_ins()
from . import rules  # noqa: E402
from .items import ITEM_NAME_TO_ID  # noqa: E402
from .locations import LOCATION_LIST, location_id  # noqa: E402

ITEM_NAMES = {code: name for name, code in ITEM_NAME_TO_ID.items()}


class _Spot:
    def __init__(self, name):
        self.name, self.rules = name, []


class _Option:
    """An option's value, as rules.py reads it (truthiness, .value; the endgame requirements come ready-made)."""
    def __init__(self, value, kind_count=None):
        self.value, self.kind_count = value, kind_count

    def __bool__(self):
        return bool(self.value)


class _World:
    def __init__(self, slot_data):
        options = dict(slot_data.get("options") or {})
        for key in ("big_cloud_requires_peach_reunion", "tattle_checks", "brobot_with_fleep", "pit_tattles",
                    "bowser_with_items"):
            options.setdefault(key, slot_data.get(key, 0))
        # seeds from before afk_tattles had the Megabite and Gigabite tattles
        options.setdefault("afk_tattles", slot_data.get("afk_tattles", 1))
        # and the WIP logic (seeds from before the logic option)
        options.setdefault("logic", slot_data.get("logic", 1))
        self.options = types.SimpleNamespace(
            castle_requirement=_Option(0, slot_data.get("castle_requirement", [rules.ENDGAME_VANILLA, 0])),
            final_requirement=_Option(0, slot_data.get("final_requirement", [rules.ENDGAME_VANILLA, 0])),
            castle_chapters=_Option(0), castle_hearts=_Option(0), final_chapters=_Option(0), final_hearts=_Option(0),
            **{key: _Option(value) for key, value in options.items()
               if key not in ("castle_requirement", "final_requirement")})
        for key in ("brobot_with_fleep", "big_cloud_requires_peach_reunion", "tattle_checks", "pit_tattles",
                    "bowser_with_items"):
            if not hasattr(self.options, key):
                setattr(self.options, key, _Option(0))
        self.player = 1
        self.starting_chapters = list(slot_data.get("starting_chapters") or [])
        self.starting_hearts_in_pool = bool(slot_data.get("starting_hearts_in_pool"))
        self.starting_character = int(slot_data.get("starting_character") or 0)
        self.spots = {location.name: _Spot(location.name) for location in LOCATION_LIST}
        self.spots[rules.VICTORY_LOCATION] = _Spot(rules.VICTORY_LOCATION)
        self.multiworld = types.SimpleNamespace(completion_condition={})

    def get_location(self, name):
        return self.spots[name]


class _State:
    def __init__(self, world, counts):
        self.world, self.counts, self.memo, self.busy = world, counts, {}, set()

    def has(self, item, player, count=1):
        return self.counts.get(item, 0) >= count

    def count(self, item, player):
        return self.counts.get(item, 0)

    def has_all(self, items, player):
        return all(self.counts.get(item, 0) for item in items)

    def has_any(self, items, player):
        return any(self.counts.get(item, 0) for item in items)

    def can_reach(self, name, kind="Location", player=None):
        if name in self.memo:
            return self.memo[name]
        if name in self.busy:  # a loop: not reachable through itself
            return False
        self.busy.add(name)
        ok = all(rule(self) for rule in self.world.spots[name].rules)
        self.busy.discard(name)
        self.memo[name] = ok
        return ok


def reachable(received_ids, slot_data) -> list:
    """Archipelago ids of the locations the received items reach (the seed's own rules)."""
    world = _World(slot_data or {})
    # rules.py's own helpers swapped while it runs: the endgame requirements come worked out in the slot data,
    # and add_rule collects onto the stand-in spots (Archipelago's, when it's there, wants its real locations)
    saved = rules.requirement, rules.add_rule
    rules.requirement = lambda choice, chapters, hearts: choice.kind_count
    rules.add_rule = lambda spot, rule, combine="and": spot.rules.append(rule)
    try:
        rules.set_rules(world)
    finally:
        rules.requirement, rules.add_rule = saved
    counts = {}
    for code in received_ids:
        name = ITEM_NAMES.get(int(code))
        if name:
            counts[name] = counts.get(name, 0) + 1
    state = _State(world, counts)
    return [location_id(i) for i, location in enumerate(LOCATION_LIST) if state.can_reach(location.name)]
