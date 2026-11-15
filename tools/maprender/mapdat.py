"""Parser for SPM/TTYD world models (dvd/map/<room>/map.dat, "ver1.02").

Follows noclip.website's src/PaperMarioTTYD/world.ts. Produces flat triangle meshes in world space,
one per (scene-graph node, material) pair, ready to hand to a renderer.
"""
import math
import struct

import numpy as np

CHUNKS = ['animation_table', 'curve_table', 'fog_table', 'information', 'light_table',
          'material_name_table', 'texture_table', 'vcd_table']

# Material layers
OPAQUE, ALPHA_TEST, BLEND = 0, 1, 2


def _str(d, off):
    return d[off:d.index(b'\0', off)].decode('ascii', 'replace')


def _srt(s, r, t):
    """Model matrix T * Rz * Ry * Rx * S (rotation in degrees)."""
    rx, ry, rz = (math.radians(a) for a in r)
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    Rx = np.array([[1, 0, 0], [0, cx, -sx], [0, sx, cx]])
    Ry = np.array([[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]])
    Rz = np.array([[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]])
    m = np.eye(4)
    m[:3, :3] = Rz @ Ry @ Rx @ np.diag(s)
    m[:3, 3] = t
    return m


def _tex_mtx(ts, tt, ss, st, rot, cs, ct):
    """2D homogeneous texture matrix, same maths as noclip's calcTexMtx."""
    th = math.radians(-rot)
    c, s = math.cos(th), math.sin(th)
    D = np.array([[1, 0, ss * 0.5 * cs], [0, 1, st * (0.5 * ct - 1)], [0, 0, 1]])
    R = np.array([[c, -s, 0], [s, c, 0], [0, 0, 1]])
    Tr = np.array([[1, 0, -ss * 0.5 * cs], [0, 1, -st * (0.5 * ct - 1)], [0, 0, 1]])
    S = np.diag([ss, st, 1.0])
    m = S @ Tr @ R @ D
    m[0, 2] += ts
    m[1, 2] += -tt
    return m


class World:
    def __init__(self, data):
        self.d = d = data
        self.base = 0x20
        _, main_size, fix_count, chunk_count = struct.unpack('>IIII', d[:16])
        assert chunk_count == 8, f'unexpected chunk count {chunk_count}'
        tbl = self.base + main_size + fix_count * 4
        strs = tbl + chunk_count * 8
        self.chunks = {}
        for i in range(chunk_count):
            off, name_off = struct.unpack('>II', d[tbl + i * 8:tbl + i * 8 + 8])
            self.chunks[_str(d, strs + name_off)] = self.base + off
        self._textures()
        self._vcd()
        self._materials()
        info = self.chunks['information']
        self.version = self.s(self.u32(info))
        self.s_node = self.s(self.u32(info + 8))
        self.a_node = self.s(self.u32(info + 12))
        self.root = self._node(self.base + self.u32(info + 4), None)

    # helpers
    def u32(self, o):
        return struct.unpack('>I', self.d[o:o + 4])[0]

    def f32(self, o, n=1):
        v = struct.unpack(f'>{n}f', self.d[o:o + 4 * n])
        return v[0] if n == 1 else v

    def s(self, rel):
        return _str(self.d, self.base + rel)

    def _textures(self):
        o = self.chunks['texture_table']
        self.texture_names = [self.s(self.u32(o + 4 + 4 * i)) for i in range(self.u32(o))]
        self.texture_index = {n: i for i, n in enumerate(self.texture_names)}

    def _vcd(self):
        o = self.chunks['vcd_table']
        u = lambda x: self.u32(o + x)
        self.pos_shift, self.tex_shift = u(0x44), [u(0x48), u(0x4C), u(0x50)]

        def arr(rel, comps, dtype):
            if rel == 0:
                return None
            a = self.base + rel
            n = self.u32(a)
            return np.frombuffer(self.d, dtype, n * comps, a + 4).reshape(n, comps)
        self.pos = arr(u(0x00), 3, '>i2').astype(np.float32) / (1 << self.pos_shift)
        self.clr = arr(u(0x0C), 4, np.uint8)
        self.tex = []
        for i in range(3):
            t = arr(u(0x18 + 4 * i), 2, '>i2')
            self.tex.append(None if t is None else t.astype(np.float32) / (1 << self.tex_shift[i]))

    def _materials(self):
        o = self.chunks['material_name_table']
        self.materials = {}
        for i in range(self.u32(o)):
            name = self.s(self.u32(o + 4 + 8 * i))
            mo = self.base + self.u32(o + 8 + 8 * i)
            d = self.d
            color = tuple(d[mo + 4:mo + 8])
            color_src = d[mo + 8]
            layer = d[mo + 0x0A]
            n = d[mo + 0x0B]
            samplers = [None] * n
            for j in range(n):
                so = self.base + self.u32(mo + 0x0C + 4 * j)
                to = self.base + self.u32(so)
                layer = max(layer, d[so + 0x0A])
                xf = mo + 0x2C + 0x1C * j
                back = n - j - 1  # the game sets samplers up backwards
                samplers[back] = {
                    'texture': self.texture_index.get(self.s(self.u32(to)), -1),
                    'wrap_s': d[so + 8], 'wrap_t': d[so + 9],
                    'mtx': _tex_mtx(*self.f32(xf, 7)),
                }
            tev = self.u32(mo + 0x110)
            tev_mode = d[self.base + tev] if tev else 0
            self.materials[mo] = {'name': name, 'color': color, 'color_src': color_src,
                                  'layer': min(layer, BLEND), 'samplers': samplers, 'tev_mode': tev_mode}

    def _node(self, o, parent_mtx):
        d = self.d
        name = self.s(self.u32(o))
        typ = self.s(self.u32(o + 4))
        child, sibling = self.u32(o + 0x0C), self.u32(o + 0x10)
        sc, rot, tr = self.f32(o + 0x18, 3), self.f32(o + 0x24, 3), self.f32(o + 0x30, 3)
        local = _srt(sc, rot, tr)
        mtx = local if parent_mtx is None else parent_mtx @ local
        dm = self.base + self.u32(o + 0x58)
        node = {'name': name, 'type': typ, 'mtx': mtx, 'local': local, 'anim': None, 'cull': d[dm + 1], 'draw_flags': d[dm + 2],
                'hit_attr': self.u32(dm + 8), 'bbox': (self.f32(o + 0x3C, 3), self.f32(o + 0x48, 3)),
                'parts': [], 'children': []}
        for i in range(self.u32(o + 0x5C)):
            mat_rel, mesh_rel = struct.unpack('>II', d[o + 0x60 + 8 * i:o + 0x68 + 8 * i])
            if mat_rel == 0:
                continue
            node['parts'].append((self.materials[self.base + mat_rel], self.base + mesh_rel))
        c = child
        while c:
            node['children'].append(self._node(self.base + c, mtx))
            c = self.u32(self.base + c + 0x10)
        return node

    def animations(self):
        """{animation name: {mesh name: (offsets, first key, last key)}}, mesh tracks only.
        offsets = (translation offset xyz, rotation offset xyz, scale divider xyz);
        keys = (translation xyz, rotation xyz, scale xyz)."""
        o = self.chunks['animation_table']
        out = {}
        for i in range(self.u32(o)):
            a = self.base + self.u32(o + 4 + 4 * i)
            name = self.s(self.u32(a))
            mt = self.u32(a + 0x0C)
            tracks = {}
            if mt:
                t0 = self.base + mt
                for j in range(self.u32(t0)):
                    e = self.base + self.u32(t0 + 4 + 4 * j)
                    mesh = self.s(self.u32(e))
                    offs = self.f32(e + 4, 9)
                    n = self.u32(e + 0x58)
                    keys = []
                    for k in (0, n - 1) if n else ():
                        f = e + 0x5C + k * (4 + 21 * 0x14) + 4
                        keys.append(tuple(self.f32(f + c * 0x14) for c in range(9)))
                    if keys:
                        tracks[mesh] = (offs, keys[0], keys[-1])
            out[name] = tracks
        return out

    def pose(self, anims, played):
        """Puts animated nodes at the first or last key of the animations a script plays.
        played: {animation name: True for its last key, False for its first}."""
        for name, last in played.items():
            for mesh, (offs, first_key, last_key) in anims.get(name, {}).items():
                node = self.find(mesh)
                if node is None:
                    continue
                k = last_key if last else first_key
                sc = [k[6 + c] / offs[6 + c] if offs[6 + c] else k[6 + c] for c in range(3)]
                node['anim'] = _srt(sc, [k[3 + c] - offs[3 + c] for c in range(3)],
                                    [k[c] - offs[c] for c in range(3)])
        self._update(self.root, np.eye(4))

    def _update(self, node, parent):
        node['mtx'] = parent @ node['local']
        if node['anim'] is not None:
            node['mtx'] = node['mtx'] @ node['anim']
        for c in node['children']:
            self._update(c, node['mtx'])

    def find(self, name, node=None):
        node = node or self.root
        if node['name'] == name:
            return node
        for c in node['children']:
            r = self.find(name, c)
            if r:
                return r
        return None

    def walk(self, node=None, skip=()):
        """Every node under a node, leaving out subtrees whose name is in skip."""
        node = node or self.root
        if node['name'] in skip:
            return
        yield node
        for c in node['children']:
            yield from self.walk(c, skip)

    def mesh(self, mesh_off):
        """Triangles of one mesh: dict of pos (N,3), clr (N,4) or None, uv[k] (N,2) or None, as triangle soup."""
        d = self.d
        packed = d[mesh_off + 3]
        assert packed, 'unpacked meshes are TTYD-only'
        count, bits = self.u32(mesh_off + 4), self.u32(mesh_off + 8)
        attrs = [('pos', 1)]
        if bits & 2:
            attrs.append(('nrm', 2))
        if bits & 4:
            attrs.append(('clr', 4))
        if bits & 8:
            attrs.append(('clr1', 8))
        for k in range(3):
            if bits & (16 << k):
                attrs.append((f'tex{k}', 16 << k))
        assert not bits & ~0x7f, f'unexpected vcd bits {bits:#x}'
        na = len(attrs)
        tris = []
        for j in range(count):
            dl, size = struct.unpack('>II', d[mesh_off + 0x10 + 8 * j:mesh_off + 0x18 + 8 * j])
            p, end = self.base + dl, self.base + dl + size
            while p < end:
                cmd = d[p]
                if cmd == 0:
                    p += 1
                    continue
                prim = cmd & 0xF8
                assert prim in (0x80, 0x90, 0x98, 0xA0), f'display list command {cmd:#x}'
                n = struct.unpack('>H', d[p + 1:p + 3])[0]
                idx = np.frombuffer(d, '>u2', n * na, p + 3).reshape(n, na).astype(np.int64)
                p += 3 + n * na * 2
                if prim == 0x90:
                    order = np.arange(n - n % 3).reshape(-1, 3)
                elif prim == 0x98:
                    i = np.arange(n - 2)
                    order = np.stack([i, i + 1 + (i & 1) * 0, i + 2], 1)
                    odd = (i & 1) == 1
                    order[odd] = np.stack([i[odd] + 1, i[odd], i[odd] + 2], 1)
                elif prim == 0xA0:
                    i = np.arange(1, n - 1)
                    order = np.stack([np.zeros_like(i), i, i + 1], 1)
                else:  # quads
                    q = np.arange(n // 4) * 4
                    order = np.concatenate([np.stack([q, q + 1, q + 2], 1), np.stack([q, q + 2, q + 3], 1)])
                if len(order):
                    tris.append(idx[order.reshape(-1)])
        if not tris:
            return None
        idx = np.concatenate(tris)
        cols = {a: i for i, (a, _) in enumerate(attrs)}
        out = {'pos': self.pos[idx[:, cols['pos']]]}
        out['clr'] = self.clr[idx[:, cols['clr']]] if 'clr' in cols else None
        out['uv'] = [self.tex[k][idx[:, cols[f'tex{k}']]] if f'tex{k}' in cols else None for k in range(3)]
        return out

    def meshes(self, root_name=None, skip=()):
        """Yields (node, material, mesh) in world space for every drawable part under a node (default: the S node)."""
        top = self.find(root_name or self.s_node)
        if top is None:
            return
        for node in self.walk(top, skip):
            for mat, mesh_off in node['parts']:
                m = self.mesh(mesh_off)
                if m is None:
                    continue
                p = np.c_[m['pos'], np.ones(len(m['pos']))] @ node['mtx'].T
                m['pos'] = p[:, :3].astype(np.float32)
                # texture matrices
                for k, smp in enumerate(mat['samplers']):
                    if k < 3 and m['uv'][k] is not None:
                        uv = m['uv'][k]
                        m['uv'][k] = (np.c_[uv, np.ones(len(uv))] @ smp['mtx'].T)[:, :2].astype(np.float32)
                yield node, mat, m
