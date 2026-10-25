"""Logs the mod's per-chapter story values whenever they change (map changes and story progress).

Usage: py -3 story_watch.py [log file]
"""

import sys
import time

from mailbox import Mailbox, MailboxError
from poc_client import format_story


def main():
    out = open(sys.argv[1], "a", buffering=1) if len(sys.argv) > 1 else sys.stdout
    mb = Mailbox()
    last = None
    while True:
        try:
            if mb.addr is None:
                mb.connect()
                out.write(f"[{time.strftime('%X')}] connected, mailbox at {mb.addr:#010x}\n")
            story = mb.story_state()
        except (MailboxError, RuntimeError) as e:
            if mb.addr is not None:
                out.write(f"[{time.strftime('%X')}] lost connection: {e}\n")
            mb.addr = None
            time.sleep(2)
            continue
        if story != last:
            out.write(f"[{time.strftime('%X')}] {format_story(*story)}\n")
            last = story
        time.sleep(0.2)


if __name__ == "__main__":
    main()
