from dataclasses import dataclass

from Options import Choice, DefaultOnToggle, OptionSet, PerGameCommonOptions, Range, Toggle


class ShufflePixls(DefaultOnToggle):
    """Shuffle the eight story Pixls (Thoreau to Dottie). Off: each joins at its vanilla spot."""
    display_name = "Shuffle Pixls"


class ShuffleOptionalPixls(Toggle):
    """Shuffle Barry, Dashell and Piccolo. Off: each joins at its vanilla spot."""
    display_name = "Shuffle Optional Pixls"


class ShuffleKeyItems(DefaultOnToggle):
    """Shuffle the key items whose spots the mod handles. Off: they stay at their vanilla spots."""
    display_name = "Shuffle Key Items"


class ShuffleOverworldItems(DefaultOnToggle):
    """Shuffle items in chests and lying around in levels and hubs. Off: they stay vanilla."""
    display_name = "Shuffle Overworld Items"


class ShuffleCatchCards(Toggle):
    """Shuffle catch cards found in chests or lying around. Off: they stay vanilla."""
    display_name = "Shuffle Catch Cards"


class RandomizeCharacters(Toggle):
    """Shuffle Peach, Luigi and Bowser. Their join scenes become locations: Peach in Flipside Tower
    (entered from Merlon's house once Chapter 1 is finished), Luigi in the Underwhere (7-1),
    Bowser after his fight in 3-1. Off: each scene gives its own character."""
    display_name = "Randomize Characters"


class SkipCutscenes(DefaultOnToggle):
    """Skip the opening and the Count Bleck scenes between chapters (plus the Peach and Luigi
    intermissions after Chapters 1 and 2). A new file skips Merlon's scenes too: it starts in
    Flipside Tower with the Return Pipe. Off: watch the story as in vanilla."""
    display_name = "Skip Cutscenes"


class StartingChapterMode(Choice):
    """How the chapters whose doors are open at the start are picked. Every other chapter's door
    opens when you receive its Pure Heart, which is shuffled into the item pool.
    chosen: the chapters listed in Starting Chapters.
    random_chapters: Starting Chapter Count chapters, picked at random.
    random_amount: a random number of chapters (1 to 7), picked at random."""
    display_name = "Starting Chapter Mode"
    option_chosen = 0
    option_random_chapters = 1
    option_random_amount = 2
    default = 0


class StartingChapters(OptionSet):
    """The chapters open at the start, for Starting Chapter Mode: chosen (1 to 7; Chapter 8's door
    opens after Chapters 1-7, as in vanilla)."""
    display_name = "Starting Chapters"
    valid_keys = {f"Chapter {n}" for n in range(1, 8)}
    default = frozenset({"Chapter 1"})


class StartingChapterCount(Range):
    """How many chapters are open at the start, for Starting Chapter Mode: random_chapters."""
    display_name = "Starting Chapter Count"
    range_start = 1
    range_end = 7
    default = 1


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


@dataclass
class SPMOptions(PerGameCommonOptions):
    shuffle_pixls: ShufflePixls
    shuffle_optional_pixls: ShuffleOptionalPixls
    shuffle_key_items: ShuffleKeyItems
    shuffle_overworld_items: ShuffleOverworldItems
    shuffle_catch_cards: ShuffleCatchCards
    randomize_characters: RandomizeCharacters
    skip_cutscenes: SkipCutscenes
    starting_chapter_mode: StartingChapterMode
    starting_chapters: StartingChapters
    starting_chapter_count: StartingChapterCount
    trap_percentage: TrapPercentage
    save_block: SaveBlock
