"""Blender side of the room renderer. Run through render.py, not directly:

    blender -b --factory-startup --python render_blender.py -- <json job file>

The job file lists rooms and settings. For each room this writes:
    build/maps/raw/<room>.png    the model alone on a transparent background
    build/maps/raw/<room>.json   camera framing (world -> pixel mapping) and background layer list
    build/maps/raw/<room>_bgN.png background layers (decoded TPLs)
"""
import json
import math
import os
import re
import sys
import time

import bpy
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from spmfiles import room_files, write_png  # noqa: E402
from mapdat import World, OPAQUE, ALPHA_TEST, BLEND  # noqa: E402
from gxtex import decode_tpl  # noqa: E402
from ceilings import Collision, ceiling_mask, normals  # noqa: E402
from animgroup import AnimGroup  # noqa: E402

WRAP = {0: 'EXTEND', 1: 'REPEAT', 2: 'MIRROR'}


def to_blender(p):
    """Game (x, y up, z toward camera) -> Blender (x, y away from camera, z up)."""
    return np.stack([p[:, 0], -p[:, 2], p[:, 1]], 1)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    for coll in (bpy.data.meshes, bpy.data.materials, bpy.data.images, bpy.data.objects, bpy.data.cameras):
        for x in list(coll):
            coll.remove(x)


def make_image(name, tex):
    h, w = tex['height'], tex['width']
    img = bpy.data.images.new(name, w, h, alpha=True)
    img.alpha_mode = 'STRAIGHT'
    img.colorspace_settings.name = 'sRGB'  # (changing this after setting pixels wipes them)
    px = tex['rgba'][::-1].astype(np.float32) / 255.0  # Blender images are bottom row first
    img.pixels.foreach_set(px.reshape(-1))
    img.pack()
    return img


def make_material(key, mat, images, textures):
    m = bpy.data.materials.new(key)
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    emit = nt.nodes.new('ShaderNodeEmission')
    transp = nt.nodes.new('ShaderNodeBsdfTransparent')
    mix = nt.nodes.new('ShaderNodeMixShader')
    nt.links.new(mix.outputs[0], out.inputs['Surface'])
    nt.links.new(transp.outputs[0], mix.inputs[1])
    nt.links.new(emit.outputs[0], mix.inputs[2])

    # rasterised colour: vertex colour or the material colour register
    if mat['color_src'] == 1:
        ras = nt.nodes.new('ShaderNodeVertexColor')
        ras.layer_name = 'clr'
        ras_rgb, ras_a = ras.outputs['Color'], ras.outputs['Alpha']
    else:
        c = [x / 255.0 for x in mat['color']]
        rgb = nt.nodes.new('ShaderNodeRGB')
        rgb.outputs[0].default_value = (srgb_to_linear(c[0]), srgb_to_linear(c[1]), srgb_to_linear(c[2]), 1)
        av = nt.nodes.new('ShaderNodeValue')
        av.outputs[0].default_value = c[3]
        ras_rgb, ras_a = rgb.outputs[0], av.outputs[0]

    col, alpha = ras_rgb, ras_a
    smp = mat['samplers'][0] if mat['samplers'] else None
    if smp is not None and 0 <= smp['texture'] < len(textures):
        tex = nt.nodes.new('ShaderNodeTexImage')
        tex.image = images[smp['texture']]
        tex.extension = WRAP.get(smp['wrap_s'], 'REPEAT')
        tex.interpolation = 'Linear'
        uv = nt.nodes.new('ShaderNodeUVMap')
        uv.uv_map = 'uv0'
        nt.links.new(uv.outputs[0], tex.inputs[0])
        mulc = nt.nodes.new('ShaderNodeVectorMath')
        mulc.operation = 'MULTIPLY'
        nt.links.new(tex.outputs['Color'], mulc.inputs[0])
        nt.links.new(ras_rgb, mulc.inputs[1])
        col = mulc.outputs[0]
        mula = nt.nodes.new('ShaderNodeMath')
        mula.operation = 'MULTIPLY'
        if mat.get('invert_alpha'):
            inv = nt.nodes.new('ShaderNodeMath')
            inv.operation = 'SUBTRACT'
            inv.inputs[0].default_value = 1.0
            nt.links.new(tex.outputs['Alpha'], inv.inputs[1])
            nt.links.new(inv.outputs[0], mula.inputs[0])
        else:
            nt.links.new(tex.outputs['Alpha'], mula.inputs[0])
        nt.links.new(ras_a, mula.inputs[1])
        alpha = mula.outputs[0]
    nt.links.new(col, emit.inputs['Color'])

    layer = mat['layer']
    if layer == OPAQUE:
        mix.inputs[0].default_value = 1.0
        m.surface_render_method = 'DITHERED'
    elif layer == ALPHA_TEST:
        gt = nt.nodes.new('ShaderNodeMath')
        gt.operation = 'GREATER_THAN'
        gt.inputs[1].default_value = 0.5
        nt.links.new(alpha, gt.inputs[0])
        nt.links.new(gt.outputs[0], mix.inputs[0])
        m.surface_render_method = 'DITHERED'
    else:
        nt.links.new(alpha, mix.inputs[0])
        m.surface_render_method = 'BLENDED'
    if mat.get('fade', 1.0) < 1.0:  # see-through (blocks that only exist in 3D)
        fade = nt.nodes.new('ShaderNodeMath')
        fade.operation = 'MULTIPLY'
        fade.inputs[1].default_value = mat['fade']
        nt.links.new(mix.inputs[0].links[0].from_socket if mix.inputs[0].is_linked else alpha, fade.inputs[0])
        nt.links.new(fade.outputs[0], mix.inputs[0])
        m.surface_render_method = 'BLENDED'
    return m


def srgb_to_linear(c):
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def build_object(name, chunks, material, cull):
    pos = np.concatenate([c['pos'] for c in chunks])
    n = len(pos)
    # GX front faces wind clockwise; Blender's wind counter-clockwise, so swap two corners
    faces = np.arange(n).reshape(-1, 3)[:, [0, 2, 1]]
    if cull == 0:  # GX culls front faces: flip so Blender's back-face culling does the same
        faces = faces[:, [0, 2, 1]]
    me = bpy.data.meshes.new(name)
    verts = to_blender(pos)
    me.vertices.add(n)
    me.vertices.foreach_set('co', verts.reshape(-1))
    me.loops.add(n)
    me.loops.foreach_set('vertex_index', faces.reshape(-1).astype(np.int32))
    me.polygons.add(n // 3)
    me.polygons.foreach_set('loop_start', np.arange(0, n, 3, dtype=np.int32))
    me.polygons.foreach_set('loop_total', np.full(n // 3, 3, dtype=np.int32))
    loop_vert = faces.reshape(-1)
    uv = np.concatenate([c['uv'][0] if c['uv'][0] is not None else np.zeros((len(c['pos']), 2), np.float32)
                         for c in chunks])
    uvl = me.uv_layers.new(name='uv0')
    uvl.data.foreach_set('uv', np.stack([uv[loop_vert, 0], 1.0 - uv[loop_vert, 1]], 1).reshape(-1))
    clr = np.concatenate([c['clr'] if c['clr'] is not None else np.full((len(c['pos']), 4), 255, np.uint8)
                          for c in chunks]).astype(np.float32) / 255.0
    ca = me.color_attributes.new('clr', 'BYTE_COLOR', 'CORNER')
    ca.data.foreach_set('color_srgb', clr[loop_vert].reshape(-1))
    me.update(calc_edges=True)
    me.validate(clean_customdata=False)
    me.materials.append(material)
    if cull in (0, 1):
        material.use_backface_culling = True
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return pos


FAR_BEHIND = 150  # model units
# Each chapter's first room, whose door back to Flipside Tower is drawn like the tower's own chapter doors
# (Chapter 8's, in ls1_01, is Castle Bleck's door: drawn as stored, like Flopside Tower's)
CHAPTER_DOOR_ROOMS = {'he1_01', 'mi1_01', 'ta1_01', 'sp1_01', 'gn1_01', 'an1_01'}
# parts drawn even though the init script hides them or they look like masks
FORCE_SHOW = {
    'he3_04': ['doa'],  # the doors (they drop out of sight for the O'Chunks fight)
    'mac_01': ['iwa_01', 'before_iwa'],  # the Piccolo block (the script hides it once it's gone; user, 2026-11-04)
    'mac_11': ['iwa_01'],      # Flopside's twin of it
    # each chapter's door back to Flipside / Flopside Tower: the script hides it unless Mario came in by it
    # (user, 2026-11-04: the only way back aside from the x-4 Star Blocks, so the map shows them)
    'he1_01': ['sc_doa_01'], 'mi1_01': ['sc_doa_02'], 'ta1_01': ['sc_doa_03'], 'sp1_01': ['sc_doa_04'],
    'gn1_01': ['sc_doa_05'], 'an1_01': ['sc_doa_07'], 'ls1_01': ['doa_01'],
    'mac_12': ['A_doa_06',  # solid backing of the Castle Bleck door on Flopside Tower (else see-through)
               'dokan_06', 'ereve_03', 'ereve_04'],  # Return Pipe and tower elevator, hidden until unlocked
    'mac_02': [f'A_doa_{n:02d}' for n in (6, 8, 9, 10, 11, 12, 13)]  # the chapter doors on Flipside Tower
              + ['dokan_03', 'ereve_03', 'ereve_04'],  # Return Pipe and tower elevator (as mac_22 shows them)
}
# thin parts drawn thicker, by how much (model units): ls3_10's 3D-only arrow lines are hard to see on the map
THICKEN = {}
# arrows the game draws as hairlines (3D only), redrawn on the image as plain lines with a triangle head
# (user, 2026-11-05): (line part, arrowhead parts). ls3_10: the route through its platforms
DRAW_ARROWS = {'ls3_10': ('pCube8_q00036', [f'pCube8_q000{n}' for n in range(37, 45)])}
# parts left out: the wall blocks above the streets, which hide the houses and layers behind them
FORCE_HIDE = {
    'mac_02': ['yane'], 'mac_12': ['yane', 'u_yane'], 'mac_09': ['o_yane'], 'mac_19': ['u_yane'],
    'mac_06': ['yane'], 'mac_16': ['yane'],
    # the sewers under the towers: the front area's back wall, which hides the area behind it (the one past
    # the old gate) from the camera (user, 2026-11-04)
    'mac_05': ['group6'], 'mac_15': ['group6'],
    'an1_06': ['C_ryou'],  # 7-1: the same for the front room's back wall (user, 2026-11-04)
}
# parts cut down to a height (model units): pieces standing behind a nearer ledge, whose tops the tilted
# camera shows above it, where the game's view hides them
CAP_HEIGHT = {
    # 3-1's retro underground: the pipe up from dokan_08 stops behind the brick row (user, 2026-11-04). The
    # bricks' top (height 27.5) is 15 units nearer than the pipe; with the 8-degree tilt it shows at
    # 27.5 - 15 * tan(8) = 25.4 for the pipe
    'ta1_03': {'polySurface51': 25.4},
}
PIT_DEPTH = 3     # a floor this far below the main floor counts as a pit
DOOR_HEIGHT = 7   # model units: door boxes taller than this are cut to their bottom part
SMALL_PIECE = 0.05  # in --crop-above rooms, collision pieces narrower than this share of the room are ignored


def has_mask(world, parts):
    """True when a drawn node named like a mask (kabe_msk, msk, ...) spans 90%+ of the room's width."""
    allx = np.concatenate([m['pos'][:, 0] for _, _, m in parts]) if parts else np.zeros(1)
    width = allx.max() - allx.min()
    for node in world.walk(world.find(world.s_node)):
        if 'msk' not in node['name'].lower():
            continue
        x = [m['pos'][:, 0] for _, _, m in world.meshes(node['name'])]
        if x and width > 0:
            x = np.concatenate(x)
            if x.max() - x.min() >= 0.9 * width:
                return True
    return False


def m_not_in(part, parts):
    return all(part[2] is not p[2] for p in parts)


def skip_reason(mat, mesh):
    """Parts the game hides or only shows in cutscenes, which we can't tell from the script: untextured
    black 'mask' geometry and untextured see-through sheets (fades, fog)."""
    if mat['samplers']:
        return None
    if mat['color_src'] == 1 and mesh['clr'] is not None:
        rgb = mesh['clr'][:, :3].astype(np.float32)
        if rgb.max() < 12:
            return 'black mask'
        if re.match(r'lambert1($|_)', mat['name']) and rgb.min() > 250:
            return 'flat white, default material'
        if mat['layer'] == BLEND and mesh['clr'][:, 3].mean() < 250:
            return 'untextured translucent'
    return None


def project(info, pts):
    """Game-space (model units) points -> image pixels, as render_room frames them."""
    right, up = np.array(info['right']), np.array(info['up'])  # see projection.py
    return np.stack([(pts @ right - info['x0']) * info['ppu'], (info['y0'] - pts @ up) * info['ppu']], 1)


def arrow_lines(world, info, line_part, head_parts):
    """[[x0, y0, x1, y1]] pixel segments of an arrow drawing, each pointing at (x1, y1). The line part is
    hairline quads (two triangles each); pieces under 3 units are arrowheads, as are head_parts."""
    segs, heads = [], []
    for _, _, m in world.meshes(line_part):
        t = m['pos'].reshape(-1, 3, 3)
        for i in range(0, len(t) - 1, 2):
            q = np.concatenate(t[i:i + 2]).astype(np.float64)
            d = np.linalg.norm(q[:, None, :2] - q[None, :, :2], axis=2)
            a, b = np.unravel_index(d.argmax(), d.shape)
            (heads if d[a, b] < 3 else segs).append(q.mean(0) if d[a, b] < 3 else (q[a], q[b]))
    for name in head_parts:
        pts = [m['pos'] for _, _, m in world.meshes(name)]
        if pts:
            heads.append(np.concatenate(pts).mean(0))
    out = []
    for a, b in segs:
        if heads:  # the end nearer an arrowhead is the point
            da = min(np.linalg.norm(a[:2] - h[:2]) for h in heads)
            db = min(np.linalg.norm(b[:2] - h[:2]) for h in heads)
            if da < db:
                a, b = b, a
        px = project(info, np.array([a, b]))
        out.append([round(float(v), 1) for v in px.reshape(-1)])
    return out


def locate_doors(world, script, info):
    """Pixel box of each door/pipe from its hit object (or map object) in the model."""
    out = []
    for d in script.get('doors', []):
        d = dict(d)
        if d.get('pos') and not any(d.get(k) for k in ('hit3d', 'hit2d', 'mapobj')):
            # placed by position only (a spring): a box about 5 model units round it
            px = project(info, np.array([d['pos']]) / 10.0)[0]
            r = 2.5 * info['ppu']
            d['located_by'] = 'position'
            d['world'] = [round(float(v), 1) for v in d['pos']]
            d['px'] = [round(float(v), 1) for v in px]
            d['box'] = [round(float(px[0] - r), 1), round(float(px[1] - 2 * r), 1),
                        round(float(px[0] + r), 1), round(float(px[1]), 1)]
            out.append(d)
            continue
        for key in ('hit3d', 'hit2d', 'mapobj'):
            name = d.get(key)
            if not name or name == info['room']:
                continue
            pts = [m['pos'] for _, _, m in world.meshes(name)]
            if not pts:
                continue
            pts = np.concatenate(pts)
            lo, hi = pts.min(0), pts.max(0)
            if d.get('kind') in ('map_door', 'door') and hi[1] - lo[1] > DOOR_HEIGHT:
                # door hit boxes often reach far up (so Mario can't jump past); the door is at the bottom
                hi = hi.copy()
                hi[1] = lo[1] + DOOR_HEIGHT
            box = project(info, np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1])
                                          for z in (lo[2], hi[2])]))
            d['located_by'] = name
            d['world'] = ((lo + hi) / 2 * 10).round(1).tolist()  # script units are 10x model units
            d['px'] = [round(float(v), 1) for v in project(info, ((lo + hi) / 2)[None])[0]]
            d['box'] = [round(float(box[:, 0].min()), 1), round(float(box[:, 1].min()), 1),
                        round(float(box[:, 0].max()), 1), round(float(box[:, 1].max()), 1)]
            break
        out.append(d)
    return out


def collision_parts(world, min_frac=0.0):
    """Collision meshes' positions, leaving out pieces narrower than min_frac of the room's collision
    width (e.g. looping lift platforms parked above and below the room)."""
    t = [m['pos'] for _, _, m in world.meshes(world.a_node)]
    if not t or not min_frac:
        return t
    allx = np.concatenate(t)[:, 0]
    width = allx.max() - allx.min()
    return [p for p in t if p[:, 0].max() - p[:, 0].min() >= min_frac * width]


def floor_levels(world, up, min_frac=0.0):
    """From the upward-facing collision surfaces: (lowest point along the camera's up axis, whether the
    room has a pit: a floor more than PIT_DEPTH below the main floor, the area-weighted median height)."""
    t = collision_parts(world, min_frac)
    if not t:
        return None, None
    t = np.concatenate(t).reshape(-1, 3, 3).astype(np.float64)
    floors = t[normals(t)[:, 1] > 0.7]
    if not len(floors):
        return None, None
    h = floors[:, :, 1].mean(1)  # world height, not tilted
    area = 0.5 * np.linalg.norm(np.cross(floors[:, 1] - floors[:, 0], floors[:, 2] - floors[:, 0]), axis=1)
    o = np.argsort(h)
    cw = np.cumsum(area[o])
    main = h[o][min(len(o) - 1, np.searchsorted(cw, cw[-1] / 2))]
    return float((floors.reshape(-1, 3) @ up).min()), bool(h.min() < main - PIT_DEPTH)


def main_floor(world):
    """The room's main floor: the area-weighted median height (world y, map units) of the upward-facing
    collision surfaces, or None. render.py puts the background where the game's camera shows it from there."""
    t = collision_parts(world)
    if not t:
        return None
    t = np.concatenate(t).reshape(-1, 3, 3).astype(np.float64)
    floors = t[normals(t)[:, 1] > 0.7]
    if not len(floors):
        return None
    h = floors[:, :, 1].mean(1)
    area = 0.5 * np.linalg.norm(np.cross(floors[:, 1] - floors[:, 0], floors[:, 2] - floors[:, 0]), axis=1)
    o = np.argsort(h)
    cw = np.cumsum(area[o])
    return float(h[o][min(len(o) - 1, np.searchsorted(cw, cw[-1] / 2))])


def collision_top(world, up, min_frac=0.0):
    """Highest point (along the camera's up axis) of the collision model."""
    t = collision_parts(world, min_frac)
    return float((np.concatenate(t) @ up).max()) if t else None


A_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                     'disc', 'DATA', 'files', 'a')
# evt_mobj_blk kind (low byte) -> model, in the order of the game's own table (main.dol near "%s_STG3")
BLOCK_MODELS = {6: 'MOBJ_damar_block',  # 6: fake blocks that look unbreakable (gn3_08 db_00...)
                0: 'MOBJ_r_block', 1: 'MOBJ_hatena_block', 2: 'MOBJ_r_hatena_block', 3: 'MOBJ_10Count_block',
                4: 'MOBJ_su_block', 7: 'MOBJ_r_block_red', 8: 'MOBJ_pera_r_block', 9: 'MOBJ_r_block',
                10: 'MOBJ_pera_hatena_block', 11: 'MOBJ_hatena_block', 12: 'MOBJ_p_b_sui_block',
                13: 'MOBJ_p_b_sui_block', 14: 'MOBJ_p_r_sui_block', 15: 'MOBJ_p_r_sui_block',
                16: 'MOBJ_Roll_block', 17: 'MOBJ_Roll_block', 18: 'MOBJ_RedSwitchBlock01',
                19: 'MOBJ_RedSwitchBlock02', 20: 'MOBJ_RedSwitchBlock03', 21: 'MOBJ_RedSwitchBlock04',
                5: 'MOBJ_mame_renga',  # the bean block that grows a ladder (user, 2026-11-04)
                22: 'MOBJ_nage_block', 24: 'MOBJ_p_r_sui_block', 25: 'MOBJ_p_b_sui_block',
                # the Whoa Zone's gravity blocks (zyu = juuryoku, gravity): up/down, then left/right
                26: 'MOBJ_zyu_sui_a', 27: 'MOBJ_zyu_sui_b'}
BLOCKS_3D_ONLY = {9, 11, 13, 15, 17}  # drawn see-through: they only exist when Mario flips to 3D
SPRING_MODELS = {0: 'MOBJ_BlueJumpStand', 1: 'MOBJ_RedJumpStand'}  # springs (scripts.JUMP_STAND)
CLOUD_MODELS = {0: 'MOBJ_jump_kumo', 1: 'MOBJ_big_jump_kumo'}  # bouncy clouds (scripts.JUMP_CLOUD; user, 2026-11-05)
CHEST_MODELS = {0: 'MOBJ_thako', 1: 'MOBJ_big_thako', 2: 'MOBJ_thako', 3: 'MOBJ_big_thako'}
IDLE_ANIMS = ('S_1', 'S_N', 'stand', 'S_close', 'close')
_models = {}


# evt_mobj_hunmer_blk type -> model (evt_mobj.h MobjHunmerBlkType: 1 the usual Cudge block, 2 the big one)
HAMMER_MODELS = {0: 'MOBJ_hunmer_block_lv1', 1: 'MOBJ_hunmer_block_lv1', 2: 'MOBJ_hunmer_block_lv2',
                 3: 'MOBJ_hunmer_block_kakushi_mac'}


def mobj_model(room, kind, typ):
    """Model file name for a spawned object, or None. Chapter 3 rooms use the _STG3 (8-bit) versions."""
    name = {'block': BLOCK_MODELS.get(typ & 0xFF), 'chest': CHEST_MODELS.get(typ),
            'save': 'MOBJ_save_block', 'star_block': 'MOBJ_eria_block', 'stake': 'MOBJ_hip_kui',
            'spring': SPRING_MODELS.get(typ), 'cloud': CLOUD_MODELS.get(typ),
            'hammer': HAMMER_MODELS.get(typ), 'lock': 'MOBJ_zyo'}.get(kind)
    if name and room.startswith('ta') and os.path.exists(os.path.join(A_DIR, name + '_STG3')):
        name += '_STG3'
    return name if name and os.path.exists(os.path.join(A_DIR, name)) else None


def parse_model(name, hide=()):
    """(TPL textures, [(material dict with the model's own texture index, cull, mesh)]) for a model in
    its idle pose, without the groups named in hide. Cached per process: Blender jobs render many rooms in a row."""
    key = (name, tuple(sorted(hide)))
    if key in _models:
        return _models[key]
    ag = AnimGroup(open(os.path.join(A_DIR, name), 'rb').read())
    ag.hidden = frozenset(hide)
    bright = any(g['name'] == 'hatena_C' for g in ag.groups)
    if not bright:
        for a in IDLE_ANIMS:
            if ag.pose(a):
                break
    tpl_path = next((os.path.join(A_DIR, n + '-') for n in (ag.tex_name, name)
                     if n and os.path.exists(os.path.join(A_DIR, n + '-'))), None)
    if tpl_path is None:
        _models[key] = ([], [])
        return _models[key]
    tpl = decode_tpl(open(tpl_path, 'rb').read())
    parts = list(ag.meshes())
    shapes = {p[0] for p in parts}
    out = []
    for shape, tex, ws, wt, cull, mesh in parts:
        if shape.startswith('su_block') and len(shapes) > 1 and 'MOBJ_su_block' not in name:
            continue  # the "used block" variant drawn on top of the real one
        if bright and not shape.startswith('hatena_C'):
            continue  # ? blocks: keep the bright frame of their flashing cycle (hatena_A/B are dimmer)
        mat = {'name': f'{name}_{shape}_{tex}', 'color': (255, 255, 255, 255), 'color_src': 1,
               'layer': ALPHA_TEST, 'tev_mode': 0,
               'samplers': [{'texture': tex, 'wrap_s': ws, 'wrap_t': wt}] if tex >= 0 else []}
        # vertex colours carry real colour (black outlines, the save block's star) and per-face shading
        out.append((mat, {0: 1, 1: 0}.get(cull, 2), mesh))
    _models[key] = (tpl, out)
    return _models[key]


# Ground-pound stakes (evt_mobj_hip_kui): the game's heights, in game units, set as it spawns them
# (constants at r2-0x52d8 / -0x52d4 / -0x52dc): medium (type 0) 50 and tall (type 1) 75 when up, 10 when
# pounded down. The model is 1 model unit (10 game units) tall: a post (downShape) with a head (hedShape)
STAKE_UP = {0: 50.0, 1: 75.0}
STAKE_DOWN = 10.0
# Stakes drawn as the room's puzzle wants them, by name: the ones pounded down in the right combination
# (user, 2026-11-04). ta2_01 (3-2, Thudley's door): the door opens with LSWF 5-10 = 0, 1, 1, 0, 1, 0
STAKES_DOWN = {'ta2_01': {'pile2', 'pile3', 'pile5'}}


def stake_height(room, m, pounded=()):
    """A stake's height in model units: up, or down if the room's solution has it pounded, or a staged
    image has (stages.py 'stakes_down')."""
    down = m['name'] in pounded or m['name'] in STAKES_DOWN.get(room.split('_s')[0] if room[-3:-1] == '_s' else room, ())
    return (STAKE_DOWN if down else STAKE_UP.get(m['type'], STAKE_UP[0])) / 10.0


CAP_TEXTURE = ('mac_02', 'kowarenai_block')  # the studded block under Flipside's elbow pipe
_cap_tex = []


def pipe_caps(parts, script, pitch_deg, yaw_deg, images, textures, pipe_tex):
    """Covers for cut-off pipe ends: pipe parts (a 'dokan' texture) 6.5-8.5 units wide and under 6
    deep (the rimless pipes; real pipe ends are narrower with a rim) whose top isn't a pipe entrance and
    can be seen by the camera. Each gets a 2x2 grid of
    studded blocks, one block high, over its footprint (user: hides the cut-off pipe insides).
    Returns [(material, mesh)]."""
    pipes, other = {}, []
    for node, mat, mesh in parts:
        t = mat['samplers'][0]['texture'] if mat['samplers'] else -1
        p = mesh['pos'].reshape(-1, 3, 3).astype(np.float64)
        if t in pipe_tex:
            pipes.setdefault(id(node), (node, []))[1].append(p)
        else:
            other.append(p)
    if not pipes or not other:
        return []
    other = np.concatenate(other)
    pitch, yaw = math.radians(pitch_deg), math.radians(yaw_deg)
    back = -np.array([math.cos(pitch) * math.sin(yaw), -math.sin(pitch), -math.cos(pitch) * math.cos(yaw)])
    entrances = [d for d in script.get('doors', []) if d.get('kind') == 'pipe']
    out = []
    for node, ps in pipes.values():
        p = np.concatenate(ps)
        n = normals(p)
        top = p[:, :, 1].max()
        near = p[:, :, 1].max(1) > top - 0.3
        if not (np.abs(n[near][:, 1]) < 0.3).any():
            continue  # no walls reaching the top
        pts = p.reshape(-1, 3)
        lo, hi = pts.min(0), pts.max(0)
        if not 6.5 <= hi[0] - lo[0] <= 8.5 or not 1 <= hi[2] - lo[2] <= 6 or hi[1] - lo[1] < 6:  # upright pipes
            continue  # only the big rimless pipes
        if any(node['name'] in (d.get('hit2d'), d.get('hit3d'), d.get('mapobj')) for d in entrances):
            continue
        # seen by the camera: nothing between the top's middle and the camera
        o = np.array([(lo[0] + hi[0]) / 2, top + 0.3, (lo[2] + hi[2]) / 2])
        a, b, c = other[:, 0], other[:, 1], other[:, 2]
        e1, e2 = b - a, c - a
        h = np.cross(back, e2)
        det = (e1 * h).sum(1)
        ok = np.abs(det) > 1e-9
        f = np.where(ok, 1 / np.where(ok, det, 1), 0)
        sv = o - a
        u = f * (sv * h).sum(1)
        q = np.cross(sv, e1)
        v = f * (q * back).sum(1)
        t = f * (e2 * q).sum(1)
        if (ok & (u >= 0) & (v >= 0) & (u + v <= 1) & (t > 0.05)).any():
            continue
        out.append((lo, hi, top))
    if not out:
        return []
    if not _cap_tex:
        room, name = CAP_TEXTURE
        f = room_files(room)
        w = World(f[f'map/{room}/map.dat'])
        _cap_tex.append(decode_tpl(f[f'map/{room}/texture.tpl'])[w.texture_names.index(name)])
    ti = len(textures)
    textures.append(_cap_tex[0])
    images.append(make_image('pipe_cap', _cap_tex[0]))
    mat = {'name': 'pipe_cap', 'color': (255, 255, 255, 255), 'color_src': 0, 'layer': OPAQUE, 'tev_mode': 0,
           'samplers': [{'texture': ti, 'wrap_s': 1, 'wrap_t': 1}]}
    pos, uv = [], []
    for lo, hi, top in out:
        w, d = (hi[0] - lo[0]) / 2, max(hi[2] - lo[2], 5.0) / 2
        cz = (lo[2] + hi[2]) / 2
        hgt = w  # one block high: as tall as a block is wide
        for i in range(2):
            for k in range(2):
                x0, z0 = lo[0] + i * w, cz - d + k * d
                pos.append(_box(x0, top, z0, w, hgt, d))
                uv.append(np.tile([[0, 0], [1, 1], [1, 0], [0, 0], [0, 1], [1, 1]], (6, 1)).astype(np.float32))
    mesh = {'pos': np.concatenate(pos).astype(np.float32), 'clr': None, 'uv': [np.concatenate(uv), None, None]}
    return [(mat, mesh)]


def _box(x, y, z, w, h, d):
    """Triangle soup (36 vertices, GX clockwise winding) of a box from (x, y, z) sized w, h, d."""
    X, Y, Z = (x, x + w), (y, y + h), (z, z + d)
    faces = [  # each as 4 corners, outward-facing, counter-clockwise seen from outside
        [(X[0], Y[0], Z[1]), (X[1], Y[0], Z[1]), (X[1], Y[1], Z[1]), (X[0], Y[1], Z[1])],  # front +z
        [(X[1], Y[0], Z[0]), (X[0], Y[0], Z[0]), (X[0], Y[1], Z[0]), (X[1], Y[1], Z[0])],  # back
        [(X[0], Y[0], Z[0]), (X[0], Y[0], Z[1]), (X[0], Y[1], Z[1]), (X[0], Y[1], Z[0])],  # left
        [(X[1], Y[0], Z[1]), (X[1], Y[0], Z[0]), (X[1], Y[1], Z[0]), (X[1], Y[1], Z[1])],  # right
        [(X[0], Y[1], Z[1]), (X[1], Y[1], Z[1]), (X[1], Y[1], Z[0]), (X[0], Y[1], Z[0])],  # top
        [(X[0], Y[0], Z[0]), (X[1], Y[0], Z[0]), (X[1], Y[0], Z[1]), (X[0], Y[0], Z[1])],  # bottom
    ]
    tris = []
    for a, b, c, e in faces:
        tris += [a, c, b, a, e, c]  # clockwise, as GX stores front faces
    return np.array(tris)


def add_mobjs(room, mobjs, images, textures, pounded=(), hidden=()):
    """Draws the blocks, chests and save blocks the room's scripts spawn. Returns (placed, unplaced).
    Each model's textures are added to this room's images/textures, and its materials point at them."""
    placed, unplaced, groups, bases = [], [], {}, {}
    for m in mobjs:
        if m.get('name') in hidden:  # a stage of the room has it gone (stages.py 'hide')
            continue
        name = mobj_model(room, m['kind'], m['type'])
        if not name:
            unplaced.append(m)
            continue
        tpl, parts = parse_model(name)
        if name not in bases:
            bases[name] = len(textures)
            for i, t in enumerate(tpl):
                textures.append(t)
                images.append(make_image(f'{name}_t{i}', t))
        fade = 0.45 if m['kind'] == 'block' and (m['type'] & 0xFF) in BLOCKS_3D_ONLY else 1.0
        offset = np.array(m['pos'], np.float32) / 10.0  # game units -> model units
        height = stake_height(room, m, pounded) if m['kind'] == 'stake' else None
        rot = math.radians(m.get('rot') or 0.0)  # turned about the vertical (locks: evt_mobj_zyo's y rotation)
        for mat, cull, mesh in parts:
            key = (mat['name'], cull, fade)
            if key not in groups:
                smp = [dict(sm, texture=bases[name] + sm['texture']) for sm in mat['samplers']]
                groups[key] = (dict(mat, samplers=smp, fade=fade), [])
            pos = mesh['pos']
            if height is not None:  # the post stretched to the stake's height, its head on top
                pos = pos.copy()
                if '_downShape_' in mat['name']:
                    pos[:, 1] *= height
                elif '_hedShape_' in mat['name']:
                    pos[:, 1] += height - 1.0
            if rot:
                c, sn = math.cos(rot), math.sin(rot)
                pos = pos.copy()
                pos[:, 0], pos[:, 2] = c * mesh['pos'][:, 0] + sn * mesh['pos'][:, 2], -sn * mesh['pos'][:, 0] + c * mesh['pos'][:, 2]
            groups[key][1].append(dict(mesh, pos=pos + offset))
        placed.append(dict(m, model=name))
    for (mname, cull, fade), (mat, chunks) in groups.items():
        key = f'mobj:{mname}#{cull}#{fade}'
        build_object(key, chunks, make_material(key, mat, images, textures), cull)
    return placed, unplaced


# Rooms the game shows from the other side (its camera on the -z side): Flopside's house street, built
# on the far side of the town (user, 2026-11-04: seen from +z, its back walls covered the houses). They
# are turned half a turn about the vertical axis, (x, z) -> (-x, -z), before drawing, so the usual
# camera sees them as the game does; the room's info says 'turned', and positions placed on the image
# (checks, Mario) are turned the same way.
TURNED = {'mac_19'}
# rooms whose image ends at a fixed height (projected model units, as the viewer's y0): ta3_04 at the top of
# its tree's leaves, leaving out the pipe above them and the tall wall (user, 2026-11-05)
TOP_AT = {'ta3_04': 27.4}


def turn_world(world):
    """Makes the world's parts come out turned half a turn: (x, z) -> (-x, -z)."""
    meshes = world.meshes
    flip = np.array([-1.0, 1.0, -1.0], np.float32)

    def turned(*args, **kwargs):
        for node, mat, mesh in meshes(*args, **kwargs):
            mesh['pos'] = (mesh['pos'] * flip).astype(np.float32)
            yield node, mat, mesh
    world.meshes = turned


def render_room(room, job):
    t0 = time.time()
    reset()
    # a variant (render.py VARIANTS) is drawn from its base room's files and script, under its own name and
    # without the base room's FORCE_SHOW / FORCE_HIDE rules
    base = job.get('variants', {}).get(room, room)
    files = room_files(base)
    world = World(files[f'map/{base}/map.dat'])
    if base in TURNED:
        turn_world(world)
    textures = decode_tpl(files[f'map/{base}/texture.tpl'])
    images = [make_image(f'{room}_t{i}', t) for i, t in enumerate(textures)]

    script = job['scripts'].get(base, {})
    if base in TURNED:  # the script's objects turn with the room
        script = dict(script, mobjs=[dict(m, pos=[-m['pos'][0], m['pos'][1], -m['pos'][2]])
                                     for m in script.get('mobjs', [])])
    # animations the init script plays: mode 2 jumps to the end (e.g. he1_01's 'anm_line' intro drawing)
    # (a staged room's image can also ask for animations at their end: stages.py 'anim_end', e.g. a grown ladder)
    # ('anim_skip': animations an image leaves out, another state's that move the same parts: an1_06's water)
    anim_end = set(job.get('force', {}).get(room, {}).get('anim_end', ()))
    anim_skip = set(job.get('force', {}).get(room, {}).get('anim_skip', ()))
    world.pose(world.animations(), {**{name: '2' in modes for name, modes in script.get('anims', {}).items()
                                       if name not in anim_skip},
                                    **{name: True for name in anim_end}})
    groups = {}  # (material name, cull) -> chunks
    mats = {}
    skipped = []
    parts = []
    # the room's own rules (a stage image takes its room's), then a staged room's image (stages.py)
    stage = job.get('force', {}).get(room, {})
    show, hide = set(stage.get('show', ())), set(stage.get('hide', ()))
    keep = (set(FORCE_SHOW.get(base, ())) | show) - hide
    leave_out = (set(FORCE_HIDE.get(base, ())) | hide) - show
    hide_tex = set(stage.get('hide_textures', ()))
    for node, mat, mesh in world.meshes(skip=(set(script.get('hide', [])) - keep) | leave_out):
        why = None if node['name'] in keep else skip_reason(mat, mesh)
        smp = mat['samplers'][0] if mat['samplers'] else None
        if smp and 0 <= smp['texture'] < len(world.texture_names) and 'bokasi' in world.texture_names[smp['texture']]:
            why = 'blur effect'
        if any(0 <= sm['texture'] < len(world.texture_names) and world.texture_names[sm['texture']] in hide_tex
               for sm in mat['samplers']):
            why = 'texture left out in this image'  # stages.py 'hide_textures' (an1_06's water, drained)  # big blurred-white glow planes of gn1_01's intro (the only room using them)
        if (why == 'black mask' and any(room.startswith(p) for p in job.get('keep_outlines', []))
                and not node['name'].lower().startswith('msk')):
            # black outline plates behind objects (e.g. the asteroids in Outer Space), and the Whoa Zone's
            # see-through dark fill on its platforms (user, 2026-11-04); parts named msk_ are real masks
            why = None
        if why == 'flat white, default material' and re.match(r'hashigo|hasigo', node['name'], re.I):
            why = None  # ladders really are drawn as plain white outlines (mi3_01; user, 2026-11-05)
        if why:
            skipped.append(f"{node['name']} ({why})")
            continue
        cap = CAP_HEIGHT.get(room, {}).get(node['name'])
        if cap is not None:
            mesh['pos'] = mesh['pos'].copy()
            mesh['pos'][:, 1] = np.minimum(mesh['pos'][:, 1], cap)
        thick = THICKEN.get(room.split('_s')[0], {}).get(node['name'])
        if thick:  # thin lines drawn bolder: copies packed closer than a line's width, so they fill solid
            r, step = thick / 2, 0.05
            k = np.arange(-r, r + 1e-6, step)
            offs = [(dx, dy) for dx in k for dy in k]
            mesh = dict(mesh, pos=np.concatenate([mesh['pos'] + np.array([dx, dy, 0], np.float32)
                                                  for dx, dy in offs]),
                        clr=None if mesh['clr'] is None else np.tile(mesh['clr'], (len(offs), 1)),
                        uv=[None if uv is None else np.tile(uv, (len(offs), 1)) for uv in mesh['uv']])
        parts.append((node, mat, mesh))
    # parts a staged image draws moved (stages.py 'move', map units: ls2_03's pipe in its five spots)
    for part, delta in stage.get('move', {}).items():
        root = world.find(part)
        under = {id(n) for n in world.walk(root)} if root else set()
        for node, _, mesh in parts:
            if id(node) in under:
                mesh['pos'] = mesh['pos'] + np.asarray(delta, dtype=mesh['pos'].dtype)
    # scenery far behind the room (made for the game's perspective camera) would stretch the
    # orthographic frame enormously: leave out parts entirely FAR_BEHIND units behind the floors
    floors_z = [m['pos'][:, 2].min() for _, _, m in world.meshes(world.a_node)]
    if floors_z:
        back = min(floors_z) - FAR_BEHIND
        far = [(n, mat, m) for n, mat, m in parts if m['pos'][:, 2].max() < back]
        if far:
            skipped += [f"{n['name']} (far background)" for n, _, _ in far]
            parts = [x for x in parts if m_not_in(x, far)]
    # indoor rooms: no background picture (only a tiny solid-colour one, 8x8 black), or a full-width
    # mask (kabe_msk, msk: a black front wall with the room cut out) hiding the background, as in
    # Merlee's mansion
    has_bg = any(t['width'] > 16 for k, v in files.items() if k.startswith('bg/') and k.endswith('.tpl')
                 for t in decode_tpl(v))
    as_drawn = any(room.startswith(p) for p in job.get('as_drawn', []))  # e.g. Castle Bleck's line art
    masked = has_bg and not as_drawn and has_mask(world, parts)
    indoor = not has_bg or masked
    if masked:
        # the mask is only black wall round the room: leave it out so the image fits the room
        under = {id(n) for top in world.walk(world.find(world.s_node)) if 'msk' in top['name'].lower()
                 for n in world.walk(top)}
        skipped += [f"{n['name']} (mask)" for n, _, _ in parts if id(n) in under]
        parts = [x for x in parts if id(x[0]) not in under]
    ceiling_tris = 0
    if not as_drawn and ((job.get('cut_ceilings', False) and indoor)
                         or any(room.startswith(p) for p in job.get('cut_ceilings_in', []))):
        # indoor/underground rooms: leave out ceilings so the room shows from above
        coll = Collision(world, [m for _, _, m in parts])
        for _, _, mesh in parts:
            keep = ~ceiling_mask(coll, mesh['pos'])
            if keep.all():
                continue
            ceiling_tris += int((~keep).sum())
            v = np.repeat(keep, 3)
            mesh['pos'] = mesh['pos'][v]
            mesh['clr'] = None if mesh['clr'] is None else mesh['clr'][v]
            mesh['uv'] = [None if uv is None else uv[v] for uv in mesh['uv']]
    for node, mat, mesh in parts:
        if not len(mesh['pos']):
            continue
        key = (mat['name'], node['cull'])
        groups.setdefault(key, []).append(mesh)
        mats[key] = mat
    if not groups:
        return {'room': room, 'error': 'no geometry'}
    pdeg = job.get('indoor_pitch', job['pitch']) if indoor else job['pitch']
    pipe_tex = {i for i, n in enumerate(world.texture_names) if 'dokan' in n.lower()}
    caps = pipe_caps(parts, script, pdeg, job.get('yaw', 0.0), images, textures, pipe_tex)
    for mat, mesh in caps:
        key = (mat['name'], 2)
        groups.setdefault(key, []).append(mesh)
        mats[key] = mat
    allpos = []
    n_caps = len(caps[0][1]['pos']) // 144 if caps else 0  # 4 boxes of 36 vertices per pipe
    for key, chunks in groups.items():
        mat = mats[key]
        # (and the same doors in each chapter's first room, Chapters 1-7: user, 2026-11-04)
        if (room == 'mac_02' or room.split('_s')[0] in CHAPTER_DOOR_ROOMS) and 'top_doa' in mat['name']:
            # Flipside Tower's chapter doors: the texture's alpha marks the line pattern; in the game the
            # door colour fills the rest and the lines show the black backing, so the mask is inverted.
            # (Flopside's Castle Bleck door really is black with coloured lines: drawn as stored.)
            mat = dict(mat, invert_alpha=True)
            # and they're lit: in the game their colour shows about twice as bright as stored
            chunks = [dict(c, clr=np.minimum(c['clr'].astype(np.int32) * [2, 2, 2, 1], 255).astype(np.uint8))
                      if c['clr'] is not None else c for c in chunks]
        material = make_material(f'{key[0]}#{key[1]}', mat, images, textures)
        allpos.append(build_object(f'{key[0]}#{key[1]}', chunks, material, key[1]))
    allpos = np.concatenate(allpos)
    # locks (evt_mobj_zyo) aren't drawn into the room: they get a picture of their own after it (below), which
    # the viewer shows until the lock is opened
    locks = [m for m in script.get('mobjs', []) if m['kind'] == 'lock'] if job.get('mobjs', True) else []
    script = dict(script, mobjs=[m for m in script.get('mobjs', []) if m['kind'] != 'lock'])
    placed, unplaced = (add_mobjs(room, script.get('mobjs', []), images, textures, set(stage.get('stakes_down', ())),
                                  set(stage.get('hide', ())) - set(stage.get('show', ())))
                        if job.get('mobjs', True) else ([], []))

    # framing: ignore stray far-away geometry by using percentiles of the vertex cloud
    lo = np.percentile(allpos, 0.5, axis=0)
    hi = np.percentile(allpos, 99.5, axis=0)
    lo_full, hi_full = allpos.min(0), allpos.max(0)
    lo = np.minimum(lo, lo_full)  # keep it simple: full x extent, y extent; z only for camera placement
    hi = np.maximum(hi, hi_full)

    pitch_deg = job.get('indoor_pitch', job['pitch']) if indoor else job['pitch']
    pitch = math.radians(pitch_deg)
    cx, cy, cz = (lo + hi) / 2
    # project the box corners onto the camera plane to get the needed ortho size
    corners = np.array([[x, y, z] for x in (lo[0], hi[0]) for y in (lo[1], hi[1]) for z in (lo[2], hi[2])])
    # camera basis in game space: tilted down by pitch, then turned by yaw about the vertical so the camera
    # stands a little to the left and looks slightly right. yaw 0: right = +x, up = (0, cos p, -sin p).
    yaw_deg = job.get('yaw', 0.0)
    yaw = math.radians(yaw_deg)
    right = np.array([math.cos(yaw), 0, math.sin(yaw)])
    up = np.array([math.sin(pitch) * math.sin(yaw), math.cos(pitch), -math.sin(pitch) * math.cos(yaw)])
    fwd = np.array([math.cos(pitch) * math.sin(yaw), -math.sin(pitch), -math.cos(pitch) * math.cos(yaw)])
    sx = corners @ right
    sy = corners @ up
    # margins: a little round the sides, more sky above, like the in-game view
    bottom, top = sy.min(), sy.max()
    cropped = False
    both_cropped = any(room.startswith(p) for p in job.get('crop_above', []))
    top_cropped = both_cropped or any(room.startswith(p) for p in job.get('crop_top', []))
    base_room = job.get('variants', {}).get(room, room)
    if base_room in TOP_AT:
        top, top_cropped = min(top, TOP_AT[base_room]), True
    if top_cropped:
        # rooms with scenery reaching far above where Mario can go: end a little above the collision
        coll_top = collision_top(world, up, SMALL_PIECE)
        if coll_top is not None:
            top = min(top, coll_top + job.get('top_margin', 6))
    if both_cropped or any(room.startswith(p) for p in job.get('crop_below_floor', [])):
        # rooms with deep cliffs (Yold Desert): stop near the floor instead of the cliffs' bottom.
        # With pits below the main floor (sandfall traps), end exactly at the pit floor, where the
        # sandfalls end; otherwise leave floor_margin of cliff showing under the floor.
        low, pit = floor_levels(world, up, SMALL_PIECE if top_cropped else 0.0)
        if low is not None:
            if pit:
                bottom, cropped = max(bottom, low), True
            else:
                bottom = max(bottom, low - job.get('floor_margin', 12))
    mx, my = (sx.max() - sx.min()) * 0.02, (top - bottom)
    sx = np.array([sx.min() - mx, sx.max() + mx])
    sy = np.array([bottom - (0 if cropped else my * 0.03),
                   top + (0 if top_cropped else my * job.get('sky', 0.25))])
    width = sx.max() - sx.min()
    height = sy.max() - sy.min()
    ppu = job['ppu']
    if width * ppu > job['max_px']:
        ppu = job['max_px'] / width
    if height * ppu > job['max_px']:
        ppu = job['max_px'] / height
    res_x, res_y = max(16, int(math.ceil(width * ppu))), max(16, int(math.ceil(height * ppu)))
    mid_x, mid_y = (sx.max() + sx.min()) / 2, (sy.max() + sy.min()) / 2
    # a room's later images (stages.py) take the room's own frame, so pins and doors line up in all of them
    # even where a stage changes the room's size (an1_06's water); render.py renders the room first
    base_info = os.path.join(job['out'], f'{base_room}.json') if base_room != room else None
    if base_info and os.path.exists(base_info):
        b = json.load(open(base_info))
        if 'error' not in b:
            ppu, res_x, res_y = b['ppu'], b['width'], b['height']
            width, height = res_x / ppu, res_y / ppu
            mid_x, mid_y = b['x0'] + width / 2, b['y0'] - height / 2
    # camera position: centre of the projected box, pulled back far along -forward
    centre = right * mid_x + up * mid_y  # point on the plane through the origin
    centre = centre + fwd * np.dot(np.array([cx, cy, cz]), fwd)  # same depth as the box centre
    dist = float(np.linalg.norm(hi - lo)) * 2 + 100
    cam_pos = centre - fwd * dist

    sc = bpy.context.scene
    cam_data = bpy.data.cameras.new('cam')
    cam_data.type = 'ORTHO'
    cam_data.ortho_scale = max(width, height)
    cam_data.clip_start = 0.1
    cam_data.clip_end = dist * 3
    cam = bpy.data.objects.new('cam', cam_data)
    sc.collection.objects.link(cam)
    cam.location = to_blender(cam_pos[None])[0]
    # Blender camera looks down its local -Z. Rotate X by 90deg - pitch to look along +Y tilted down, then
    # about Blender's Z (the game's vertical) by -yaw to look slightly right.
    cam.rotation_euler = (math.pi / 2 - pitch, 0, -yaw)
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = res_x, res_y
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.engine = 'BLENDER_EEVEE'
    sc.eevee.taa_render_samples = job['samples']
    sc.view_settings.view_transform = 'Standard'
    sc.world = bpy.data.worlds.new('w')
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    out = os.path.join(job['out'], f'{room}.png')
    sc.render.filepath = out
    bpy.ops.render.render(write_still=True)
    if locks:
        # the locks alone, same camera and frame: render.py cuts each one out (rooms/locks/)
        for ob in list(sc.collection.objects):
            if ob.type == 'MESH':
                ob.hide_render = True
        add_mobjs(room, [dict(m, rot=(m.get('rot') or 0.0) + (180.0 if base in TURNED else 0.0)) for m in locks],
                  images, textures)
        sc.render.filepath = os.path.join(job['out'], f'{room}_locks.png')
        bpy.ops.render.render(write_still=True)

    bgs = []
    for name in sorted(k for k in files if k.startswith('bg/') and k.endswith('.tpl') and not masked):
        for i, t in enumerate(decode_tpl(files[name])):
            path = os.path.join(job['out'], f'{room}_bg{len(bgs)}.png')
            write_png(path, t['rgba'].tobytes(), t['width'], t['height'])
            bgs.append({'file': os.path.basename(path), 'source': name})
    info = {
        'room': room, 'width': res_x, 'height': res_y, 'ppu': float(ppu), 'pitch_deg': pitch_deg,
        'yaw_deg': yaw_deg, 'right': [round(float(v), 6) for v in right], 'up': [round(float(v), 6) for v in up],
        # pixel = ((dot(m, right) - x0) * ppu, (y0 - dot(m, up)) * ppu) for a map-space point m (projection.py)
        'x0': float(mid_x - width / 2), 'y0': float(mid_y + height / 2),
        'bounds': [lo_full.tolist(), hi_full.tolist()], 'triangles': int(len(allpos) // 3),
        'backgrounds': bgs, 'skipped_parts': skipped, 'ceiling_triangles_cut': ceiling_tris, 'pipe_caps': n_caps,
        'mobjs_not_drawn': unplaced, 'indoor': bool(indoor),
        'masked': bool(masked), 'hidden_by_script': script.get('hide', []),
        'turned': base in TURNED,  # game positions go (x, z) -> (-x, -z) before the formula above
        'main_floor': main_floor(world),
    }
    info['doors'] = locate_doors(world, script, info)
    if base in DRAW_ARROWS:
        info['arrows'] = arrow_lines(world, info, *DRAW_ARROWS[base])
    # spawned objects with their pixel position (bottom centre; script units are 10x model units)
    info['mobjs'] = [dict(m, px=[round(float(v), 1) for v in project(info, np.array([m['pos']]) / 10.0)[0]])
                     for m in placed]
    info['locks'] = [dict(m, px=[round(float(v), 1) for v in project(info, np.array([m['pos']]) / 10.0)[0]])
                     for m in locks]
    info['seconds'] = round(time.time() - t0, 1)
    json.dump(info, open(os.path.join(job['out'], f'{room}.json'), 'w'), indent=1)
    return info


def main():
    job = json.load(open(sys.argv[sys.argv.index('--') + 1]))
    job['scripts'] = json.load(open(job['scripts_file'])) if os.path.exists(job['scripts_file']) else {}
    os.makedirs(job['out'], exist_ok=True)
    for room in job['rooms']:
        try:
            info = render_room(room, job)
            print(f'ROOM {room} ok {info.get("width")}x{info.get("height")} {info.get("seconds")}s', flush=True)
        except Exception as e:  # keep going; report at the end
            import traceback
            traceback.print_exc()
            json.dump({'room': room, 'error': repr(e)}, open(os.path.join(job['out'], f'{room}.json'), 'w'))
            print(f'ROOM {room} FAILED {e!r}', flush=True)


main()
