"""The dev tools' view of the mailbox: re-exports apworld/spm/game_link.py, which the Archipelago client uses too."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "apworld", "spm"))

from game_link import *  # noqa: E402,F401,F403
from game_link import Mailbox, MailboxError  # noqa: E402,F401
