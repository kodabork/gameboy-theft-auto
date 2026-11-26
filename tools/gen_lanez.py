# Lane-line zone of every 2x2 block of road samples, for the ground renderer (render.c lane_px / vfill.s).
# Byte rk (the block's top-left sample index in terrain_r): 0..8 = every pixel of the block is in that
# zone (see lane_zone in render.c), 0xFE = not every sample is on a road (plain terrain colour),
# 0xFF = a zone boundary may cross the block (worked out per pixel).
#   python3 tools/gen_lanez.py build/terrain.bin build/lanez.bin
import sys
import numpy as np
W = 1424
d = open(sys.argv[1], 'rb').read()
R = np.frombuffer(d[W * W:2 * W * W], np.uint8).reshape(W, W).astype(np.int32)
r0, r1, r2, r3 = R[:-1, :-1], R[:-1, 1:], R[1:, :-1], R[1:, 1:]
ok = (r0 != 0) & (r1 != 0) & (r2 != 0) & (r3 != 0)
q = [x & 127 for x in (r0, r1, r2, r3)]
lo = np.minimum(np.minimum(q[0], q[1]), np.minimum(q[2], q[3])) * 8
hi = np.maximum(np.maximum(q[0], q[1]), np.maximum(q[2], q[3])) * 8
def zone(t):
    z = np.zeros_like(t)
    for th in (218, 230, 262, 499, 526, 763, 795, 807): z += (t >= th)
    return z
zl, zh = zone(lo), zone(hi)
out = np.full((W, W), 0xFE, np.uint8)
inner = np.where(ok, np.where(zl == zh, zl, 0xFF), 0xFE).astype(np.uint8)
out[:-1, :-1] = inner
open(sys.argv[2], 'wb').write(out.tobytes())
print('lanez: constant %d, varies %d' % (int(((inner < 9)).sum()), int((inner == 0xFF).sum())))
