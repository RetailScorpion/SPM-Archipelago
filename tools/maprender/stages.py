"""Rooms the map shows in stages, as the story or a save flag moves on (user, 2026-11-04).

Each room's first stage is the room's own render (rooms/<room>.png); every later stage is rendered as a
variant (rooms/<room>_s<N>.png, see render.py) from the same map files and framing, so pins, doors and
Mario's marker line up in all of them. The viewer shows the last stage the running game has reached.

A stage is reached when its condition holds:
  ('story', N)  the room's chapter's story value is N or more (GSW(0) inside the chapter; the mod keeps
                each chapter's own value in the save, which tools/maprender/serve.py reports)
  ('flag', N)   save flag GSWF(N) is set
  ('lswf', N)   level save flag LSWF(N) is set, while Mario is in the room's own level (LSWF is per level;
                elsewhere the room shows its first image)
show / hide name the room's model parts drawn or left out from that stage on (later stages build on
earlier ones); hide also takes the names of objects the scripts spawn (gn2_02's Cudge block ham_blk). The numbers come from each room's own scripts: a door the init script hides while
"IF_SMALL GSW(0), N" is shown from story value N. Doors Tippi reveals are registered with 0x80247edc
under the same kind of check (he2_09: doa_02 below 24).

A room can instead pick one image from a level save byte ('choice': ('lsw', N), with 'options' {value: change}):
each option is rendered as <room>_c<value>; its change can also 'move' parts (map units) and say which doors
the viewer shows there ('doors_hidden') and how far they've moved ('doors_moved', map units). 'init' names the
level save flag (LSWF) the room's script sets once it has given the byte its first value, 'default' the value
before that; the byte is only read while Mario is in the room's own level (LSW is per level).

Stage 0 (before any condition) also takes show / hide, for parts the renderer would otherwise get wrong:
Tippi's hidden doors are drawn by the renderer unless hidden here, since it doesn't know Tippi's system.
"""

# chapter of a room, from its name (he = 1, mi = 2, ... ls = 8)
CHAPTERS = {'he': 1, 'mi': 2, 'ta': 3, 'sp': 4, 'gn': 5, 'wa': 6, 'an': 7, 'ls': 8}


def chapter_of(room):
    return CHAPTERS.get(room[:2])


def tippi_door(part, story):
    """A door Tippi reveals: hidden until the story value it sets when found."""
    return {'base': {'hide': [part]}, 'stages': [(('story', story), {'show': [part]})]}


def later_door(part, story):
    """A door the room's script hides until the story gets there. Hidden at stage 0 here too: the
    renderer only leaves a part out when no script in the room shows it again (ta2_01's puzzle does)."""
    return {'base': {'hide': [part]}, 'stages': [(('story', story), {'show': [part]})]}


def ladder(part, flag):
    """A ladder a bean block grows: hidden until its flag is set."""
    return {'base': {'hide': [part]}, 'stages': [(('flag', flag), {'show': [part]})]}


STAGES = {
    # ---- Doors Tippi reveals (registered hidden with 0x80247edc while the story value is below N) ----
    # 1-1's Bestovius's house door (he1_01 ie_doa_02, from 14) faces sideways: edge-on in the map, nothing to switch
    'he2_09': tippi_door('doa_02', 24),      # 1-2: the door to Thoreau's room
    'sp4_16': tippi_door('doa_02', 167),     # 4-4
    'an2_02': tippi_door('doa_02', 309),     # 7-2
    'an2_05': tippi_door('doa_02', 317),
    'an2_08': tippi_door('doa_02', 324),
    'ls4_13': tippi_door('doa_02', 407),     # 8-4

    # ---- Doors that appear later (the init script hides them while the story value is below N) ----
    'mi1_08': later_door('doa_03', 70),      # 2-1
    'mi1_10': later_door('doa_02', 74),
    'ta2_01': later_door('doa_01', 109),     # 3-2: opened by the Thudley stakes (that sets 109)
    # 3-3: the stake b_05 raises the lift idy_2 when pounded (flag 819; user, 2026-11-05): drawn down from then
    'ta3_01': {'stages': [(('flag', 819), {'stakes_down': ['b_05']})]},
    'ta3_02': later_door('doa_02', 115),     # 3-3
    'gn4_15': later_door('doa_02', 218),     # 5-4
    'ls3_05': later_door('doa_02', 391),     # 8-3
    'ls3_06': later_door('doa_02', 392),
    'ls3_07': later_door('doa_03', 393),
    # 7-1: below 305 the door is hidden; from 305 it shows and its animated stand-in goes
    'an1_04': {'stages': [(('story', 305), {'show': ['doa_02'], 'hide': ['anm_doa']})]},
    # 3-4: the King's door swaps for its open version once flag 804 is set
    'ta4_04': {'base': {'show': ['King_doa'], 'hide': ['King_doa_1']},
               'stages': [(('flag', 804), {'show': ['King_doa_1'], 'hide': ['King_doa']})]},

    # ---- Ladders a bean block grows (the script hides each until its flag is set; user, 2026-11-04) ----
    # he2_02 has two, each with its own flag: hashigo_B (619) and hashigo_A (620), in either order
    'he2_02': {'switches': [(('flag', 619), {'show': ['hashigo_B']}), (('flag', 620), {'show': ['hashigo_A']})]},
    'mi2_01': ladder('hashigo', 701),     # 2-2
    # 2-3: this ladder grows by an animation, drawn at its end once grown (user, 2026-11-05)
    'mi3_01': {'base': {'hide': ['hashigo']},
               'stages': [(('flag', 705), {'show': ['hashigo'], 'anim_end': ['hashigo']})]},
    'ta1_01': ladder('hashigo_A', 831),   # 3-1
    # 5-2: the ladder a bean block grows (flag 1008); and the Cudge block ham_blk over pipe dokan_04
    # (the pipe to gn2_05 sinks under it), until it's broken: story 193 raises the pipe (user, 2026-11-05)
    'gn2_02': {'base': {'hide': ['hashigo', 'dokan_04']},
               'switches': [(('flag', 1008), {'show': ['hashigo']}),
                            (('story', 193), {'show': ['dokan_04'], 'hide': ['ham_blk']})]},
    'gn3_06': ladder('hashigo', 1021),    # 5-3
    'gn3_15': ladder('hashigo', 1046),

    # ---- Flipside / Flopside's blue pipes: each pipe's model shows once its pair's flag is set (the towns'
    # shared scripts 0xe94a0 / 0xe9568 / 0xe95d8 / 0xe96a8 hide them while it isn't; user, 2026-11-05).
    # 533: Flipside Tower <-> its sewer, 534: Flipside Tower <-> Flopside Tower, 535: Flopside Tower <-> its sewer
    'mac_02': {'switches': [(('flag', 533), {'show': ['dokan_05']}), (('flag', 534), {'show': ['dokan_04']})]},
    'mac_05': {'switches': [(('flag', 533), {'show': ['dokan_04']})]},
    'mac_12': {'switches': [(('flag', 534), {'show': ['dokan_05']}), (('flag', 535), {'show': ['dokan_04']})]},
    'mac_15': {'switches': [(('flag', 535), {'show': ['dokan_04']})]},

    # ---- 7-1's water room (an1_06): the switch drains it and fills it again (LSWF(1): 1 drained) ----
    # Its init script plays the filling animations (anm_in, anm_sui_a_2/b_2) or the draining ones (anm_out,
    # anm_sui_a_1/b_1) to their end, and hides the water's parts on_a to on_c while drained. Drained, no water
    # is drawn at all (user, 2026-11-12): every part with the water texture is left out ('hide_textures';
    # the sunk water bodies, the waterfall and the ripples would otherwise show). 'anim_skip'
    # leaves out the other state's animations, which move the same parts
    # The ladder its bean block grows (flag 1205) is the room's other switch
    'an1_06': {'base': {'show': ['on_a', 'on_b', 'on_c'], 'hide': ['hashigo'],
                        'anim_skip': ['anm_out', 'anm_sui_a_1', 'anm_sui_b_1']},
               'switches': [(('flag', 1205), {'show': ['hashigo']}),
                            (('lswf', 1), {'hide': ['on_a', 'on_b', 'on_c'], 'hide_textures': ['an1_06_mizu_1'],
                                           'anim_skip': ['anm_in', 'anm_sui_a_2', 'anm_sui_b_2'],
                                           'anim_unskip': ['anm_out', 'anm_sui_a_1', 'anm_sui_b_1']})]},

    # ---- 1-2's bridges (he2_03): none before the quiz; hasi_N_01 is the built bridge ----
    'he2_03': {'base': {'hide': ['hasi_1', 'hasi_1_01', 'hasi_2', 'hasi_2_01']},
               'stages': [(('story', 22), {'show': ['hasi_1_01']}),
                          (('story', 27), {'show': ['hasi_2_01']})]},

    # ---- 4-3's gates: the pillars go up, then the doors (sp3_01, sp3_02) ----
    'sp3_01': {'base': {'hide': ['doa_01', 'hasira']},
               'stages': [(('story', 152), {'show': ['hasira']}),
                          (('story', 153), {'show': ['doa_01']})]},
    'sp3_02': {'base': {'hide': ['doa_01', 'hasira']},
               'stages': [(('story', 154), {'show': ['hasira']}),
                          (('story', 157), {'show': ['doa_01']})]},

    # ---- 8-2's foyer (ls2_03): one pipe, moved to one of five spots (user, 2026-11-05). LSW(1) is its slot
    # (0-4, first set to 2); the init script moves the pipe model ('dokan') to marker dokan_05 ... dokan_01
    # (x 20 ... -20) and switches on only entrance dokan_<slot + 1> ----
    'ls2_03': {'choice': ('lsw', 1), 'init': 1, 'default': 2,
               'options': {k: {'move': {'dokan': [20 - 10 * k, 0, 0]},
                               'doors_hidden': [f'dokan_{n}' for n in range(1, 6) if n != k + 1],
                               'doors_moved': {f'dokan_{k + 1}': [20 - 10 * k, 0, 0]}}
                           for k in range(5)}},

    # ---- 7-4's rainbow bridge (an4_03): hidden while Chapter 7 is below 350, when the three orbs are in
    # place (user, 2026-11-05) ----
    'an4_03': {'base': {'hide': ['nizi_2']}, 'stages': [(('story', 350), {'show': ['nizi_2']})]},

    # ---- 3-1's Bowser room: the castle, then the arena walls during the fight, then neither ----
    'ta1_04': {'base': {'show': ['toride_3'], 'hide': ['kabe']},
               'stages': [(('story', 103), {'show': ['kabe'], 'hide': ['toride_3']}),
                          (('story', 104), {'hide': ['kabe']})]},
}


def stage_parts(room):
    """[(variant name, condition or None, show set, hide set)] for each stage of a room, the room's own
    name first. Each stage's sets include the earlier stages' changes."""
    spec = STAGES[room]
    show, hide = set(spec.get('base', {}).get('show', ())), set(spec.get('base', {}).get('hide', ()))
    anims = set(spec.get('base', {}).get('anim_end', ()))
    skip = set(spec.get('base', {}).get('anim_skip', ()))
    ANIM_SKIP[room] = set(skip)
    out = [(room, None, set(show), set(hide))]
    if 'choice' in spec:  # one image per value of a level save byte (<room>_c<value>)
        ANIM_END[room] = set(anims)
        for value, change in spec['options'].items():
            s = (show | set(change.get('show', ()))) - set(change.get('hide', ()))
            h = (hide | set(change.get('hide', ()))) - set(change.get('show', ()))
            out.append((f'{room}_c{value}', ('choice', value), s, h))
            ANIM_END[f'{room}_c{value}'] = set(anims) | set(change.get('anim_end', ()))
            MOVE[f'{room}_c{value}'] = dict(change.get('move', {}))
        return out
    if 'switches' in spec:
        # independent switches: an image for every combination reached (<room>_m<bits>, bit i = switch i)
        ANIM_END[room] = set(anims)
        switches = spec['switches']
        for mask in range(1, 1 << len(switches)):
            s, h, a, k, tx = set(show), set(hide), set(anims), set(skip), set()
            for i, (cond, change) in enumerate(switches):
                if mask >> i & 1:
                    s |= set(change.get('show', ())); h -= set(change.get('show', ()))
                    h |= set(change.get('hide', ())); s -= set(change.get('hide', ()))
                    a |= set(change.get('anim_end', ()))
                    k = (k - set(change.get('anim_unskip', ()))) | set(change.get('anim_skip', ()))
                    tx |= set(change.get('hide_textures', ()))
            out.append((f'{room}_m{mask}', ('mask', mask), s, h))
            ANIM_END[f'{room}_m{mask}'] = a
            ANIM_SKIP[f'{room}_m{mask}'] = k
            HIDE_TEXTURES[f'{room}_m{mask}'] = tx
        return out
    ANIM_END[room] = set(anims)
    for i, (cond, change) in enumerate(spec['stages'], 1):
        for part in change.get('show', ()):
            show.add(part)
            hide.discard(part)
        for part in change.get('hide', ()):
            hide.add(part)
            show.discard(part)
        anims |= set(change.get('anim_end', ()))
        stakes = STAKES.get(f'{room}_s{i - 1}' if i > 1 else room, set()) | set(change.get('stakes_down', ()))
        out.append((f'{room}_s{i}', cond, set(show), set(hide)))
        ANIM_END[f'{room}_s{i}'] = set(anims)
        STAKES[f'{room}_s{i}'] = stakes
    return out


# animations each image draws at their end ('anim_end': a ladder that grows), filled in by stage_parts
ANIM_END = {}
# animations each image leaves out though the script plays them ('anim_skip': the other state's), by stage_parts
ANIM_SKIP = {}
# textures whose parts an image leaves out ('hide_textures': an1_06's water, drained), by stage_parts
HIDE_TEXTURES = {}
# stakes each image draws pounded down (stages.py 'stakes_down'), filled in by stage_parts
STAKES = {}
# parts each image draws moved ({part: [dx, dy, dz]} in map units), filled in by stage_parts
MOVE = {}


def variants():
    """{variant name: base room} for every later stage (render.py VARIANTS)."""
    return {name: room for room in STAGES for name, cond, _, _ in stage_parts(room)[1:]}


def force_parts():
    """{room or variant: {'show': [...], 'hide': [...]}} for render_blender: what each image draws."""
    return {name: {'show': sorted(show), 'hide': sorted(hide), 'anim_end': sorted(ANIM_END.get(name, ())),
                   'anim_skip': sorted(ANIM_SKIP.get(name, ())),
                   'hide_textures': sorted(HIDE_TEXTURES.get(name, ())),
                   'move': MOVE.get(name, {}), 'stakes_down': sorted(STAKES.get(name, ()))}
            for room in STAGES for name, _, show, hide in stage_parts(room)}


def viewer_states():
    """{room: {'chapter': N, 'stages': [[kind, value, image], ...]}} for the viewer (overview.py)."""
    out = {}
    for room in STAGES:
        spec = STAGES[room]
        if 'choice' in spec:  # the image (and doors) for the byte's value
            out[room] = {'chapter': chapter_of(room), 'stages': [], 'choice': list(spec['choice']),
                         'init': spec.get('init'), 'default': spec.get('default'),
                         'images': {value: f'rooms/{room}_c{value}.png' for value in spec['options']},
                         'doors': {value: {'hidden': change.get('doors_hidden', []),
                                           'moved': change.get('doors_moved', {})}
                                   for value, change in spec['options'].items()}}
            continue
        if 'switches' in STAGES[room]:  # the image for the combination of switches reached
            out[room] = {'chapter': chapter_of(room), 'stages': [],
                         'switches': [list(cond) for cond, _ in STAGES[room]['switches']],
                         'images': {cond[1]: f'rooms/{name}.png' for name, cond, _, _ in stage_parts(room)[1:]}}
            continue
        out[room] = {'chapter': chapter_of(room),
                     'stages': [[cond[0], cond[1], f'rooms/{name}.png']
                                for name, cond, _, _ in stage_parts(room)[1:]]}
    return out
