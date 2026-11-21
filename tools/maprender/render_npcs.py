"""Blender script: renders NPC models (disc/DATA/files/a/<name>, e.g. the bosses') to transparent PNGs, front on,
in their idle pose, at the rooms' scale (PPU pixels per map unit), so a boss drawn on a room has its size there.

    blender -b --factory-startup --python render_npcs.py -- <out dir> <picture> [<picture> ...]

where a picture is <model>, or <name>=<model>:<group>,<group>... to leave groups out (a boss without its sprout).
Writes <out dir>/<name>.png cropped to the model, and <out dir>/<name>.json: {"width", "height", "foot": [x, y]}
where foot is the pixel under the model's origin (the point the game stands it on). Reuses render_blender.py's
model loading and materials (its source, without its main()).
"""
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
_src = open(os.path.join(HERE, 'render_blender.py'), encoding='utf-8').read()
_src = _src[:_src.rindex('\nmain()')]  # everything but the room renderer's run
_ns = {'__file__': os.path.join(HERE, 'render_blender.py'), '__name__': 'render_blender'}
exec(compile(_src, 'render_blender.py', 'exec'), _ns)
bpy, np = _ns['bpy'], _ns['np']
reset, parse_model, make_image = _ns['reset'], _ns['parse_model'], _ns['make_image']
make_material, build_object, to_blender = _ns['make_material'], _ns['build_object'], _ns['to_blender']

PPU = 8  # pixels per map unit, as render.py's rooms


def render(spec, out_dir):
    name, _, model = spec.partition('=')
    model, _, hide = (model or name).partition(':')
    reset()
    tpl, parts = parse_model(model, [g for g in hide.split(',') if g])
    if not parts:
        return None
    images = [make_image(f'{name}_t{i}', t) for i, t in enumerate(tpl)]
    groups = {}
    for mat, cull, mesh in parts:
        groups.setdefault((mat['name'], cull), (mat, []))[1].append(mesh)
    allpos = []
    for (mname, cull), (mat, chunks) in groups.items():
        allpos.append(build_object(f'{mname}#{cull}', chunks, make_material(f'{mname}#{cull}', mat, images, tpl), cull))
    allpos = np.concatenate(allpos)
    lo, hi = allpos.min(0), allpos.max(0)
    width, height = float(hi[0] - lo[0]) + 1, float(hi[1] - lo[1]) + 1  # half a unit round it
    res_x, res_y = max(8, math.ceil(width * PPU)), max(8, math.ceil(height * PPU))
    cx, cy = float(lo[0] + hi[0]) / 2, float(lo[1] + hi[1]) / 2
    sc = bpy.context.scene
    cam_data = bpy.data.cameras.new('cam')
    cam_data.type = 'ORTHO'
    cam_data.ortho_scale = max(res_x, res_y) / PPU
    cam_data.clip_start, cam_data.clip_end = 0.1, 10000
    cam = bpy.data.objects.new('cam', cam_data)
    sc.collection.objects.link(cam)
    cam.location = to_blender(np.array([[cx, cy, float(hi[2]) + 500]]))[0]  # in front (+z), looking back
    cam.rotation_euler = (math.pi / 2, 0, 0)
    sc.camera = cam
    sc.render.resolution_x, sc.render.resolution_y = res_x, res_y
    sc.render.resolution_percentage = 100
    sc.render.film_transparent = True
    sc.render.engine = 'BLENDER_EEVEE'
    sc.eevee.taa_render_samples = 16
    sc.view_settings.view_transform = 'Standard'
    sc.world = bpy.data.worlds.new('w')
    sc.render.image_settings.file_format = 'PNG'
    sc.render.image_settings.color_mode = 'RGBA'
    sc.render.filepath = os.path.join(out_dir, f'{name}.png')
    bpy.ops.render.render(write_still=True)
    # the model's origin in the picture (frame centred on cx, cy; the longer side sets the scale)
    foot = [res_x / 2 + (0 - cx) * PPU, res_y / 2 - (0 - cy) * PPU]
    info = {'width': res_x, 'height': res_y, 'foot': [round(foot[0], 1), round(foot[1], 1)]}
    json.dump(info, open(os.path.join(out_dir, f'{name}.json'), 'w'))
    return info


def main():
    args = sys.argv[sys.argv.index('--') + 1:]
    out_dir, names = args[0], args[1:]
    os.makedirs(out_dir, exist_ok=True)
    for name in names:
        try:
            info = render(name, out_dir)
            name = name.partition('=')[0]
            print(f'NPC {name} {"ok " + str(info) if info else "no model"}', flush=True)
        except Exception as e:
            import traceback
            traceback.print_exc()
            print(f'NPC {name} FAILED {e!r}', flush=True)


main()
