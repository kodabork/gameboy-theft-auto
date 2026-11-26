# Lane-line lookup tables for the off-grid ground renderer (vfill.s gmiss; the C version in render.c
# works the same values out directly). A "block" is a 2x2 group of road samples (terrain_r), named by
# its top-left sample; every off-grid ground pixel lies in a block of the coast ring: x or y < 64 or
# >= 1358 (block coordinates).
#   lanering.bin: u16 per ring block, ring order (rows y < 64, then rows 64..1357 holding x < 64 and
#     x >= 1358, then rows y >= 1358):
#       0xFFF0 + z  one lane zone z (0..8, see lane_zone in render.c) for every pixel of the block
#       0xFFFE      not every sample is on a road: the plain terrain colour
#       0xFFFD      more than 4 zones: worked out per pixel
#       k           zones per 1/16 m texel in laneside.bin entry k (identical entries shared)
#   laneside.bin: 68 bytes per entry: a 4-zone palette, then 16x16 2-bit palette codes (row fy, 4
#     texels per byte, texel fx in bits (fx & 3) * 2).
#   python3 tools/gen_lanez.py build/terrain.bin build/lanering.bin build/laneside.bin
import sys
import numpy as np
W = 1424
d = open(sys.argv[1], 'rb').read()
R = np.frombuffer(d[W * W:2 * W * W], np.uint8).reshape(W, W).astype(np.int64)
TH = np.array([218, 230, 262, 499, 526, 763, 795, 807])
def zone(t): return np.searchsorted(TH, t, side='right')
ring = np.full((W, W), 0xFFFE, np.uint32)
r0, r1, r2, r3 = R[:-1, :-1], R[:-1, 1:], R[1:, :-1], R[1:, 1:]
ok = (r0 != 0) & (r1 != 0) & (r2 != 0) & (r3 != 0)
q = [x & 127 for x in (r0, r1, r2, r3)]
lo = np.minimum(np.minimum(q[0], q[1]), np.minimum(q[2], q[3])) * 8
hi = np.maximum(np.maximum(q[0], q[1]), np.maximum(q[2], q[3])) * 8
zl, zh = zone(lo), zone(hi)
inner = np.where(ok, np.where(zl == zh, 0xFFF0 + zl, 0x10000), 0xFFFE)
ring[:-1, :-1] = inner
fx = np.arange(16)[None, :]; fy = np.arange(16)[:, None]
side = bytearray(); k = 0; many = 0; seen = {}
for j, i in zip(*np.nonzero(ring == 0x10000)):
    a0, a1, b0, b1 = q[0][j, i], q[1][j, i], q[2][j, i], q[3][j, i]
    la = a0 * 16 + fx * (a1 - a0); lb = b0 * 16 + fx * (b1 - b0)
    t = (16 * la + fy * (lb - la)) >> 5
    zz = zone(t)
    pal = sorted(set(int(v) for v in zz.ravel()))
    if len(pal) == 1: ring[j, i] = 0xFFF0 + pal[0]; continue
    if len(pal) > 4: ring[j, i] = 0xFFFD; many += 1; continue
    code = np.zeros_like(zz)
    for c, zv in enumerate(pal): code[zz == zv] = c
    ent = bytearray(pal + [0] * (4 - len(pal)))
    for y in range(16):
        for x4 in range(4):
            b = 0
            for e in range(4): b |= int(code[y, x4 * 4 + e]) << (2 * e)
            ent.append(b)
    ent = bytes(ent)
    if ent not in seen: seen[ent] = k; side += ent; k += 1      # identical maps are stored once
    ring[j, i] = seen[ent]
assert k < 0xFFF0
rows = []
for y in range(W):
    if y < 64 or y >= 1358: rows.append(ring[y, :])
    else: rows.append(np.concatenate([ring[y, :64], ring[y, 1358:]]))
out = np.concatenate(rows).astype(np.uint16)
assert len(out) == 64 * W + (1358 - 64) * 130 + (W - 1358) * W
open(sys.argv[2], 'wb').write(out.tobytes())
open(sys.argv[3], 'wb').write(bytes(side))
print('lane tables: %d ring blocks, %d texel maps (%d bytes), %d worked out per pixel' % (len(out), k, len(side), many))
