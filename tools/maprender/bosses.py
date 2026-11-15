"""Bosses on the map: each boss's own catch card, pinned in its room until it's beaten (user, 2026-11-05).

Found from the game's scripts: each boss is spawned with evt_npc_entry_from_template (its template's
tribe is the boss; npcEnemyTemplates at 0x80409BE8), and the fight's script sets the chapter's story
value once it's won (data/story_us0.json agrees on the room). Positions are the spawn's own where the
script gives numbers; where it copies a cutscene stand-in's position, the pin goes in the middle of the
room at floor height (pos None).

Not listed yet (their fights' rooms weren't traced): Dimentio's fights before Chapter 8, O'Chunks in
Chapter 5, Mimi in Chapter 6, Big Blooper and the Flopside Pit's Dark bosses.
"""

from stages import chapter_of

# (room, card item, (x, y, z) in game units or None for the room's middle, (kind, value) once beaten, label)
BOSSES = [
    ("he3_04", 453, None, ("story", 33), "O'Chunks"),
    ("he4_10", 456, (0, 0, 0), ("story", 51), "Fracktail"),
    ("mi4_06", 455, None, ("story", 91), "Mimi"),
    ("ta1_04", 458, (3695, 0, 0), ("story", 104), "Bowser"),
    ("ta4_13", 464, None, ("story", 125), "Francis"),
    ("sp4_13", 466, (100, 0, 0), ("story", 170), "Mr. L"),
    ("sp4_13", 467, (0, 1000, 0), ("story", 170), "Brobot"),
    ("gn4_16", 465, (0, 0, 0), ("story", 219), "King Croacus"),
    ("wa1_27", 468, (100, 0, 0), ("story", 286), "Brobot L-type"),
    ("an2_04", 473, None, ("story", 313), "Bowser"),
    ("an2_08", 361, (3200, 1000, 0), ("story", 324), "The Underchomp"),
    ("an4_10", 469, (100, 0, 0), ("story", 352), "Bonechill"),
    ("ls1_11", 453, (100, 0, 0), ("story", 364), "O'Chunks"),
    ("ls2_17", 455, None, ("story", 381), "Mimi"),
    ("ls3_12", 454, (100, 100, 0), ("story", 401), "Dimentio"),
    ("ls4_11", 472, (250, 100, 50), ("story", 412), "Super Dimentio"),
    ("ls4_12", 470, (100, 0, 0), ("story", 417), "Count Bleck"),
    ("dan_30", 457, None, ("flag", 409), "Wracktail"),
]

CARD_ITEMS = sorted({item for _, item, _, _, _ in BOSSES})


def viewer_bosses(rooms, pitch_of):
    """[{room, item, label, px, cond: [kind, value], chapter}] for the rooms the map has. rooms: graph
    rooms (x0, y0, ppu, width, pitch_deg, turned); positions as the viewer places Mario."""
    import projection
    out = []
    for room, item, pos, cond, label in BOSSES:
        R = rooms.get(room)
        if not R:
            continue
        R = dict(R, pitch_deg=pitch_of(R))
        # pins without a position: the middle of the room, at floor height
        px, py = projection.middle(R) if pos is None else projection.to_pixels(R, pos)
        out.append({'room': room, 'item': item, 'label': label, 'px': [px, py],
                    'cond': list(cond), 'chapter': chapter_of(room)})
    return out
