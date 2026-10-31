"""Reading the running game's memory, for the helper windows (tracker, coords, SPM Archipelago Tools).

These read the game through Dolphin with the dolphin_memory_engine library. Only the game is needed,
not the mod's mailbox, so they work with any version of the mod.
"""

import struct

import dolphin_memory_engine as dme

from mailbox import GP_PTR, GSWF_OFFSET

GAME_ID = b"R8PE01"      # Super Paper Mario (USA)
MAP_NAME_OFFSET = 0x44   # the current map's name, in the game's main state (SpmarioGlobals)
MARIO_WORK = 0x804CD458  # Mario's state (US 1.0)
MARIO_POSITION = 0x5C    # his position in it: x, y, z as 4-byte decimal numbers


def connect_dolphin():
    """Connects to Dolphin. Raises RuntimeError if Dolphin isn't running Super Paper Mario (USA)."""
    if not dme.is_hooked():
        dme.hook()
    if not dme.is_hooked():
        raise RuntimeError("Dolphin isn't running, or no game is loaded")
    if dme.read_bytes(0x80000000, 6) != GAME_ID:
        raise RuntimeError("Super Paper Mario (USA) isn't running")


def read_flags() -> bytes:
    """All 8192 save flags (GSWF), as 1024 bytes."""
    return dme.read_bytes(dme.read_word(GP_PTR) + GSWF_OFFSET, 1024)


def current_map() -> str:
    """The name of the map Mario is in (like "he1_01")."""
    return dme.read_bytes(dme.read_word(GP_PTR) + MAP_NAME_OFFSET, 32).split(b"\0")[0].decode(errors="replace")


def mario_position() -> tuple[float, float, float]:
    """Mario's position: (x left to right, y height, z depth)."""
    return struct.unpack(">3f", dme.read_bytes(MARIO_WORK + MARIO_POSITION, 12))
