# Packs the terrain maps for the ROM. The renderer reads terrain_m / terrain_r only off the street grid,
# i.e. in the coast ring: rows y < 64 and y >= 1358 in full, and x < 64 / x >= 1358 of the rows between.
# Inside the grid the game only asks "is this water?" (collisions), so the interior keeps one bit per cell.
#   python3 tools/pack_terrain.py build/terrain.bin build/tring.bin
# Layout: ring_m[RING_N], ring_r[RING_N] (ring order: rows y < 64, then for rows 64..1357 cells x < 64 and
# x >= 1358, then rows y >= 1358), then the interior water bits: rows 64..1357, cells 64..1357, 1295 bits
# per row padded to 1296 (162 bytes), bit x - 64 of row y - 64 at byte (y - 64) * 162 + (x - 64) / 8, bit (x - 64) & 7,
# then u32 ring_lo[1424], ring_hi[1424] (ring index = ring_lo[v] + u for u < 712, else ring_hi[v] + u; mod 2^32).
import sys
import numpy as np
W, A, B = 1424, 64, 1358
d = open(sys.argv[1], 'rb').read()
M = np.frombuffer(d[:W * W], np.uint8).reshape(W, W)
R = np.frombuffer(d[W * W:2 * W * W], np.uint8).reshape(W, W)
def ring(X):
    rows = []
    for y in range(W):
        rows.append(X[y] if y < A or y >= B else np.concatenate([X[y, :A], X[y, B:]]))
    return np.concatenate(rows)
rm, rr = ring(M), ring(R)
assert len(rm) == A * W + (B - A) * (A + W - B) + (W - B) * W
assert not R[A:B, A:B].any()                       # no road samples inside the grid
water = (M[A:B, A:B] & 127) == 1
bits = np.zeros((B - A, 1296), np.uint8); bits[:, :B - A] = water
packed = np.packbits(bits, axis=1, bitorder='little')
# row tables for the renderer: ring index of cell (u, v) = ring_lo[v] + u (u < 712) or ring_hi[v] + u
MID = A * W; BOT = MID + (B - A) * (A + W - B)
lo = np.zeros(W, np.uint32); hi = np.zeros(W, np.uint32)
for v in range(W):
    if v < A: lo[v] = hi[v] = v * W
    elif v >= B: lo[v] = hi[v] = BOT + (v - B) * W
    else: lo[v] = MID + (v - A) * (A + W - B); hi[v] = lo[v] + A - B
out = rm.tobytes() + rr.tobytes() + packed.tobytes() + lo.tobytes() + hi.tobytes()
open(sys.argv[2], 'wb').write(out)
print('terrain packed: %d + %d ring bytes, %d water-mask bytes (%.2f MB, was %.2f MB)' % (len(rm), len(rr), packed.size, len(out) / 1e6, 2 * W * W / 1e6))
