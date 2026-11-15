"""Renders SPM rooms to PNG: side view of the 3D model with its background layers behind it.

    py -3 tools/maprender/render.py                  every playable room
    py -3 tools/maprender/render.py he1_01 he1_02    just these rooms
    py -3 tools/maprender/render.py --composite-only redo the background step only

Options: --pitch DEG (camera tilt, default 18), --yaw DEG (camera turned from the left, default 5), --ppu N (pixels per game unit, default 8),
--max-px N (cap on either image side, default 8000), --jobs N (parallel Blender processes, default 3),
--box (draw a thin outline round the model), --blender PATH,
--crop-below-floor PREFIX... (rooms whose frame stops --floor-margin model units, default 12, below the
lowest collision floor instead of the bottom of the model, or right at the floor of any pit below
the main floor; default he3, he4_10, mi1_01, mi1_08 to mi1_11, mi2_01, ta1_02: rooms whose ground goes
far deeper than anything in them),
--crop-above PREFIX... (rooms cropped at both ends: the top ends --top-margin model units, default 6, above
the highest point of the collision model, with no extra sky; the bottom as for --crop-below-floor;
default ta1_03, ta3_01, ta3_04, sp1, sp3 (Outer Space and the Outer Limits) and some Underwhere rooms),
--crop-top PREFIX... (the same top crop only, bottom left as usual; default an2_05),
--keep-outlines PREFIX... (rooms whose untextured black parts are drawn: there they are outlines behind
objects, e.g. the asteroids in Outer Space, sp1; elsewhere such parts are usually masks and are skipped),
--cut-ceilings (leave out ceilings in indoor rooms; see ceilings.py. Off by default: the lower indoor
tilt already keeps doors from being hidden, and cutting leaves gaps in some rooms),
--as-drawn PREFIX... (rooms drawn as-is: no ceiling cut even with --cut-ceilings, no mask handling;
default ls and sp4: Castle Bleck's line art and the Whoa Zone, whose mask nodes are the 2D view itself),
--indoor-pitch DEG (camera tilt in rooms without a background, default 8: less tilt means floors of
upper storeys hang less over the doors below them).
Output: build/maps/rooms/<room>.png and build/maps/raw/ (model-only renders, framing JSON, bg layers).
"""
import argparse
import json
import math
import os
import subprocess
import sys
import time

from PIL import Image, ImageDraw, ImageEnhance, ImageStat

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from spmfiles import OUT_DIR, all_rooms  # noqa: E402
from rooms import rendered_rooms  # noqa: E402
import scripts  # noqa: E402
import stages  # noqa: E402

BLENDER = r'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe'
RAW = os.path.join(OUT_DIR, 'raw')
BG_TILE = ['mac_']  # rooms whose background repeats at screen size instead of stretching to the image
BG_SCREEN = 60      # model units: height of one background tile in those rooms (about one screen)
# Outdoor rooms (not BG_TILE, not indoor) draw their background at the game's screen size, once, where the
# game's camera shows it, instead of stretched to the whole image (which made 5-1's mountains several times
# too big; user, 2026-11-08: all outdoor rooms). BG_STRETCH: rooms left stretched.
# The game's 3D camera (read from Dolphin in 5-1, camPtrTbl[5]): 25 degree fovY, 66 map units from the
# path, 10 above the floor; each layer fills the screen's height. Above and below it, a layer's top and
# bottom rows carry on (sky, ground).
BG_STRETCH = []
BG_BAND_SCREEN = 2 * 66 * math.tan(math.radians(25 / 2))  # map units: the screen's height at the path, 29.3
BG_BAND_EYE = 10   # map units: the camera's height above the main floor
# Backgrounds the game draws darker than their layers: Flopside's rooms (mac_11-mac_19), about 80% of
# Flipside's brightness in game (user's screenshots, 2026-11-04). Only the background, not the model
BG_BRIGHTNESS = {'mac_1': 0.8}
ROOMS = os.path.join(OUT_DIR, 'rooms')
# Rooms shown in stages (stages.py): each later stage is a variant, drawn from its room's map files
# under its own name (rooms/<room>_sN.png) with that stage's parts. Rendered whenever the room is
VARIANTS = stages.variants()


def run_blender(rooms, args):
    """Renders rooms, then their later images (VARIANTS), which take their room's frame from its raw json."""
    bases = [r for r in rooms if r not in VARIANTS]
    later = [r for r in rooms if r in VARIANTS]
    for batch in (bases, later):
        if batch:
            run_blender_batch(batch, args)


def run_blender_batch(rooms, args):
    os.makedirs(RAW, exist_ok=True)
    chunks = [rooms[i::args.jobs] for i in range(args.jobs) if rooms[i::args.jobs]]
    procs = []
    for i, chunk in enumerate(chunks):
        job = os.path.join(RAW, f'_job{i}.json')
        json.dump({'rooms': chunk, 'out': RAW, 'pitch': args.pitch, 'yaw': args.yaw, 'ppu': args.ppu,
                   'max_px': args.max_px, 'samples': 16,
                   'crop_below_floor': args.crop_below_floor, 'floor_margin': args.floor_margin,
                   'crop_above': args.crop_above, 'crop_top': args.crop_top, 'top_margin': args.top_margin,
                   'keep_outlines': args.keep_outlines,
                   'cut_ceilings': args.cut_ceilings, 'cut_ceilings_in': args.cut_ceilings_in, 'as_drawn': args.as_drawn,
                   'indoor_pitch': args.indoor_pitch, 'variants': VARIANTS, 'force': stages.force_parts(),
                   'scripts_file': os.path.join(OUT_DIR, 'scripts.json')}, open(job, 'w'))
        log = open(os.path.join(RAW, f'_job{i}.log'), 'w')
        procs.append((subprocess.Popen([args.blender, '-b', '--factory-startup', '--python',
                                        os.path.join(HERE, 'render_blender.py'), '--', job],
                                       stdout=log, stderr=subprocess.STDOUT), log))
    for p, log in procs:
        p.wait()
        log.close()
    for i in range(len(chunks)):
        for line in open(os.path.join(RAW, f'_job{i}.log'), errors='replace'):
            if line.startswith('ROOM '):
                print(line.rstrip())


def open_retry(path, tries=10):
    """Opens an image; a freshly written file can be locked for a moment (e.g. by a virus scanner)."""
    for i in range(tries):
        try:
            return Image.open(path).convert('RGBA')
        except PermissionError:
            if i == tries - 1:
                raise
            time.sleep(1)


def draw_arrows(img, arrows, ppu):
    """Plain white arrows (a line and a triangle head, dark edge) for render_blender.DRAW_ARROWS."""
    if not arrows:
        return
    import math
    d = ImageDraw.Draw(img)
    width, head = max(3, round(ppu * 0.4)), max(10, round(ppu * 1.6))
    for outline, w, grow in (((20, 20, 20, 255), width + 4, 2), ((255, 255, 255, 255), width, 0)):
        for x0, y0, x1, y1 in arrows:
            ang = math.atan2(y1 - y0, x1 - x0)
            h = head + grow
            bx, by = x1 - math.cos(ang) * h, y1 - math.sin(ang) * h  # where the head's base sits
            d.line([(x0, y0), (bx, by)], fill=outline, width=w)
            tip = (x1 + math.cos(ang) * grow, y1 + math.sin(ang) * grow)
            side = (math.cos(ang + math.pi / 2) * h * 0.6, math.sin(ang + math.pi / 2) * h * 0.6)
            d.polygon([tip, (bx + side[0], by + side[1]), (bx - side[0], by - side[1])], fill=outline)


def bg_band(layer, info, w, h):
    """One background layer for an outdoor room: one screen tall, repeated sideways, with its bottom where
    the bottom of the game's screen is when the camera stands over the main floor; its top row carries on
    above and its bottom row below. Follows the image's slant (the camera's yaw) along the room."""
    ppu, (r0, _, _), (u0, u1, _) = info['ppu'], info['right'], info['up']
    lh = max(1, round(BG_BAND_SCREEN * u1 * ppu))
    lw = max(1, round(layer.width * lh / layer.height))
    layer = layer.convert('RGBA').resize((lw, lh), Image.LANCZOS)
    # the screen's top at column 0, on the path (z = 0): map x there is x0 / r0
    y_top = info['main_floor'] + BG_BAND_EYE + BG_BAND_SCREEN / 2
    top = round((info['y0'] - info['x0'] / r0 * u0 - y_top * u1) * ppu)
    slant = u0 / r0  # rows the band moves up per column
    pad = int(abs(slant) * w) + 2
    strip = Image.new('RGBA', (w, h + 2 * pad))
    # Past the layer's top and bottom: a solid layer that looks alike at both ends (a star field) repeats;
    # one that changes from top to bottom (a sky gradient, a sunset) carries on in its edge row's average
    # colour (repeating it bands, carrying the row itself streaks); one with gaps (clouds, mountains) stops
    row_top, row_bot = layer.crop((0, 0, lw, 1)), layer.crop((0, lh - 1, lw, lh))
    def mean(row):
        return tuple(round(v) for v in ImageStat.Stat(row).mean)
    solid_top, solid_bot = (r.getchannel('A').getextrema()[0] == 255 for r in (row_top, row_bot))
    ct, cb = mean(row_top), mean(row_bot)
    repeat = solid_top and solid_bot and sum(abs(p - q) for p, q in zip(ct[:3], cb[:3])) <= 30
    t = top + pad
    for x in range(0, w, lw):
        strip.paste(layer, (x, t))
        if repeat:
            for y in [*range(t - lh, -lh, -lh), *range(t + lh, strip.height, lh)]:
                strip.paste(layer, (x, y))
    if not repeat and solid_top and t > 0:
        strip.paste(ct, (0, 0, w, t))
    if not repeat and solid_bot and t + lh < strip.height:
        strip.paste(cb, (0, t + lh, w, strip.height))
    # output (px, py) shows strip (px, py + pad + px * slant)
    return strip.transform((w, h), Image.AFFINE, (1, 0, 0, slant, 1, pad), Image.BILINEAR)


def cut_locks(room, info):
    """Each lock's picture (rooms/locks/<room>_<name>.png), cut from the locks-only render
    (raw/<room>_locks.png) round its position; the viewer draws it over the room until it's opened.
    Its place in the room image goes into the room's raw json ('img', 'left', 'top')."""
    if not info.get('locks'):
        return
    sheet = open_retry(os.path.join(RAW, f'{room}_locks.png'))
    r = round(8 * info['ppu'])  # model units round the lock's position to look in
    os.makedirs(os.path.join(ROOMS, 'locks'), exist_ok=True)
    for lock in info['locks']:
        x, y = (round(v) for v in lock['px'])
        box = (max(0, x - r), max(0, y - r), min(sheet.width, x + r), min(sheet.height, y + r))
        piece = sheet.crop(box)
        bb = piece.getchannel('A').getbbox()
        if not bb:
            continue
        name = f"{room}_{lock['name']}.png"
        piece.crop(bb).save(os.path.join(ROOMS, 'locks', name), optimize=True)
        lock.update(img=f'rooms/locks/{name}', left=box[0] + bb[0], top=box[1] + bb[1])
    with open(os.path.join(RAW, f'{room}.json'), 'w') as f:
        json.dump(info, f, indent=1)


def composite(room, box=False):
    """Background layers (tiled sideways, scaled to the image height, back to front; or tiled both ways at
    screen size for BG_TILE rooms) + the model render."""
    info = json.load(open(os.path.join(RAW, f'{room}.json')))
    if 'error' in info:
        return info
    model = open_retry(os.path.join(RAW, f'{room}.png'))
    w, h = model.size
    out = Image.new('RGBA', (w, h), (0, 0, 0, 255))
    tile = any(room.startswith(p) for p in BG_TILE)
    if (not tile and not info.get('indoor') and info.get('main_floor') is not None
            and not any(room.startswith(p) for p in BG_STRETCH)):
        for bg in info['backgrounds']:
            out.alpha_composite(bg_band(open_retry(os.path.join(RAW, bg['file'])), info, w, h))
        tile, bgs = False, []
    else:
        bgs = info['backgrounds']
    for bg in bgs:
        layer = open_retry(os.path.join(RAW, bg['file']))
        # tiled rooms: the layer is one screen tall (BG_SCREEN model units) and repeats both ways, so a
        # pattern background (Flipside/Flopside) keeps its in-game size in tall rooms
        lh = min(h, round(BG_SCREEN * info['ppu'])) if tile else h
        lw = max(1, round(layer.width * lh / layer.height))
        layer = layer.resize((lw, lh), Image.LANCZOS)
        strip = Image.new('RGBA', (w, h))
        for y in range(h - lh, -lh, -lh) if tile else [0]:
            for x in range(0, w, lw):
                strip.paste(layer, (x, y))
        out.alpha_composite(strip)
    brightness = next((v for p, v in BG_BRIGHTNESS.items() if room.startswith(p)), 1.0)
    if brightness != 1.0:
        out = ImageEnhance.Brightness(out).enhance(brightness)
    out.alpha_composite(model)
    cut_locks(room, info)
    draw_arrows(out, info.get('arrows', ()), info['ppu'])
    if box:
        ImageDraw.Draw(out).rectangle([0, 0, w - 1, h - 1], outline=(40, 40, 40, 255), width=2)
    os.makedirs(ROOMS, exist_ok=True)
    out.convert('RGB').save(os.path.join(ROOMS, f'{room}.png'), optimize=True)
    return info


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('rooms', nargs='*')
    ap.add_argument('--pitch', type=float, default=18)
    ap.add_argument('--yaw', type=float, default=5)
    ap.add_argument('--ppu', type=float, default=8)
    ap.add_argument('--max-px', type=int, default=8000)
    ap.add_argument('--jobs', type=int, default=3)
    ap.add_argument('--box', action='store_true')
    ap.add_argument('--crop-below-floor', nargs='*', metavar='PREFIX',
                    default=['he3', 'he4_10', 'mi1_01', 'mi1_08', 'mi1_09', 'mi1_10', 'mi1_11', 'mi2_01',
                             'ta1_02', 'ta2_05', 'mac_'])  # ta2_05: its water reaches far below the floors (user, 2026-11-04)
    ap.add_argument('--crop-above', nargs='*', metavar='PREFIX',
                    default=['ta1_03', 'ta3_01', 'ta3_04', 'sp1', 'sp3',
                             'an1_01', 'an1_02', 'an1_03', 'an1_04', 'an2_02', 'an2_08'])
    ap.add_argument('--crop-top', nargs='*', metavar='PREFIX', default=['an2_05'])
    ap.add_argument('--keep-outlines', nargs='*', metavar='PREFIX', default=['sp1', 'sp4'])
    ap.add_argument('--top-margin', type=float, default=6)
    ap.add_argument('--floor-margin', type=float, default=12)
    ap.add_argument('--cut-ceilings', action='store_true')
    ap.add_argument('--cut-ceilings-in', nargs='*', default=[], metavar='PREFIX')
    ap.add_argument('--as-drawn', nargs='*', default=['ls', 'sp4'], metavar='PREFIX')
    ap.add_argument('--indoor-pitch', type=float, default=8)
    ap.add_argument('--composite-only', action='store_true')
    ap.add_argument('--blender', default=BLENDER)
    args = ap.parse_args()
    rooms = args.rooms or rendered_rooms()  # rooms sharing a map file share one image (rooms.drawn_as)
    rooms += [v for v, base in VARIANTS.items() if base in rooms and v not in rooms]
    unknown = set(rooms) - set(all_rooms()) - set(VARIANTS)
    if unknown:
        sys.exit(f'no such room: {sorted(unknown)}')
    if not args.composite_only:
        scripts.main()
        run_blender(rooms, args)
    bad = composite_all(rooms, args.box)
    if bad and not args.composite_only:
        # a render file that can't be read back (seen when something else locks it mid-write): one more try
        print(f're-rendering unreadable output: {bad}')
        run_blender(bad, args)
        bad = composite_all(bad, args.box)
    for room in bad:
        print(f'{room}: output still unreadable')


def composite_all(rooms, box):
    """Composites each room; returns the rooms whose files couldn't be read."""
    bad = []
    for room in rooms:
        try:
            info = composite(room, box)
        except (OSError, SyntaxError) as e:  # PIL raises SyntaxError for some broken PNGs
            print(f'{room}: {e}')
            bad.append(room)
            continue
        if 'error' in info:
            print(f'{room}: FAILED {info["error"]}')
    return bad


if __name__ == '__main__':
    main()
