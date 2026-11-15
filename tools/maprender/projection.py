"""Game positions -> pixels in a room image (build/maps/rooms/<room>.png), from a graph.json room.

A room is drawn by an orthographic camera tilted down by pitch_deg and turned yaw_deg from the left.
graph.json gives each room the camera's right and up axes in map units (game units / 10):

    px = (dot(m, right) - x0) * ppu
    py = (y0 - dot(m, up)) * ppu        with m = position / 10, after (x, z) -> (-x, -z) if 'turned'

Rooms rendered before the turn was added have no right/up: then right = (1, 0, 0) and
up = (0, cos pitch, -sin pitch). The viewer's JavaScript has the same function (projectPoint).
"""
import math


def axes(room):
    """(right, up) camera axes of a room, in map units."""
    if room.get('right') and room.get('up'):
        return room['right'], room['up']
    p = math.radians(room['pitch_deg'])
    return [1.0, 0.0, 0.0], [0.0, math.cos(p), -math.sin(p)]


def map_to_pixels(room, m):
    """A point in map units (already turned if the room is) -> [px, py]."""
    right, up = axes(room)
    sx = sum(a * b for a, b in zip(m, right))
    sy = sum(a * b for a, b in zip(m, up))
    return [round((sx - room['x0']) * room['ppu'], 1), round((room['y0'] - sy) * room['ppu'], 1)]


def to_pixels(room, pos):
    """A game/script position (10x map units) -> [px, py] in the room image."""
    x, y, z = (v / 10 for v in pos)
    if room.get('turned'):  # drawn from the other side (render_blender.TURNED)
        x, z = -x, -z
    return map_to_pixels(room, (x, y, z))


def middle(room):
    """The middle of the room image horizontally, at floor height (y = 0), as [px, py]."""
    right, _ = axes(room)
    s = room['x0'] + room['width'] / room['ppu'] / 2
    n = math.hypot(right[0], right[2]) or 1.0
    return map_to_pixels(room, (right[0] / n * s, 0.0, right[2] / n * s))
