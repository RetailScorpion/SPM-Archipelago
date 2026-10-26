"""Client side of the mod's mailbox (mod/include/mailbox.h). Keep the layout in sync with it."""

import struct
import time

import dolphin_memory_engine as dme

MAGIC = b"SPMAP_MAILBOX_v1"
VERSION = 5
RECV_SIZE = 16
SENT_SIZE = 32

MEM1_START = 0x80000000
MEM1_SIZE = 0x01800000
SCAN_CHUNK = 0x100000

OFF_VERSION = 0x10
OFF_MOD_STATE = 0x14
OFF_RECV_COUNT = 0x18
OFF_RECV_HANDLED = 0x1C
OFF_RECV_ITEMS = 0x20
OFF_SENT_COUNT = 0x40
OFF_SENT_ACKED = 0x44
OFF_SENT_LOCATIONS = 0x48
OFF_STORY_STATE = 0xC8
OFF_WARP_REQUEST = 0xCC
OFF_WARP_MAP = 0xD0
OFF_WARP_DOOR = 0xE0
OFF_DEBUG_FLAGS = 0x100
OFF_PLACEHOLDER_ITEM = 0x104
DEBUG_JOINS_GIVE_NOTHING = 0x1
DEBUG_JOINS_GIVE_PLACEHOLDER = 0x2
STORY_SLOTS_GSW = 1792  # mod/include/savedata.h AP_GSW_STORY_SLOTS
GSW_OFFSET = 0x544

STORY_SLOT_COUNT = 9
GP_PTR = 0x8056D198  # US 1.0: pointer to SpmarioGlobals
GSW0_OFFSET = 0x140

STATE_NAMES = {0: "boot", 1: "title", 2: "in game"}


class MailboxError(Exception):
    pass


class Mailbox:
    def __init__(self):
        self.addr = None

    def connect(self, game_id: bytes = b"R8PE01"):
        """Hooks Dolphin and finds the mailbox. Raises MailboxError if anything is missing."""
        if not dme.is_hooked():
            dme.hook()
        if not dme.is_hooked():
            raise MailboxError("Dolphin isn't running, or no game is loaded")
        try:
            found_id = dme.read_bytes(MEM1_START, 6)
            if found_id != game_id:
                raise MailboxError(f"Wrong game in Dolphin: {found_id!r}, expected {game_id!r}")
            candidates = self._scan()
            # the magic string also exists in the mod's constant data; the real mailbox has a version after it
            versions = {addr: dme.read_word(addr + OFF_VERSION) for addr in candidates}
        except RuntimeError as e:
            # Dolphin closed or stopped the game mid-read
            dme.un_hook()
            raise MailboxError(f"Lost connection to Dolphin ({e})") from e
        if not candidates:
            raise MailboxError("Mod mailbox not found; is the mod loaded?")
        matching = [addr for addr, v in versions.items() if v == VERSION]
        if not matching:
            raise MailboxError(f"Mod mailbox version {sorted(set(versions.values()))}, client expects {VERSION}")
        self.addr = matching[0]

    def _scan(self) -> list[int]:
        """Every word-aligned address of MAGIC in MEM1."""
        found = []
        # chunks overlap by len(MAGIC) so a match can't straddle two reads
        for start in range(MEM1_START, MEM1_START + MEM1_SIZE, SCAN_CHUNK):
            size = min(SCAN_CHUNK + len(MAGIC), MEM1_START + MEM1_SIZE - start)
            chunk = dme.read_bytes(start, size)
            pos = chunk.find(MAGIC)
            while pos != -1:
                if (start + pos) % 4 == 0 and pos < SCAN_CHUNK:
                    found.append(start + pos)
                pos = chunk.find(MAGIC, pos + 1)
        return found

    def mod_state(self) -> int:
        return dme.read_word(self.addr + OFF_MOD_STATE)

    def give(self, item_id: int):
        """Queues an item for the mod to give. Waits if the queue is full."""
        while True:
            count = dme.read_word(self.addr + OFF_RECV_COUNT)
            handled = dme.read_word(self.addr + OFF_RECV_HANDLED)
            if count - handled < RECV_SIZE:
                break
            time.sleep(0.05)
        slot = self.addr + OFF_RECV_ITEMS + (count % RECV_SIZE) * 2
        dme.write_bytes(slot, struct.pack(">H", item_id))
        dme.write_word(self.addr + OFF_RECV_COUNT, count + 1)

    def story_state(self) -> tuple[int, list[int], int]:
        """(context, slots, live story value). Context: -1 none, 0 hub, 1-8 chapter."""
        ptr = dme.read_word(self.addr + OFF_STORY_STATE)
        raw = dme.read_bytes(ptr, 4 * (1 + STORY_SLOT_COUNT))
        context, *slots = struct.unpack(f">{1 + STORY_SLOT_COUNT}i", raw)
        live = struct.unpack(">i", dme.read_bytes(dme.read_word(GP_PTR) + GSW0_OFFSET, 4))[0]
        return context, slots, live

    def warp(self, map_name: str, door: str = ""):
        """Debug: change to a map (by a door, or the map's default spawn if empty)."""
        dme.write_bytes(self.addr + OFF_WARP_MAP, map_name.encode()[:15].ljust(16, b"\0"))
        dme.write_bytes(self.addr + OFF_WARP_DOOR, door.encode()[:31].ljust(32, b"\0"))
        dme.write_word(self.addr + OFF_WARP_REQUEST, 1)

    def set_debug_flag(self, flag: int, on: bool):
        flags = dme.read_word(self.addr + OFF_DEBUG_FLAGS)
        dme.write_word(self.addr + OFF_DEBUG_FLAGS, (flags | flag) if on else (flags & ~flag))

    def set_placeholder_item(self, item_id: int):
        """Debug: the item Pixl joins give with DEBUG_JOINS_GIVE_PLACEHOLDER (0 = Gold Bar)."""
        dme.write_word(self.addr + OFF_PLACEHOLDER_ITEM, item_id)

    def set_chapter_value(self, chapter: int, value: int):
        """Debug: set a chapter's story value. Writes the live value too if that chapter is the
        current one (the mod copies the live value into the slot every frame)."""
        gp = dme.read_word(GP_PTR)
        dme.write_bytes(gp + GSW_OFFSET + STORY_SLOTS_GSW + 2 * (chapter - 1), struct.pack(">H", value))
        if self.story_state()[0] == chapter:
            dme.write_word(gp + GSW0_OFFSET, value)

    def poll_sent(self) -> list[int]:
        """Returns location ids reported since the last call, and acknowledges them."""
        count = dme.read_word(self.addr + OFF_SENT_COUNT)
        acked = dme.read_word(self.addr + OFF_SENT_ACKED)
        locations = []
        for i in range(acked, count):
            slot = self.addr + OFF_SENT_LOCATIONS + (i % SENT_SIZE) * 4
            locations.append(dme.read_word(slot))
        if count != acked:
            dme.write_word(self.addr + OFF_SENT_ACKED, count)
        return locations
