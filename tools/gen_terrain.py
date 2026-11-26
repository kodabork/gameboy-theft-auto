# Generates the island terrain: a 1 m heightmap and material map covering the city grid
# plus a 64 m strip of land around it with a winding, hilly coastal road.
#   python3 gen_terrain.py build/terrain.bin src/terrain.c src/terrain.h [preview.png]
import sys, math, random
import numpy as np

out_bin, out_c, out_h = sys.argv[1:4]
MARGIN = 64                     # metres of land/sea modelled around the grid
NB = 20                         # city blocks per side (64 m each)
GRID = NB * 64 + 15.0           # grid extent in metres (NB blocks of 64 m + edge road)
MAPW = int(GRID) + 1 + 2 * MARGIN   # cells (1 m) per side
MAPW += MAPW & 1
x = np.arange(MAPW) - MARGIN + 0.5
X, Y = np.meshgrid(x, x)        # X[row, col] = x in metres, Y = y

# material codes (0 = city grid: use the street tile texture)
GRIDM, WATER, SAND, GRASS, DIRT, ROAD, ROADC, ROADE, PATH, GRAVEL = range(10)

# ---------------------------------------------------------------- city hills
def city_height(xm, ym):
    h = 6.0 + 4.5 * np.sin(xm / 95.0 + 0.7) * np.sin(ym / 128.0 + 1.3) + 2.2 * np.sin((xm + ym) / 72.0 + 0.4)
    h += 1.4 * np.sin((xm - 0.6 * ym) / 41.0)
    return h

H = city_height(X, Y)
M = np.full(X.shape, GRIDM, np.uint8)

# ---------------------------------------------------------------- districts
# block kinds: 0 buildings, 1 park, 2 water. Districts: 0 Docks/Old Town, 1 Downtown, 2 Industrial port,
# 3 the Heights (residential), 4 Gull Island, 5 the river.
KIND = np.zeros((NB, NB), np.uint8); DIST = np.zeros((NB, NB), np.uint8)
lrng = random.Random(21)
park_odds = {0: 14, 1: 12, 2: 22, 3: 5, 4: 9}
for by in range(NB):
    for bx in range(NB):
        if bx < 8: d = 0 if by < 8 else 3
        elif bx == 8: d = 5
        elif bx >= 15 and by >= 15: d = 4
        else: d = 1 if by < 10 else 2
        DIST[by, bx] = d
        if d == 5 or (bx == 14 and by >= 14) or (by == 14 and bx >= 14): KIND[by, bx] = 2
        elif (bx, by) in ((2, 5), (6, 2)) or (by == 8 and bx < 8): KIND[by, bx] = 1
        elif lrng.randrange(park_odds[d]) == 0: KIND[by, bx] = 1
for by in range(NB):
    for bx in range(NB):
        x0, y0 = bx * 64 + 15 + MARGIN, by * 64 + 15 + MARGIN
        if KIND[by, bx] == 2: M[y0:y0 + 47, x0:x0 + 47] = WATER
        elif KIND[by, bx] == 1:
            M[y0:y0 + 47, x0:x0 + 47] = GRASS
            for k in range(3 if (bx + by) % 3 else 2):   # curved footpaths
                t = np.linspace(0, 1, 300)
                if k == 2: px, py = 23 + 13 * np.sin(t * math.pi * 1.6 + bx), 46 * t
                else: px, py = 46 * t, 23 + 13 * np.sin(t * math.pi * (1.3 + 0.4 * k) + k * 1.7 + by) + (k - 0.5) * 12
                for qx, qy in zip(px, py):
                    ix, iy = int(qx) + x0, int(qy) + y0
                    M[max(iy - 1, y0):min(iy + 2, y0 + 47), max(ix - 1, x0):min(ix + 2, x0 + 47)] = PATH

# ---------------------------------------------------------------- coastal road
# rounded rectangle around the grid, pushed out and wiggled along its length
def rounded_rect_path(off, rad, step=1.0):
    lo, hi = -off, GRID + off
    pts = []
    corners = [(hi - rad, lo + rad, -90), (hi - rad, hi - rad, 0), (lo + rad, hi - rad, 90), (lo + rad, lo + rad, 180)]
    starts = [(lo + rad, lo), (hi, lo + rad), (hi - rad, hi), (lo, hi - rad)]
    for i in range(4):
        sx, sy = starts[i]; cx, cy, a0 = corners[i]
        ex, ey = cx + rad * math.cos(math.radians(a0)), cy + rad * math.sin(math.radians(a0))
        n = int(math.hypot(ex - sx, ey - sy) / step)
        for j in range(n): pts.append((sx + (ex - sx) * j / n, sy + (ey - sy) * j / n))
        for j in range(int(rad * math.pi / 2 / step)):
            a = math.radians(a0 + 90 * j / (rad * math.pi / 2 / step))
            pts.append((cx + rad * math.cos(a), cy + rad * math.sin(a)))
    return np.array(pts)

base = rounded_rect_path(30.0, 55.0)
seg = np.diff(np.vstack([base, base[:1]]), axis=0)
s = np.concatenate([[0], np.cumsum(np.hypot(seg[:, 0], seg[:, 1]))])[:-1]
tang = seg / np.hypot(seg[:, 0], seg[:, 1])[:, None]
nrm = np.stack([tang[:, 1], -tang[:, 0]], 1)          # points outward (clockwise path)
Ltot = s[-1] + np.hypot(*seg[-1])
def per(p): return Ltot / max(1, round(Ltot / p))          # periods that close the loop seamlessly
wig = 7.0 * np.sin(2 * math.pi * s / per(190.0) + 0.5) + 3.5 * np.sin(2 * math.pi * s / per(71.0) + 2.0)
road = base + nrm * wig[:, None]
# smooth the wiggled centreline
for _ in range(20):
    road = 0.25 * np.roll(road, 1, 0) + 0.5 * road + 0.25 * np.roll(road, -1, 0)
L = s[-1]
road_h = 5.0 + 3.6 * np.sin(2 * math.pi * s / per(260.0) + 1.0) + 1.8 * np.sin(2 * math.pi * s / per(109.0) + 0.3)

# distance from every ring cell to the road (coarse search via KD-like brute force in chunks)
ring = ~((X >= -2) & (X <= GRID + 2) & (Y >= -2) & (Y <= GRID + 2))
pts = np.stack([X[ring], Y[ring]], 1)
d_best = np.full(len(pts), 1e9); i_best = np.zeros(len(pts), int)
for k in range(0, len(road), 6):
    d = np.hypot(pts[:, 0] - road[k, 0], pts[:, 1] - road[k, 1])
    m = d < d_best; d_best[m] = d[m]; i_best[m] = k
# refine around the best coarse index
for off in range(-6, 7):
    kk = (i_best + off) % len(road)
    d = np.hypot(pts[:, 0] - road[kk, 0], pts[:, 1] - road[kk, 1])
    m = d < d_best; d_best[m] = d[m]; i_best[m] = kk[m]
k = i_best
side = ((pts[:, 0] - road[k, 0]) * nrm[k, 0] + (pts[:, 1] - road[k, 1]) * nrm[k, 1])   # >0 seaward
rh = road_h[k]

# grid-edge height to blend toward on the land side
gx = np.clip(pts[:, 0], 0, GRID); gy = np.clip(pts[:, 1], 0, GRID)
edge_h = city_height(gx, gy)
dist_to_grid = np.hypot(pts[:, 0] - gx, pts[:, 1] - gy)
rgx = np.clip(road[k, 0], 0, GRID); rgy = np.clip(road[k, 1], 0, GRID)
road_to_grid = np.hypot(road[k, 0] - rgx, road[k, 1] - rgy)
side = np.where(dist_to_grid < road_to_grid, -np.abs(side), np.abs(side))   # <0 land side, >0 sea side
gap = np.maximum(road_to_grid - 5.0, 1.0)                # land between grid edge and the road edge
rng = np.random.default_rng(4)
hill = 9.0 * np.clip(np.sin(np.pi * np.clip(dist_to_grid / gap, 0, 1)), 0, 1) * \
       (0.6 + 0.4 * np.sin(pts[:, 0] / 37.0 + 1.1) * np.cos(pts[:, 1] / 29.0))
t_land = np.clip(dist_to_grid / gap, 0, 1)
h_land = edge_h * (1 - t_land) + rh * t_land + hill * (gap > 14)
coast = 13.0 + 5.0 * np.sin(2 * math.pi * s[k] / per(120.0) + 0.8)       # beach width varies
h_sea = rh - (rh + 0.6) * np.clip((side - 6.0) / coast, 0, 1) ** 0.8
hr = np.where(side < -6.0, h_land, np.where(side > 6.0, h_sea, rh))
mr = np.full(len(pts), GRASS, np.uint8)
mr[side > 6.0] = DIRT
mr[(side > 6.0) & (hr < 2.0)] = SAND
mr[hr < 0.05] = WATER
mr[(np.abs(side) <= 6.0)] = GRAVEL
mr[(np.abs(side) <= 4.6)] = ROAD
mr[(np.abs(side) > 3.9) & (np.abs(side) <= 4.4)] = ROADE
mr[np.abs(side) <= 0.45] = ROADC
# dashed centre line
dash = ((s[k] % 9.0) < 4.5)
mr[(np.abs(side) <= 0.45) & ~dash] = ROAD
H[ring] = np.maximum(hr, -1.0)
M[ring] = mr
# lateral road coordinate (1/8 m, +64; 0 = none) so the renderer can draw crisp lane lines at
# any angle by interpolating it; bit 7 = centre-line dash on
R = np.zeros(X.shape, np.uint8)
rv = np.clip(np.round(side * 8) + 64, 1, 127).astype(np.uint8) | (dash.astype(np.uint8) << 7)
rv[np.abs(side) > 7.8] = 0
R[ring] = rv

# the river (block column 8) and Gull Island's channel (row/column 14) run out to the sea; the coast
# road crosses them on bridges
def cut_channel(xa, xb, ya, yb):
    sl = M[ya + MARGIN:yb + MARGIN, xa + MARGIN:xb + MARGIN]
    keep = np.isin(sl, [ROAD, ROADC, ROADE, GRAVEL])
    sl[~keep] = WATER
cut_channel(8 * 64 + 15, 8 * 64 + 62, -MARGIN, 0)
cut_channel(8 * 64 + 15, 8 * 64 + 62, int(GRID), MAPW - MARGIN)
cut_channel(14 * 64 + 15, 14 * 64 + 62, int(GRID), MAPW - MARGIN)
cut_channel(int(GRID), MAPW - MARGIN, 14 * 64 + 15, 14 * 64 + 62)
# connector roads from the grid edge out to the coastal road (roads 2 and 6 on every side)
def stamp_road(xa, ya, xb, yb, c0):
    n = int(math.hypot(xb - xa, yb - ya))
    for j in range(n + 1):
        qx, qy = xa + (xb - xa) * j / n, ya + (yb - ya) * j / n
        ix, iy = int(qx + MARGIN), int(qy + MARGIN)
        for dy in range(-5, 6):
            for dx in range(-5, 6):
                if abs(dx if xa == xb else dy) > 5: continue
                lat = dx if xa == xb else dy
                cx_, cy_ = ix + dx, iy + dy
                if 0 <= cx_ < MAPW and 0 <= cy_ < MAPW and M[cy_, cx_] != GRIDM:
                    if R[cy_, cx_] and abs(int(R[cy_, cx_] & 127) - 64) < 44: continue   # coast road wins
                    M[cy_, cx_] = ROADC if (lat == 0 and (j % 9) < 5) else ROADE if abs(lat) == 5 else ROAD
                    lm = (cx_ - MARGIN + 0.5 - c0) if xa == xb else (cy_ - MARGIN + 0.5 - c0)
                    R[cy_, cx_] = int(np.clip(round(lm * 8 * 0.8) + 64, 1, 127)) | (0x80 if (j % 9) < 5 else 0)
for rr in (2, 6, 10, 14, 18):
    c = rr * 64 + 6
    stamp_road(c, -2, c, -48, c); stamp_road(c, GRID + 2, c, GRID + 48, c)
    stamp_road(-2, c, -48, c, c); stamp_road(GRID + 2, c, GRID + 48, c, c)

# grid cells keep code 0, but the far edge strip of the grid that is not road/sidewalk is land
H = np.clip(H, -1.0, 60.0)
hq = np.clip(np.round((H + 1.0) * 8), 0, 255).astype(np.uint8)  # eighth metres, +1 m offset
# shading bit (bit 7) for natural ground: slopes facing the sun are lit, plus 1 m speckle
gyH, gxH = np.gradient(H)
sun = -(gxH * 0.6 + gyH * 0.8)                                   # sun from -x,-y
nz = np.random.default_rng(11).random(H.shape)
lit = (sun > -0.06) & ~((M == GRASS) & (nz < 0.22)) & ~((M == SAND) & (nz < 0.12)) & ~((M == DIRT) & (nz < 0.3))
lit |= (M == WATER) & (nz < 0.75)
lit |= np.isin(M, [ROAD, ROADC, ROADE, PATH])
lit[M == GRIDM] = False
Ms = (M | (lit.astype(np.uint8) << 7)).astype(np.uint8)
# radar minimap, 2 m per texel (radar palette indices)
MMW = MAPW // 2
TORG = -MARGIN * 16
mm = np.zeros((MMW, MMW), np.uint8)
for j in range(MMW):
    wy = TORG + j * 32 + 16
    for i in range(MMW):
        wx = TORG + i * 32 + 16
        tx, ty = wx >> 3, wy >> 3
        bx, by, lx, ly = tx >> 7, ty >> 7, tx & 127, ty & 127
        if wx < 0 or wy < 0 or bx > NB or by > NB or (bx == NB and lx >= 30) or (by == NB and ly >= 30):
            mi, mj = (wx - TORG) >> 4, (wy - TORG) >> 4
            m = M[mj, mi] if 0 <= mi < MAPW and 0 <= mj < MAPW else WATER
            col = 3 if m == WATER else 2 if m in (ROAD, ROADC, ROADE) else 12 if m == SAND else 11
        elif lx < 24 or ly < 24: col = 2
        elif lx < 30 or ly < 30 or lx >= 122 or ly >= 122: col = 10
        else: col = 1 if bx >= NB or by >= NB else (1, 11, 3)[KIND[by, bx]]
        mm[j, i] = col
open(out_bin, 'wb').write(Ms.tobytes() + R.tobytes() + mm.tobytes())
n2 = MAPW * MAPW
open(out_c.replace('terrain.c', 'terrain.s'), 'w').write('''    .section .rodata.terrain, "a"
    .global radar_map
    .align 2
radar_map:
    .incbin "build/terrain.bin", %d, %d
''' % (2 * n2, MMW * MMW))
# terrain_m / terrain_r go into the ROM packed (tools/pack_terrain.py -> build/tring.bin, src/tring.s)

# coastal-road waypoints every 8 m (units: 1/16 m), trees on the wooded hills, lamps
wp = road[::8]
ring_trees = []
rnd = random.Random(9)
for _ in range(12000):
    j = rnd.randrange(len(pts))
    if mr[j] == GRASS and side[j] < -9 and hill[j] > 2.0 and len(ring_trees) < 400:
        ring_trees.append((pts[j, 0], pts[j, 1]))
lamps = []
for j in range(0, len(road), 34):
    lx, ly = road[j] - nrm[j] * 6.2
    ang = math.atan2(nrm[j, 1], nrm[j, 0])                       # arm reaches seaward over the road
    lamps.append((lx, ly, int(round(ang / (2 * math.pi) * 4096)) & 4095))
def u(v): return int(round(v * 16))
C = ['#include "terrain.h"', '',
     'const short coast_wp[%d][2] = {' % len(wp)] + ['  {%d,%d},' % (u(a), u(b)) for a, b in wp] + ['};',
     'const short ring_trees[%d][2] = {' % len(ring_trees)] + ['  {%d,%d},' % (u(a), u(b)) for a, b in ring_trees] + ['};',
     'const short coast_lamps[%d][3] = {' % len(lamps)] + ['  {%d,%d,%d},' % (u(a), u(b), c) for a, b, c in lamps] + ['};',
     'const unsigned char city_kind[%d] = {' % (NB * NB)] + ['  ' + ','.join(str(int(v)) for v in row) + ',' for row in KIND] + ['};',
     'const unsigned char city_district[%d] = {' % (NB * NB)] + ['  ' + ','.join(str(int(v)) for v in row) + ',' for row in DIST] + ['};']
open(out_c, 'w').write('\n'.join(C) + '\n')
Hh = ['#ifndef TERRAIN_H', '#define TERRAIN_H', '/* generated by tools/gen_terrain.py */',
      '#define TMAP_W %d' % MAPW, '#define TMAP_ORIGIN %d   /* units: map cell 0 starts at -64 m */' % (-MARGIN * 16),
      'enum { TM_GRID, TM_WATER, TM_SAND, TM_GRASS, TM_DIRT, TM_ROAD, TM_ROADC, TM_ROADE, TM_PATH, TM_GRAVEL };',
      '#define CITY_NB %d   /* city blocks per side */' % NB,
      'enum { BK_BUILT, BK_PARK, BK_WATER };',
      'enum { DI_DOCKS, DI_DOWNTOWN, DI_INDUSTRIAL, DI_HEIGHTS, DI_ISLAND, DI_RIVER };',
      'extern const unsigned char city_kind[%d], city_district[%d];' % (NB * NB, NB * NB),
      '#ifdef HOST', 'extern unsigned char terrain_m[], terrain_r[], radar_map[];', '#else',
      'extern const unsigned char tring_m[], tring_r[], tring_water[];   /* packed terrain_m / terrain_r (tools/pack_terrain.py, ground.h) */',
      'extern const unsigned char radar_map[];     /* RADAR_MAP_W^2, 2 m texels, radar colours */', '#endif',
      '#define RADAR_MAP_W %d' % MMW,
      '#define NCOAST_WP %d' % len(wp), 'extern const short coast_wp[%d][2];' % len(wp),
      '#define NRING_TREES %d' % len(ring_trees), 'extern const short ring_trees[%d][2];' % len(ring_trees),
      '#define NCOAST_LAMPS %d' % len(lamps), 'extern const short coast_lamps[%d][3];' % len(lamps),
      '#endif']
open(out_h, 'w').write('\n'.join(Hh) + '\n')
print('terrain', MAPW, 'x', MAPW, 'height range', float(H.min()), float(H.max()), 'waypoints', len(wp), 'trees', len(ring_trees), 'lamps', len(lamps))

if len(sys.argv) > 4:
    from PIL import Image
    pal = np.array([(90, 90, 95), (50, 80, 120), (210, 195, 150), (80, 120, 60), (120, 100, 80), (60, 60, 64), (220, 190, 60), (230, 230, 230), (170, 160, 140), (140, 130, 120)], np.uint8)
    img = pal[M].astype(np.float32)
    shade = 0.6 + 0.4 * np.clip(H / 16, 0, 1)
    img *= shade[..., None]
    Image.fromarray(img.clip(0, 255).astype(np.uint8)).save(sys.argv[4])
