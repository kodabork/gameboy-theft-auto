# v0.3 asset build: software sprites (cars, knocked-down people, blood, scenery) with the
# BG palette, plus full-resolution hardware-sprite people (OBJ tiles, own palette).
import sys, os, time, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render, Car, Tree, Lamp, SHADOW
from models2 import Human, Blood, Tuner, OUT

SRC, BUILD = sys.argv[1], sys.argv[2]
NCOL = 151
t0 = time.time()
def log(*a): print(*a, round(time.time() - t0, 1), flush=True)

# ---------------------------------------------------------------- BG sprites
frames = []
def add(model, yaw, elev, s, w_sq, up, down, glow=None):
    rgb, a, sh = render(model, yaw, elev, s / 2, w_sq * 2, up * 2, down * 2)   # 2x2 supersample
    H = up + down
    rgb = rgb.reshape(H, 2, w_sq, 2, 3).mean(1); a = a.reshape(H, 2, w_sq, 2).sum(1); sh = sh.reshape(H, 2, w_sq, 2).sum(1)
    rgb = (rgb * (a[..., None] > 0)).sum(2) / np.maximum((a > 0).sum(2), 1)[..., None] if False else rgb.mean(2)
    # halve width into logical pixels: combine pairs
    cnt = a.sum(-1); mask = cnt >= 4
    smask = (~mask) & (sh.sum(-1) >= 4)
    yy, xx = np.mgrid[0:H, 0:mask.shape[1]]
    dith = ((xx + yy) & 1) == 0
    gmask = np.zeros_like(mask)
    if glow is not None:
        gmask = smask & ~dith
    smask &= dith
    if mask.any() or smask.any():
        anyp = mask | smask | gmask
        ys, xs = np.nonzero(anyp)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    else:
        y0 = x0 = 0; y1 = x1 = 1
    col = rgb.copy()
    if glow is not None: col[gmask] = glow
    frames.append((col[y0:y1, x0:x1], (mask | gmask)[y0:y1, x0:x1], smask[y0:y1, x0:x1],
                   int(x0 - w_sq // 4), int(y0 - up), int(round(s * 16 * 256))))
sets = []
def begin(n): sets.append((n, len(frames)))

def add_obj_halfwidth_fix():
    pass

CARS = [('CAR_PLAYER', Tuner('coupe', (0.05, 0.22, 0.65), stripe=(0.90, 0.90, 0.92)), (0.25, 0.85, 1.0)),
        ('CAR_MUSCLE', Tuner('muscle', (0.04, 0.04, 0.045), stripe=(0.65, 0.05, 0.05)), None),
        ('CAR_EXOTIC', Tuner('exotic', (0.85, 0.35, 0.03)), None),
        ('CAR_COUPE_GREEN', Tuner('coupe', (0.20, 0.55, 0.12), stripe=(0.05, 0.05, 0.05), rim=(0.15, 0.15, 0.16)), (0.5, 1.0, 0.3)),
        ('CAR_SEDAN_SILVER', Car('sedan', (0.60, 0.61, 0.63)), None),
        ('CAR_SEDAN_BLUE', Car('sedan', (0.07, 0.11, 0.26)), None),
        ('CAR_TAXI', Car('sedan', (0.85, 0.62, 0.06), taxi=True), None),
        ('CAR_SUV_BLACK', Car('suv', (0.04, 0.04, 0.045)), None),
        ('CAR_SUV_WHITE', Car('suv', (0.82, 0.82, 0.80)), None)]
for name, car, glow in CARS:
    begin(name)
    for i in range(32): add(car, i * 2 * np.pi / 32, np.radians(16), 0.04, 136, 62, 24, glow)
    for i in range(32): add(car, i * 2 * np.pi / 32, np.radians(6), 0.08, 68, 26, 8, glow)
    log(name)
OUTFITS = ['player', 'man_coat', 'woman_red', 'man_hoodie', 'woman_white', 'man_suit', 'man_brown']
for o in OUTFITS:
    begin('DOWN_' + o.upper())
    for d in range(8): add(Human(o, moving=False, lying=True), d * 2 * np.pi / 8, np.radians(16), 0.03, 80, 24, 24)
log('lying')
begin('BLOOD')
for k, (sz, st) in enumerate(((1.0, 0.0), (1.25, 0.2), (0.8, 0.9))):
    for d in range(4): add(Blood(k * 7 + 1, sz, st), d * np.pi / 2, np.radians(16), 0.03, 120, 30, 30)
begin('TREE'); add(Tree(), 0.0, np.radians(9), 0.05, 136, 142, 16)
begin('LAMP')
for i in range(8): add(Lamp(), i * 2 * np.pi / 8, np.radians(9), 0.05, 96, 145, 8)
log('scenery')

# ---------------------------------------------------------------- OBJ people
OW, OH, OUP = 32, 64, 60
objf = []          # (rgb[64,32], mask, shadow)
osets = []
def obj_frame(model, yaw, s=0.028):
    rgb, a, sh = render(model, yaw, np.radians(17), s / 2, OW * 2, OUP * 2, (OH - OUP) * 2)
    rgb = rgb.reshape(OH, 2, OW, 2, 3); a = a.reshape(OH, 2, OW, 2); sh = sh.reshape(OH, 2, OW, 2)
    cnt = a.sum((1, 3))
    col = (rgb * a[..., None]).sum((1, 3)) / np.maximum(cnt, 1)[..., None]
    m = cnt >= 2
    yy, xx = np.mgrid[0:OH, 0:OW]
    shm = (~m) & (sh.sum((1, 3)) >= 2) & (((xx + yy) & 1) == 0)
    objf.append((col, m, shm))
for o in OUTFITS:
    osets.append(('PEOPLE_' + o.upper(), len(objf)))
    for d in range(8):
        yaw = d * 2 * np.pi / 8
        obj_frame(Human(o, moving=False), yaw)
        for f in range(12): obj_frame(Human(o, f * 2 * np.pi / 12), yaw)
    if o == 'player':
        osets.append(('PEOPLE_PLAYER_RUN', len(objf)))
        for d in range(8):
            for f in range(8): obj_frame(Human(o, f * 2 * np.pi / 8, run=True), d * 2 * np.pi / 8)
    log('people', o)

# ---------------------------------------------------------------- palettes
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)
def kmeans(px, k, seed=1, iters=18):
    rng = np.random.default_rng(seed)
    S = tolab(px[rng.choice(len(px), min(220000, len(px)), replace=False)])
    cent = S[rng.choice(len(S), k, replace=False)]
    for _ in range(iters):
        lab = np.empty(len(S), np.int32)
        for i in range(0, len(S), 20000):
            lab[i:i + 20000] = ((S[i:i + 20000, None] - cent[None]) ** 2).sum(-1).argmin(1)
        for c in range(k):
            m = lab == c
            cent[c] = S[m].mean(0) if m.any() else S[rng.integers(len(S))]
    return cent
def assign(cols, cent):
    L = tolab(cols); idx = np.empty(len(L), np.int32)
    for i in range(0, len(L), 20000):
        idx[i:i + 20000] = ((L[i:i + 20000, None] - cent[None]) ** 2).sum(-1).argmin(1)
    return idx
def unlab(c): return (c / np.array((1.0, 1.25, 0.8), np.float32)) ** 2

bgpx = np.concatenate([f[0][f[1]] for f in frames]).astype(np.float32)
bgc = kmeans(bgpx, NCOL - 1)
bg_pal = np.concatenate([unlab(bgc), SHADOW[None]])
log('bg palette')
oppx = np.concatenate([f[0][f[1]] for f in objf]).astype(np.float32)
NOBJ = 239
objc = kmeans(oppx, NOBJ, seed=2)
obj_pal = np.concatenate([unlab(objc), SHADOW[None]])          # index 1..239 colours, 240 = shadow
log('obj palette')

def rgb15(c):
    r, g, b = [min(31, int(round(v * 31))) for v in np.clip(c, 0, 1)]
    return r | (g << 5) | (b << 10)

# ---------------------------------------------------------------- write BG
blob = bytearray(); table = []
for col, mask, smask, ox, oy, upx in frames:
    h, w = mask.shape
    lab = np.zeros((h, w), np.uint8)
    if mask.any(): lab[mask] = assign(col[mask], bgc) + 1
    lab[smask] = NCOL
    off = len(blob); blob += lab.T.tobytes()
    while len(blob) % 4: blob.append(0)
    table.append((w, h, ox, oy, upx, off))
open(os.path.join(BUILD, 'assets.bin'), 'wb').write(blob)

# ---------------------------------------------------------------- write OBJ
def tiles(img):   # img [H, W] -> 8bpp 1D tile order
    H, W = img.shape
    out = bytearray()
    for ty in range(H // 8):
        for tx in range(W // 8):
            out += img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8].tobytes()
    return out
oblob_full = bytearray(); oblob_half = bytearray()
for col, m, shm in objf:
    lab = np.zeros((OH, OW), np.uint8)
    if m.any(): lab[m] = assign(col[m], objc) + 1
    lab[shm] = NOBJ + 1
    oblob_full += tiles(lab)
    # half resolution: average 2x2 colours where covered
    c2 = col.reshape(OH // 2, 2, OW // 2, 2, 3); m2 = m.reshape(OH // 2, 2, OW // 2, 2)
    cnt = m2.sum((1, 3)); cc = (c2 * m2[..., None]).sum((1, 3)) / np.maximum(cnt, 1)[..., None]
    hm = cnt >= 2
    s2 = shm.reshape(OH // 2, 2, OW // 2, 2).sum((1, 3)) >= 1
    lab2 = np.zeros((OH // 2, OW // 2), np.uint8)
    if hm.any(): lab2[hm] = assign(cc[hm], objc) + 1
    yy, xx = np.mgrid[0:OH // 2, 0:OW // 2]
    lab2[(~hm) & s2 & (((xx + yy) & 1) == 0)] = NOBJ + 1
    oblob_half += tiles(lab2)
open(os.path.join(BUILD, 'people.bin'), 'wb').write(oblob_full + oblob_half)

L = ['#include "assets.h"', '', 'const SprFrame spr_frames[%d] = {' % len(table)]
for t in table: L.append('  {%d,%d,%d,%d,%d,%d},' % t)
L.append('};')
L.append('const unsigned short spr_palette[%d] = {' % NCOL)
L.append('  ' + ','.join(str(rgb15(c)) for c in bg_pal) + ',')
L.append('};')
L.append('const unsigned short obj_palette[%d] = {' % (NOBJ + 1))
L.append('  ' + ','.join(str(rgb15(c)) for c in obj_pal) + ',')
L.append('};')
open(os.path.join(SRC, 'assets.c'), 'w').write('\n'.join(L) + '\n')
H = ['#ifndef ASSETS_H', '#define ASSETS_H',
     'typedef struct { unsigned char w, h; signed char ox; signed short oy; unsigned short upx; unsigned int off; } SprFrame;',
     'extern const SprFrame spr_frames[];', 'extern const unsigned short spr_palette[];',
     'extern const unsigned short obj_palette[];',
     'extern const unsigned char spr_pixels[];', 'extern const unsigned char people_pixels[];',
     '#define SPR_NCOL %d' % NCOL, '#define SPR_PAL_BASE 103', '#define OBJ_NCOL %d' % (NOBJ + 1),
     '#define PEOPLE_NFRAMES %d' % len(objf),
     '#define PEOPLE_FULL_BYTES 2048', '#define PEOPLE_HALF_BYTES 512',
     '#define PEOPLE_HALF_OFS %d' % len(oblob_full),
     '#define PEOPLE_FRAMES_PER_DIR 13']
for n, b in sets: H.append('#define %s %d' % (n, b))
for n, b in osets: H.append('#define %s %d' % (n, b))
H.append('#define SPR_NFRAMES %d' % len(table))
H.append('#endif')
open(os.path.join(SRC, 'assets.h'), 'w').write('\n'.join(H) + '\n')
log('done: bg frames', len(table), 'bytes', len(blob), 'people frames', len(objf), 'bytes', len(oblob_full) + len(oblob_half))

# review sheet
if len(sys.argv) > 3:
    from PIL import Image
    pp = np.zeros((256, 3)); pp[1:NOBJ + 2] = obj_pal; pp[0] = (0.42, 0.42, 0.44)
    full = np.frombuffer(bytes(oblob_full), np.uint8)
    def untile(b):
        img = np.zeros((OH, OW), np.uint8)
        k = 0
        for ty in range(8):
            for tx in range(4):
                img[ty * 8:ty * 8 + 8, tx * 8:tx * 8 + 8] = np.frombuffer(b[k:k + 64], np.uint8).reshape(8, 8); k += 64
        return img
    sel = [osets[i][1] + d * 13 + f for i in (0, 2, 3, 4, 5, 6) for d, f in ((0, 0), (4, 3), (2, 2), (2, 8))]
    tl = [pp[untile(full[i * 2048:(i + 1) * 2048].tobytes())] for i in sel]
    rows = [np.concatenate(tl[i:i + 12], 1) for i in range(0, 24, 12)]
    Image.fromarray((np.concatenate(rows, 0) * 255).astype(np.uint8)).resize((12 * 32 * 3, 2 * 64 * 3), Image.NEAREST).save(sys.argv[3])
