"""Bosses on the map: each boss's model, pinned in its room until it's beaten (user, 2026-11-05; the model
instead of its catch card, user 2026-11-16).

Each boss is drawn as its own model from disc/DATA/files/a, front on in its idle pose (render_npcs.py), from
its tribe's model name (npcTribes at 0x803FC290: animPoseName, matched by the catch card each tribe carries).
Bosses the game builds from several models get their most telling one: Brobot's body for the L-type too, King Croacus's face, and one of the Underchomp's
heads. The 3D Mimi spider is drawn as Mimi. Fracktail and Wracktail are put together from their parts (head,
neck, arms, wings, legs and plain body segments) as in their intro, the body rising out of the ground and
curling over like a backwards lowercase r (user, 2026-11-16; dragon_picture). Super Dimentio is put together
from his head, ruffed torso and boots (user, 2026-11-16; super_dimentio_picture).

Some models carry parts only some fights have (user, 2026-11-16): the sprout on O'Chunks's head (group
Kinome, the Floro sprout he wears in his Chapter 5 fights, which have no pin yet) is left out of his 1-3 and
8-1 pictures, and Bowser never has the sprout or his helmet (Sp_boushi).

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

# Each boss's picture (by its label): (picture name, model, groups left out). Pictures go to icons/boss_<name>.png
NO_SPROUT = ('Kinome|all',)
MODELS = {"O'Chunks": ('e_ddtas_nosprout', 'e_ddtas', NO_SPROUT),
          'Fracktail': ('e_zun_intro', 'dragon:e_zun', ()), 'Mimi': ('e_2D_manera', 'e_2D_manera', ()),
          'Bowser': ('p_wii_kuppa_plain', 'p_wii_kuppa', NO_SPROUT + ('Sp_boushi|all',)),
          'Francis': ('e_kamere', 'e_kamere', ()), 'Mr. L': ('e_dark_luigi', 'e_dark_luigi', ()),
          'Brobot': ('e_lui_robo', 'e_lui_robo', ()), 'King Croacus': ('e_bara_f', 'e_bara_f', ()),
          'Brobot L-type': ('e_lui_robo', 'e_lui_robo', ()), 'The Underchomp': ('e_wanwan_3', 'e_wanwan_3', ()),
          'Bonechill': ('e_shinigami', 'e_shinigami', ()), 'Dimentio': ('e_dmen', 'e_dmen', ()),
          'Super Dimentio': ('e_DmenL_whole', 'super_dimentio', ()), 'Count Bleck': ('e_jigen', 'e_jigen', ()),
          'Wracktail': ('e_W_zun_intro', 'dragon:e_W_zun', ())}
# A dragon's body from the ground up to its head (part models <prefix>_<part>): plain segments, hind legs, wings, arms, neck
DRAGON_BODY = ['nomal', 'foot', 'nomal', 'nomal', 'hane', 'nomal', 'arm', 'neck']


# Super Dimentio's parts: head (green hat and mask), torso (the ruff and the spikes below it), boot, fists
SUPER_DIMENTIO_PARTS = ['e_DmenL_f', 'e_DmenL_k', 'e_DmenL_s', 'e_DmenL_lh', 'e_DmenL_rh']


def super_dimentio_picture(raw_dir):
    """Super Dimentio from his part pictures in raw_dir: the head, its chin under the torso's ruff, a boot under
    each side of the torso, toes outward, and a fist floating at each side (user, 2026-11-16)."""
    import os
    from PIL import Image, ImageOps

    def load(part):
        im = Image.open(os.path.join(raw_dir, f'{part}.png')).convert('RGBA')
        return im.crop(im.getchannel('A').getbbox())
    head, torso, boot, fist_l, fist_r = (load(p) for p in SUPER_DIMENTIO_PARTS)
    c = Image.new('RGBA', (torso.width + 2 * fist_l.width + 400, head.height + torso.height + boot.height + 400))
    cx = c.width // 2
    top = head.height - int(head.height * 0.12)  # the torso's top: its ruff over the chin
    feet = top + int(torso.height * 0.85)      # the boots hang just below the spikes
    gap = int(torso.width * 0.05)
    c.alpha_composite(boot, (cx - boot.width - gap, feet))  # as modelled, the toe points left
    c.alpha_composite(ImageOps.mirror(boot), (cx + gap, feet))
    c.alpha_composite(head, (cx - head.width // 2, 0))
    c.alpha_composite(torso, (cx - torso.width // 2, top))
    # the fists beside the torso's upper half, their knuckle (the small hexagon) outward
    fy = top + int(torso.height * 0.25)
    c.alpha_composite(ImageOps.mirror(fist_l), (cx - torso.width // 2 - int(fist_l.width * 0.75), fy))
    c.alpha_composite(fist_r, (cx + torso.width // 2 - int(fist_r.width * 0.25), fy))
    return c.crop(c.getchannel('A').getbbox())


def dragon_picture(raw_dir, prefix):
    """Fracktail or Wracktail as in its intro: its body segments chained up out of the ground and curling over
    to the left at the top, each turned along the curve, the head at the end facing left; cut flat at the ground. From the part pictures
    render_npcs.py made in raw_dir (<prefix>_<part>.png)."""
    import math
    import os
    from PIL import Image, ImageOps

    def load(part):
        im = Image.open(os.path.join(raw_dir, f'{prefix}_{part}.png')).convert('RGBA')
        return im.crop(im.getchannel('A').getbbox())
    segs, head = [load(p) for p in DRAGON_BODY], load('hed')
    step = segs[0].width * 0.8  # joint to joint: a segment's length less its bracket, which covers the joint
    n = len(segs)
    # Like a backwards lowercase r (user, 2026-11-16): nearly straight up out of the ground, curling over at the
    # top. Laid out curling right (the body parts as drawn), then the whole picture is mirrored
    pts = [(0.0, 0.0)]
    for i in range(n):
        # each segment's direction, anticlockwise from right: up out of the ground, ending level (not curling down)
        a = math.radians(95 - 95 * (i / (n - 1)) ** 1.8)
        x, y = pts[-1]
        pts.append((x + step * math.cos(a), y - step * math.sin(a)))
    height = -min(y for _, y in pts)
    size = int(height + step * 8)
    canvas = Image.new('RGBA', (size, size))
    ox, oy = size // 2, size - int(step * 2)  # where the body leaves the ground

    def paste(img, anchor, angle, at):  # img turned angle degrees anticlockwise, its anchor point put at at
        r = img.rotate(angle, expand=True, resample=Image.BICUBIC)
        ax, ay = anchor[0] - img.width / 2, anchor[1] - img.height / 2
        a = math.radians(angle)
        rx, ry = ax * math.cos(a) + ay * math.sin(a), -ax * math.sin(a) + ay * math.cos(a)
        canvas.alpha_composite(r, (round(at[0] - r.width / 2 - rx), round(at[1] - r.height / 2 - ry)))
    # from the head down, so each segment's bracket (its head end) lies over the joint
    for i in reversed(range(n)):
        (x0, y0), (x1, y1) = pts[i], pts[i + 1]
        paste(segs[i], (0, segs[i].height * 0.45), math.degrees(math.atan2(y0 - y1, x1 - x0)), (ox + x0, oy + y0))
    # the head faces left as modelled (jaw on the left, crest at the back): drawn mirrored here so the final mirror
    # puts it back, the back of the head (15% across as mirrored here, 57% down: user, 2026-11-16) on the neck
    paste(ImageOps.mirror(head), (head.width * 0.15, head.height * 0.57), 0, (ox + pts[n][0], oy + pts[n][1]))
    canvas = canvas.crop((0, 0, size, oy))  # into the ground
    return ImageOps.mirror(canvas.crop(canvas.getchannel('A').getbbox()))
PICTURE_SIZE = 160  # the pictures' longer side, in pixels (the viewer draws them smaller)


def make_pictures(icons_dir, raw_dir, blender):
    """icons/boss_<model>.png for every boss model not drawn yet: rendered by Blender (render_npcs.py) into
    raw_dir, then cut to the model and shrunk to PICTURE_SIZE."""
    import os
    import subprocess
    from PIL import Image
    todo = sorted({spec for spec in MODELS.values() if not os.path.exists(os.path.join(icons_dir, f'boss_{spec[0]}.png'))})
    if not todo:
        return
    here = os.path.dirname(os.path.abspath(__file__))
    pictures = []  # what render_npcs.py draws: the models, and the dragons' parts
    for name, model, hide in todo:
        if model.startswith('dragon:'):
            pictures += [f'{model[7:]}_{p}' for p in sorted(set(DRAGON_BODY) | {'hed'})]
        elif model == 'super_dimentio':
            pictures += SUPER_DIMENTIO_PARTS
        else:
            pictures.append(f'{name}={model}:{",".join(hide)}')
    subprocess.run([blender, '-b', '--factory-startup', '--python', os.path.join(here, 'render_npcs.py'), '--',
                    raw_dir, *pictures], check=True, capture_output=True)
    os.makedirs(icons_dir, exist_ok=True)
    for m, model, _ in todo:
        if model.startswith('dragon:'):
            im = dragon_picture(raw_dir, model[7:])
        elif model == 'super_dimentio':
            im = super_dimentio_picture(raw_dir)
        else:
            im = Image.open(os.path.join(raw_dir, f'{m}.png')).convert('RGBA')
            im = im.crop(im.getchannel('A').getbbox())
        im.thumbnail((PICTURE_SIZE, PICTURE_SIZE), Image.LANCZOS)
        im.save(os.path.join(icons_dir, f'boss_{m}.png'), optimize=True)


def viewer_bosses(rooms, pitch_of):
    """[{room, item, model, label, px, cond: [kind, value], chapter}] for the rooms the map has. rooms: graph
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
        out.append({'room': room, 'item': item, 'model': MODELS[label][0] if label in MODELS else None,
                    'label': label, 'px': [px, py],
                    'cond': list(cond), 'chapter': chapter_of(room)})
    return out
