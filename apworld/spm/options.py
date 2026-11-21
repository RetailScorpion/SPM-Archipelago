"""The options players set in their YAML.

Each class is one option. Its text (the docstring) is what Archipelago shows on the options page and
in the YAML template; display_name is its title there. SPMOptions at the bottom lists them all, by the
name used in the YAML.
"""

from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, OptionSet, PerGameCommonOptions, Range, Toggle


class Logic(Choice):
    """Which logic places the items.
    no_logic: only the rules the game itself enforces: a chapter needs its Pure Heart (unless its door
    starts open), Flopside needs Boomer, Fleep and Mario, and Chapter 8 needs its door's requirement and
    Peach, Luigi and Bowser. Inside a chapter, items can be anywhere, so you may need something you
    can't reach yet (the Return Pipe gets you out of rooms you can't leave).
    wip_logic: the logic mapped out so far by playing. It's incomplete: a seed will probably need
    something it doesn't say."""
    display_name = "Logic"
    option_no_logic = 0
    option_wip_logic = 1
    default = 1


class ShufflePixls(Choice):
    """Shuffle the eight story Pixls (Thoreau to Dottie).
    vanilla: each joins at its vanilla spot.
    anywhere: they can be anywhere.
    among_pixls: only Pixl join spots hold them, and those hold only Pixls (with shuffle_optional_pixls
    also among_pixls, the optional Pixls and their spots join in)."""
    display_name = "Shuffle Pixls"
    option_vanilla = 0
    option_anywhere = 1
    option_among_pixls = 2
    alias_false = 0
    alias_true = 1
    default = 1


class ShuffleOptionalPixls(Choice):
    """Shuffle Barry, Dashell and Piccolo.
    vanilla: each joins at its vanilla spot.
    anywhere: they can be anywhere.
    among_pixls: only Pixl join spots hold them (see shuffle_pixls)."""
    display_name = "Shuffle Optional Pixls"
    option_vanilla = 0
    option_anywhere = 1
    option_among_pixls = 2
    alias_false = 0
    alias_true = 1
    default = 0


class ShuffleKeyItems(DefaultOnToggle):
    """Shuffle the key items whose spots the mod handles. Off: they stay at their vanilla spots."""
    display_name = "Shuffle Key Items"


class ShuffleOverworldItems(DefaultOnToggle):
    """Shuffle items in chests and lying around in levels and hubs. Off: they stay vanilla."""
    display_name = "Shuffle Overworld Items"


class ShuffleCatchCards(Toggle):
    """Shuffle catch cards found in chests or lying around. Off: they stay vanilla."""
    display_name = "Shuffle Catch Cards"


class RandomizeCharacters(Choice):
    """Shuffle Peach, Luigi and Bowser. Their join scenes are locations: Peach in Flipside (unconscious
    from the start: talk to her holding a Spicy Soup), Luigi in the Underwhere (7-1), Bowser after his
    fight in 3-1.
    vanilla: each scene gives its own character.
    anywhere: they can be anywhere.
    among_characters: only the three join scenes hold them (and Mario, with a starting_character other
    than Mario), and those hold only characters."""
    display_name = "Randomize Characters"
    option_vanilla = 0
    option_anywhere = 1
    option_among_characters = 2
    alias_false = 0
    alias_true = 1
    default = 0


class StartingCharacter(Choice):
    """Who you start as. Anyone but Mario makes Mario an item to find, in his place: the character you
    start as is yours from the start, and Mario is shuffled the way randomize_characters says (with it
    vanilla, at your starting character's own join scene).
    The spots that need Mario's flip to 3D aren't all mapped yet, so a seed may need him somewhere it
    doesn't say."""
    display_name = "Starting Character"
    option_mario = 0
    option_peach = 1
    option_bowser = 2
    option_luigi = 3
    option_random = 4
    default = 0


class TattleChecks(Toggle):
    """Adds Tatl, a Pixl to find, and a location for every kind of enemy (114; fewer without pit_tattles or afk_tattles): with Tatl as your Pixl,
    press 1, move her over an enemy with the arrow buttons and press 1 again to read its tattle and
    send that kind's check."""
    display_name = "Tattle Checks"


class PitTattles(Toggle):
    """With tattle_checks: whether the logic counts the Pits of 100 Trials as places to tattle an enemy.
    Most enemies are in the Flipside Pit too, so on, nearly every tattle can be expected from there.
    Off: the logic only expects tattles from the chapters and towns (you can still tattle in a Pit), and
    the Dark Dayzee, found only in the Flopside Pit, has no tattle check."""
    display_name = "Pit Tattles"


class AfkTattles(Toggle):
    """With tattle_checks: whether the Megabite and Gigabite get tattle checks. They only show up after
    you stand still for a while, so off (the default) leaves them out (112 tattle locations, 111
    without pit_tattles)."""
    display_name = "Megabite and Gigabite Tattles"


class ProgressiveWallet(Choice):
    """Coins need a bigger wallet, as in the Ocarina of Time randomizer: you start able to carry 99
    coins, and each Progressive Wallet you find raises it: 200, 500, then 999 (the game's own limit).
    off: no wallets; you can carry 999 coins from the start, as in vanilla.
    on: three Progressive Wallets in the item pool.
    with_infinite: a fourth one that lifts the limit (no coin cap).
    The Ancient Clue (for Fleep) costs more than 99 coins, so with wallets on, Fleep needs one."""
    display_name = "Progressive Wallet"
    option_off = 0
    option_on = 1
    option_with_infinite = 2
    default = 0


class StartingChapterMode(Choice):
    """How the chapters whose doors are open at the start are picked. Every other chapter's door
    opens when you receive its Pure Heart, which is shuffled into the item pool.
    chosen: the chapters listed in Starting Chapters.
    random_chapters: Starting Chapter Count chapters, picked at random.
    random_amount: a random number of chapters (1 to Starting Chapter Max), picked at random."""
    display_name = "Starting Chapter Mode"
    option_chosen = 0
    option_random_chapters = 1
    option_random_amount = 2
    default = 0


class StartingChapters(OptionSet):
    """The chapters open at the start, for Starting Chapter Mode: chosen (1 to 7; Chapter 8's door
    follows Castle Requirement)."""
    display_name = "Starting Chapters"
    valid_keys = {f"Chapter {n}" for n in range(1, 8)}
    default = frozenset({"Chapter 1"})


class StartingChapterCount(Range):
    """How many chapters are open at the start, for Starting Chapter Mode: random_chapters."""
    display_name = "Starting Chapter Count"
    range_start = 1
    range_end = 7
    default = 1


class StartingChapterMax(Range):
    """The most chapters that can be open at the start, for Starting Chapter Mode: random_amount."""
    display_name = "Starting Chapter Max"
    range_start = 1
    range_end = 7
    default = 7


class TrapPercentage(Range):
    """Percentage of the shuffled filler items replaced by Cursya Traps. In Super Paper Mario a trap
    looks like a key item or Pixl you haven't found; collecting it gives you nothing ("You are a
    FOOL!") and a random Cursya curse: slow, flipped controls, no jump or no skills."""
    display_name = "Trap Percentage"
    range_start = 0
    range_end = 100
    default = 10


class SaveBlock(Choice):
    """The Save Block: a key item that saves your game from the key item menu, anywhere.
    shuffle: it's somewhere in the multiworld.
    start_with: you have it from the start.
    off: there's no Save Block."""
    display_name = "Save Block"
    option_shuffle = 0
    option_start_with = 1
    option_off = 2
    default = 1


class ShufflePureHearts(DefaultOnToggle):
    """The Pure Heart each of Chapters 1-7 ends with is a location.
    On: the hearts for the chapters that start closed are anywhere in the multiworld, and those
    locations can hold anything.
    Off: each chapter's ending gives its vanilla heart (Chapter 1 gives Pure Heart 2, and so on), and
    Chapter 1 is always open at the start, so every chapter can be reached in order."""
    display_name = "Shuffle Pure Hearts"


class CastleRequirement(Choice):
    """What opens Chapter 8's door (Castle Bleck, in Flopside Tower).
    pure_heart_8: Pure Heart 8, as in vanilla (it's found in the item pool, or at Chapter 7's end with
    shuffle_pure_hearts off).
    chapters: castle_chapters of Chapters 1-7 finished.
    pure_hearts: castle_hearts Pure Hearts (the starting doors' hearts count, unless shuffle_pure_hearts
    puts them in the pool).
    always_open: open from the start."""
    display_name = "Castle Requirement"
    option_pure_heart_8 = 0
    alias_vanilla = 0
    option_chapters = 1
    option_pure_hearts = 2
    option_always_open = 3
    default = 1


class CastleChapters(Range):
    """How many of Chapters 1-7 must be finished to open Chapter 8's door (castle_requirement: chapters)."""
    display_name = "Castle Chapters"
    range_start = 1
    range_end = 7
    default = 7


class CastleHearts(Range):
    """How many Pure Hearts open Chapter 8's door (castle_requirement: pure_hearts)."""
    display_name = "Castle Hearts"
    range_start = 1
    range_end = 8
    default = 8


class FinalRequirement(Choice):
    """What the final battle needs. Count Bleck's throne room door in 8-4 is locked, and a sign beside it
    says what's left; once you meet this, you get the Throne Room Key that opens it.
    pure_heart_8: Pure Heart 8.
    chapters: final_chapters of Chapters 1-7 finished.
    pure_hearts: final_hearts Pure Hearts (the ones you start with count)."""
    display_name = "Final Requirement"
    option_pure_heart_8 = 0
    alias_vanilla = 0
    option_chapters = 1
    option_pure_hearts = 2
    default = 2


class FinalChapters(Range):
    """How many of Chapters 1-7 must be finished for the final battle (final_requirement: chapters)."""
    display_name = "Final Chapters"
    range_start = 1
    range_end = 7
    default = 7


class FinalHearts(Range):
    """How many Pure Hearts the final battle needs (final_requirement: pure_hearts)."""
    display_name = "Final Hearts"
    range_start = 1
    range_end = 8
    default = 8


class BrobotWithFleep(Toggle):
    """Brobot L-type (Chapter 6's last fight) is in the logic with Fleep as well as Boomer. Without it,
    the logic expects Boomer (user: hurting him without either is hard, and with only Fleep tedious)."""
    display_name = "Brobot L-type with Fleep"


class BowserWithItems(Toggle):
    """Bowser's fight in 7-2 Underwhere Road is in the logic with nothing but items (Thunder Rage, Fire
    Burst and the like, bought in the shops). Without it, the logic expects Carrie, Boomer or Cudge
    (user: Barry only does 1 damage and Bowser has defence there, but with a strategy items can do it)."""
    display_name = "Bowser (7-2) with Items"


class SammerGauntlet(Toggle):
    """Once Chapter 6 is finished, Sammer's Kingdom has its 100 duels, and King Sammer's seven Catch Card
    rewards after the 100th are locations (shuffled with Shuffle Catch Cards).
    Off: those locations only ever hold filler, so you never need to beat all 100 duels.
    On: they can hold anything, progression included."""
    display_name = "Sammer Gauntlet"


class BigCloudRequiresPeachReunion(DefaultOnToggle):
    """7-3's big bouncy cloud (an3_15) only appears once you've reunited with Peach (giving her a
    Black Apple, an3_06) and then given the cloud a Red Apple, as in vanilla. Off: the big cloud is
    there from the start."""
    display_name = "Big Cloud Requires Peach Reunion"


class SammerDuels(Range):
    """How many Sammer Guys you duel in each part of Sammer's Kingdom; the seed picks which ones.
    The story (20 duels): the first and the last (Rolling Thwomp) always, the rest at random; after
    Rolling Thwomp you go straight to King Sammer's Pure Heart scene.
    The post-game (100 duels, in four groups of 25): each group's last one always, the rest at random;
    the first group also always starts with its first one."""
    display_name = "Sammer Duels"
    range_start = 1
    range_end = 25
    default = 25


@dataclass
class SPMOptions(PerGameCommonOptions):
    logic: Logic
    shuffle_pixls: ShufflePixls
    shuffle_optional_pixls: ShuffleOptionalPixls
    shuffle_key_items: ShuffleKeyItems
    shuffle_overworld_items: ShuffleOverworldItems
    shuffle_catch_cards: ShuffleCatchCards
    randomize_characters: RandomizeCharacters
    starting_character: StartingCharacter
    tattle_checks: TattleChecks
    pit_tattles: PitTattles
    afk_tattles: AfkTattles
    progressive_wallet: ProgressiveWallet
    starting_chapter_mode: StartingChapterMode
    starting_chapters: StartingChapters
    starting_chapter_count: StartingChapterCount
    starting_chapter_max: StartingChapterMax
    trap_percentage: TrapPercentage
    save_block: SaveBlock
    shuffle_pure_hearts: ShufflePureHearts
    castle_requirement: CastleRequirement
    castle_chapters: CastleChapters
    castle_hearts: CastleHearts
    final_requirement: FinalRequirement
    final_chapters: FinalChapters
    final_hearts: FinalHearts
    sammer_gauntlet: SammerGauntlet
    brobot_with_fleep: BrobotWithFleep
    bowser_with_items: BowserWithItems
    sammer_duels: SammerDuels
    big_cloud_requires_peach_reunion: BigCloudRequiresPeachReunion
