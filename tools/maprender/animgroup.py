"""Parser for TTYD/SPM "AnimGroup" models (disc/DATA/files/a/<name>, textures in a/<name>-).

These are the models of map objects (blocks, switches, ...) and characters. Follows noclip.website's
src/PaperMarioTTYD/AnimGroup.ts. Only still poses: the rest pose, optionally with an animation's first
keyframe applied for part visibility and texture frames (pose()).

AnimGroup(data).meshes() yields (shape name, tex index or -1, wrap (s, t), cull, mesh) in model space, where
mesh is a triangle soup like mapdat's: pos (N,3), clr (N,4) or None, uv [(N,2) or None].
"""
import math
import struct

import numpy as np

WRAP_CLAMP, WRAP_REPEAT, WRAP_MIRROR = 0, 1, 2


def _str(d, off, n):
    s = d[off:off + n]
    return s[:s.index(b'\0')].decode('ascii', 'replace') if b'\0' in s else s.decode('ascii', 'replace')


def _t(x, y, z):
    m = np.eye(4)
    m[:3, 3] = (x, y, z)
    return m


def _s(x, y, z):
    return np.diag([x, y, z, 1.0])


def _rx(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[1, 0, 0, 0], [0, c, -s, 0], [0, s, c, 0], [0, 0, 0, 1.0]])


def _ry(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, 0, s, 0], [0, 1, 0, 0], [-s, 0, c, 0], [0, 0, 0, 1.0]])


def _rz(a):
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s, 0, 0], [s, c, 0, 0], [0, 0, 1, 0], [0, 0, 0, 1.0]])


class AnimGroup:
    def __init__(self, data):
        self.d = d = data
        u = lambda o: struct.unpack('>I', d[o:o + 4])[0]
        self.name = _str(d, 0x04, 0x40)
        self.tex_name = _str(d, 0x44, 0x40)  # its texture file is a/<tex_name>- (shared by some models)
        self.bbox = (struct.unpack('>3f', d[0xD0:0xDC]), struct.unpack('>3f', d[0xDC:0xE8]))
        (shape_n, _, pos_n, _, nrm_n, _, clr_n, _) = struct.unpack('>8I', d[0xE8:0x108])
        (texmtx_n, texbase_n, tex_n, draw_n, vis_n, node_n, group_n, _) = struct.unpack('>8I', d[0x12C:0x14C])
        self.shape_off, self.dc_off = u(0x14C), u(0x150)
        self.pos_off, self.idx_pos = u(0x154), u(0x158)
        self.idx_nrm = u(0x160)
        self.clr_off, self.idx_clr = u(0x164), u(0x168)
        self.idx_tex = [u(0x16C + 4 * i) for i in range(8)]
        self.tex_off = u(0x18C)
        texmtx_off, texbase_off, textures_off = u(0x190), u(0x194), u(0x198)
        self.draw_off = u(0x19C)
        vis_off, node_off, group_off = u(0x1A0), u(0x1A4), u(0x1A8)
        self.shape_n = shape_n
        self.texmtx = [struct.unpack('>B3x5f', d[texmtx_off + 0x18 * i:texmtx_off + 0x18 * i + 0x18])
                       for i in range(texmtx_n)]  # (idx add, trans s, trans t, scale s, scale t, rotate)
        self.texbase = [struct.unpack('>Ii', d[texbase_off + 8 * i:texbase_off + 8 * i + 8]) for i in range(texbase_n)]
        self.textures = [u(textures_off + 0x40 * i + 4) for i in range(tex_n)]  # index into the TPL
        self.vis = d[vis_off:vis_off + vis_n]
        self.node = np.frombuffer(d, '>f4', node_n, node_off).astype(np.float64)
        self.vis = bytearray(self.vis)
        self.texmtx = [list(t) for t in self.texmtx]
        anim_n, anim_off = u(0x148), u(0x1AC)
        self.anims = {}
        for i in range(anim_n):
            a = anim_off + 0x40 * i
            self.anims[_str(d, a, 0x3C)] = u(a + 0x3C)
        self.groups = []
        for i in range(group_n):
            g = group_off + 0x58 * i
            nxt, child, shape, visi, nodei, ssc = struct.unpack('>iiiIII', d[g + 0x40:g + 0x58])
            self.groups.append({'name': _str(d, g, 0x40), 'next': nxt, 'child': child, 'shape': shape,
                                'vis': visi, 'node': nodei, 'ssc': bool(ssc)})

    def pose(self, anim):
        """Applies an animation's first keyframe: which parts are visible and which texture frame shows."""
        off = self.anims.get(anim)
        if not off:
            return False
        d = self.d
        u = lambda o: struct.unpack('>I', d[o:o + 4])[0]
        frame = off + u(off + 0x28)
        texmtx_upd, vis_upd = off + u(off + 0x34), off + u(off + 0x38)
        tm_start, tm_n, vis_start, vis_n = struct.unpack('>4I', d[frame + 0x14:frame + 0x24])
        idx = 0
        for k in range(tm_n):
            e = texmtx_upd + (tm_start + k) * 0x0C
            delta, tex_delta = d[e], struct.unpack('>b', d[e + 1:e + 2])[0]
            ts, tt = struct.unpack('>2f', d[e + 4:e + 12])
            idx += delta
            if idx < len(self.texmtx):
                self.texmtx[idx][0] += tex_delta
                self.texmtx[idx][1] += ts
                self.texmtx[idx][2] += tt
        idx = 0
        for k in range(vis_n):
            e = vis_upd + (vis_start + k) * 2
            idx += d[e]
            v = struct.unpack('>b', d[e + 1:e + 2])[0]
            if v and idx < len(self.vis):
                self.vis[idx] = 1 if v == 1 else 0
        return True

    def _node_matrix(self, i, ssc_parent):
        n = self.node[i:i + 24]
        t, sc, r1, r2 = n[0:3], n[3:6], n[6:9], n[9:12]
        rc, scc, rp, sp = n[12:15], n[15:18], n[18:21], n[21:24]
        scale = _t(*(scc + sp)) @ _s(*sc) @ _t(*(-scc))
        rad = math.radians
        rot = (_rz(rad(r2[2])) @ _ry(rad(r2[1])) @ _rx(rad(r2[0])) @
               _rz(rad(r1[2] * 2)) @ _ry(rad(r1[1] * 2)) @ _rx(rad(r1[0] * 2)))
        rot = _t(*(rc + rp)) @ rot @ _t(*(-rc))
        if ssc_parent >= 0:
            p = self.node[ssc_parent + 3:ssc_parent + 6]
            rot = _s(1 / p[0], 1 / p[1], 1 / p[2]) @ rot
        return _t(*t) @ rot @ scale

    hidden = frozenset()  # group names left out with everything under them (a boss without its sprout or helmet)

    def _walk(self, gi, parent_gi, parent_mtx):
        while gi != -1:
            g = self.groups[gi]
            if g['name'] not in self.hidden and (g['vis'] >= len(self.vis) or self.vis[g['vis']]):
                ssc = self.groups[parent_gi]['node'] if g['ssc'] and parent_gi >= 0 else -1
                m = parent_mtx @ self._node_matrix(g['node'], ssc)
                if g['shape'] != -1:
                    yield g['shape'], m
                if g['child'] != -1:
                    yield from self._walk(g['child'], gi, m)
            gi = g['next']

    def _shape(self, si):
        d = self.d
        s = self.shape_off + 0xA8 * si
        name = _str(d, s, 0x40)
        pos_first, _, _, _, clr_first, _ = struct.unpack('>6I', d[s + 0x40:s + 0x58])
        tex_first, tex_cnt = struct.unpack('>II', d[s + 0x58:s + 0x60])
        draw_start, draw_cnt, _, cull = struct.unpack('>4I', d[s + 0x98:s + 0xA8])
        out = []
        for k in range(draw_cnt):
            dr = self.draw_off + (draw_start + k) * 0x6C
            tex_count = struct.unpack('>I', d[dr:dr + 4])[0]
            tex_id = struct.unpack('>i', d[dr + 0x10:dr + 0x14])[0] if tex_count else -1
            run_start, run_cnt, ip, _, ic = struct.unpack('>5I', d[dr + 0x38:dr + 0x4C])
            it = struct.unpack('>I', d[dr + 0x4C:dr + 0x50])[0]
            pos, clr, uv = [], [], []
            for j in range(run_cnt):
                e = self.dc_off + (run_start + j) * 8
                start, n = struct.unpack('>II', d[e:e + 8])
                if n < 3:
                    continue
                fan = np.stack([np.zeros(n - 2, int), np.arange(1, n - 1), np.arange(2, n)], 1).reshape(-1)
                gp = np.frombuffer(d, '>u4', n, self.idx_pos + (start + ip) * 4).astype(np.int64)[fan]
                pos.append(np.frombuffer(d, '>f4', (pos_first + gp.max() + 1) * 3, self.pos_off)
                           .reshape(-1, 3)[pos_first + gp])
                gc = np.frombuffer(d, '>u4', n, self.idx_clr + (start + ic) * 4).astype(np.int64)[fan]
                clr.append(np.frombuffer(d, np.uint8, (clr_first + gc.max() + 1) * 4, self.clr_off)
                           .reshape(-1, 4)[clr_first + gc])
                if tex_count:
                    gt = np.frombuffer(d, '>u4', n, self.idx_tex[0] + (start + it) * 4).astype(np.int64)[fan]
                    uv.append(np.frombuffer(d, '>f4', (tex_first + gt.max() + 1) * 2, self.tex_off)
                              .reshape(-1, 2)[tex_first + gt])
            if pos:
                out.append((name, tex_id, cull, {'pos': np.concatenate(pos).astype(np.float32),
                                                 'clr': np.concatenate(clr),
                                                 'uv': [np.concatenate(uv).astype(np.float32) if uv else None,
                                                        None, None]}))
        return out

    def texture_for(self, tex_id):
        """(TPL texture index, wrap_s, wrap_t, (trans s, trans t, scale s, scale t)) for a draw's tex id."""
        add, ts, tt, ss, st, _ = self.texmtx[tex_id] if tex_id < len(self.texmtx) else (0, 0, 0, 1, 1, 0)
        base, flags = self.texbase[tex_id]
        wrap_s = WRAP_MIRROR if flags & 4 else WRAP_REPEAT if flags & 1 else WRAP_CLAMP
        wrap_t = WRAP_MIRROR if flags & 8 else WRAP_REPEAT if flags & 2 else WRAP_CLAMP
        return self.textures[base + add], wrap_s, wrap_t, (ts, tt, ss, st)

    def meshes(self):
        """(shape name, TPL texture index or -1, wrap_s, wrap_t, cull, mesh) for the rest pose, in model space."""
        for si, m in self._walk(len(self.groups) - 1, -1, np.eye(4)):
            for name, tex_id, cull, mesh in self._shape(si):
                p = np.c_[mesh['pos'], np.ones(len(mesh['pos']))] @ m.T
                mesh['pos'] = p[:, :3].astype(np.float32)
                tex, ws, wt = -1, WRAP_REPEAT, WRAP_REPEAT
                if tex_id >= 0 and mesh['uv'][0] is not None:
                    tex, ws, wt, (ts, tt, ss, st) = self.texture_for(tex_id)
                    uv = mesh['uv'][0]
                    mesh['uv'][0] = np.stack([uv[:, 0] * ss + ts, uv[:, 1] * st + tt], 1).astype(np.float32)
                yield name, tex, ws, wt, cull, mesh
