"""Gives the tools in this folder the Archipelago client's game connection (apworld/spm/game_link.py),
so both use the same mailbox layout. It makes that file importable from here, then re-exports it."""

import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "apworld", "spm"))

from game_link import *  # noqa: E402,F401,F403
from game_link import Mailbox, MailboxError  # noqa: E402,F401
