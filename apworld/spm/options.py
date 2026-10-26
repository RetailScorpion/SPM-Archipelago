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


@dataclass
class SPMOptions(PerGameCommonOptions):
    shuffle_pixls: ShufflePixls
    shuffle_optional_pixls: ShuffleOptionalPixls
    shuffle_key_items: ShuffleKeyItems
    shuffle_overworld_items: ShuffleOverworldItems
    shuffle_catch_cards: ShuffleCatchCards
