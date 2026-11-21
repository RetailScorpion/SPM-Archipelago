"""Whole-area overview images from the room renders and graph.json.

    py -3 tools/maprender/overview.py [--scale 0.3]

One image per area (build/maps/overview/<area>.png): each chapter's levels as bands (he1, he2, ...),
Flipside, Flopside, and the other rooms. Rooms are placed by their connections: a door in a room's
right third puts the next room to its right, left third to the left, anything else (pipes, doors in
the middle of a room) below. Lines join connected doors; exits to rooms in another band or area get
a label. Also writes build/maps/overview/layout.json (room rectangles in each overview) and
build/maps/viewer.html.
"""
import argparse
import json
import os
import sys
from collections import deque

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from spmfiles import OUT_DIR  # noqa: E402
from rooms import AREA_TITLES  # noqa: E402
import stages  # noqa: E402
import bosses  # noqa: E402

OV = os.path.join(OUT_DIR, 'overview')
GAP = 60
EDGE_COLORS = {'map_door': (255, 200, 40), 'pipe': (60, 220, 90), 'door': (80, 170, 255),
               'black_hole': (200, 90, 255), 'warp': (255, 255, 255), 'elevator': (255, 120, 60),
               'pit': (80, 230, 230)}


def font(size):
    for f in ('arialbd.ttf', 'arial.ttf', 'DejaVuSans.ttf'):
        try:
            return ImageFont.truetype(f, size)
        except OSError:
            pass
    return ImageFont.load_default()


def band_of(room):
    """Level id for chapter rooms (he1), the area for the rest."""
    return room[:3] if room[2].isdigit() and not room.startswith(('mac', 'dan')) else None


def door_side(room, door):
    if not door.get('px'):
        return 'below'
    x = door['px'][0] / room['width']
    if door.get('kind') == 'pipe':
        return 'below'
    return 'right' if x > 0.65 else 'left' if x < 0.35 else 'below'


def overlaps(a, rects):
    x, y, w, h = a
    return any(x < X + W + GAP // 2 and X < x + w + GAP // 2 and y < Y + H + GAP // 2 and Y < y + h + GAP // 2
               for X, Y, W, H in rects)


def search_offsets(n=400, unit=40):
    """0, then mostly forwards (down/right) with an occasional step back, so levels read in order."""
    fwd, back = 0, 0
    for i in range(n):
        if i % 4 == 3:
            back -= unit
            yield back
        else:
            yield fwd
            fwd += unit


# Bands laid out along their main route instead of automatically: the route's rooms in a row, left to
# right in playing order, bottoms lined up; every other room sits beside the room it connects to, on
# the side of the door it uses, at that door's height.
ROUTES = {
    'an3': ['an3_01', 'an3_12', 'an3_13', 'an3_02', 'an3_03', 'an3_14', 'an3_15', 'an3_04'],  # Overthere Stair 1-7
}


def route_layout(group, route, graph, scale):
    rooms = graph['rooms']
    size = {r: (max(40, int(rooms[r]['width'] * scale)), max(30, int(rooms[r]['height'] * scale))) for r in group}
    route = [r for r in route if r in group]
    # side rooms: (route room, side, door y in route room px) for each room off the route
    sides = {r: {'left': [], 'right': []} for r in route}
    loose = []
    for r in group:
        if r in route:
            continue
        hit = None
        for a in route:
            d = next((d for d in rooms[a]['doors'] if d['dest_room'] == r and d.get('px')), None)
            if d:
                side = 'left' if d['px'][0] / rooms[a]['width'] < 0.5 else 'right'
                hit = (a, side, d['px'][1] * scale)
                break
        if hit:
            sides[hit[0]][hit[1]].append((hit[2], r))
        else:
            loose.append(r)
    base = max(size[r][1] for r in route)  # every route room's bottom sits on this line
    pos, x = {}, 0
    for i, a in enumerate(route):
        left = sides[a]['left']
        lw = max((size[r][0] for _, r in left), default=0)
        x += (lw + GAP) if lw else 0
        aw, ah = size[a]
        ay = base - ah
        pos[a] = (x, ay, aw, ah)
        for side, items in (('left', left), ('right', sides[a]['right'])):
            next_y = -1e9
            for dy, r in sorted(items):
                w, h = size[r]
                y = max(ay + dy - h * 0.65, next_y)  # door roughly level with the side room's floor
                rx = x - GAP // 2 - w if side == 'left' else x + aw + GAP // 2
                pos[r] = (int(rx), int(y), w, h)
                next_y = y + h + GAP // 2
        rw = max((size[r][0] for _, r in sides[a]['right']), default=0)
        x += aw + GAP + ((rw + GAP // 2) if rw else 0)
    for r in loose:  # nothing to attach to (e.g. a cutscene room): at the end of the row
        w, h = size[r]
        pos[r] = (x, base - h, w, h)
        x += w + GAP
    minx = min(v[0] for v in pos.values())
    miny = min(v[1] for v in pos.values())
    return {r: (x - minx, y - miny, w, h) for r, (x, y, w, h) in pos.items()}


def layout(group, graph, scale):
    """{room: (x, y, w, h)} for one band, in band pixels."""
    rooms = graph['rooms']
    size = {r: (max(40, int(rooms[r]['width'] * scale)), max(30, int(rooms[r]['height'] * scale))) for r in group}
    adj = {r: [] for r in group}
    for e in graph['edges']:
        if e['from'] in adj and e['to'] in adj:
            adj[e['from']].append(e)
    pos = {}
    order = sorted(group)
    for start in order:
        if start in pos:
            continue
        # a new component goes below everything placed so far
        y0 = max((y + h for x, y, w, h in pos.values()), default=-GAP) + GAP
        pos[start] = (0, y0) + size[start]
        q = deque([start])
        while q:
            a = q.popleft()
            ax, ay, aw, ah = pos[a]
            for e in sorted(adj[a], key=lambda e: e['to']):
                b = e['to']
                if b in pos:
                    continue
                bw, bh = size[b]
                da = next((d for d in rooms[a]['doors'] if d['name'] == e['from_door']), {})
                db = next((d for d in rooms[b]['doors'] if d['name'] == e['to_door']), {})
                side = door_side(rooms[a], da)
                dy_a = da['px'][1] * scale if da.get('px') else ah / 2
                dy_b = db['px'][1] * scale if db.get('px') else bh / 2
                dx_a = da['px'][0] * scale if da.get('px') else aw / 2
                if side == 'right':
                    cand, step = (ax + aw + GAP, ay + dy_a - dy_b), (0, 1)
                elif side == 'left':
                    cand, step = (ax - GAP - bw, ay + dy_a - dy_b), (0, 1)
                else:
                    cand, step = (ax + dx_a - bw / 2, ay + ah + GAP), (1, 0)
                x, y = cand
                rects = list(pos.values())
                for k in search_offsets():
                    tx, ty = x + step[0] * k, y + step[1] * k
                    if not overlaps((tx, ty, bw, bh), rects):
                        x, y = tx, ty
                        break
                pos[b] = (int(x), int(y), bw, bh)
                q.append(b)
    minx = min(x for x, y, w, h in pos.values())
    miny = min(y for x, y, w, h in pos.values())
    return {r: (x - minx, y - miny, w, h) for r, (x, y, w, h) in pos.items()}


def render_area(area_id, graph, scale):
    rooms = graph['rooms']
    # only the rooms the doors lead to (graph.py linked); the others are shown only while following Mario
    # (rooms sharing another's image, Chapter 6's repeated gates, are drawn once: graph.py same_as)
    members = sorted(r for r, v in rooms.items() if v['area'] == area_id and v.get('linked', True)
                     and not v.get('same_as'))
    if not members:
        return None
    bands = {}
    for r in members:
        bands.setdefault(band_of(r) or area_id, []).append(r)
    title_f, band_f, room_f, small_f = font(48), font(32), font(18), font(15)
    placed, y = {}, 90
    band_titles = []
    width = 0
    for band in sorted(bands):
        lay = (route_layout(bands[band], ROUTES[band], graph, scale) if band in ROUTES
               else layout(bands[band], graph, scale))
        band_titles.append((band, y))
        y += 50
        for r, (x, ry, w, h) in lay.items():
            placed[r] = (x + 40, ry + y, w, h)
            width = max(width, x + 40 + w + 40)
        y += max(ry + h for _, ry, _, h in lay.values()) + 120
    W, H = max(width, 1200), y
    canvas = Image.new('RGB', (W, H), (24, 24, 30))
    dr = ImageDraw.Draw(canvas)
    dr.text((40, 20), AREA_TITLES.get(area_id, area_id), fill=(255, 255, 255), font=title_f)
    for band, by in band_titles:
        if band != area_id:
            dr.text((40, by), band, fill=(200, 200, 210), font=band_f)
    for r, (x, ry, w, h) in placed.items():
        im = Image.open(os.path.join(OUT_DIR, rooms[r]['image'])).convert('RGB').resize((w, h), Image.LANCZOS)
        canvas.paste(im, (x, ry))
        dr.rectangle([x - 1, ry - 1, x + w, ry + h], outline=(90, 90, 100))
        dr.text((x + 4, ry - 22), r, fill=(255, 255, 255), font=room_f)

    def door_point(room, door_name):
        x, y, w, h = placed[room]
        d = next((d for d in rooms[room]['doors'] if d['name'] == door_name), None)
        if d and d.get('px'):
            return x + d['px'][0] * w / rooms[room]['width'], y + d['px'][1] * h / rooms[room]['height']
        return x + w / 2, y + h / 2

    drawn, labels, exits = set(), [], []
    for e in graph['edges']:
        if e['from'] not in placed:
            continue
        p = door_point(e['from'], e['from_door'])
        col = EDGE_COLORS.get(e['kind'], (255, 255, 255))
        if e['to'] in placed:
            key = tuple(sorted([(e['from'], e['from_door']), (e['to'], e['to_door'] or '')]))
            if key in drawn:
                continue
            drawn.add(key)
            q = door_point(e['to'], e['to_door'])
            dr.line([p, q], fill=col, width=3)
            for c, room, door in ((p, e['from'], e['from_door']), (q, e['to'], e['to_door'])):
                d = next((d for d in rooms[room]['doors'] if d['name'] == door), {})
                if d.get('direction'):  # elevator: a big arrow, up or down
                    s, up = 13, d['direction'] == 'up'
                    pts = ([(c[0], c[1] - s), (c[0] - s, c[1] + s * 0.8), (c[0] + s, c[1] + s * 0.8)] if up else
                           [(c[0], c[1] + s), (c[0] - s, c[1] - s * 0.8), (c[0] + s, c[1] - s * 0.8)])
                    dr.polygon(pts, fill=col, outline=(0, 0, 0), width=2)
                else:
                    dr.ellipse([c[0] - 6, c[1] - 6, c[0] + 6, c[1] + 6], fill=col, outline=(0, 0, 0))
        elif not e['inferred']:
            dr.ellipse([p[0] - 6, p[1] - 6, p[0] + 6, p[1] + 6], fill=col, outline=(0, 0, 0))
            labels.append((p, f"-> {e['to']}", col))
            if e['to'] in rooms:  # a way into another area: clickable in the viewer
                exits.append({'room': e['from'], 'door': e['from_door'], 'to': e['to'],
                              'area': rooms[e['to']]['area'], 'px': [round(p[0]), round(p[1])]})
    # exits to other areas: label to the right of the dot; where that would overlap another label (doors
    # side by side, e.g. the chapter doors on Flipside Tower) the label stands upright above its own dot
    taken = []
    for p, text, col in sorted(labels, key=lambda t: (t[0][1], t[0][0])):
        x0, y0, x1, y1 = dr.textbbox((0, 0), text, font=small_f, stroke_width=2)
        tw, th = x1 - x0, y1 - y0
        box = (p[0] + 9, p[1] - th / 2 - 2, p[0] + 13 + tw, p[1] + th / 2 + 2)
        free = lambda b: not any(b[0] < t[2] and t[0] < b[2] and b[1] < t[3] and t[1] < b[3] for t in taken)
        if free(box) and not any(abs(q[0] - p[0]) < tw + 20 and abs(q[1] - p[1]) < th
                                 for q, _, _ in labels if q is not p):
            taken.append(box)
            dr.text((p[0] + 11 - x0, p[1] - th / 2 - y0), text, fill=col, font=small_f, stroke_width=2,
                    stroke_fill=(0, 0, 0))
            continue
        img = Image.new('RGBA', (tw + 6, th + 6), (0, 0, 0, 0))
        ImageDraw.Draw(img).text((3 - x0, 3 - y0), text, fill=col, font=small_f, stroke_width=2,
                                 stroke_fill=(0, 0, 0))
        img = img.rotate(90, expand=True)
        px, py = int(p[0] - img.width / 2), int(p[1] - 9 - img.height)
        canvas.paste(img, (px, py), img)
        taken.append((px, py, px + img.width, py + img.height))
    os.makedirs(OV, exist_ok=True)
    canvas.save(os.path.join(OV, f'{area_id}.png'), optimize=True)
    return {'image': f'overview/{area_id}.png', 'width': W, 'height': H,
            'rooms': {r: list(v) for r, v in placed.items()}, 'exits': exits}


VIEWER = r"""<!doctype html>
<html><head><meta charset="utf-8"><title>SPM room map</title>
<style>
body{margin:0;font:14px sans-serif;background:#18181e;color:#eee}
#bar{position:fixed;top:0;left:0;right:0;z-index:5;background:#26262e;box-shadow:0 2px 6px rgba(0,0,0,.5)}
header{padding:8px 12px}
button{margin:2px;background:#33333d;color:#eee;border:1px solid #555;padding:4px 8px;cursor:pointer}
button.on{background:#556}
#wrap{position:relative;display:inline-block}
#wrap img.map{display:block;max-width:none}
.hot{position:absolute;border:2px solid transparent;cursor:pointer}
.hot:hover{border-color:#ff0}
.hot.flash{border-color:#ff0;box-shadow:0 0 0 4px rgba(255,255,0,.4)}
.door{position:absolute;border:2px solid #fc3;background:rgba(255,200,40,.25);font-size:11px;color:#fff;
 text-shadow:0 0 3px #000;white-space:nowrap}
.door.pipe{border-color:#3d5}.door.door{border-color:#4af}.door.black_hole{border-color:#c5f}.door.pit{border-color:#5ee;background:rgba(80,230,230,.3)}.door.elevator{border-color:#f74;border-width:3px;background:rgba(255,120,60,.35);font-weight:bold}
.chk.flash{outline:3px solid #ff0;outline-offset:3px;box-shadow:0 0 14px 6px rgba(255,255,0,.7);z-index:3}
.door.flash{box-shadow:0 0 0 4px #ff0,0 0 14px 6px rgba(255,255,0,.6)}
.door.away{border-color:#fff;background:rgba(255,255,255,.3);cursor:pointer;font-weight:bold}
.door.ret{border-color:#f5a;background:rgba(255,90,170,.3)}  /* a chapter's door back to the tower */
.star{position:absolute;transform:translate(-50%,-100%);margin-top:-18px;padding:2px 6px;font-size:12px;
 color:#fff;background:rgba(0,0,0,.55);border:2px solid #fd4;border-radius:4px;white-space:nowrap;z-index:2}
.star.go{cursor:pointer}.star.go:hover{background:#fd4;color:#000}
.label{position:absolute;font-size:11px;color:#fff;text-shadow:0 0 3px #000;white-space:nowrap}
.exit{position:absolute;width:18px;height:18px;margin:-12px 0 0 -12px;border-radius:50%;border:3px solid #fff;
 background:rgba(255,255,255,.25);cursor:pointer;z-index:2}
.exit:hover{background:#ff0}
.chk{position:absolute;transform:translate(-50%,-50%);cursor:pointer;z-index:3}
.chk.dot{width:12px;height:12px;border-radius:50%;background:#e22;border:2px solid #fff;box-shadow:0 0 3px #000}
.chk.icon{width:32px;height:32px;filter:drop-shadow(0 0 2px #000) drop-shadow(0 0 2px #fff)}
.small .chk.dot{width:7px;height:7px;border-width:1px}
.small .chk.icon{width:20px;height:20px}
.chk.approx{outline:2px dashed #ff0;outline-offset:1px}
.lock{position:absolute;pointer-events:none}
.boss{position:absolute;transform:translate(-50%,-100%);max-width:96px;max-height:96px;z-index:2;pointer-events:auto;filter:drop-shadow(0 0 2px #000)}
.small .boss{max-width:40px;max-height:40px}
.chk.done{opacity:.25;filter:grayscale(1)}
#info{padding:6px 12px;min-height:20px;background:#1f1f26}
#ap{float:right;padding:6px 12px}
#ap input{background:#18181e;color:#eee;border:1px solid #555;padding:3px 6px;width:130px}
#ap input.slot{width:110px}#ap input.pw{width:80px}
#apstate{display:inline-block;width:10px;height:10px;border-radius:50%;background:#666;margin:0 4px 0 8px}
#apstate.ok{background:#3d5}#apstate.busy{background:#fc3}#apstate.bad{background:#e33}
/* The side column: Items, Coords and Tracker, each shown with its button; open ones share the height */
#side{position:fixed;right:0;bottom:0;z-index:4;width:322px;display:none;flex-direction:column;
 background:#2a2a30;border-left:2px solid #444}
#side.open{display:flex}
#side > div{display:none;overflow-y:auto;padding:8px 10px 12px;min-height:0;box-sizing:border-box}
#side > div.open{display:block;flex:1 1 0}
#side > #coords.open{flex:0 0 auto;max-height:45%}
#side > div.open ~ div.open{border-top:2px solid #444}
#side.open ~ #wrap{margin-right:324px}  /* room to scroll the map's right edge out from under the column */
#side.open ~ #list{margin-right:324px}
#side h3{margin:10px 0 4px;font-size:12px;font-weight:normal;color:#aab;text-transform:uppercase;letter-spacing:1px}
#coords .pos{font:bold 16px Consolas,monospace;margin:4px 0}
#coords .here{color:#aab;margin-bottom:6px}
#coords li, #tracker li{margin:2px 0;font-size:13px}
#coords ul, #tracker ul{margin:2px 0 6px;padding-left:18px}
#tracker summary{cursor:pointer;padding:2px 0;font-size:13px}
#tracker summary .st{float:right;color:#aab}
#tracker summary .st.full{color:#3d5}
#tracker summary.here{color:#fd4}
#tracker li.got{color:#777;text-decoration:line-through}
#tracker li.out, #coords li.out, #tracker summary.out{color:#5c5c66}  /* connected: out of logic */
#tracker li.go, #coords li.go{cursor:pointer}
#tracker li.go:hover, #coords li.go:hover{color:#ff0}
.muted{color:#aab;font-size:12px}
#mario{position:absolute;width:30px;height:30px;transform:translate(-50%,-100%);transform-origin:50% 100%;
 transition:transform .25s ease-out;z-index:3;pointer-events:none;
 filter:drop-shadow(0 0 2px #000) drop-shadow(0 0 3px #ff0)}
.small #mario{width:20px;height:20px}
#items h3{margin:10px 0 4px;font-size:12px;font-weight:normal;color:#aab;text-transform:uppercase;letter-spacing:1px}
.tiles{display:flex;flex-wrap:wrap;gap:6px}
.tile{position:relative;width:52px;height:52px;border-radius:8px;background:#1b1b20;border:2px solid #3a3a42;
 display:flex;align-items:center;justify-content:center;cursor:pointer}
.tile img{width:40px;height:40px;opacity:.28;filter:grayscale(1)}
.tile.have{border-color:#c9c9d6;background:#34343c}
.tile.have img{opacity:1;filter:none}
.tile .n{position:absolute;right:2px;bottom:0;font-size:11px;font-weight:bold;text-shadow:0 0 3px #000}
#menu{position:fixed;z-index:6;display:none;background:#2a2a30;border:1px solid #555;box-shadow:0 4px 10px rgba(0,0,0,.6);min-width:180px}
#menu div{padding:6px 12px;cursor:pointer;white-space:nowrap}
#menu div:hover{background:#556}
#menu div:first-child{border-bottom:1px solid #444;color:#aab}
#list{padding:6px 12px 6px 30px;display:none}
#list li{margin:2px 0}
a{color:#9cf}
</style></head><body>
<div id="bar"><div id="ap"><input id="apserver" placeholder="server (localhost:38281)">
<input id="apslot" class="slot" placeholder="slot name"><input id="appw" class="pw" type="password" placeholder="password">
<button id="apgo">Connect</button><span id="apstate" title="not connected"></span><span id="apmsg"></span></div>
<header id="tabs"></header><div id="info"></div></div>
<div id="side"><div id="items"></div><div id="coords"></div><div id="tracker"></div></div><div id="spacer"></div><ul id="list"></ul><div id="wrap"></div>
<script>
const G = __GRAPH__, L = __LAYOUT__, C = __CHECKS__, ITEMS = __ITEMS__, LEVELS = __LEVELS__, BOSSES = __BOSSES__;
const tabs = document.getElementById('tabs'), wrap = document.getElementById('wrap'),
      info = document.getElementById('info'), list = document.getElementById('list');
// The tabs and info line stay on screen (fixed); the spacer keeps the page below them
const bar = document.getElementById('bar'), spacer = document.getElementById('spacer');
new ResizeObserver(() => { spacer.style.height = bar.offsetHeight + 'px'; }).observe(bar);
const ICON_CATEGORIES = ['pixl', 'optional_pixl', 'character'];
// Collected checks, marked by hand (kept in this browser only)
let done = {};
try { done = JSON.parse(localStorage.getItem('spm_done') || '{}'); } catch (e) {}
function save(){ try { localStorage.setItem('spm_done', JSON.stringify(done)); } catch (e) {} }
// From the Archipelago server, while connected: checked location ids, and received items by id
let serverChecked = new Set(), received = {}, connected = false;
// Items marked by hand (when not connected), by id
let haveByHand = {};
try { haveByHand = JSON.parse(localStorage.getItem('spm_items') || '{}'); } catch (e) {}
// From the running game (serve.py's game.json): locations the loaded save collected, or the server has
let game = null, gameChecked = new Set();
function isDone(c){ return !!done[c.name] || serverChecked.has(c.id) || gameChecked.has(c.id); }
let view = null;  // what's on screen: {room} or {area}, for Mario's marker
const markers = [];  // [element, check] on screen, to update when the server sends news
function refreshMarkers(){ for (const [e, c] of markers) e.classList.toggle('done', isDone(c)); }

function describe(c){
  return `<b>${c.label}</b> &nbsp; ${c.name}` + (c.where ? ` &nbsp; <i>${c.where}</i>` : '') +
    (c.via ? ` &nbsp; (inside, through door ${c.via})` : (c.exact ? '' : ' &nbsp; (spot approximate)')) +
    (isDone(c) ? ' &nbsp; [collected]' : '');
}
// A marker for a check at (x, y) in the image on screen: the game's icon for Pixls and characters,
// a red dot for the rest
function marker(c, x, y){
  const icon = ICON_CATEGORIES.includes(c.category);
  const e = document.createElement(icon ? 'img' : 'div');
  if (icon) e.src = 'icons/' + c.item + '.png';
  e.className = 'chk ' + (icon ? 'icon' : 'dot') + (c.exact ? '' : ' approx') + (isDone(c) ? ' done' : '');
  markers.push([e, c]);
  Object.assign(e.style, {left: x + 'px', top: y + 'px'});
  e.title = c.label + (c.exact ? '' : ' (approximate spot)') + '\nClick: details. Right-click: mark collected.';
  e.onclick = ev => { ev.stopPropagation(); info.innerHTML = describe(c); };
  e.oncontextmenu = ev => { ev.preventDefault(); ev.stopPropagation();
    if (done[c.name]) delete done[c.name]; else done[c.name] = 1; save();
    e.classList.toggle('done', isDone(c)); info.innerHTML = describe(c); };
  wrap.appendChild(e);
}
// Bosses (bosses.py): each one's model in its room until it's beaten
function bossBeaten(b){ return reached(b.cond[0], b.cond[1], b.chapter); }
// Locks on doors and gates (graph.py 'locks', each its own picture cut from the render): drawn until the game
// opens them, which the room's script decides from its chapter's story value or a save flag. Without the game
// they're all drawn (a new file's state)
function lockShut(l){
  if (!game || !game.ok) return true;
  const story = game.story ? game.story[l.chapter - 1] : 0;
  return l.while.every(([kind, n]) => kind === 'story_below' ? story < n : kind === 'story_from' ? story >= n
    : kind === 'flag_set' ? flagSet(n) : !flagSet(n));
}
let locksDrawn = null, locksOn = null;  // what's drawn, on which picture: the game updates twice a second
function drawLocks(){
  if (!view) return;
  const rooms = view.area ? Object.keys(L[view.area].rooms) : [view.room];
  const key = (view.area || view.room) + ':' + rooms.map(r => (G.rooms[r] && G.rooms[r].locks || []).map(lockShut).join()).join(';');
  const map = wrap.querySelector('img.map');
  if (key === locksDrawn && map === locksOn) return;  // nothing changed (a new view makes a new picture)
  locksDrawn = key; locksOn = map;
  wrap.querySelectorAll('img.lock').forEach(e => e.remove());
  for (const r of rooms) {
    const R = G.rooms[r]; if (!R || !R.locks) continue;
    const box = view.area ? L[view.area].rooms[r] : [0, 0, R.width, R.height], k = box[2] / R.width;
    for (const l of R.locks) {
      if (!lockShut(l)) continue;
      const e = document.createElement('img'); e.className = 'lock'; e.src = l.img; e.title = 'Locked';
      e.onload = () => { e.style.width = e.naturalWidth * k + 'px'; };
      Object.assign(e.style, {left: box[0] + l.left * k + 'px', top: box[1] + l.top * box[3] / R.height + 'px'});
      wrap.insertBefore(e, map ? map.nextSibling : wrap.firstChild);  // just above the picture, under doors and pins
    }
  }
}
function drawBosses(){
  wrap.querySelectorAll('img.boss').forEach(e => e.remove());
  if (!view) return;
  for (const b of BOSSES) {
    if (bossBeaten(b)) continue;
    let x = b.px[0], y = b.px[1];
    if (view.area) {
      const box = L[view.area].rooms[b.room]; if (!box) continue;
      const R = G.rooms[b.room]; x = box[0] + x * box[2] / R.width; y = box[1] + y * box[3] / R.height;
    } else if (view.room !== b.room) continue;
    const e = document.createElement('img'); e.className = 'boss'; e.src = b.model ? 'icons/boss_' + b.model + '.png' : 'icons/' + b.item + '.png';
    e.title = b.label + ' (not beaten yet)';
    Object.assign(e.style, {left: x + 'px', top: y + 'px'});
    wrap.appendChild(e);
  }
}
// Rooms the map opens by hand: those the doors lead to from Flipside (graph.py linked). The rest (Mario's
// house, rooms only a boss fight's script goes to, ...) are shown only while following Mario.
function linked(r){ return !!(G.rooms[r] && G.rooms[r].linked); }
function onMap(c){ return !!c.room && linked(c.room); }
function setTab(a){ [...tabs.children].forEach(b => { if (b.dataset.a) b.classList.toggle('on', b.dataset.a === a); }); list.style.display = 'none'; }
function showArea(a, focusRoom){
  setTab(a);
  const l = L[a]; location.hash = a; wrap.className = 'small'; markers.length = 0; view = {area: a};
  wrap.innerHTML = `<img class="map" src="${l.image}" width="${l.width}" height="${l.height}">`;
  info.textContent = 'Click a room to open it, or a white ring to go to that area. Markers: click for details, right-click to mark collected.';
  for (const [r, [x, y, w, h]] of Object.entries(l.rooms)) {
    const d = document.createElement('div'); d.className = 'hot'; d.title = r + ' ' + G.rooms[r].name;
    Object.assign(d.style, {left: x+'px', top: y+'px', width: w+'px', height: h+'px'}); d.onclick = () => showRoom(r);
    wrap.appendChild(d);
    if (r === focusRoom) { d.classList.add('flash'); setTimeout(() => d.scrollIntoView({block: 'center', inline: 'center'}), 0); }
  }
  for (const x of (l.exits || [])) {
    if (!L[x.area] || x.area === a) continue;
    const e = document.createElement('div'); e.className = 'exit'; e.title = 'To ' + x.to + ' (' + x.area + ')';
    Object.assign(e.style, {left: (x.px[0] + 3)+'px', top: (x.px[1] + 3)+'px'}); e.onclick = () => showArea(x.area, x.to);
    wrap.appendChild(e);
  }
  for (const c of C) {
    const box = c.room && l.rooms[c.room]; if (!box) continue;
    const R = G.rooms[c.room], [x, y, w, h] = box;
    marker(c, x + c.px[0] * w / R.width, y + c.px[1] * h / R.height);
  }
  placeMario(); drawBosses(); drawLocks();
}
// focusDoor: the door Mario comes out of (the name the door he went through leads to); it's centred
function showRoom(r, focusDoor){
  let target = null;
  const R = G.rooms[r]; location.hash = r; wrap.className = ''; setTab(R.area); markers.length = 0; view = {room: r};
  wrap.innerHTML = `<img class="map" src="${roomImage(r)}" width="${R.width}" height="${R.height}">`;
  const onArea = L[R.area] && [r, R.same_as].find(x => x && L[R.area].rooms[x]);  // a shared gate: the one drawn
  info.innerHTML = `<b>${r}</b> ${R.name}` + (onArea && R.linked
    ? ` &nbsp; <a href="#" onclick="showArea('${R.area}','${onArea}');return false">back to ${R.area}</a>`
    : ' &nbsp; (only shown while following Mario)');
  for (const d of R.doors) {
    if (!d.box) continue;
    const [x0, y0, x1, y1] = d.box, e = document.createElement('div'), dest = linked(d.dest_room) && G.rooms[d.dest_room];
    const away = dest && dest.area !== R.area && L[dest.area];
    e.className = 'door ' + d.kind + (away ? ' away' : '');
    Object.assign(e.style, {left: x0+'px', top: y0+'px', width: Math.max(8, x1-x0)+'px', height: Math.max(8, y1-y0)+'px'});
    e.textContent = d.direction ? (d.direction === 'up' ? '▲ ' : '▼ ') + 'elevator to ' + d.dest_room
      : d.ret ? '↩ back to ' + (d.dest_room === 'mac_12' ? 'Flopside' : 'Flipside') + ' Tower'  // a chapter's way back (graph.py)
      : d.name + (d.dest_room ? ' -> ' + d.dest_room + (away ? ' (' + dest.area + ')' : '') : '');
    if (d.ret) e.classList.add('ret');
    e.dataset.door = d.name; e.dataset.left = x0; e.dataset.top = y0;
    if (away) e.onclick = () => showArea(dest.area, d.dest_room);  // another chapter or hub: its map
    else if (dest) { e.style.cursor = 'pointer'; e.onclick = () => showRoom(d.dest_room, d.dest_door); }
    if (focusDoor && [d.name, d.name_l, d.name_r].includes(focusDoor)) target = e;
    wrap.appendChild(e);
  }
  placeChoiceDoors();
  for (const o of (R.objects || [])) {
    if (o.kind !== 'star_block' || !o.px) continue;
    const next = nextSection(r), e = document.createElement('div');
    e.className = 'star' + (next ? ' go' : ''); e.textContent = 'Star Block' + (next ? ' -> ' + next : '');
    Object.assign(e.style, {left: o.px[0]+'px', top: o.px[1]+'px'});
    if (next) { e.title = 'Go to ' + next; e.onclick = () => showRoom(next); }
    wrap.appendChild(e);
  }
  for (const c of C) if (c.room === r) marker(c, c.px[0], c.px[1]);
  placeMario(); drawBosses(); drawLocks();
  if (target) {
    target.classList.add('flash');
    setTimeout(() => target.scrollIntoView({block: 'center', inline: 'center'}), 0);
  } else window.scrollTo(0, 0);
}
// Rooms shown in stages as the story or a save flag moves on (stages.py): the room's own image until the
// running game reaches a stage, then that stage's (the last one reached wins)
const ROOM_STATES = __STATES__;
function flagSet(n){ return !!(game && game.flags && (parseInt(game.flags.substr((n >> 5) * 8, 8), 16) >>> (n & 31) & 1)); }
function reached(kind, value, chapter, room){
  if (!game || !game.ok) return false;
  if (kind === 'lswf')  // a level save flag: only while Mario is in that room's level (they're per level)
    return !!room && !!game.map && game.map.slice(0, 3) === room.slice(0, 3) && !!(game.lswf >>> value & 1);
  if (kind === 'story') return !!game.story && game.story[chapter - 1] >= value;
  if (kind === 'flag') return flagSet(value);
  return false;
}
// A room picked by a level save byte (stages.py 'choice'): its value, or the room's default before its
// script has set it (its LSWF) or while Mario is in another level (the bytes are that level's)
function choiceValue(r){
  const s = ROOM_STATES[r];
  if (!game || !game.ok || !game.lsw || !game.map || game.map.slice(0, 3) !== r.slice(0, 3)) return s.default;
  if (s.init != null && !(game.lswf >>> s.init & 1)) return s.default;
  return game.lsw[s.choice[1]];
}
// Its doors: the ones that don't work at this value hidden, the moved ones drawn where they are now
// Game positions -> pixels in a room image; the same maths as tools/maprender/projection.py
function cameraAxes(R){
  if (R.right && R.up) return [R.right, R.up];
  const p = R.pitch_deg * Math.PI / 180;  // rooms rendered before the camera was turned
  return [[1, 0, 0], [0, Math.cos(p), -Math.sin(p)]];
}
function dot3(a, b){ return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
function projectPoint(R, pos){  // pos: game units (10x map units)
  let [x, y, z] = pos.map(v => v / 10);
  if (R.turned) { x = -x; z = -z; }  // a room drawn from the other side (render_blender.TURNED)
  const [right, up] = cameraAxes(R);
  return [(dot3([x, y, z], right) - R.x0) * R.ppu, (R.y0 - dot3([x, y, z], up)) * R.ppu];
}
function placeChoiceDoors(){
  const s = view && view.room && ROOM_STATES[view.room];
  if (!s || !s.choice) return;
  const R = G.rooms[view.room], doors = s.doors[choiceValue(view.room)] || {hidden: [], moved: {}};
  const [right, up] = cameraAxes(R);
  wrap.querySelectorAll('.door[data-door]').forEach(e => {
    const name = e.dataset.door, d = doors.moved[name] || [0, 0, 0];
    e.style.display = doors.hidden.includes(name) ? 'none' : '';
    e.style.left = (+e.dataset.left + dot3(d, right) * R.ppu) + 'px';
    e.style.top = (+e.dataset.top - dot3(d, up) * R.ppu) + 'px';
  });
}
function roomImage(r){
  const s = ROOM_STATES[r];
  let image = G.rooms[r].image;
  if (s && s.choice) return s.images[choiceValue(r)] || image;
  if (s && s.switches) {  // independent switches: the image for the combination reached
    let mask = 0;
    s.switches.forEach(([kind, value], i) => { if (reached(kind, value, s.chapter, r)) mask |= 1 << i; });
    return s.images[mask] || image;
  }
  if (s) for (const [kind, value, img] of s.stages) if (reached(kind, value, s.chapter)) image = img;
  return image;
}
// The room on screen shows the other way as soon as the story passes that point
function refreshRoomImage(){
  if (!view || !view.room || !ROOM_STATES[view.room]) return;
  const img = wrap.querySelector('img.map'), want = roomImage(view.room);
  if (img && img.getAttribute('src') !== want) img.setAttribute('src', want);
  placeChoiceDoors();
}
// The first room of the next part of a chapter (he1_05 -> he2_01), or null
function nextSection(r){
  const m = r.match(/^([a-z]+)(\d)_/); if (!m) return null;
  const next = m[1] + (+m[2] + 1) + '_01';
  return linked(next) ? next : null;
}
// ---- Item grid ----
const itemsBox = document.getElementById('items');
// The Throne Room Key isn't an Archipelago item: the game gives it once the final requirement is met
// (mod endgame_requirements.cpp), worked out here from the seed's settings as the mod does
const AP_ID_BASE = 0x53504D00, HEART_FIRST = AP_ID_BASE + 1001;
const ENDGAME_VANILLA = 1, ENDGAME_CHAPTERS = 2, ENDGAME_PURE_HEARTS = 3;
let slotData = null;
// What's in logic (user, 2026-11-12), while connected: which locations the items received so far reach, by
// the APworld's own rules (tracker_logic.py, asked through the tracker app or serve.py); the Tracker and
// Coords lists grey out the rest
let reach = null, logicAsked = 0;
async function askLogic(){
  if (!connected || !slotData) return;
  const ask = ++logicAsked, ids = [];
  for (const [id, n] of Object.entries(received)) for (let i = 0; i < n; i++) ids.push(+id);
  try {
    const r = window.pywebview && window.pywebview.api && window.pywebview.api.logic
      ? await window.pywebview.api.logic(ids, slotData)
      : await (await fetch('logic.json', {method: 'POST', body: JSON.stringify({received: ids, slot_data: slotData})})).json();
    if (ask === logicAsked && connected) { reach = new Set(r.reachable); lastTracker = lastCoords = null; drawGame(); }
  } catch (e) { reach = null; }  // an older tracker app: no logic, nothing greyed
}
function chaptersFinished(){  // a chapter's ending is its Pure Heart check
  return C.filter(c => c.kind === 'heart' && serverChecked.has(c.id)).length;
}
function heartsHeld(){  // received hearts, plus the starting doors' hearts unless those are items to find
  const held = new Set();
  for (let n = 1; n <= 8; n++) if (received[HEART_FIRST + n - 1]) held.add(n);
  if (slotData && !slotData.starting_hearts_in_pool) for (const n of slotData.starting_chapters || []) held.add(n);
  return held.size;
}
function throneKey(){  // [earned, progress text]
  const [kind, need] = (slotData && slotData.final_requirement) || [0, 0];
  if (kind === ENDGAME_VANILLA) return [!!received[HEART_FIRST + 7], 'needs Pure Heart 8'];
  if (kind === ENDGAME_CHAPTERS) { const n = chaptersFinished(); return [n >= need, `${n} of ${need} chapters finished`]; }
  if (kind === ENDGAME_PURE_HEARTS) { const n = heartsHeld(); return [n >= need, `${n} of ${need} Pure Hearts`]; }
  return [false, 'final requirement unknown'];
}
function count(it){
  if (it.id === 'throne_key' && connected) return throneKey()[0] ? 1 : 0;
  if (!connected) return +(haveByHand[it.id] || 0);
  // starting as Mario, he's no item to find (the server never sends him): held from the start
  if (it.id === AP_ID_BASE + 216 && slotData && !slotData.starting_character) return 1;
  return (it.ids || [it.id]).reduce((sum, id) => sum + (received[id] || 0), 0);  // a key kind: all its items
}
function drawItems(){
  itemsBox.innerHTML = '';
  for (const g of ITEMS) {
    const h = document.createElement('h3'); h.textContent = g.title; itemsBox.appendChild(h);
    const row = document.createElement('div'); row.className = 'tiles'; itemsBox.appendChild(row);
    for (const it of g.items) {
      // A key there are several of (it.total: 3 Ruins Keys, 7 Castle Bleck Keys...) is one tile with how
      // many are held in its corner; clicking it by hand counts up one at a time, then back to none
      const n = count(it), total = it.total || 1, t = document.createElement('div');
      t.className = 'tile' + (n ? ' have' : '');
      t.title = it.name + (total > 1 ? ` (${n}/${total})` : '') +
        (it.id === 'throne_key' && connected ? '\n' + throneKey()[1] : '') +
        (connected ? '' : total > 1 ? '\nClick to count up, right-click to count down (not connected)'
                                    : '\nClick to mark, right-click to unmark (not connected)');
      t.innerHTML = `<img src="${it.icon}">` + ((total > 1 ? n : n > 1) ? `<span class="n">${n}</span>` : '');
      // by hand: click counts up one (then back to none), right-click counts down one
      const mark = have => { if (have) haveByHand[it.id] = have; else delete haveByHand[it.id];
        try { localStorage.setItem('spm_items', JSON.stringify(haveByHand)); } catch (e) {}
        drawItems(); };
      t.onclick = () => { if (!connected) mark((n + 1) % (total + 1)); };
      t.oncontextmenu = ev => { ev.preventDefault(); if (!connected && n > 0) mark(n - 1); };
      row.appendChild(t);
    }
  }
}
// ---- The side column: Items, Coords, Tracker ----
const side = document.getElementById('side'), coordsBox = document.getElementById('coords'),
      trackerBox = document.getElementById('tracker');
const PANELS = {items: itemsBox, coords: coordsBox, tracker: trackerBox};
function syncSide(){
  side.classList.toggle('open', Object.values(PANELS).some(p => p.classList.contains('open')));
  side.style.top = bar.offsetHeight + 'px';
}
function togglePanel(name){
  PANELS[name].classList.toggle('open');
  try { localStorage.setItem('spm_' + name + '_open', PANELS[name].classList.contains('open') ? '1' : ''); } catch (e) {}
  syncSide();
  if (name === 'items') drawItems(); else { lastCoords = lastTracker = null; drawGame(); }
}
function toggleItems(){ togglePanel('items'); }
new ResizeObserver(syncSide).observe(bar);

// ---- The running game (serve.py), like the old coords and tracker windows ----
function esc(s){ return String(s).replace(/[&<>"]/g, ch => ({'&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;'}[ch])); }
function spot(c){  // where a location is: its spot and position, or how to get it
  if (['gift', 'spot', 'heart'].includes(c.kind) || !c.pos) return c.where || '';
  const [x, y, z] = c.pos;
  return `${c.where} at x ${x}, height ${y}, depth ${z}`;
}
// A check picked from a list: its room, scrolled so its marker is in the middle where the room is big enough
// (the browser stops at the room's edges), with the marker flashing and its details shown
function goTo(c){
  if (!onMap(c)) return;
  showRoom(c.room);
  const m = markers.find(([, mc]) => mc === c);
  if (!m) return;
  const e = m[0];
  e.classList.add('flash'); info.innerHTML = describe(c);
  setTimeout(() => e.scrollIntoView({block: 'center', inline: 'center'}), 0);
  setTimeout(() => e.classList.remove('flash'), 2500);
}
let lastCoords = null, lastTracker = null;
const openLevels = new Set();
function drawCoords(){
  if (!coordsBox.classList.contains('open')) return;
  let html;
  if (!game || !game.ok) html = `<h3>Coords</h3><div class="muted">Waiting for the game: ${esc(game ? game.error : 'no answer')}</div>`;
  else {
    const [x, y, z] = game.pos, R = G.rooms[game.map];
    const left = C.filter(c => c.map === game.map && !isDone(c));
    html = `<h3>Coords</h3><div class="pos">x ${x.toFixed(0)} &nbsp; height ${y.toFixed(0)} &nbsp; depth ${z.toFixed(0)}</div>` +
      `<div class="here">Map: ${R ? `<a href="#" data-room="${game.map}">${game.map}</a> ${esc(R.name)}` : esc(game.map)}</div>` +
      (left.length ? 'Unchecked here:<ul>' + left.map(c => `<li class="${onMap(c) ? 'go' : ''} ${outOfLogic(c) ? 'out' : ''}" data-id="${c.id}">${esc(c.label)}: ${esc(spot(c))}</li>`).join('') + '</ul>'
                   : '<div class="muted">No unchecked locations in this map</div>');
  }
  if (html === lastCoords) return;
  lastCoords = html; coordsBox.innerHTML = html;
  coordsBox.querySelectorAll('a[data-room]').forEach(a => a.onclick = ev => { ev.preventDefault(); showRoom(a.dataset.room); });
  coordsBox.querySelectorAll('li[data-id]').forEach(li => li.onclick = () => goTo(C.find(c => c.id === +li.dataset.id)));
}
// Out of logic (connected): not reachable yet with the items received, by the APworld's rules (askLogic)
function outOfLogic(c){ return !!reach && !isDone(c) && !reach.has(c.id); }
function drawTracker(){
  if (!trackerBox.classList.contains('open')) return;
  const byLevel = new Map(LEVELS.map(t => [t, []]));
  for (const c of C) { const t = c.name.split(' - ')[0]; if (!byLevel.has(t)) byLevel.set(t, []); byLevel.get(t).push(c); }
  const total = C.length, got = C.filter(isDone).length, here = game && game.ok ? game.map : null;
  let html = `<h3>Tracker</h3><div>${got}/${total} checked${here ? ` &nbsp;<span class="muted">(you're in ${here})</span>` : ''}</div>`;
  if (game && game.ok) html += `<div class="muted">Pure Hearts held: ${game.hearts.length ? game.hearts.join(' ') : 'none'}</div>`;
  else html += `<div class="muted">Not reading the game (${esc(game ? game.error : 'no answer')}): marks and server checks only</div>`;
  for (const [title, cs] of byLevel) {
    if (!cs.length) continue;
    const n = cs.filter(isDone).length, full = n === cs.length, isHere = cs.some(c => c.map === here);
    const stuck = !full && cs.every(c => isDone(c) || outOfLogic(c));  // nothing left here is in logic
    html += `<details data-level="${esc(title)}"${openLevels.has(title) ? ' open' : ''}>` +
      `<summary class="${isHere ? 'here' : ''}${stuck ? ' out' : ''}"${stuck ? ' title="Nothing left here is in logic yet"' : ''}>${esc(title)}${isHere ? ' &larr; here' : ''}<span class="st${full ? ' full' : ''}">${full ? 'Complete' : n + '/' + cs.length}</span></summary><ul>` +
      cs.map(c => `<li class="${isDone(c) ? 'got' : ''} ${onMap(c) ? 'go' : ''} ${outOfLogic(c) ? 'out' : ''}" data-id="${c.id}" title="${esc(spot(c))}${outOfLogic(c) ? '&#10;Out of logic' : ''}">${esc(c.label)}</li>`).join('') +
      '</ul></details>';
  }
  if (html === lastTracker) return;
  lastTracker = html; trackerBox.innerHTML = html;
  trackerBox.querySelectorAll('details').forEach(d => d.ontoggle = () => {
    if (d.open === openLevels.has(d.dataset.level)) return;  // drawn open: nothing changed
    if (d.open) openLevels.add(d.dataset.level); else openLevels.delete(d.dataset.level);
    lastTracker = null; });
  trackerBox.querySelectorAll('li[data-id]').forEach(li => li.onclick = () => goTo(C.find(c => c.id === +li.dataset.id)));
}
// Mario's marker: his position in the room or area on screen, if he's in it
// The marker glides to each new reading (marioAt is where it's drawn, marioTarget where Mario is)
let marioAt = null, marioTarget = null, marioFrom = null, marioSince = 0;
const CHARACTER_ICONS = {Mario: 216, Peach: 217, Bowser: 218, Luigi: 219};  // serve.py's character -> icon
function placeMario(){
  let e = document.getElementById('mario');
  const hide = () => { if (e) e.remove(); marioAt = marioTarget = null; };
  if (!game || !game.ok || !view) return hide();
  const R = G.rooms[game.map]; if (!R) return hide();
  let [px, py] = projectPoint(R, game.pos);
  if (view.area) {
    const box = L[view.area].rooms[game.map] || L[view.area].rooms[R.same_as]; if (!box) return hide();
    px = box[0] + px * box[2] / R.width; py = box[1] + py * box[3] / R.height;
  } else if (view.room !== game.map) return hide();
  // Glide from where the marker is now to the new reading, over the time until the next one
  if (!marioTarget || px !== marioTarget[0] || py !== marioTarget[1]) {
    marioFrom = marioAt ? marioAt.slice() : [px, py]; marioSince = performance.now();
  }
  marioTarget = [px, py];
  if (!e) {  // new view: drawn straight at his position
    e = document.createElement('img'); e.id = 'mario'; e.src = 'icons/216.png'; e.title = 'Mario';
    wrap.appendChild(e); marioAt = null;
  }
  // Turned with his gravity (the Whoa Zone): his head points the way that's up for him, his feet
  // stay on the spot. Up (0, 1) is upright; up (1, 0) (gravity pulling left) turns him a quarter clockwise
  const up = game.up || [0, 1, 0];
  const turn = Math.round(Math.atan2(R.turned ? -up[0] : up[0], up[1]) * 180 / Math.PI);
  e.style.transform = `translate(-50%, -100%) rotate(${turn}deg)`;
  // the icon of whoever's being played (the tracker's own character icons, item numbers 216-219)
  const icon = 'icons/' + (CHARACTER_ICONS[game.character] || 216) + '.png';
  if (e.getAttribute('src') !== icon) { e.setAttribute('src', icon); e.title = game.character || 'Mario'; }
  // A big jump (a door, a pipe, a warp): no gliding across the room
  if (!marioAt || Math.hypot(px - marioAt[0], py - marioAt[1]) > 400) {
    marioAt = [px, py]; marioFrom = [px, py]; Object.assign(e.style, {left: px + 'px', top: py + 'px'}); snapCamera = true;
  }
}
function drawGame(){ drawCoords(); drawTracker(); placeMario(); refreshRoomImage(); drawBosses(); drawLocks(); }
// Follow Mario (a setting, kept in this browser): open the room he's in, and keep him in the middle
let follow = false;
try { follow = !!localStorage.getItem('spm_follow'); } catch (e) {}
let followButton = null;
function setFollow(on){
  follow = on;
  try { localStorage.setItem('spm_follow', on ? '1' : ''); } catch (e) {}
  if (followButton) followButton.classList.toggle('on', on);
  if (on) followMario(true);
}
let snapCamera = false;  // the next frame centres at once (a new room, or a jump)
function followMario(jump){
  if (!follow || !game || !game.ok || !G.rooms[game.map]) return;
  if (!view || view.room !== game.map) { showRoom(game.map); jump = true; }
  if (jump) snapCamera = true;
}
// Every frame: the marker moves evenly from the last reading's spot to the newest one over the half
// second until the next reading, so it keeps moving between readings; while following, the view
// moves part of the way to centre on the marker.
let lastFrame = 0;  // when a frame last ran (none run while the page isn't drawn)
function animate(){ lastFrame = performance.now(); glide(); requestAnimationFrame(animate); }
function glide(){
  const e = document.getElementById('mario');
  if (e && marioAt && marioTarget && marioFrom) {
    const t = Math.min(1, (performance.now() - marioSince) / POLL_MS);
    marioAt = [marioFrom[0] + (marioTarget[0] - marioFrom[0]) * t, marioFrom[1] + (marioTarget[1] - marioFrom[1]) * t];
    e.style.left = marioAt[0] + 'px'; e.style.top = marioAt[1] + 'px';
    if (follow && view && view.room) {
      const r = e.getBoundingClientRect(), barH = bar.offsetHeight;
      const sideW = side.classList.contains('open') ? side.offsetWidth : 0;
      const cx = (window.innerWidth - sideW) / 2, cy = barH + (window.innerHeight - barH) / 2;
      const dx = r.left + r.width / 2 - cx, dy = r.top + r.height / 2 - cy, k = snapCamera ? 1 : 0.15;
      if (Math.abs(dx) > 0.5 || Math.abs(dy) > 0.5) window.scrollBy(dx * k, dy * k);
      snapCamera = false;
    }
  }
}
requestAnimationFrame(animate);
async function pollGame(){
  // In the desktop app (tracker.pyw) the game state comes straight from the app; in a browser, from serve.py
  try { game = window.pywebview && window.pywebview.api ? await window.pywebview.api.game_state()
                                                       : await (await fetch('game.json', {cache: 'no-store'})).json(); }
  catch (e) { game = {ok: false, error: 'no game link (open tools/maprender/tracker.pyw, or run serve.py)'}; }
  const now = new Set(game.ok ? game.checked.concat(game.server) : []);
  if (now.size !== gameChecked.size || [...now].some(i => !gameChecked.has(i))) {
    gameChecked = now; refreshMarkers(); lastTracker = null;
  }
  drawGame();
  followMario(false);
  // No frames since the last reading (the page isn't being drawn): no gliding, just put him there
  if (performance.now() - lastFrame > POLL_MS) { marioSince = 0; glide(); }
}
// Two readings a second (more slowed the game down, user 2026-11-04); the marker glides in between
const POLL_MS = 500;
async function pollLoop(){ await pollGame(); setTimeout(pollLoop, POLL_MS); }

// ---- Archipelago connection: read-only, as a tracker ----
const apServer = document.getElementById('apserver'), apSlot = document.getElementById('apslot'),
      apPw = document.getElementById('appw'), apGo = document.getElementById('apgo'),
      apState = document.getElementById('apstate'), apMsg = document.getElementById('apmsg');
let ws = null, wantOpen = false;
try { apServer.value = localStorage.getItem('spm_ap_server') || ''; apSlot.value = localStorage.getItem('spm_ap_slot') || ''; } catch (e) {}
function setState(cls, msg){ apState.className = cls; apState.title = msg; apMsg.textContent = ' ' + msg; }
// The addresses to try: as typed if it has a scheme; otherwise ws:// for this computer, wss:// first elsewhere
function urls(addr){
  if (/^wss?:\/\//.test(addr)) return [addr];
  if (!/:\d+$/.test(addr)) addr += ':38281';
  const local = /^(localhost|127\.|\[::1\])/.test(addr);
  return local ? ['ws://' + addr, 'wss://' + addr] : ['wss://' + addr, 'ws://' + addr];
}
function apConnect(){
  if (ws) { wantOpen = false; ws.close(); return; }
  const addr = apServer.value.trim() || 'localhost:38281', slot = apSlot.value.trim();
  if (!slot) { setState('bad', 'enter your slot name'); return; }
  try { localStorage.setItem('spm_ap_server', apServer.value.trim()); localStorage.setItem('spm_ap_slot', slot); } catch (e) {}
  wantOpen = true; tryUrls(urls(addr), slot);
}
function tryUrls(list, slot){
  const url = list[0]; setState('busy', 'connecting to ' + url);
  let opened = false;
  ws = new WebSocket(url);
  ws.onopen = () => { opened = true; };
  ws.onmessage = ev => { for (const m of JSON.parse(ev.data)) onPacket(m, slot); };
  ws.onclose = () => {
    ws = null; connected = false; apGo.textContent = 'Connect';
    if (wantOpen && !opened && list.length > 1) { tryUrls(list.slice(1), slot); return; }
    serverChecked = new Set(); received = {}; reach = null; refreshMarkers(); drawItems();
    if (wantOpen) setState('bad', opened ? 'disconnected' : 'could not reach the server'); else setState('', 'not connected');
  };
}
function onPacket(m, slot){
  if (m.cmd === 'RoomInfo') {
    ws.send(JSON.stringify([{cmd: 'Connect', game: 'Super Paper Mario', name: slot, password: apPw.value,
      uuid: 'spm-map-viewer', version: {major: 0, minor: 6, build: 7, class: 'Version'},
      items_handling: 7, tags: ['Tracker'], slot_data: true}]));
  } else if (m.cmd === 'ConnectionRefused') {
    wantOpen = false; setState('bad', 'refused: ' + (m.errors || []).join(', ')); ws.close();
  } else if (m.cmd === 'Connected') {
    connected = true; apGo.textContent = 'Disconnect';
    serverChecked = new Set(m.checked_locations); slotData = m.slot_data || null; setState('ok', 'connected as ' + slot);
    reach = null;
    refreshMarkers(); drawItems(); askLogic();
  } else if (m.cmd === 'ReceivedItems') {
    if (m.index === 0) received = {};
    for (const it of m.items) received[it.item] = (received[it.item] || 0) + 1;
    drawItems(); askLogic();
  } else if (m.cmd === 'RoomUpdate' && m.checked_locations) {
    for (const id of m.checked_locations) serverChecked.add(id);
    refreshMarkers(); drawItems();  // a chapter's ending can earn the Throne Room Key
  }
}
apGo.onclick = apConnect;
for (const el of [apServer, apSlot, apPw]) el.onkeydown = ev => { if (ev.key === 'Enter') apConnect(); };

function showUnplaced(){
  setTab('unplaced'); location.hash = 'unplaced';
  wrap.innerHTML = ''; info.textContent = "Checks in rooms the map doesn't open by hand (no door leads there; they show while following Mario):";
  list.innerHTML = C.filter(c => !onMap(c) && c.kind !== 'tattle').map(c => `<li>${describe(c)}</li>`).join('');
  list.style.display = 'block';
}
// A chapter's levels, from its rooms' names (he1_xx is 1-1, he2_xx 1-2...): [label, first room], in order
function levelsOf(a){
  const m = a.match(/^chapter(\d)$/); if (!m) return [];
  const firsts = {};
  for (const r of Object.keys(L[a].rooms).sort()) {
    const p = r.match(/^([a-z]+)(\d)_/); if (!p) continue;
    const key = p[1] + p[2];
    if (!firsts[key]) firsts[key] = [+p[2], r];
  }
  return Object.values(firsts).sort((x, y) => x[0] - y[0] || x[1].localeCompare(y[1])).map(([n, r]) => {
    const label = m[1] + '-' + n;
    return [LEVELS.find(t => t.startsWith(label + ' ')) || label, r];  // "1-2 Mount Lineland"
  });
}
// The dropdown under a chapter's tab: the whole chapter, or one level's first room
const menu = document.createElement('div'); menu.id = 'menu'; document.body.appendChild(menu);
function closeMenu(){ menu.style.display = 'none'; }
document.addEventListener('click', ev => { if (!menu.contains(ev.target) && !ev.target.dataset.menu) closeMenu(); });
function openMenu(b, a){
  if (menu.style.display === 'block' && menu.dataset.a === a) { closeMenu(); return; }
  menu.innerHTML = ''; menu.dataset.a = a;
  const add = (text, go) => { const i = document.createElement('div'); i.textContent = text;
    i.onclick = () => { closeMenu(); go(); }; menu.appendChild(i); };
  add('Whole chapter', () => showArea(a));
  for (const [label, r] of levelsOf(a)) add(label, () => showRoom(r));
  const box = b.getBoundingClientRect();
  Object.assign(menu.style, {display: 'block', left: box.left + 'px', top: box.bottom + 'px'});
}
for (const a of Object.keys(L)) {
  const b = document.createElement('button'); b.textContent = a; b.dataset.a = a; tabs.appendChild(b);
  if (levelsOf(a).length) { b.textContent = a + ' ▾'; b.dataset.menu = 1; b.onclick = () => openMenu(b, a); }
  else b.onclick = () => showArea(a);
}
for (const [name, label] of [['items', 'Items'], ['coords', 'Coords'], ['tracker', 'Tracker']]) {
  const b = document.createElement('button'); b.textContent = label; if (name === 'items') b.style.marginLeft = '12px';
  b.onclick = () => togglePanel(name); tabs.appendChild(b);
  try { if (localStorage.getItem('spm_' + name + '_open')) PANELS[name].classList.add('open'); } catch (e) {}
}
{ const b = document.createElement('button'); b.textContent = 'Follow Mario'; b.title = 'Open the room Mario is in and keep him in the middle';
  b.onclick = () => setFollow(!follow); tabs.appendChild(b); followButton = b; b.classList.toggle('on', follow); }
syncSide();
drawItems();
pollLoop();
{ const b = document.createElement('button'); b.dataset.a = 'unplaced';
  b.textContent = 'not on the map (' + C.filter(c => !onMap(c) && c.kind !== 'tattle').length + ')'; b.onclick = showUnplaced; tabs.appendChild(b); }
const h = location.hash.slice(1);
if (linked(h)) showRoom(h); else if (L[h]) showArea(h); else if (h === 'unplaced') showUnplaced();
else showArea(L.flipside ? 'flipside' : Object.keys(L)[0]);
</script></body></html>
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--scale', type=float, default=0.3)
    args = ap.parse_args()
    graph = json.load(open(os.path.join(OUT_DIR, 'graph.json')))
    areas = ['flipside'] + [f'chapter{n}' for n in range(1, 9)] + ['flopside', 'pits', 'other']  # the viewer opens on Flipside
    lay = {}
    for a in areas:
        res = render_area(a, graph, args.scale)
        if res:
            lay[a] = res
            print(f"{a}: {res['width']}x{res['height']}, {len(res['rooms'])} rooms")
    json.dump(lay, open(os.path.join(OV, 'layout.json'), 'w'), indent=1)
    checks_path = os.path.join(OUT_DIR, 'checks.json')  # from checks.py; the viewer works without it
    checks = json.load(open(checks_path)) if os.path.exists(checks_path) else []
    items_path = os.path.join(OUT_DIR, 'items.json')  # the item grid, also from checks.py
    items = json.load(open(items_path)) if os.path.exists(items_path) else []
    # Every level's title ("1-2 Mount Lineland"), for the chapter tabs' dropdowns
    locations_path = os.path.join(os.path.dirname(OUT_DIR), '..', 'data', 'ap_locations.json')
    levels = json.load(open(locations_path))['levels'] if os.path.exists(locations_path) else []
    html = (VIEWER.replace('__GRAPH__', json.dumps(graph)).replace('__LAYOUT__', json.dumps(lay))
            .replace('__CHECKS__', json.dumps(checks)).replace('__ITEMS__', json.dumps(items))
            .replace('__LEVELS__', json.dumps(levels))
            .replace('__STATES__', json.dumps(stages.viewer_states()))
            .replace('__BOSSES__', json.dumps(bosses.viewer_bosses(graph['rooms'], lambda R: R['pitch_deg']))))
    open(os.path.join(OUT_DIR, 'viewer.html'), 'w', encoding='utf-8').write(html)


if __name__ == '__main__':
    main()
