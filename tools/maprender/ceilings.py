"""Finds ceiling triangles in indoor rooms so the renderer can leave them out.

Seen from slightly above, a ceiling's top and front faces cover the top of the room (door tops, the
upper wall). At each spot, the ceiling is the highest downward-facing surface (in the collision model or
the drawn model) with a collision floor 3+ units below it; a triangle at or above that height is ceiling.
Each triangle is tested at its own depth and, when nothing is there, at a few depths across the room,
because ceiling front faces sit in front of where the floors end. render_blender.py only applies this in
rooms without a background picture (indoor and underground rooms).
"""
import numpy as np

HEADROOM = 3.0  # model units (Mario is a bit under 3 tall)
INSIDE = 0.3    # how far to push a test point into the solid behind a face


def _tris(meshes):
    if not meshes:
        return np.zeros((0, 3, 3), np.float32), np.zeros((0, 3), np.float32)
    t = np.concatenate([m['pos'] for m in meshes]).reshape(-1, 3, 3).astype(np.float64)
    return t, normals(t)


def normals(t):
    """Outward normals. GX front faces wind clockwise, so the cross product points inwards: negate it."""
    n = -np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    return n / (np.linalg.norm(n, axis=1, keepdims=True) + 1e-12)


class Collision:
    """Up- and down-facing collision triangles, for vertical ray queries."""
    def __init__(self, world, visual=()):
        """world: the parsed map; visual: the drawn meshes (their downward faces count as ceilings too,
        since many rooms have no ceiling collision where Mario can't jump that high)."""
        t, n = _tris([m for _, _, m in world.meshes(world.a_node)])
        self.up = t[n[:, 1] > 0.5]
        down = [t[n[:, 1] < -0.5]]
        if visual:
            vt, vn = _tris(list(visual))
            down.append(vt[vn[:, 1] < -0.5])
        self.down = np.concatenate(down)
        # depths to test at as well as a point's own: ceiling front faces sit in front of the floors
        if len(self.up):
            lo, hi = np.percentile(self.up[:, :, 2], [5, 95])
            self.depths = np.linspace(lo, hi, 5)
        else:
            self.depths = np.zeros(0)

    @staticmethod
    def _heights(tris, pts):
        """(len(pts), len(tris)) heights of each triangle at each point's x/z, NaN where it misses."""
        out = np.full((len(pts), len(tris)), np.nan)
        if not len(tris) or not len(pts):
            return out
        a, b, c = tris[:, 0], tris[:, 1], tris[:, 2]
        x, z = pts[:, 0:1], pts[:, 2:3]
        v0x, v0z = c[:, 0] - a[:, 0], c[:, 2] - a[:, 2]
        v1x, v1z = b[:, 0] - a[:, 0], b[:, 2] - a[:, 2]
        v2x, v2z = x - a[:, 0], z - a[:, 2]
        den = v0x * v1z - v1x * v0z
        ok = np.abs(den) > 1e-9
        den = np.where(ok, den, 1.0)
        u = (v2x * v1z - v1x * v2z) / den
        v = (v0x * v2z - v2x * v0z) / den
        hit = ok & (u >= -1e-6) & (v >= -1e-6) & (u + v <= 1 + 1e-6)
        y = a[:, 1] + u * (c[:, 1] - a[:, 1]) + v * (b[:, 1] - a[:, 1])
        out[hit] = y[hit]
        return out

    def is_ceiling(self, pts, chunk=256):
        """Boolean per point: True = ceiling. Each depth probe votes (see _vote)."""
        res = np.zeros(len(pts), bool)
        for i in range(0, len(pts), chunk):
            p = pts[i:i + chunk]
            probes = [p] + [np.c_[p[:, :2], np.full(len(p), z)] for z in self.depths]
            votes = np.zeros((len(probes), len(p)))
            for j, q in enumerate(probes):
                ups, downs = self._heights(self.up, q), self._heights(self.down, q)
                for k in range(len(p)):
                    votes[j, k] = self._vote(p[k, 1], ups[k][~np.isnan(ups[k])], downs[k][~np.isnan(downs[k])])
            # the probe at the point's own depth decides when it has an opinion; the others are for
            # faces in front of the floors (ceiling front edges)
            own, others = votes[0], votes[1:].sum(0)
            res[i:i + chunk] = np.where(own != 0, own > 0, others > 0)
        return res

    @staticmethod
    def _vote(y, u, d):
        """+1 ceiling, -1 inside the room, 0 no opinion (nothing at this depth).
        The ceiling at this spot is the highest downward surface with a floor HEADROOM or more below it;
        anything at or above it is ceiling. Hanging things (crushers) and upper storeys sit below it."""
        tops = [dd for dd in d if (u <= dd - HEADROOM).any()]
        if not tops:
            return -1 if (u <= y).any() else 0
        return 1 if y >= max(tops) - INSIDE else -1


def ceiling_mask(coll, pos):
    """Boolean per triangle of a triangle-soup position array: True = ceiling (leave out)."""
    t = pos.reshape(-1, 3, 3).astype(np.float64)
    n = normals(t)
    centre = t.mean(1) - n * INSIDE  # just inside the solid the face belongs to
    return coll.is_ceiling(centre)
