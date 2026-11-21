"""The logic: what each location needs before it can be reached.

Two kinds of rule:

1. What the game enforces (always on, even with logic: no_logic):
   - a Chapter 1-7 location needs that chapter's Pure Heart, unless its door starts open
   - Flopside (the town and its Pit) needs Boomer, Fleep and Mario (the blue pipe between the towers
     only works with all three); Chapter 8's door is in Flopside, so Chapter 8 and the goal need them too
   - Chapter 8 needs castle_requirement, Peach and Luigi; past 8-1 also Bowser (its party splits)
   - the goal needs all of that, final_requirement, and Slim past 8-3

2. The WIP logic (logic: wip_logic), mapped out by playing:
   - ROOM_NEEDS: whole rooms past a gap, pipe or lock (by room-name prefix)
   - LOCATION_NEEDS: single locations
   - LOCATION_AFTER: a location that needs another one reachable first
   - the section rule: a location in section 2-4 of a chapter needs the previous section's Star Block
     room reachable (SECTION_END_ROOMS, SECTION_END_NEEDS)
   - TATTLE_NEEDS: tattle checks whose enemy hides somewhere that needs more than its room
   - a few special cases in set_rules: Brobot L-type, Peach's Fire Burst, 7-3's big cloud, the
     Underwhere's side lock

A rule is a list of alternatives, each a list of items: [["Peach"], ["Luigi"]] means Peach or Luigi.
Starting as Mario, he's no item, so he's dropped from every rule (set_rules' in_level).

Endgame requirements: Pure Heart 8 (vanilla), a number of Chapters 1-7 finished, a number of Pure
Hearts held, or (Chapter 8's door only) always open. A chapter counts as finishable once its door is
open. Held hearts include the starting chapters' ones, unless those are items to find too
(world.starting_hearts_in_pool).
"""

from worlds.generic.Rules import add_rule

from .items import HEART_8, TOWER_CHAPTERS, VICTORY_ITEM
from .data import TATTLE_MAPS
from .items import FIRE_BURST_ITEM, MARIO_ITEM, TATL_ITEM, WALLET_ITEM
from .locations import LOCATION_LIST, chapter_of, in_flopside

VICTORY_LOCATION = "Chapter 8 complete"

# The Pixls needed to reach Flopside from Flipside (user, 2026-11-03)
FLOPSIDE_PIXLS = ["Boomer", "Fleep"]
# Getting to Flopside at all: those Pixls and Mario (user, 2026-11-13: every Flopside check needs Mario
# to get to normally). Starting as Mario he's no item, so set_rules drops him then
FLOPSIDE_NEEDS = [FLOPSIDE_PIXLS + [MARIO_ITEM]]
# What Chapter 8 needs besides its door (user, 2026-11-05): Luigi (every Chapter 8 check is behind him) and
# Peach (8-1's checks in ls1_06 and ls1_07, its key included). Pixls aren't needed so far
CHAPTER_8_NEEDS = ["Luigi", "Peach"]
# What Chapter 8 past 8-1 needs as well: Bowser (user, 2026-11-05: nothing after 8-1's checks without him)
PAST_8_1_NEEDS = ["Bowser"]
# What the final battle needs as well: Slim (user, 2026-11-05: everything after 8-3's Dimentio fight)
PAST_8_3_NEEDS = ["Slim"]

# ---- In-level logic (mapped out by playing; DESIGN.md §4 "Known requirements") ----
# A requirement is a list of alternatives, each a list of items that together meet it: [["Thoreau"],
# ["Peach"]] is "Thoreau or Peach", [["Water Tablet", "Stone Tablet", "Fire Tablet"]] all three. An item
# can be (name, count) for several copies: [[("Door Key (40)", 2)]]. Rules that
# apply to one location add up with the room rules it falls under. Where a note left something to check
# (Dashell crossing Peach's gaps, say), the stricter reading is used: seeds stay beatable, only less varied.
# Rooms that can only be left on foot with a Pixl found there (Carrie's, Dottie's, Luigi's) aren't rules:
# the Return Pipe gets out, and a rule needing the room's own Pixl couldn't be met with Pixls unshuffled.

# Keys that open any lock of their kind in their chapter (user, 2026-11-12; the mod's key_groups): a
# requirement counts the group's keys held, ((names...), n). Locks in a row need one more key each; a
# lock off the main line (the Underwhere's, to Luigi) could take a key first, so the main line counts
# that one too, and that lock counts the main-line locks that could have taken one (set_rules)
FORT_KEYS = ("Fort Key (19)", "Fort Key (20)", "Fort Key (21)")
DIMENSION_KEYS = ("Dimension Key (27)", "Dimension Key (28)", "Dimension Key (29)")
CAVE_KEYS = ("Cave Key (33)", "Cave Key (34)")
UNDERWHERE_KEYS = ("Door Key (37)", "Door Key (38)", "Door Key (40)", "Door Key (49)")

UNDERWHERE_KEY_COUNT = 5  # Door Key (40) twice
# The Underwhere's main-line locks in order (an2_03's lock2, an2_06's lock2, an4_01's, an4_02's), and its
# side lock, an1_03's, to Luigi's rooms (vanilla only has it before Chapter 7; the mod brings it back,
# user 2026-11-12, now that its key's spot, Queen Jaydes in an1_04, is reachable)
UNDERWHERE_MAIN_LOCK_ROOMS = ("an2_03", "an2_06", "an4_01", "an4_02")
UNDERWHERE_SIDE_ROOMS = ("an1_06", "an1_07", "an1_09")


def underwhere_keys(main_locks: int) -> list:
    """Rooms past the nth main-line Underwhere lock: n keys, and one more for the side lock (all five at most)."""
    return [[(UNDERWHERE_KEYS, min(main_locks + 1, UNDERWHERE_KEY_COUNT))]]


# Abilities with more than one source
HURTS_SPIKED = [["Bowser"], ["Carrie"], ["Boomer"], ["Cudge"], ["Barry"]]  # damages spiked enemies
# Bowser's 7-2 fight: hurting spiked enemies, but not with his own fire, and not Barry either: he does
# 1 damage and Bowser has at least 1 defence here (user, 2026-11-13). With bowser_with_items, items
# alone are enough, so the requirement is dropped (set_rules)
BEATS_BOWSER_7_2 = [["Carrie"], ["Boomer"], ["Cudge"]]
# Locations that need a wallet holding at least 100 coins: with progressive_wallet you start able to carry
# 99, so one Progressive Wallet. The Ancient Clue (unshuffled) costs more than 99, and Fleep joins takes it
# (user, 2026-11-16)
NEEDS_100_COIN_WALLET = ("4-2 Planet Blobule - Fleep joins (sp2_01)",)
KNOCKS_FRUIT = [["Thudley"], ["Cudge"]]  # knocks fruit off the trees (7-3's apples)
BREAKS_BLOCKS = [["Cudge"], ["Thudley"], ["Boomer"]]                       # breaks a wall of blocks
BLOCKS_AND_SLIM_OR_DOTTIE = [b + [p] for b in BREAKS_BLOCKS for p in ("Slim", "Dottie")]  # 4-2 (sp2_04, sp2_08)
CROSSES_1_2_GAP = [["Thoreau"], ["Peach"]]  # Watchitt's bridge (Thoreau gets it built) or Peach's float
CROSSES_5_1_GAP = [["Peach"], ["Luigi"]]  # 5-1's gap: Peach's float or Luigi's high jump (user, 2026-11-14)
# Flipside's Peach check (her revival: Saffron's Spicy Soup) needs a Fire Burst: one found in the game,
# or bought at Flipside's shop (reachable in 2D) with coins. Coins come from the chapters with ordinary
# levels (Sammer's Kingdom has none) or, once Mario can flip, from Flipside (user, 2026-11-08: none
# there in 2D). Starting as Mario, Flipside always has them, so the check needs nothing
PEACH_JOIN = "Flipside/Flopside - Peach joins (mac_01)"
COIN_CHAPTERS = (1, 2, 3, 4, 5, 7)  # 7 only with Luigi or Mario (an1_02's pipe)

# Brobot L-type (Chapter 6's last fight, wa1_27) needs Boomer's bombs (user, 2026-11-08); Fleep too with
# brobot_with_fleep. Everything after him needs the same: Chapter 6's Pure Heart and the post-game duels
BEATS_BROBOT = [["Boomer"]]
BEATS_BROBOT_WITH_FLEEP = [["Boomer"], ["Fleep"]]
AFTER_BROBOT = ("6-1 Sammer's Kingdom - Brobot L-type beaten (wa1_27)", "6-1 Sammer's Kingdom - Pure Heart 7 (wa1_27)")
AFTER_BROBOT_MAPS = ("(wa2_", "(wa3_", "(wa4_")  # the post-game duels' groups 2-4

# Gets past 3-1's pipe that's too tall to get over as Bowser (user, 2026-11-08): Peach floats over it,
# Luigi jumps over it, Mario goes around it in 3D
GETS_OVER_TALL_PIPE = [[MARIO_ITEM], ["Luigi"], ["Peach"]]
# Gets past the pipe early in 7-1 (an1_02, x about -800) that Bowser and Peach can't (user, 2026-11-13):
# Luigi jumps over it, Mario goes around it in 3D
GETS_PAST_7_1_PIPE = [[MARIO_ITEM], ["Luigi"]]
# Mario's flip to 3D. Starting as Mario he's no item, so set_rules drops him from requirements then
FLIPS_TO_3D = [[MARIO_ITEM]]


def room_of(location_name: str) -> str:
    """The room in a location's name: "1-2 Mount Lineland - Red & Green Card (he2_05)" -> "he2_05"."""
    inside = location_name.rsplit("(", 1)[-1]
    return inside.split(")", 1)[0]


# Rooms (by name prefix) and what every location in them needs (user's notes, 2026-10-26 onward)
ROOM_NEEDS = [
    # 1-2 to 1-4 need Mario's flip to 3D (user, 2026-11-08, starting as Peach: every Chapter 1 check
    # not yet collected needs 3D, all of 1-2 to 1-4 among them)
    (("he2_", "he3_", "he4_"), FLIPS_TO_3D),
    # 1-1: nothing past the first room (he1_01, its side room he1_03) and Bestovius's house (he1_06) without
    # Mario (user, 2026-11-14: tattles in he1_02/04/05 had shown up in logic as Luigi)
    (("he1_02", "he1_04", "he1_05"), FLIPS_TO_3D),
    # Flipside (mac_01-mac_09): every check but Peach's join needs Mario's flip to 3D (user, 2026-11-08,
    # starting as Luigi: the Cooking Disk 2 chest needs it too). mac_01 holds Peach, so its chest is a
    # location rule below; the rooms here also cover tattles in Flipside
    (("mac_02", "mac_03", "mac_04", "mac_05", "mac_06", "mac_07", "mac_08", "mac_09"), FLIPS_TO_3D),
    # Past Watchitt's bridge in Yold Town: all of 1-3 and 1-4. Every 1-2 location is before it (user,
    # 2026-11-05: the Red & Green Card in the builder's house, he2_05, needs nothing; so the Paratroopa
    # Card, he2_01)
    (("he3_", "he4_"), CROSSES_1_2_GAP),
    # 2-1: a gap right at the start (mi1_01) that needs Peach's float
    (("mi",), [["Peach"]]),
    # 4-2: sp2_01's door doa2_l (to sp2_03 and every room past it) is behind a wall of blocks. The
    # Star Block's room (sp2_02) isn't
    (("sp2_03", "sp2_04", "sp2_05", "sp2_06", "sp2_07", "sp2_08", "sp2_09", "sp2_10"), BREAKS_BLOCKS),
    # 4-3 and 4-4 are reached through 4-2's Star Block room (sp2_02, Door Key 26), not past the wall of
    # blocks (user, 2026-11-08: only Fleep's check and the rooms past the wall need a block breaker)
    (("sp3_", "sp4_"), [["Door Key (26)"]]),
    # 4-3 past sp3_01's middle (x about 1820), and all of 4-4, need Fleep (user, 2026-11-14). sp3_01's
    # Shooting Star (x -1025) is before that point
    (("sp3_02", "sp3_03", "sp3_04", "sp3_05", "sp3_06", "sp3_07", "sp4_"), [["Fleep"]]),
    # 3-1: a pipe very early on too tall for Bowser to get over (user, 2026-11-08, starting as Bowser). Which
    # of ta1_01's own checks are before it isn't mapped, so all of 3-1 (and through the section rule, all of
    # Chapter 3) needs someone who gets past it
    (("ta1_",), GETS_OVER_TALL_PIPE),
    # Chapter 5: everything is past a gap, except Cooking Disk 6 in its first room (below): Peach floats
    # over it, Luigi's high jump reaches it (user, 2026-11-14)
    (("gn2_", "gn3_", "gn4_"), CROSSES_5_1_GAP),
    (("gn1_02", "gn1_03", "gn1_04", "gn1_05", "gn1_06"), CROSSES_5_1_GAP),
    # Chapter 5 past Cudge's spot: 5-2's last room (gn2_05), all of 5-3 and 5-4 need Cudge's hammer (user,
    # 2026-11-05: seed6 put Cudge on Dottie's spot in 5-4). The Fire Tablet (gn2_04) can't: vanilla's Cudge
    # joins only once all three tablets are placed
    (("gn2_05", "gn3_", "gn4_"), [["Cudge"]]),
    # 8-4: everything after 8-3's Dimentio fight (ls3_12) needs Slim (user, 2026-11-05); the final battle
    # too (PAST_8_3_NEEDS)
    (("ls4_",), [["Slim"]]),
    # Chapter 6: the duels (and the post-game gauntlet) have spiked enemies stomping can't beat
    (("wa",), HURTS_SPIKED),
    # 7-1: every check is past an1_02's pipe (and so is the rest of Chapter 7, through the section rule)
    (("an1_",), GETS_PAST_7_1_PIPE),
    # 7-1: Luigi's room (an1_07) needs Mario's flip: Luigi's join (user, 2026-11-08: seed10 put Mario there,
    # starting as Luigi), and the Long-Last Shake and Dry Bones Card too (user, 2026-11-16)
    (("an1_07",), FLIPS_TO_3D),
    # Chapter 7's locks (each a door; the keys are shuffled, user, 2026-11-05):
    # 7-1: an1_03's lock, on the door to an1_06 (and on to Luigi's room, an1_07, and an1_09): its rule is in
    # set_rules (it depends on how many main-line locks could take a key first). 7-1's Star Block (an1_05)
    # is through an1_04, not behind it
    # 7-2: an2_03's lock2, on the door to an2_05: the rest of 7-2, and so 7-3 and 7-4
    (("an2_05", "an2_06", "an2_07", "an2_08", "an2_09", "an2_10", "an3_", "an4_"), underwhere_keys(1)),
    # 7-2: an2_06's lock2, on the door to an2_08: 7-2's Star Block (an2_09), and so 7-3 and 7-4
    (("an2_08", "an2_09", "an3_", "an4_"), underwhere_keys(2)),
    # 7-4: can't be reached without Peach and Luigi (user, 2026-11-13; maybe Bowser too, unchecked)
    (("an4_",), [["Peach", "Luigi"]]),
    # 7-4: an4_01's lock, on the door to an4_02: everything past sector 1 and sector 2 (an4_04)
    (("an4_02", "an4_03", "an4_05", "an4_06", "an4_07", "an4_08", "an4_09", "an4_10", "an4_12"),
     underwhere_keys(3)),
    # 7-4: an4_02's lock, on the door to an4_05: sectors 5-8 (the Yellow and Red Orbs' spots, an4_06 and
    # an4_07) and an4_12
    (("an4_05", "an4_06", "an4_07", "an4_08", "an4_12"), underwhere_keys(4)),
    # 7-4: an4_03's rainbow to an4_09 and an4_10 (Pure Heart 8) takes the three orbs (shuffled too)
    (("an4_09", "an4_10"), [["Blue Orb", "Yellow Orb", "Red Orb"]]),
    # Every other chapter's locks (user, 2026-11-07: all keys shuffled). The rooms behind each lock's door
    # are the ones the map graph (build/maps/graph.json) only reaches through it. Where one key opens
    # several locks in a row, the rooms past the nth need n of them.
    # 1-4: the Ruins Key's three locks, he4_03 (to he4_05), he4_05 (to he4_12) and he4_07 (to he4_08)
    (("he4_05", "he4_06", "he4_07", "he4_08", "he4_09", "he4_10", "he4_11", "he4_12"), [["Ruins Key"]]),
    (("he4_07", "he4_08", "he4_09", "he4_10", "he4_11", "he4_12"), [[("Ruins Key", 2)]]),
    (("he4_08", "he4_09", "he4_10", "he4_11"), [[("Ruins Key", 3)]]),
    # 2-1: mi1_01's lock, on the door to mi1_08: the rest of Chapter 2 but mi1_01 and mi1_05 (the key's
    # room). User, 2026-11-08: every Chapter 2 check but the key itself is past that door, 2-2 to 2-4 included
    (("mi1_02", "mi1_03", "mi1_04", "mi1_06", "mi1_07", "mi1_08", "mi1_09", "mi1_10", "mi1_11", "mi2_", "mi3_", "mi4_"),
     [["Door Key (17)"]]),
    # 2-2: mi2_03's lock, to mi2_08
    (("mi2_08",), [["House Key"]]),
    # 3-4: ta4_01's lock, to ta4_03: everything past the first two rooms
    (("ta4_03", "ta4_04", "ta4_05", "ta4_06", "ta4_07", "ta4_08", "ta4_09", "ta4_10", "ta4_11", "ta4_12", "ta4_13",
      "ta4_14", "ta4_15"), [[(FORT_KEYS, 1)]]),
    # 4-2: sp2_01's lock, to sp2_02 (the Star Block's room)
    (("sp2_02",), [["Door Key (26)"]]),
    # 4-4: the Whoa Zone's in-room doors aren't mapped well enough to work out each lock's rooms, so this
    # follows the story's order: sp4_03's lock (Dimension Key 27, from sp4_04/05's box) comes before
    # everything past sp4_05; sp4_07's (28) before sp4_16 and on; sp4_11's (29) before Brobot (sp4_13)
    (("sp4_06", "sp4_07", "sp4_08", "sp4_09", "sp4_10", "sp4_11", "sp4_12", "sp4_13", "sp4_14", "sp4_15", "sp4_16"),
     [[(DIMENSION_KEYS, 1)]]),
    (("sp4_10", "sp4_11", "sp4_12", "sp4_13", "sp4_14", "sp4_16"), [[(DIMENSION_KEYS, 2)]]),
    (("sp4_13", "sp4_14"), [[(DIMENSION_KEYS, 3)]]),
    # 5-3: gn3_03's lock, to gn3_13 (and gn3_09, gn3_14)
    (("gn3_09", "gn3_13", "gn3_14"), [[(CAVE_KEYS, 1)]]),
    # 5-4: gn4_02's lock (Cave Key 34) is on the door to Dottie's room (a location rule below); the door to
    # gn4_10 isn't locked (user, 2026-11-12). gn4_10's card door, to gn4_11
    (("gn4_11", "gn4_12", "gn4_13"), [["Card Key"]]),
    # Chapter 8: seven locks for seven Castle Bleck Keys, in order (a section starts only once the one
    # before it is finished, so each section starts with the keys its predecessors used)
    (("ls1_08", "ls1_09", "ls1_10", "ls1_11", "ls1_12", "ls2_", "ls3_", "ls4_"), [["Castle Bleck Key"]]),
    (("ls2_07", "ls2_08", "ls2_09", "ls2_10", "ls2_11", "ls2_12", "ls2_13", "ls2_14", "ls2_15", "ls2_16", "ls2_17",
      "ls2_18", "ls3_", "ls4_"), [[("Castle Bleck Key", 2)]]),
    (("ls2_13", "ls2_14", "ls2_15", "ls2_16", "ls2_17", "ls2_18", "ls3_", "ls4_"), [[("Castle Bleck Key", 3)]]),
    (("ls3_04", "ls3_05", "ls3_06", "ls3_07", "ls3_08", "ls3_09", "ls3_10", "ls3_11", "ls3_12", "ls3_13", "ls4_"),
     [[("Castle Bleck Key", 4)]]),
    (("ls3_10", "ls3_11", "ls3_12", "ls3_13", "ls4_"), [[("Castle Bleck Key", 5)]]),
    (("ls3_12", "ls3_13", "ls4_"), [[("Castle Bleck Key", 6)]]),
    (("ls4_06", "ls4_07", "ls4_08", "ls4_09", "ls4_10", "ls4_11", "ls4_12"), [[("Castle Bleck Key", 7)]]),
]

# Items single locations need, from their level's own locks (in-level logic, as it's mapped out).
# 3-4's Pure Heart (Francis, ta4_13) is behind the King's door in ta4_04: it opens once both key holes
# are filled (flags 802 and 803), each taking either Fort Key (user, 2026-11-05: a seed put Fort Key (21)
# on that heart)
LOCATION_NEEDS = {
    # 1-1: these need Mario's flip to 3D (user, 2026-11-08, starting as Peach; she got the Squiglet Card,
    # he1_03's Shroom Shake and Bestovius's lesson without it)
    "1-1 Lineland Road - Fire Burst (he1_01)": FLIPS_TO_3D,
    "1-1 Lineland Road - Goomba Card (he1_02)": FLIPS_TO_3D,
    "1-1 Lineland Road - Koopa Troopa Card (he1_05)": FLIPS_TO_3D,
    "1-1 Lineland Road - Shell Shock (he1_06)": FLIPS_TO_3D,
    "1-1 Lineland Road - Shroom Shake (he1_06)": FLIPS_TO_3D,
    # (with keys grouped: the lock in ta4_01 and the two key holes, three Fort Keys)
    "3-4 Fort Francis - Pure Heart 4 (ta4_13)": [[(FORT_KEYS, 3)]],
    # 3-1: the Catch Card SP needs Peach's float (user, 2026-11-08; bouncing off an enemy with precise
    # timing also works: for glitched logic, DESIGN.md "Known requirements")
    "3-1 The Bitlands - Catch Card SP (ta1_02)": [["Peach"]],
    # 5-3: these need Mario's flip (user, 2026-11-09, starting as Peach)
    "5-3 Floro Caverns - Shroom Shake (gn3_05)": FLIPS_TO_3D,
    "5-3 Floro Caverns - Spania Card (gn3_08)": FLIPS_TO_3D,
    "5-3 Floro Caverns - Cave Key (gn3_01)": FLIPS_TO_3D,
    # 5-4: these need Mario's flip (user, 2026-11-12, starting as Peach)
    "5-4 Floro Caverns - Cave Key (gn4_06)": FLIPS_TO_3D,
    "5-4 Floro Caverns - POW Block (gn4_09)": FLIPS_TO_3D,
    "5-4 Floro Caverns - Pure Heart 6 (gn4_16)": FLIPS_TO_3D,
    # 5-4: Dottie's room (gn4_03) is reached from gn4_02 through its small door (Dottie) or its locked
    # door (Cave Key 34) (user, 2026-11-12)
    "5-4 Floro Caverns - Dottie joins (gn4_03)": [["Dottie"], [(CAVE_KEYS, 2)]],
    # 5-4: the Card Key's spot needs Dottie (user, 2026-11-12)
    "5-4 Floro Caverns - Card Key (gn4_10)": [["Dottie"]],
    # 5-2: these need Mario's flip (user, 2026-11-09, starting as Peach)
    "5-2 Gap of Crag - Primordial Fruit (gn2_01)": FLIPS_TO_3D,
    "5-2 Gap of Crag - Mighty Tonic (gn2_02)": FLIPS_TO_3D,
    "5-2 Gap of Crag - Stone Tablet (gn2_03)": FLIPS_TO_3D,
    "5-2 Gap of Crag - Stone Buzzy Card (gn2_05)": FLIPS_TO_3D,
    # 5-1: gn1_02's Moon Cleft Card needs Mario's flip (user, 2026-11-09)
    "5-1 Downtown of Crag - Moon Cleft Card (gn1_02)": FLIPS_TO_3D,
    # 3-1: ta1_06's Catch Card SP needs Mario's flip (user, 2026-11-09)
    "3-1 The Bitlands - Catch Card SP (ta1_06)": FLIPS_TO_3D,
    # 3-1: ta1_07's chest (the Magikoopa Card) needs Mario's flip (user, 2026-11-08)
    "3-1 The Bitlands - Magikoopa Card (ta1_07)": FLIPS_TO_3D,
    # 4-2: sp2_01's Ultra Shroom Shake needs Mario's flip, its Door Key spot needs Fleep (user, 2026-11-09,
    # starting as Luigi)
    "4-2 Planet Blobule - Ultra Shroom Shake (sp2_01)": FLIPS_TO_3D,
    "4-2 Planet Blobule - Door Key (sp2_01)": [["Fleep"]],
    # 4-1: the checks the user couldn't get starting as Peach need Mario's flip (user, 2026-11-08)
    "4-1 Outer Space - Foton Card (sp1_02)": FLIPS_TO_3D,
    "4-1 Outer Space - Catch Card SP (sp1_05)": FLIPS_TO_3D,
    # Flipside's lower level, by Peach: its chest needs Mario's flip (user, 2026-11-08; her join doesn't)
    # and a block breaker: Boomer, Thudley or Cudge (user, 2026-11-13; was Boomer alone)
    "Flipside/Flopside - Cooking Disk 2 (mac_01)": [[MARIO_ITEM] + items for items in BREAKS_BLOCKS],
    # Flipside's InterChet Card (mac_07) needs Flopside reached, Mario's flip and Cudge (user, 2026-11-12;
    # the flip from mac_07's room rule)
    "Flipside/Flopside - The InterChet Card (mac_07)": [FLOPSIDE_PIXLS + [MARIO_ITEM, "Cudge"]],
    # 3-1: beating Bowser means hurting spiked enemies; his own fire can't count, he joins after
    # Boomer blows up the castle to start the fight (user, 2026-11-09); his bombs hurt spiked enemies too
    "3-1 The Bitlands - Bowser joins (ta1_04)": [["Boomer"]],
    # 3-4: Thudley presses the switch that opens Carrie's cage
    "3-4 Fort Francis - Carrie joins (ta4_12)": [["Thudley"]],
    # 4-2: the outhouse takes the Ancient Clue (sp2_08, unshuffled), which is past the wall of blocks
    # and needs Slim or Dottie as well (user, 2026-11-16); Space Food (sp2_04) needs the same (user, 2026-11-16)
    "4-2 Planet Blobule - Fleep joins (sp2_01)": BLOCKS_AND_SLIM_OR_DOTTIE,
    "4-2 Planet Blobule - Space Food (sp2_04)": BLOCKS_AND_SLIM_OR_DOTTIE,
    # 2-1: the checks besides Boomer's join and the Door Key need Boomer (user, 2026-11-13; 2-2 to 2-4
    # through SECTION_END_NEEDS)
    "2-1 Gloam Valley - Shlurp Card (mi1_04)": [["Boomer"]],
    "2-1 Gloam Valley - Swooper Card (mi1_04)": [["Boomer"]],
    "2-1 Gloam Valley - Watchitt Card (mi1_07)": [["Boomer"]],
    # 4-4: the first Dimension Key's box (sp4_04/05) needs Mario (user, 2026-11-14)
    "4-4 The Whoa Zone - Dimension Key (sp4_04)": [[MARIO_ITEM]],
    # 1-4: the Ruins Key in he4_05 needs Dottie or Thoreau (user, 2026-11-16)
    "1-4 Yold Ruins - Ruins Key (he4_05)": [["Dottie"], ["Thoreau"]],
    # 5-1: Cooking Disk 6 in the first room needs Dottie (not Peach), and Mario as well (user, 2026-11-16)
    "5-1 Downtown of Crag - Cooking Disk 6 (gn1_01)": [["Dottie", MARIO_ITEM]],
    # 5-2: Cudge's stone takes the three tablets
    "5-2 Gap of Crag - Cudge joins (gn2_02)": [["Water Tablet", "Stone Tablet", "Fire Tablet"]],
    # 5-2: the Fire Tablet's spot needs Bowser (user, 2026-11-05) and Mario (user, 2026-11-16)
    "5-2 Gap of Crag - Fire Tablet (gn2_04)": [["Bowser", MARIO_ITEM]],
    # 7-2: Bowser rejoins after his fight, which takes hurting spiked enemies (not with his own fire, not
    # Barry: user, 2026-11-13); the D-Men give their Door Key after it
    "7-2 Underwhere Road - Bowser rejoins (an2_04)": BEATS_BOWSER_7_2,
    "7-2 Underwhere Road - Door Key (an2_03)": BEATS_BOWSER_7_2,
    # 7-3: Peach rejoins once the fruit is knocked off the trees: Thudley or Cudge (user, 2026-11-05)
    "7-3 Overthere Stair - Peach rejoins (an3_06)": KNOCKS_FRUIT,
    # The Flipside Pit's floor 100: the Wracktail fight before Dashell needs Thoreau
    "Pit of 100 Trials - Dashell joins (dan_30)": [["Thoreau"]],
}


# 7-3's big bouncy cloud (an3_15) is the only way up to the room's top (the Gold Bar, y 7060) and its
# door to an3_04 (7-3's Star Block), and so to 7-4. With big_cloud_requires_peach_reunion on (vanilla)
# it starts as the cloud character, who grows into the big cloud for a Red Apple from the trees:
# Thudley or Cudge (user, 2026-11-05; the Peach reunion before it needs the same). Off: it's there from
# the start, and nothing more is needed
ABOVE_BIG_CLOUD = ["7-3 Overthere Stair - Gold Bar (an3_15)"]
ABOVE_BIG_CLOUD_ROOMS = ("an3_04", "an4_")


def above_big_cloud(location_name: str) -> bool:
    """Whether a location is only reached by 7-3's big cloud."""
    return location_name in ABOVE_BIG_CLOUD or room_of(location_name).startswith(ABOVE_BIG_CLOUD_ROOMS)


# Locations that open only once another location's part of the story is done: they need that location
# reachable (its own requirements, whatever they turn out to be)
LOCATION_AFTER = {
    # Barry (3-1) only appears once Chapter 4 is beaten (user, 2026-11-05): 4-4's Pure Heart spot is
    # Chapter 4's ending
    "3-1 The Bitlands - Barry joins (ta1_01)": "4-4 The Whoa Zone - Pure Heart 5 (sp4_14)",
    # Piccolo's house (Flopside) needs the Old House Key (user, 2026-11-13). The Piccolo chain isn't
    # shuffled (piccolo_chain is only planned), and in vanilla the key needs Chapter 1 beatable: 1-4's
    # Pure Heart spot is Chapter 1's ending. If the key ever becomes an item, this becomes a need for it
    "Flipside/Flopside - Piccolo joins (mac_19)": "1-4 Yold Ruins - Pure Heart 2 (he4_11)",
}


# The Pits of 100 Trials (dan_01 Flipside, dan_41 Flopside in TATTLE_MAPS): places to tattle only with
# pit_tattles (user, 2026-11-12)
PIT_MAP_PREFIX = "dan_"


# Tattles that need more than reaching one of the enemy's rooms: what the enemy itself is behind
TATTLE_NEEDS = {
    # Frost Piranhas (an1_02, an1_08) can't be found as Mario (user, 2026-11-13: both need Luigi)
    "Tattle - Frost Piranha": [["Luigi"]],
    # Gigabite in 7-1 (an1_11) can't be reached without Luigi (user, 2026-11-13)
    "Tattle - Gigabite": [["Luigi"]],
}


# Enemies that only appear after standing still a while: tattles only with afk_tattles (user, 2026-11-13)
AFK_TATTLES = ("Tattle - Megabite", "Tattle - Gigabite")


def tattle_rooms(options, location_name: str) -> list:
    """The rooms the logic counts for a tattle check: its enemy's rooms, the Pits only with pit_tattles.
    Empty: the seed has no such check (the Dark Dayzee, only in the Flopside Pit, without pit_tattles)."""
    rooms = TATTLE_MAPS[location_name]
    if location_name in AFK_TATTLES and not getattr(options, "afk_tattles", False):
        return []
    if not getattr(options, "pit_tattles", False):
        rooms = [room for room in rooms if not room.startswith(PIT_MAP_PREFIX)]
    return rooms


def item_held(state, player, item) -> bool:
    """Whether an item is held: a name, (name, how many copies), or ((names...), how many in all)."""
    if isinstance(item, tuple) and isinstance(item[0], tuple):
        return sum(state.count(name, player) for name in item[0]) >= item[1]
    if isinstance(item, tuple):
        return state.count(item[0], player) >= item[1]
    return state.has(item, player)


def requirement_met(state, player, alternatives) -> bool:
    """Whether any one alternative's items are all held."""
    return any(all(item_held(state, player, item) for item in items) for items in alternatives)


def location_requirements(location_name: str) -> list:
    """Every in-level requirement on a location: its room's, then its own."""
    room = room_of(location_name)
    needs = [alternatives for prefixes, alternatives in ROOM_NEEDS if room.startswith(prefixes)]
    if location_name in LOCATION_NEEDS:
        needs.append(LOCATION_NEEDS[location_name])
    return needs


def open_checks(chapter: int, held: set) -> int:
    """How many of a chapter's locations (tattles aside) need nothing in the level beyond the items in
    `held`: what a seed starting with that chapter's door open has to begin with."""
    def has(item):
        if isinstance(item, tuple) and isinstance(item[0], tuple):  # a key group: its distinct keys held
            return sum(name in held for name in item[0]) >= min(item[1], len(item[0]))
        return (item[0] if isinstance(item, tuple) else item) in held

    def met(alternatives):
        return any(all(has(item) for item in items) for items in alternatives)

    def doable(name):
        # its own needs, and (sections past the first) the section before it beatable, as set_rules has it
        if not all(met(alternatives) for alternatives in location_requirements(name)):
            return False
        if room_of(name).startswith(UNDERWHERE_SIDE_ROOMS) and not has((UNDERWHERE_KEYS, UNDERWHERE_KEY_COUNT)):
            return False  # the side lock, at its worst
        before = previous_section_end(room_of(name))
        if before is None:
            return True
        return doable(room_as_location(before)) and met(SECTION_END_NEEDS.get(before, [[]]))
    return sum(1 for location in LOCATION_LIST
               if location.kind != "tattle" and chapter_of(location.name) == chapter
               and location.name not in LOCATION_AFTER and doable(location.name))


# A map's chapter, by its name's start (the hubs and the Pit have none)
CHAPTER_PREFIXES = {"he": 1, "mi": 2, "ta": 3, "sp": 4, "gn": 5, "wa": 6, "an": 7, "ls": 8}


# Each section's Star Block room (the map graph's star_block objects, build/maps/graph.json): sections X-1
# to X-3, whose Star Block leads on to the next section. Chapter 6's duels run in order on their own
SECTION_END_ROOMS = {
    1: ("he1_05", "he2_03", "he3_08"),
    2: ("mi1_04", "mi2_08", "mi3_06"),
    3: ("ta1_04", "ta2_06", "ta3_04"),
    4: ("sp1_06", "sp2_02", "sp3_03"),
    5: ("gn1_03", "gn2_05", "gn3_09"),
    7: ("an1_05", "an2_09", "an3_04"),
    8: ("ls1_12", "ls2_18", "ls3_13"),
}
# What a section's end needs beyond its room: 3-1's Star Block appears once Bowser's fight is won, and
# the fight only starts once Boomer has blown up the castle (user, 2026-11-09)
SECTION_END_NEEDS = {
    "ta1_04": [["Boomer"]],
    # 2-1's Star Block (mi1_04): every Chapter 2 check after Boomer's join and 2-1's Door Key needs Boomer
    # (user, 2026-11-13), so 2-2 to 2-4 need him through this
    "mi1_04": [["Boomer"]],
}


def previous_section_end(room: str) -> str | None:
    """For a room in section X-2 to X-4, the Star Block room of the section before it (user, 2026-11-08:
    every X-2, X-3 and X-4 check needs the section before it beatable)."""
    chapter = CHAPTER_PREFIXES.get(room[:2])
    if chapter not in SECTION_END_ROOMS or not room[2:3].isdigit():
        return None
    section = int(room[2])
    if section < 2 or section > 4:
        return None
    return SECTION_END_ROOMS[chapter][section - 2]


def room_as_location(room: str) -> str:
    """A name the location rules read like a location in that room: "he1_02" -> "1-1 - (he1_02)"."""
    chapter = CHAPTER_PREFIXES.get(room[:2])
    if chapter and room[2].isdigit():
        return f"{chapter}-{room[2]} - ({room})"
    return f"- ({room})"


def past_8_1(location_name: str) -> bool:
    """Whether a Chapter 8 location is in 8-2, 8-3 or 8-4."""
    return location_name[:3] in ("8-2", "8-3", "8-4")

# The requirement kinds the mod knows (mod endgame_requirements.h Kind)
ENDGAME_VANILLA, ENDGAME_CHAPTERS, ENDGAME_PURE_HEARTS, ENDGAME_OPEN = 1, 2, 3, 4


def heart_needed(chapter: int, starting_chapters: list[int]) -> str | None:
    """The Pure Heart a chapter's locations need, or None if its door is open from the start."""
    return None if chapter in starting_chapters else f"Pure Heart {chapter}"


def requirement(choice, chapters, hearts) -> list[int]:
    """A castle_requirement / final_requirement option as the mod reads it: [kind, count]."""
    if choice == choice.option_pure_heart_8:
        return [ENDGAME_VANILLA, 0]
    if getattr(choice, "option_always_open", None) == choice.value:
        return [ENDGAME_OPEN, 0]
    if choice == choice.option_chapters:
        return [ENDGAME_CHAPTERS, chapters.value]
    return [ENDGAME_PURE_HEARTS, hearts.value]


def requirement_rule(world, kind_count: list[int]):
    """A rule for a requirement: whether the player can meet it."""
    player, starting = world.player, world.starting_chapters
    closed = [f"Pure Heart {chapter}" for chapter in TOWER_CHAPTERS if chapter not in starting]
    kind, count = kind_count

    every = [f"Pure Heart {chapter}" for chapter in TOWER_CHAPTERS]

    def open_chapters(state) -> int:
        return len(starting) + sum(state.has(heart, player) for heart in closed)

    def hearts_held(state) -> int:
        if world.starting_hearts_in_pool:
            return sum(state.has(heart, player) for heart in every) + state.has(HEART_8, player)
        return open_chapters(state) + state.has(HEART_8, player)

    if kind == ENDGAME_OPEN:
        return lambda state: True
    if kind == ENDGAME_VANILLA:
        return lambda state: state.has(HEART_8, player)
    if kind == ENDGAME_CHAPTERS:
        return lambda state: open_chapters(state) >= count
    return lambda state: hearts_held(state) >= count


def set_rules(world) -> None:
    """Adds every rule to a player's world."""
    player = world.player
    castle = requirement_rule(world, requirement(world.options.castle_requirement, world.options.castle_chapters,
                                                 world.options.castle_hearts))
    final = requirement_rule(world, requirement(world.options.final_requirement, world.options.final_chapters,
                                                world.options.final_hearts))
    # Starting as Mario, he's no item to find: requirements naming him are met from the start
    starts_as_mario = world.starting_character == 0
    beats_brobot = BEATS_BROBOT_WITH_FLEEP if world.options.brobot_with_fleep else BEATS_BROBOT
    bowser_with_items = bool(getattr(world.options, "bowser_with_items", False))
    wallets = bool(getattr(getattr(world.options, "progressive_wallet", None), "value", 0))
    # logic: no_logic keeps only what the game enforces (doors, Flopside, Chapter 8's party); wip_logic
    # adds everything mapped out by playing (old seeds without the option had it)
    wip = getattr(getattr(world.options, "logic", None), "value", 1) != 0
    def in_level(alternatives) -> list:
        if not starts_as_mario:
            return alternatives
        return [[item for item in items if item != MARIO_ITEM] for items in alternatives]

    flopside_needs = in_level(FLOPSIDE_NEEDS)
    def flopside(state) -> bool:
        return requirement_met(state, player, flopside_needs)

    unknown = (set(LOCATION_NEEDS) | set(LOCATION_AFTER) | set(LOCATION_AFTER.values())) - {
        location.name for location in LOCATION_LIST}
    if unknown:
        raise ValueError(f"LOCATION_NEEDS names locations that don't exist: {sorted(unknown)}")
    def rules_for(name: str) -> list:
        """What a location (by its name) needs: a list of state -> bool, all of which must hold."""
        needs = []
        chapter = chapter_of(name)
        if chapter == 8 or in_flopside(name):
            needs.append(flopside)
        if chapter == 8:
            needs.append(castle)
            needs.append(lambda state: state.has_all(CHAPTER_8_NEEDS, player))
            if past_8_1(name):
                needs.append(lambda state: state.has_all(PAST_8_1_NEEDS, player))
        elif chapter is not None:
            heart = heart_needed(chapter, world.starting_chapters)
            if heart:
                needs.append(lambda state, heart=heart: state.has(heart, player))
        if not wip:
            return needs
        after = LOCATION_AFTER.get(name)
        if after:
            needs.append(lambda state, after=after: state.can_reach(after, "Location", player))
        # A section past the first: the one before it has to be beatable, its Star Block room reached
        # (that room's own needs, and so on back to the chapter's start)
        before = previous_section_end(room_of(name))
        if before is not None:
            end_needs = rules_for(room_as_location(before))
            if before in SECTION_END_NEEDS:
                end_needs = end_needs + [lambda state, alternatives=in_level(SECTION_END_NEEDS[before]):
                                         requirement_met(state, player, alternatives)]
            needs.append(lambda state, end_needs=end_needs: all(need(state) for need in end_needs))
        if name in AFTER_BROBOT or any(prefix in name for prefix in AFTER_BROBOT_MAPS):
            needs.append(lambda state: requirement_met(state, player, beats_brobot))
        if name == PEACH_JOIN and not starts_as_mario:
            # (heart, extra items): Chapter 7's coins are past 7-1's pipe, so they also need Luigi (user,
            # 2026-11-13; Mario is already enough on his own)
            hearts = [(heart_needed(n, world.starting_chapters), ["Luigi"] if n == 7 else [])
                      for n in COIN_CHAPTERS]
            needs.append(lambda state, hearts=hearts: state.has_any([FIRE_BURST_ITEM, MARIO_ITEM], player)
                         or any((heart is None or state.has(heart, player)) and state.has_all(extra, player)
                                for heart, extra in hearts))
        if world.options.big_cloud_requires_peach_reunion and above_big_cloud(name):
            needs.append(lambda state: requirement_met(state, player, KNOCKS_FRUIT))
        for alternatives in location_requirements(name):
            if alternatives is BEATS_BOWSER_7_2 and bowser_with_items:
                continue
            needs.append(lambda state, alternatives=in_level(alternatives): requirement_met(state, player, alternatives))
        if room_of(name).startswith(UNDERWHERE_SIDE_ROOMS):
            needs.append(underwhere_side)
        if wip and wallets and name in NEEDS_100_COIN_WALLET:
            needs.append(lambda state: state.has(WALLET_ITEM, player))
        return needs

    # The Underwhere's side lock (an1_03, to Luigi's rooms) takes a key the main-line locks haven't: with
    # n keys, the player may first have used one on every main-line lock they could reach, in order
    main_lock_rules = [rules_for(room_as_location(room)) for room in UNDERWHERE_MAIN_LOCK_ROOMS]

    def underwhere_side(state) -> bool:
        keys = sum(state.count(name, player) for name in UNDERWHERE_KEYS)
        spent = 0
        for needs in main_lock_rules:
            if spent == keys or not all(need(state) for need in needs):
                break
            spent += 1
        return keys > spent

    for location in LOCATION_LIST:
        if location.kind == "tattle":
            continue
        for need in rules_for(location.name):
            add_rule(world.get_location(location.name), need)

    # Tattle checks: Tatl, and any room the enemy is in reachable (each room checked as if it held a
    # location: its chapter's door, its level's and room's needs)
    if world.options.tattle_checks:
        for location in LOCATION_LIST:
            if location.kind != "tattle":
                continue
            places = tattle_rooms(world.options, location.name)
            if not places:
                continue  # not in this seed
            rooms = [rules_for(room_as_location(room)) for room in places]
            extra = TATTLE_NEEDS.get(location.name) if wip else None
            if extra:
                extra_need = lambda state, alternatives=in_level(extra): requirement_met(state, player, alternatives)
                rooms = [needs + [extra_need] for needs in rooms]
            add_rule(world.get_location(location.name), lambda state, rooms=rooms: state.has(TATL_ITEM, player)
                     and any(all(need(state) for need in needs) for needs in rooms))
    add_rule(world.get_location(VICTORY_LOCATION), lambda state: flopside(state) and castle(state) and final(state)
             and state.has_all(CHAPTER_8_NEEDS + PAST_8_1_NEEDS + PAST_8_3_NEEDS, player)
             # the throne room (ls4_11) is past all seven Castle Bleck locks
             and (not wip or all(requirement_met(state, player, in_level(needs))
                                 for needs in location_requirements("(ls4_11)"))))
    world.multiworld.completion_condition[player] = lambda state: state.has(VICTORY_ITEM, player)
