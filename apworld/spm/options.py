from dataclasses import dataclass

from Options import DefaultOnToggle, PerGameCommonOptions, Toggle


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
    Flipside Tower with the first Pure Heart in its pillar, the Lineland door open and the Return
    Pipe. Off: watch the story as in vanilla."""
    display_name = "Skip Cutscenes"


@dataclass
class SPMOptions(PerGameCommonOptions):
    shuffle_pixls: ShufflePixls
    shuffle_optional_pixls: ShuffleOptionalPixls
    shuffle_key_items: ShuffleKeyItems
    shuffle_overworld_items: ShuffleOverworldItems
    shuffle_catch_cards: ShuffleCatchCards
    randomize_characters: RandomizeCharacters
    skip_cutscenes: SkipCutscenes
