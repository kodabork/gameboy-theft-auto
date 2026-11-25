# Renders every sprite frame, builds one shared palette, writes src/assets.c + build/assets.bin.
import sys, os, time, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import *
from frame_render import crop, run_all

OUT_C, OUT_BIN = sys.argv[1], sys.argv[2]
NCOL = 148                       # sprite colours (palette indices 104..254); 0 = transparent

frames = []                      # (rgb_logical, mask, shadowmask, ox, oy, upx_q8)

def add_image(col, mask, smask, ox, oy, upx):
    frames.append(crop(col, mask, smask, ox, oy, upx))

jobs = []                        # deferred SDF renders: run in parallel (and cached) before the palette
def add(model, yaw, elev, s, w_sq, up, down, shadow=True, ss=1, outline=False):
    frames.append(len(jobs))
    jobs.append((model, (yaw, elev, s, w_sq, up, down), dict(shadow=shadow, ss=ss, outline=outline)))

sets = []
def begin(name): sets.append((name, len(frames)))

t0 = time.time()
from sprites_people import Human, SportCar, blood_pool
from cars_real import RealCar
from cars3 import Car3
CARS = [('CAR_COUPE_BLUE', Car3('gt', (0.60, 0.04, 0.04))),
        ('CAR_COUPE_BLACK', Car3('gt', (0.025, 0.025, 0.03))),
        ('CAR_MUSCLE_ORANGE', Car3('muscle', (0.80, 0.30, 0.03), stripe=(0.03, 0.03, 0.03))),
        ('CAR_MUSCLE_SILVER', Car3('muscle', (0.60, 0.61, 0.64), stripe=(0.05, 0.10, 0.40))),
        ('CAR_HATCH_LIME', Car3('compact', (0.06, 0.18, 0.58), sport_wheels=True)),
        ('CAR_HATCH_WHITE', Car3('compact', (0.82, 0.83, 0.84))),
        ('CAR_SUPER_RED', Car3('super', (0.62, 0.63, 0.66))),
        ('CAR_SUPER_YELLOW', Car3('super', (0.88, 0.66, 0.05))),
        ('CAR_SEDAN_SILVER', Car3('fullsize', (0.62, 0.60, 0.55))),
        ('CAR_SEDAN_BLUE', Car3('fullsize', (0.09, 0.17, 0.14))),
        ('CAR_TAXI', Car3('fullsize', (0.86, 0.64, 0.07), taxi=True)),
        ('CAR_SUV_BLACK', Car3('suv', (0.03, 0.03, 0.035))),
        ('CAR_SUV_WHITE', Car3('suv', (0.82, 0.82, 0.80))),
        ('CAR_POLICE', Car3('fullsize', (0.03, 0.03, 0.035), police=True)),
        ('CAR_HATCH5_RED', Car3('hatch5', (0.55, 0.06, 0.07))),
        ('CAR_HATCH5_GREEN', Car3('hatch5', (0.14, 0.26, 0.20))),
        ('CAR_PICKUP_WHITE', Car3('pickup', (0.80, 0.80, 0.78))),
        ('CAR_PICKUP_BLUE', Car3('pickup', (0.10, 0.18, 0.38))),
        ('CAR_VAN_WHITE', Car3('van', (0.84, 0.84, 0.82))),
        ('CAR_WAGON_BROWN', Car3('wagon', (0.42, 0.30, 0.18))),
        ('CAR_TUNER_GREEN', Car3('tuner', (0.20, 0.65, 0.25))),
        ('CAR_LUXURY_BLACK', Car3('luxury', (0.03, 0.03, 0.035))),
        ('CAR_MINIVAN_SILVER', Car3('minivan', (0.60, 0.61, 0.63))),
        ('CAR_BOXTRUCK_WHITE', Car3('boxtruck', (0.90, 0.90, 0.88))),
        ('CAR_YACHT_MAROON', Car3('yacht', (0.40, 0.05, 0.08))),
        ('CAR_AMBULANCE', Car3('van', (0.93, 0.93, 0.92), ambulance=True))]
# camera elevation levels: the game picks the one matching each car's real view angle
ELEV = [(16, 0.040, 136, 62, 24), (11, 0.050, 112, 46, 20), (7, 0.065, 86, 30, 14), (4, 0.090, 62, 18, 10)]
group_of = []
def group(): group_of.append(len(frames))
# frames per car set: 64 view angles at the two nearest elevations, 32 at the two far ones
CAR_ANGLES = (64, 64, 32, 32)
car_dims = []
for name, car in CARS:
    begin(name)
    tall = max(0, max([z for _, z in car.S['top']] + [car.S.get('box_z', (0, 0))[1]]) - 2.2) if hasattr(car, 'S') else 0
    for (e, s_, w, up, dn), na in zip(ELEV, CAR_ANGLES):
        upx = up + int(np.ceil(tall / s_)) + (6 if tall else 0)
        # only headings 0..180 degrees are stored; the game mirrors them for the other half
        for i in range(na // 2 + 1): add(car, i * 2 * np.pi / na, np.radians(e), s_, w, upx, dn, ss=2, outline=True)
    if hasattr(car, 'S'): car_dims.append((int(round((car.S['L'] / 2 - 0.05) * 16)), int(round((car.S['W'] / 2) * 16))))
    print(name, round(time.time() - t0, 1), flush=True)
group()
# people: 16 view directions, of which 9 (0..180 degrees) are stored and the rest drawn mirrored.
# Per stored direction 26 frames = idle, walk 0-11, run 0-11, aim; then 8 knocked-down frames
# (8 directions) and 24 ragdoll tumble frames (3 views: from behind, side, front x 8 pitch angles).
PED_DIRS, PED_DIR_FRAMES, PED_CYCLE = 9, 26, 12
def ped_block(outfit, elev, s_, w, up, dn):
    for d in range(PED_DIRS):
        yaw = d * 2 * np.pi / 16
        add(Human(outfit, 0, mode='idle'), yaw, np.radians(elev), s_, w, up, dn)
        for f in range(PED_CYCLE): add(Human(outfit, f * 2 * np.pi / PED_CYCLE, mode='walk'), yaw, np.radians(elev), s_, w, up, dn)
        for f in range(PED_CYCLE): add(Human(outfit, f * 2 * np.pi / PED_CYCLE, mode='run'), yaw, np.radians(elev), s_, w, up, dn)
        add(Human(outfit, 0, mode='aim'), yaw, np.radians(elev), s_, w, up, dn)
    for d in range(8):
        add(Human(outfit, 0, mode='down', down_seed=d % 3), d * 2 * np.pi / 8, np.radians(elev), s_, int(w * 2), up // 3, dn * 2)
    for view in (0.0, np.pi / 2, np.pi):
        for k in range(8):
            add(Human(outfit, mode='tumble', tumble=k * 2 * np.pi / 8), view, np.radians(elev), s_, int(w * 2), int(up * 1.15), dn, shadow=False)
PED_DOWN = PED_DIRS * PED_DIR_FRAMES
PED_TUMBLE = PED_DOWN + 8
PED_BLOCK = PED_TUMBLE + 24
begin('PED_PLAYER'); ped_block('player', 17, 0.022, 56, 92, 8)
# the player's two-handed weapon stances, stored after the player's (only) block:
# PED_PLAYER + PED_STANCE + direction * PED_NSTANCE + (SMG, shotgun, rifle, rocket launcher)
STANCES = ('smg', 'shotgun', 'rifle', 'rocket')
for d in range(PED_DIRS):
    for wpn in STANCES:
        add(Human('player', 0, mode='aim', weapon=wpn), d * 2 * np.pi / 16, np.radians(17), 0.022, 100, 96, 8)
print('PED_PLAYER', round(time.time() - t0, 1), flush=True)
for name, outfit in (('PED_GREY', 'grey'), ('PED_RED', 'red'), ('PED_NAVY', 'navy'), ('PED_SUIT', 'suit'),
                     ('PED_GREEN', 'green'), ('PED_WHITE', 'white'), ('PED_COP', 'cop'),
                     ('PED_DEE', 'dee'), ('PED_MARTA', 'marta'), ('PED_WOMAN_A', 'woman_a'), ('PED_WOMAN_B', 'woman_b')):
    begin(name)
    ped_block(outfit, 15, 0.032, 40, 64, 6)          # near
    ped_block(outfit, 6, 0.060, 22, 34, 4)           # far (index + PED_BLOCK)
    print(name, round(time.time() - t0, 1), flush=True)
begin('BLOOD_POOL')
for e in (24, 12, 6):
    for k, sz in enumerate((0.30, 0.55, 0.80, 1.05)):
        col, m_, ox, oy = blood_pool(sz, np.radians(e), 0.03, k + e)
        add_image(col, m_, np.zeros_like(m_), ox, oy, int(round(0.03 * 16 * 256)))
begin('BLOOD_DROP')
col = np.zeros((4, 3, 3)); col[:] = (0.45, 0.03, 0.04); m_ = np.array([[0,1,0],[1,1,1],[1,1,1],[0,1,0]], bool)
add_image(col, m_, np.zeros_like(m_), -1, -2, int(round(0.03 * 16 * 256)))
# effects
def blob(w, h, color, core=None):
    j, i = np.mgrid[0:h, 0:w].astype(np.float32)
    r = np.sqrt(((i - w / 2 + 0.5) / (w / 2)) ** 2 + ((j - h / 2 + 0.5) / (h / 2)) ** 2)
    col = np.zeros((h, w, 3), np.float32); col[:] = color
    if core is not None: col[r < 0.45] = core
    return col, r < 1.0
begin('FX_SIREN')      # 0 red, 1 blue
for c in ((1.0, 0.10, 0.08), (0.15, 0.35, 1.0)):
    col, mm = blob(9, 5, c, (1, 1, 1)); add_image(col, mm, np.zeros_like(mm), -4, -2, int(0.03 * 16 * 256))
begin('FX_FLASH')
j, i = np.mgrid[0:9, 0:9]
star = (np.abs(i - 4) + np.abs(j - 4) <= 2) | ((i == 4) | (j == 4)) & (np.abs(i - 4) + np.abs(j - 4) <= 4)
col = np.zeros((9, 9, 3), np.float32); col[:] = (1.0, 0.85, 0.35); col[np.abs(i - 4) + np.abs(j - 4) <= 1] = (1, 1, 0.9)
add_image(col, star, np.zeros_like(star), -4, -4, int(0.025 * 16 * 256))
begin('FX_MARKER')     # 0 yellow (job start), 1 green (destination / checkpoint)
for c in ((1.0, 0.82, 0.10), (0.20, 0.95, 0.35)):
    W_, H_ = 24, 120
    j, i = np.mgrid[0:H_, 0:W_].astype(np.float32)
    xr = np.abs(i - W_ / 2 + 0.5) / (W_ / 2)
    fade = 1 - j / H_
    dens = (1 - xr ** 2) * (0.25 + 0.75 * (1 - fade) ** 0.5)
    thr = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]])[(j.astype(int) & 3), (i.astype(int) & 3)] + 0.5) / 16
    mm = dens * 0.85 > thr
    col = np.zeros((H_, W_, 3), np.float32); col[:] = c
    col[xr < 0.3] = np.minimum(np.array(c) * 1.2 + 0.2, 1)
    add_image(col, mm, np.zeros_like(mm), -W_ // 2, -H_, int(0.035 * 16 * 256))
begin('FX_PICKUP')     # 0 pistol, 1 SMG, 2 health, 3 shotgun, 4 rifle, 5 rocket launcher
for kind in range(6):
    col = np.zeros((14, 18, 3), np.float32); mm = np.zeros((14, 18), bool)
    if kind == 2:
        mm[3:11, 7:11] = True; mm[5:9, 4:14] = True; col[:] = (0.95, 0.15, 0.15)
        ring = np.zeros_like(mm); ring[2:12, 3:15] = True; ring &= ~mm
        col[ring] = (0.95, 0.95, 0.95); mm |= ring
    else:
        if kind == 5:      # rocket launcher: fat tube, flared muzzle, grip and sight
            mm[3:8, 0:18] = True; mm[2:9, 0:2] = True; mm[8:11, 7:9] = True; mm[8:10, 11:13] = True; mm[1:3, 9:12] = True
        elif kind == 3:    # shotgun: long twin barrel, pump, stock
            mm[4:6, 0:18] = True; mm[6:8, 7:12] = True; mm[5:10, 0:4] = True; mm[6:9, 13:15] = True
        elif kind == 4:    # rifle: barrel, curved magazine, grip, stock
            mm[3:6, 0:18] = True; mm[6:9, 9:11] = True; mm[9:11, 10:12] = True; mm[6:10, 5:7] = True; mm[4:9, 0:3] = True
        else:
            mm[3:7, 2:16 if kind else 13] = True; mm[6:12, 4:7] = True
            if kind: mm[6:11, 10:12] = True; mm[4:6, 16:18] = True
        col[:] = (0.20, 0.21, 0.23) if kind != 5 else (0.24, 0.30, 0.16); col[3:4] = (0.55, 0.56, 0.60)
        glow = np.zeros_like(mm); glow[1:13, 0:18] = True; glow &= ~mm
        gj, gi = np.nonzero(glow); keep = (gi + gj) % 2 == 0
        g2 = np.zeros_like(mm); g2[gj[keep], gi[keep]] = True
        col[g2] = (0.35, 1.0, 0.45); mm |= g2
    add_image(col, mm, np.zeros_like(mm), -9, -18, int(0.03 * 16 * 256))
begin('FX_ROCKET')     # rocket in flight: hot exhaust core in a flame halo
col, mm = blob(9, 7, (1.0, 0.55, 0.10), (1.0, 0.95, 0.70)); add_image(col, mm, np.zeros_like(mm), -4, -3, int(0.03 * 16 * 256))
group()
begin('TREE'); add(Tree(), 0.0, np.radians(9), 0.05, 136, 142, 16)
begin('LAMP')
for i in range(8): add(Lamp(), i * 2 * np.pi / 8, np.radians(9), 0.05, 96, 145, 8)
group()
print('rendering', len(jobs), 'frames', flush=True)
done = run_all(jobs, os.path.join(os.path.dirname(OUT_BIN), 'cache'))
frames = [done[f] if isinstance(f, int) else f for f in frames]
print('rendered', round(time.time() - t0, 1), flush=True)

# ----------------------------------------------------------- palette (k-means)
rng = np.random.default_rng(1)
bounds = [0] + group_of
quota = {0: 110000, 1: 90000, 2: 25000}
samp = []
for gi in range(len(group_of)):
    px = np.concatenate([f[0][f[1]] for f in frames[bounds[gi]:bounds[gi + 1]]]).astype(np.float32)
    samp.append(px[rng.choice(len(px), min(quota[gi], len(px)), replace=False)])
samp = np.concatenate(samp)
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)  # rough perceptual
S = tolab(samp)
cent = S[rng.choice(len(S), NCOL - 1, replace=False)]
for it in range(18):
    d = ((S[:, None, :] - cent[None]) ** 2).sum(-1) if len(S) < 1 else None
    lab = np.empty(len(S), np.int32)
    for k in range(0, len(S), 20000):
        lab[k:k + 20000] = ((S[k:k + 20000, None, :] - cent[None]) ** 2).sum(-1).argmin(1)
    for c in range(len(cent)):
        m = lab == c
        cent[c] = S[m].mean(0) if m.any() else S[rng.integers(len(S))]
pal_lab = np.concatenate([cent, tolab(SHADOW[None])])        # last = shadow colour
pal_rgb = (pal_lab / np.array((1.0, 1.25, 0.8), np.float32)) ** 2
SH_IDX = NCOL                                                # index of shadow colour (1-based)

# ------------------------------------------------------------------- write
blob = bytearray(); table = []
for col, mask, smask, ox, oy, upx in frames:
    h, w = mask.shape
    lab = np.zeros((h, w), np.uint8)
    if mask.any():
        L_ = tolab(col[mask])
        idx = np.empty(len(L_), np.int32)
        for k in range(0, len(L_), 20000):
            idx[k:k + 20000] = ((L_[k:k + 20000, None, :] - pal_lab[None, :NCOL - 1]) ** 2).sum(-1).argmin(1)
        lab[mask] = idx + 1
    lab[smask] = SH_IDX
    off = len(blob)
    blob += lab.T.tobytes()                                  # column-major
    while len(blob) % 4: blob.append(0)
    table.append((w, h, ox, oy, upx, off))
open(OUT_BIN, 'wb').write(blob)

L = ['#include "assets.h"', '', 'const SprFrame spr_frames[%d] = {' % len(table)]
for w, h, ox, oy, upx, off in table:
    L.append('  {%d,%d,%d,%d,%d,%d},' % (w, h, ox, oy, upx, off))
L.append('};')
L.append('const unsigned short spr_palette[%d] = {' % NCOL)
def rgb15(c):
    r, g, b = [min(31, int(round(v * 31))) for v in np.clip(c, 0, 1)]
    return r | (g << 5) | (b << 10)
L.append('  ' + ','.join(str(rgb15(c)) for c in pal_rgb) + ',')
L.append('};')
L.append('const unsigned char car_dims[%d][2] = {' % len(car_dims))
L.append('  ' + ' '.join('{%d,%d},' % d for d in car_dims))
L.append('};')
open(OUT_C, 'w').write('\n'.join(L) + '\n')
H = ['#ifndef ASSETS_H', '#define ASSETS_H',
     'typedef struct { unsigned char w, h; signed char ox; signed short oy; unsigned short upx; unsigned int off; } SprFrame;',
     'extern const SprFrame spr_frames[];', 'extern const unsigned short spr_palette[];',
     'extern const unsigned char spr_pixels[];', '#define SPR_NCOL %d' % NCOL, '#define SPR_PAL_BASE 103',
     'extern const unsigned char car_dims[][2];   /* half length, half width per car set (1/16 m) */',
     '#define CAR_SET_FRAMES %d' % sum(n // 2 + 1 for n in CAR_ANGLES), '#define CAR_NSETS %d' % len(car_dims),
     '#define PED_DIR_FRAMES %d' % PED_DIR_FRAMES, '#define PED_CYCLE %d' % PED_CYCLE, '#define PED_DOWN %d' % PED_DOWN,
     '#define PED_TUMBLE %d' % PED_TUMBLE, '#define PED_BLOCK %d' % PED_BLOCK,
     '#define PED_STANCE %d' % PED_BLOCK, '#define PED_NSTANCE %d' % len(STANCES)]
for name, base in sets: H.append('#define %s %d' % (name, base))
H.append('#define SPR_NFRAMES %d' % len(table))
H.append('#endif')
open(os.path.join(os.path.dirname(OUT_C), 'assets.h'), 'w').write('\n'.join(H) + '\n')
print('frames', len(table), 'bytes', len(blob), 'time', round(time.time() - t0, 1))

# contact sheet for review
if len(sys.argv) > 3:
    from PIL import Image
    pal8 = np.zeros((256, 3)); pal8[1:NCOL + 1] = pal_rgb; pal8[0] = (0.42, 0.42, 0.44)
    names = dict(sets)
    pick = [names[n] + k for n in ('CAR_HATCH5_RED', 'CAR_PICKUP_WHITE', 'CAR_VAN_WHITE') for k in (4, 12, 20)] + [names['CAR_POLICE'] + 4] + \
           [names['PED_PLAYER'] + k for k in (0, 3, 13, 17, 36, 39)] + [names['PED_COP'] + k for k in (13, 35, PED_BLOCK + 13)] + \
           [names['PED_WHITE'] + 4, names['PED_GREEN'] + 22, names['PED_GREY'] + 144, names['FX_MARKER'], names['FX_PICKUP'] + 1, names['FX_SIREN']]
    tiles = []
    for fi in pick:
        w, h, ox, oy, upx, off = table[fi]
        a = np.frombuffer(blob[off:off + w * h], np.uint8).reshape(w, h).T
        img = pal8[a]
        canvas = np.full((160, 140, 3), 0.42); canvas[:min(160, h), :min(140, w)] = img[:160, :140]
        tiles.append(canvas)
    rows = [np.concatenate(tiles[i:i + 8], 1) for i in range(0, len(tiles) - len(tiles) % 8, 8)]
    Image.fromarray((np.concatenate(rows, 0) * 255).astype(np.uint8)).save(sys.argv[3])
