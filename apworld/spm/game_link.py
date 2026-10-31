"""The client's connection to the game: the mod's mailbox, and reading the game's memory.

The client reads and writes the game's memory through Dolphin, using the dolphin_memory_engine
library. The mod keeps a "mailbox" there (mod/include/mailbox.h): a block of memory the two share.
This file finds it and reads and writes its fields. The layout below must match the mod's exactly:
each OFF_ number is where a field sits, in bytes from the mailbox's start.

Used by the Archipelago client and by the tools in client/ (tracker, coords, SPM Archipelago Tools).
"""

import json
import os
import struct
import time

import dolphin_memory_engine as dme

# ---- The mailbox's layout (mod/include/mailbox.h) ----

MAGIC = b"SPMAP_MAILBOX_v1"  # the text the client searches memory for
VERSION = 12                 # the mod and client must agree on this
RECV_SIZE = 16               # items waiting to be given
SENT_SIZE = 32               # locations waiting (developer tools only)

# The Wii's main memory, where the mailbox is searched for, one piece at a time
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
OFF_JOIN_ITEMS = 0x108
JOIN_ITEM_OTHER_WORLD = 0xFFFF
OFF_RELF_BASE = 0x120
OFF_SAVED_RECV = 0x124
OFF_LOC_NAMES = 0x128
OFF_LOC_NAMES_MAX = 0x12C
OFF_PLAY_CUTSCENES = 0x130
OFF_GIFT_ITEMS = 0x134
OFF_STARTING_HEARTS = 0x144
OFF_GAME_FRAMES = 0x148
GIFT_FLAG_BASE = 7168 + 16  # the first story gift's save flag (save_data.h AP_GSWF_GIFT_BASE)
RECV_ALREADY_GIVEN = 0x8000  # marks an item the game already gave (the mod only counts it)
NO_SAVE = 0xFFFFFFFF         # savedRecvCount when no save is loaded
LOC_NAME_SIZE = 0x40  # one entry of the names list (archipelago_item.h LocName): 2 bytes flag,
                      # 2 bytes kind, 36 bytes item name, 24 bytes player name
LOC_KIND_OTHER_WORLD, LOC_KIND_TRAP, LOC_KIND_OTHER_WORLD_FILLER = 0, 1, 2  # archipelago_item.h LocKind
DEBUG_JOINS_GIVE_NOTHING = 0x1
DEBUG_JOINS_GIVE_PLACEHOLDER = 0x2

STATE_BOOT, STATE_TITLE, STATE_IN_GAME = 0, 1, 2  # mailbox.h ModState
STATE_NAMES = {STATE_BOOT: "boot", STATE_TITLE: "title", STATE_IN_GAME: "in game"}

# ---- The game's own memory (US 1.0) ----

GP_PTR = 0x8056D198   # where the game keeps the address of its main state (SpmarioGlobals)
GSW0_OFFSET = 0x140   # the story value, in that state
GSWF_OFFSET = 0x144   # the save flags: flag n is bit (n & 31) of 4-byte word (n >> 5)
GSW_OFFSET = 0x544    # the save bytes
STORY_SLOTS_GSW = 1792  # each chapter's own story value (save_data.h AP_GSW_STORY_SLOTS)
STORY_SLOT_COUNT = 9

# ---- The file shared with the tracker ----

# The locations the server has as checked, kept by the client for the tracker and coords windows, so a
# location checked and then lost to a reset (or a reload without saving) still shows as checked.
# Written while the client is connected; older than SERVER_CHECKS_STALE seconds means it isn't.
SERVER_CHECKS_PATH = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"),
                                  "SPM_Archipelago", "server_checks.json")
SERVER_CHECKS_HEARTBEAT = 5
SERVER_CHECKS_STALE = 15


def write_server_checks(seed: str, slot: str, flags):
    """Saves the save flags of the locations the server has as checked (client side)."""
    os.makedirs(os.path.dirname(SERVER_CHECKS_PATH), exist_ok=True)
    temp = SERVER_CHECKS_PATH + ".tmp"
    with open(temp, "w") as f:
        json.dump({"updated": time.time(), "seed": seed, "slot": slot, "flags": sorted(flags)}, f)
    os.replace(temp, SERVER_CHECKS_PATH)


def read_server_checks() -> set[int]:
    """The save flags of the locations the server has as checked, or nothing if no client is connected."""
    try:
        with open(SERVER_CHECKS_PATH) as f:
            data = json.load(f)
        if time.time() - data["updated"] > SERVER_CHECKS_STALE:
            return set()
        return set(data["flags"])
    except (OSError, ValueError, KeyError):
        return set()


# ---- The mailbox ----

class MailboxError(Exception):
    """Something is missing: Dolphin, the game, or the mod."""


class Mailbox:
    """The mod's mailbox in the running game. connect() first, then read and write its fields."""

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
            # The text also sits in the mod's own data; the real mailbox has a version number after it
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
        """Every address of MAGIC in main memory (only addresses divisible by 4 count)."""
        found = []
        # The pieces overlap by len(MAGIC), so a match can't be split between two reads
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
        """STATE_BOOT, STATE_TITLE or STATE_IN_GAME."""
        return dme.read_word(self.addr + OFF_MOD_STATE)

    # ---- Used by the Archipelago client ----

    def relf_base(self) -> int:
        """Where the game's map script file (relF.rel) starts in memory."""
        return dme.read_word(self.addr + OFF_RELF_BASE)

    def saved_recv_count(self) -> int:
        """Items from the server the loaded save has handled, or NO_SAVE."""
        return dme.read_word(self.addr + OFF_SAVED_RECV)

    def recv_pending(self) -> int:
        """How many items in the mailbox are waiting to be given."""
        return dme.read_word(self.addr + OFF_RECV_COUNT) - dme.read_word(self.addr + OFF_RECV_HANDLED)

    def queue_received(self, item_id: int, already_given: bool):
        """Queues one item from the server (never waits; check recv_pending() < RECV_SIZE first)."""
        count = dme.read_word(self.addr + OFF_RECV_COUNT)
        slot = self.addr + OFF_RECV_ITEMS + (count % RECV_SIZE) * 2
        dme.write_bytes(slot, struct.pack(">H", item_id | (RECV_ALREADY_GIVEN if already_given else 0)))
        dme.write_word(self.addr + OFF_RECV_COUNT, count + 1)

    def patch_relf(self, offset: int, original: int, value: int, also=()) -> bool:
        """Writes a word at a relF file offset if it's the original, already the value or one of also
        (values written there before)."""
        addr = self.relf_base() + offset
        current = struct.unpack(">i", dme.read_bytes(addr, 4))[0]
        if current not in (original, value, *also):
            return False
        dme.write_bytes(addr, struct.pack(">i", value))
        return True

    def write_loc_names(self, entries):
        """entries: [(flag, item name, player name, kind)] for locations holding other worlds' items
        (kind 0, or LOC_KIND_OTHER_WORLD_FILLER for filler: the grey icon) or this world's traps (kind 1,
        LOC_KIND_TRAP)."""
        table = dme.read_word(self.addr + OFF_LOC_NAMES)
        count = dme.read_word(self.addr + OFF_LOC_NAMES_MAX)
        data = bytearray(LOC_NAME_SIZE * count)
        for i, (flag, item, player, kind) in enumerate(entries[:count]):
            struct.pack_into(">HH36s24s", data, i * LOC_NAME_SIZE, flag, kind,
                             item.encode("ascii", "replace")[:35], player.encode("ascii", "replace")[:23])
        dme.write_bytes(table, bytes(data))

    def set_gift_item(self, index: int, item_id: int):
        """What a story gift location gives (0 = vanilla, JOIN_ITEM_OTHER_WORLD = another world's item)."""
        dme.write_bytes(self.addr + OFF_GIFT_ITEMS + 2 * index, struct.pack(">H", item_id))

    def set_starting_hearts(self, hearts):
        """The Pure Hearts the seed starts with (numbers 1-8)."""
        dme.write_word(self.addr + OFF_STARTING_HEARTS, sum(1 << (n - 1) for n in hearts))

    def set_play_cutscenes(self, play: bool):
        """Whether the mod plays the story scenes it can skip."""
        dme.write_word(self.addr + OFF_PLAY_CUTSCENES, 1 if play else 0)

    def read_flags(self) -> bytes:
        """All 8192 GSWF save flags as 1024 bytes (the raw words)."""
        return dme.read_bytes(dme.read_word(GP_PTR) + GSWF_OFFSET, 1024)

    def game_frames(self) -> int:
        """Frames the mod has run in a loaded save; stops while Dolphin resets."""
        return dme.read_word(self.addr + OFF_GAME_FRAMES)

    @staticmethod
    def flag_set(flags: bytes, flag: int) -> bool:
        """Whether save flag `flag` is set, in flags read by read_flags."""
        word = struct.unpack_from(">I", flags, 4 * (flag >> 5))[0]
        return bool((word >> (flag & 31)) & 1)

    # ---- Used by the tools in client/ ----

    def give(self, item_id: int):
        """Queues an item for the mod to give. Waits if the mailbox is full."""
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
        """Debug: switches a DEBUG_ test setting on or off."""
        flags = dme.read_word(self.addr + OFF_DEBUG_FLAGS)
        dme.write_word(self.addr + OFF_DEBUG_FLAGS, (flags | flag) if on else (flags & ~flag))

    def set_placeholder_item(self, item_id: int):
        """Debug: the item Pixl joins give with DEBUG_JOINS_GIVE_PLACEHOLDER (0 = Gold Bar)."""
        dme.write_word(self.addr + OFF_PLACEHOLDER_ITEM, item_id)

    def set_join_item(self, pixl: int, item_id: int):
        """Debug: what one Pixl's join gives (0 = no override, JOIN_ITEM_OTHER_WORLD = another world's item)."""
        dme.write_bytes(self.addr + OFF_JOIN_ITEMS + 2 * (pixl - 220), struct.pack(">H", item_id))

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
