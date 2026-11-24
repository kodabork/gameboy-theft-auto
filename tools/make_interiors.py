# Shop interiors: one pre-rendered room per shop type (gun shop, 24/7 store, pawn shop, car showroom),
# a counter layer drawn in front of the clerk, and the clerk in two poses (serving, hands up).
# Rooms are ray-cast boxes with simple lighting, then mapped onto the game's palette like the logo.
#   python3 tools/make_interiors.py src/assets.c src/interiors.c [preview.png]
import sys, os, re, math, random, numpy as np
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image

f32 = np.float32
csrc = open(sys.argv[1]).read()
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
cols = [(i + 104, c) for i, c in enumerate(pal15) if i + 104 < 252]
cols += [(252, 0x10DF), (253, 0x03FF), (254, 0x7FFF), (255, 0x0000)]
P = np.array([((c & 31) / 31, ((c >> 5) & 31) / 31, ((c >> 10) & 31) / 31) for _, c in cols], f32)
IDX = np.array([i for i, _ in cols], np.uint8)
def lab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.3, 0.8), f32)
PL = lab(P)
BAY = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) + 0.5) / 16 - 0.5

def to_idx(rgb, mask):
    h, w, _ = rgb.shape
    out = np.zeros((h, w), np.uint8)
    yy, xx = np.mgrid[0:h, 0:w]
    rgbd = np.clip(rgb + BAY[yy & 3, xx & 3][..., None] * 0.07, 0, 1)
    L = lab(rgbd[mask])
    best = np.empty(len(L), np.int64)
    for k in range(0, len(L), 20000):
        best[k:k + 20000] = ((L[k:k + 20000, None, :] - PL[None]) ** 2).sum(-1).argmin(1)
    out[mask] = IDX[best]
    return out

# ------------------------------------------------------------------ ray-cast room
W, H, SS = 240, 160, 2
CAM = np.array((0.0, -0.9, 1.66), f32)
TILT = math.radians(4)
FOCAL = 0.78 * W
HZ = 0.36                            # horizon row as a fraction of the height                     # pixels (logical 240-wide screen)
CLERK_Y = 4.75                       # depth of the clerk's feet
CLERK_X = {'gun': 0.0, 'store': 0.3, 'pawn': -0.3, 'cars': 2.0, 'bowl': 0.0, 'pool': 1.6}

def rays():
    j, i = np.mgrid[0:H * SS, 0:W * SS].astype(f32)
    u = (i + 0.5) / SS - W / 2; v = H * HZ - (j + 0.5) / SS
    d = np.stack([u, np.full_like(u, FOCAL), v], -1)
    ct, st = math.cos(TILT), math.sin(TILT)
    y = d[..., 1] * ct - d[..., 2] * st; z = d[..., 1] * st + d[..., 2] * ct
    d = np.stack([d[..., 0], y, z], -1)
    return d / np.linalg.norm(d, axis=-1, keepdims=True)

def project(p):
    """world point -> screen (x, y) on the 240x160 image"""
    q = np.array(p, f32) - CAM
    ct, st = math.cos(TILT), math.sin(TILT)
    y = q[1] * ct + q[2] * st; z = -q[1] * st + q[2] * ct
    return W / 2 + q[0] * FOCAL / y, H * HZ - z * FOCAL / y, FOCAL / y

class Room:
    def __init__(self, seed):
        self.boxes = []; self.rng = random.Random(seed)
    def box(self, lo, hi, col, tex='plain', fg=False):
        self.boxes.append((np.array(lo, f32), np.array(hi, f32), np.array(col, f32), tex, fg))

def shade_tex(tex, col, p, n):
    x, y, z = p[:, 0], p[:, 1], p[:, 2]
    c = np.tile(col, (len(p), 1))
    if tex == 'checker':
        k = ((np.floor(x / 0.5) + np.floor(y / 0.5)) % 2)[:, None]
        c = c * (0.82 + 0.25 * k)
    elif tex == 'tile':
        g = (np.abs((x / 0.6) % 1 - 0.5) > 0.47) | (np.abs((y / 0.6) % 1 - 0.5) > 0.47)
        c = c * np.where(g, 0.75, 1.0)[:, None]
    elif tex == 'brick':
        row = np.floor(z / 0.09); off = (row % 2) * 0.12
        a = np.where(np.abs(n[:, 1]) > 0.5, x, y)
        mort = ((z / 0.09) % 1 < 0.14) | (((a + off) / 0.24) % 1 < 0.07)
        var = 0.9 + 0.2 * ((np.sin(row * 12.9 + np.floor((a + off) / 0.24) * 7.1) * 43758.5) % 1)[:, None]
        c = np.where(mort[:, None], np.array((0.62, 0.60, 0.56), f32), c * var)
    elif tex == 'peg':
        a = np.where(np.abs(n[:, 1]) > 0.5, x, y)
        hole = (((a / 0.05) % 1 - 0.5) ** 2 + ((z / 0.05) % 1 - 0.5) ** 2) < 0.04
        c = c * np.where(hole, 0.45, 1.0)[:, None]
    elif tex == 'wood':
        a = np.where(np.abs(n[:, 2]) > 0.5, x, np.where(np.abs(n[:, 1]) > 0.5, x, y))
        c = c * (0.88 + 0.12 * np.sin(a * 37 + np.sin(z * 9 + y * 5) * 2))[:, None]
    elif tex == 'boards':                                    # bowling lane: 39 narrow maple boards, glossy
        b = np.floor((x + 5) / 0.0273)
        c = c * (0.90 + 0.10 * ((np.sin(b * 12.9898) * 43758.5) % 1))[:, None]
        c = c * np.where(((x + 5) / 0.0273) % 1 < 0.06, 0.85, 1.0)[:, None]
    elif tex == 'window':
        t = np.clip((z - 0.3) / 2.4, 0, 1)[:, None]
        c = np.array((0.80, 0.88, 0.96), f32) * (1 - t) + np.array((0.48, 0.66, 0.90), f32) * t
        frame = ((y / 1.6) % 1 < 0.03) | (z < 0.32) | (z > 2.68)
        c = np.where(frame[:, None], np.array((0.20, 0.21, 0.23), f32), c)
    return c

def render_room(room, extra=None):
    d = rays().reshape(-1, 3)
    n_r = len(d)
    tbest = np.full(n_r, 1e9, f32); ib = np.full(n_r, -1, np.int32); nb = np.zeros((n_r, 3), f32)
    inv = 1.0 / np.where(np.abs(d) < 1e-6, 1e-6, d)
    for k, (lo, hi, col, tex, fg) in enumerate(room.boxes):
        if tex == 'glass': continue
        t0 = (lo - CAM) * inv; t1 = (hi - CAM) * inv
        tmin = np.minimum(t0, t1); tmax = np.maximum(t0, t1)
        tn = tmin.max(1); tf = tmax.min(1)
        ok = (tf >= np.maximum(tn, 1e-3)) & (tn > 1e-3) & (tn < tbest)
        if not ok.any(): continue
        tbest[ok] = tn[ok]; ib[ok] = k
        ax = tmin[ok].argmax(1); m = ok.sum()
        nn = np.zeros((m, 3), f32)
        nn[np.arange(m), ax] = -np.sign(d[ok][np.arange(m), ax])
        nb[ok] = nn
    p = CAM + d * tbest[:, None]
    rgb = np.zeros((n_r, 3), f32); fgm = np.zeros(n_r, bool)
    L1 = np.array((0.25, -0.35, 0.90), f32); L1 /= np.linalg.norm(L1)
    for k, (lo, hi, col, tex, fg) in enumerate(room.boxes):
        sel = ib == k
        if not sel.any(): continue
        c = shade_tex(tex, col, p[sel], nb[sel])
        if tex in ('emit', 'window', 'screen'):
            rgb[sel] = c; fgm[sel] = fg; continue
        nn = nb[sel]
        lam = 0.50 + 0.42 * np.clip(nn @ L1, 0, 1) + 0.10 * (nn[:, 1] < -0.5)
        # soft darkening toward the floor and corners, and with distance
        q = p[sel]
        ao = 0.80 + 0.20 * np.clip(q[:, 2] / 1.2, 0, 1)
        corner = np.minimum(np.minimum(q[:, 0] + 3.2, 3.2 - q[:, 0]), 7.2 - q[:, 1])
        ao *= 0.86 + 0.14 * np.clip(corner / 0.6, 0, 1)
        fog = np.clip((tbest[sel] - 3.0) / 14, 0, 0.25)[:, None]
        if tex == 'gloss':
            refl = np.clip(1 - np.abs(q[:, 0]) / 3.2, 0, 1) * 0.18
            lam = lam + refl
        rgb[sel] = c * (lam * ao)[:, None] * (1 - fog) + fog * np.array((0.55, 0.55, 0.56), f32)
        fgm[sel] = fg
    for k, (lo, hi, col, tex, fg) in enumerate(room.boxes):     # glass: see-through, tinted, a little shine
        if tex != 'glass': continue
        t0 = (lo - CAM) * inv; t1 = (hi - CAM) * inv
        tn = np.minimum(t0, t1).max(1); tf = np.maximum(t0, t1).min(1)
        ok = (tf >= np.maximum(tn, 1e-3)) & (tn < tbest)
        q = CAM + d[ok] * tn[ok, None]
        shine = 0.10 + 0.10 * (((q[:, 0] * 0.8 + q[:, 2] * 1.6) % 1.0) < 0.18)
        rgb[ok] = rgb[ok] * 0.62 + col * 0.26 + shine[:, None]
        fgm[ok] |= fg
    rgb = rgb.reshape(H * SS, W * SS, 3).reshape(H, SS, W, SS, 3).mean((1, 3))
    fgm = fgm.reshape(H * SS, W * SS).reshape(H, SS, W, SS).mean((1, 3)) >= 0.5
    return np.clip(rgb, 0, 1), fgm

# ------------------------------------------------------------------ the rooms
def shell(r, wall, floor, ftex='tile', wtex='plain', ceil=(0.86, 0.86, 0.84)):
    r.box((-3.4, -1, -0.2), (3.4, 7.4, 0.0), floor, ftex)              # floor
    r.box((-3.4, -1, 3.0), (3.4, 7.4, 3.2), ceil)                       # ceiling
    r.box((-3.4, -1, 0), (-3.2, 7.4, 3), wall, wtex)                    # left wall
    r.box((3.2, -1, 0), (3.4, 7.4, 3), wall, wtex)                      # right wall
    r.box((-3.4, 7.2, 0), (3.4, 7.4, 3), wall, wtex)                    # back wall
    for y in (1.5, 4.0, 6.3):                                           # ceiling light panels
        r.box((-0.9, y - 0.25, 2.97), (0.9, y + 0.25, 3.0), (1.0, 1.0, 0.94), 'emit')
    r.box((-3.2, -1, 0), (-3.15, 7.2, 0.12), (0.18, 0.18, 0.19))        # skirting
    r.box((3.15, -1, 0), (3.2, 7.2, 0.12), (0.18, 0.18, 0.19))
    r.box((-3.2, 7.15, 0), (3.2, 7.2, 0.12), (0.18, 0.18, 0.19))

def counter(r, top=(0.30, 0.20, 0.14), front=(0.42, 0.28, 0.18), glass=False, x0=-2.3, x1=2.3, ftex='wood'):
    y0, y1 = 3.75, 4.35
    if glass:
        r.box((x0, y0, 0), (x1, y1, 0.35), front, 'wood', True)
        r.box((x0, y0, 0.35), (x1, y1, 0.98), (0.70, 0.80, 0.84), 'glass', True)
        r.box((x0, y0, 0.35), (x0 + 0.04, y1, 0.98), front, 'plain', True); r.box((x1 - 0.04, y0, 0.35), (x1, y1, 0.98), front, 'plain', True)
        r.box((x0 + 0.1, y0 + 0.1, 0.36), (x1 - 0.1, y1 - 0.1, 0.38), (0.55, 0.12, 0.12), 'plain', True)   # velvet
    else:
        r.box((x0, y0, 0), (x1, y1, 0.98), front, ftex, True)
    r.box((x0 - 0.05, y0 - 0.08, 0.98), (x1 + 0.05, y1, 1.05), top, 'wood', True)
    r.box((1.2, 3.95, 1.05), (1.65, 4.3, 1.28), (0.16, 0.16, 0.17), 'plain', True)   # cash register
    r.box((1.25, 3.9, 1.28), (1.6, 4.0, 1.36), (0.25, 0.55, 0.30), 'emit', True)

def shelf_unit(r, x0, x1, y0, y1, zs, back, depth_axis='y', palette=None, col=(0.72, 0.72, 0.74), tex='plain'):
    """open shelving: a back panel, two end posts, boards and goods. depth_axis 'y': runs along x, back at y1;
    'x': runs along y with its back at x0 (left wall) or x1 (right wall) as given by back ('-' or '+')"""
    top = zs[-1] + 0.45
    if depth_axis == 'y':
        r.box((x0, y1 - 0.04, 0), (x1, y1, top), col, tex)
        r.box((x0, y0, 0), (x0 + 0.04, y1, top), col, tex); r.box((x1 - 0.04, y0, 0), (x1, y1, top), col, tex)
        products(r, x0 + 0.04, x1 - 0.04, y0, y1 - 0.04, zs, 'y', palette)
    else:
        if back == '-': r.box((x0, y0, 0), (x0 + 0.04, y1, top), col, tex); a0, a1 = x0 + 0.04, x1
        else: r.box((x1 - 0.04, y0, 0), (x1, y1, top), col, tex); a0, a1 = x0, x1 - 0.04
        r.box((x0, y0, 0), (x1, y0 + 0.04, top), col, tex); r.box((x0, y1 - 0.04, 0), (x1, y1, top), col, tex)
        products(r, a0, a1, y0 + 0.04, y1 - 0.04, zs, 'x', palette)
    r.box((x0, y0, top - 0.03), (x1, y1, top), col, tex)

def products(r, x0, x1, y0, y1, zs, depth_axis='y', palette=None):
    pal = palette or [(0.80, 0.15, 0.12), (0.95, 0.75, 0.10), (0.15, 0.40, 0.75), (0.20, 0.60, 0.25),
                      (0.90, 0.90, 0.88), (0.85, 0.45, 0.10), (0.55, 0.20, 0.55), (0.10, 0.65, 0.70)]
    for z in zs:
        r.box((x0, y0, z - 0.03), (x1, y1, z), (0.70, 0.70, 0.72))               # shelf board
        a = x0 if depth_axis == 'y' else y0
        end = x1 if depth_axis == 'y' else y1
        while a < end - 0.08:
            w = r.rng.uniform(0.08, 0.22); h = r.rng.uniform(0.12, 0.30)
            c = r.rng.choice(pal); g = r.rng.uniform(0.8, 1.1)
            col = tuple(min(1, v * g) for v in c)
            dd = r.rng.uniform(0.05, 0.12)
            if depth_axis == 'y': r.box((a, y1 - 0.02 - (y1 - y0) * 0.8, z), (min(a + w, end), y1 - 0.02, z + h), col)
            else: r.box((x0 + 0.02, a, z), (x1 - 0.02, min(a + w, end), z + h), col)
            a += w + r.rng.uniform(0.0, 0.03)

def gun_shape(r, x, z, length, col=(0.08, 0.08, 0.09), y=7.03, long=True):
    r.box((x, y, z), (x + length, y + 0.06, z + 0.07), col)                             # barrel + body
    if long: r.box((x + length * 0.62, y, z - 0.10), (x + length, y + 0.06, z + 0.04), (0.40, 0.24, 0.12), 'wood')   # stock
    r.box((x + length * 0.45, y, z - 0.13), (x + length * 0.52, y + 0.06, z), col)      # grip

def gun_room():
    r = Room(1)
    shell(r, (0.52, 0.30, 0.22), (0.40, 0.40, 0.42), 'tile', 'brick')
    r.box((-2.9, 7.1, 0.9), (2.9, 7.15, 2.6), (0.78, 0.66, 0.46), 'peg')              # pegboard
    for k in range(6):
        gun_shape(r, -2.7 + (k % 3) * 1.85, 2.35 - (k // 3) * 0.55, 1.15 if k < 3 else 0.95)
    for k in range(5):
        gun_shape(r, -2.6 + k * 1.1, 1.18, 0.38, long=False)
    counter(r, glass=True)
    for k in range(5):                                                                  # handguns in the case
        r.box((-2.0 + k * 0.85, 3.95, 0.39), (-1.6 + k * 0.85, 4.15, 0.43), (0.10, 0.10, 0.11), 'plain', True)
    shelf_unit(r, -3.2, -2.6, 0.6, 3.2, (0.4, 0.85, 1.3, 1.75), '-', 'x',
               [(0.75, 0.62, 0.20), (0.30, 0.40, 0.25), (0.70, 0.15, 0.10), (0.20, 0.22, 0.25)], (0.30, 0.22, 0.16), 'wood')
    r.box((3.1, 1.2, 1.0), (3.15, 2.4, 2.2), (0.92, 0.90, 0.85))                      # target poster
    for rad, cc in ((0.45, (0.10, 0.10, 0.10)), (0.32, (0.92, 0.90, 0.85)), (0.18, (0.75, 0.10, 0.10))):
        r.box((3.08, 1.8 - rad, 1.6 - rad), (3.1, 1.8 + rad, 1.6 + rad), cc)
    r.box((2.5, 0.6, 0), (3.15, 3.0, 1.0), (0.24, 0.25, 0.22))                        # crates
    r.box((2.6, 1.0, 1.0), (3.1, 2.0, 1.45), (0.36, 0.38, 0.30))
    return r

def store_room():
    r = Room(2)
    shell(r, (0.85, 0.84, 0.78), (0.82, 0.82, 0.80), 'checker')
    r.box((-3.2, 7.1, 2.3), (3.2, 7.15, 2.7), (0.10, 0.45, 0.25))                     # sign band
    shelf_unit(r, -3.0, 3.0, 6.75, 7.15, (0.3, 0.75, 1.2, 1.65), None)                  # back shelves
    shelf_unit(r, -3.2, -2.65, 0.3, 3.3, (0.3, 0.75, 1.2), '-', 'x')                 # wall shelves, left
    r.box((3.15, 0.3, 0), (3.2, 3.3, 2.2), (0.85, 0.92, 0.98), 'emit')                # fridges, right
    shelf_unit(r, 2.7, 3.15, 0.3, 3.3, (0.3, 0.72, 1.14, 1.56), '+', 'x',
               [(0.85, 0.10, 0.10), (0.10, 0.30, 0.80), (0.95, 0.85, 0.20), (0.20, 0.70, 0.30)], (0.6, 0.6, 0.62))
    r.box((2.68, 0.3, 0), (2.72, 3.3, 2.2), (0.80, 0.90, 0.96), 'glass')
    counter(r, top=(0.80, 0.80, 0.80), front=(0.22, 0.45, 0.30), ftex='plain')
    r.box((-2.2, 3.85, 1.05), (-1.2, 4.25, 1.5), (0.82, 0.80, 0.70))                  # lottery / snack rack
    products(r, -2.15, -1.25, 3.9, 4.2, (1.1, 1.3), palette=[(0.9, 0.2, 0.1), (0.9, 0.8, 0.1), (0.2, 0.3, 0.8)])
    return r

def pawn_room():
    r = Room(3)
    shell(r, (0.46, 0.40, 0.30), (0.35, 0.24, 0.16), 'wood', 'plain')
    for k in range(5):                                                                # guitars on the back wall
        x = -2.6 + k * 1.15; c = [(0.70, 0.30, 0.10), (0.15, 0.15, 0.16), (0.80, 0.10, 0.12), (0.85, 0.65, 0.30), (0.20, 0.30, 0.65)][k]
        r.box((x, 7.05, 1.3), (x + 0.40, 7.15, 1.75), c)
        r.box((x + 0.06, 7.05, 1.75), (x + 0.34, 7.15, 2.0), c)
        r.box((x + 0.17, 7.07, 2.0), (x + 0.23, 7.15, 2.65), (0.30, 0.20, 0.12))
    r.box((-3.2, 0.6, 0), (-3.15, 3.6, 2.4), (0.30, 0.22, 0.16), 'wood')              # TV shelves
    for z in (0.1, 1.15):
        r.box((-3.15, 0.6, z - 0.05), (-2.4, 3.6, z), (0.30, 0.22, 0.16), 'wood')
    for k in range(6):
        y = 0.8 + (k % 3) * 0.95; z = 0.1 + (k // 3) * 1.05
        r.box((-2.95, y, z), (-2.45, y + 0.8, z + 0.65), (0.12, 0.12, 0.13))
        r.box((-2.45, y + 0.08, z + 0.08), (-2.43, y + 0.72, z + 0.57), (0.30, 0.42, 0.55), 'screen')
    counter(r, glass=True, top=(0.25, 0.18, 0.12), front=(0.35, 0.24, 0.16))
    for k in range(7):                                                                # jewellery
        r.box((-2.0 + k * 0.6, 3.95, 0.39), (-1.85 + k * 0.6, 4.1, 0.44), (0.95, 0.80, 0.25), 'plain', True)
    r.box((2.4, 0.5, 0), (3.15, 1.6, 0.9), (0.55, 0.15, 0.12))                        # armchair
    r.box((2.4, 1.4, 0), (3.15, 1.6, 1.4), (0.50, 0.13, 0.10))
    r.box((2.6, 2.2, 0), (3.1, 3.2, 1.6), (0.25, 0.25, 0.27))                         # safe
    r.box((2.58, 2.6, 0.7), (2.6, 2.8, 0.9), (0.75, 0.70, 0.40))
    r.box((-1.0, 7.1, 2.75), (1.0, 7.15, 2.95), (0.85, 0.65, 0.15), 'emit')           # neon sign
    return r

def car_room():
    r = Room(4)
    shell(r, (0.88, 0.88, 0.90), (0.70, 0.71, 0.74), 'tile', 'plain', ceil=(0.92, 0.92, 0.93))
    r.boxes[0] = (r.boxes[0][0], r.boxes[0][1], np.array((0.62, 0.63, 0.66), f32), 'gloss', False)
    r.box((-3.21, 0.0, 0.0), (-3.19, 7.2, 3.0), (0.8, 0.9, 1.0), 'window')            # glass wall
    r.box((-3.2, 7.1, 2.2), (3.2, 7.15, 2.7), (0.70, 0.08, 0.08))                     # brand band
    r.box((1.0, 3.75, 0), (3.0, 4.35, 0.98), (0.20, 0.20, 0.22), 'plain', True)       # sales desk (right)
    r.box((0.95, 3.67, 0.98), (3.05, 4.35, 1.05), (0.85, 0.85, 0.86), 'plain', True)
    r.box((2.0, 3.9, 1.05), (2.5, 4.2, 1.35), (0.15, 0.15, 0.16), 'plain', True)      # monitor
    r.box((2.05, 3.89, 1.09), (2.45, 3.9, 1.31), (0.40, 0.60, 0.85), 'screen', True)
    for k in range(3):                                                                # posters
        r.box((3.15, 0.6 + k * 1.6, 1.1), (3.2, 1.8 + k * 1.6, 2.1), [(0.2, 0.3, 0.7), (0.8, 0.6, 0.1), (0.1, 0.5, 0.3)][k])
    r.box((-2.9, 3.0, 0), (-0.2, 6.4, 0.02), (0.55, 0.12, 0.10))                      # display rug
    return r

def bowl_room():
    """the bowling alley front desk: shoe cubbies behind, lanes glimpsed to the sides"""
    r = Room(5)
    shell(r, (0.22, 0.16, 0.30), (0.20, 0.14, 0.26), 'checker', 'plain', ceil=(0.12, 0.10, 0.16))
    r.boxes = [b for b in r.boxes if b[3] != 'emit']
    for x in (-2.4, -0.8, 0.8, 2.4):                                                 # neon strips
        r.box((x - 0.06, 1.0, 2.96), (x + 0.06, 6.5, 3.0), (0.95, 0.35, 0.80) if x < 0 else (0.30, 0.85, 0.95), 'emit')
    r.box((-2.9, 7.0, 0.3), (2.9, 7.15, 2.3), (0.35, 0.25, 0.18), 'wood')            # shoe cubbies
    for k in range(10):
        for z in (0.5, 0.9, 1.3, 1.7):
            c = [(0.75, 0.15, 0.12), (0.15, 0.35, 0.70), (0.85, 0.80, 0.70), (0.20, 0.55, 0.30)][(k + int(z * 10)) % 4]
            r.box((-2.7 + k * 0.55, 6.95, z), (-2.35 + k * 0.55, 7.0, z + 0.22), c)
    r.box((-2.0, 7.1, 2.45), (2.0, 7.15, 2.85), (1.0, 0.85, 0.25), 'emit')           # sign
    counter(r, top=(0.85, 0.25, 0.20), front=(0.25, 0.20, 0.35), ftex='plain')
    for x in (-3.15, 2.55):                                                           # ball racks with balls
        r.box((x, 1.0, 0), (x + 0.6, 3.2, 0.7), (0.30, 0.30, 0.32))
        for k in range(8):
            c = [(0.10, 0.20, 0.70), (0.70, 0.10, 0.15), (0.10, 0.55, 0.30), (0.45, 0.10, 0.55)][k % 4]
            r.box((x + 0.12, 1.1 + k * 0.26, 0.7), (x + 0.36, 1.32 + k * 0.26, 0.94), c)
    return r

def pool_room():
    """the pool hall: a table under a hanging lamp, the bar at the back"""
    r = Room(6)
    shell(r, (0.30, 0.22, 0.16), (0.28, 0.18, 0.12), 'wood', 'brick', ceil=(0.15, 0.13, 0.12))
    r.boxes = [b for b in r.boxes if b[3] != 'emit']
    r.box((-1.9, 2.9, 0), (0.6, 4.2, 0.72), (0.30, 0.18, 0.10), 'wood')              # table body
    r.box((-1.95, 2.85, 0.72), (0.65, 4.25, 0.80), (0.38, 0.22, 0.12), 'wood')        # rails
    r.box((-1.8, 3.0, 0.80), (0.5, 4.1, 0.81), (0.10, 0.45, 0.22))                    # felt
    for (x, y, c) in ((-1.2, 2.1, (0.9, 0.85, 0.2)), (-0.9, 2.3, (0.8, 0.1, 0.1)), (-1.0, 2.0, (0.1, 0.2, 0.7)),
                      (-0.6, 2.2, (0.05, 0.05, 0.05)), (0.0, 2.4, (0.95, 0.95, 0.9))):
        r.box((x - 0.03, y + 1.27, 0.81), (x + 0.03, y + 1.33, 0.87), c)
    r.box((-1.2, 3.2, 2.2), (-0.1, 3.9, 2.4), (0.10, 0.30, 0.15))                     # lamp shade
    r.box((-1.1, 3.3, 2.18), (-0.2, 3.8, 2.2), (1.0, 0.95, 0.75), 'emit')
    r.box((-0.7, 3.5, 2.4), (-0.6, 3.6, 3.0), (0.1, 0.1, 0.1))
    counter(r, top=(0.30, 0.18, 0.10), front=(0.22, 0.14, 0.09), x0=0.6, x1=3.0)      # the bar, right
    shelf_unit(r, 0.8, 3.1, 6.8, 7.15, (1.0, 1.4, 1.8), None, palette=[(0.55, 0.30, 0.10), (0.2, 0.45, 0.2), (0.75, 0.70, 0.55), (0.35, 0.10, 0.10)],
               col=(0.25, 0.16, 0.10), tex='wood')
    r.box((-3.1, 2.0, 0.9), (-3.15, 4.0, 2.0), (0.10, 0.10, 0.10))                    # cue rack
    for k in range(6): r.box((-3.1, 2.2 + k * 0.3, 0.95), (-3.05, 2.25 + k * 0.3, 1.95), (0.70, 0.50, 0.25))
    r.box((-1.0, 7.12, 2.2), (0.6, 7.15, 2.7), (0.95, 0.30, 0.25), 'emit')            # neon
    return r

ROOMS = [('gun', gun_room), ('store', store_room), ('pawn', pawn_room), ('cars', car_room), ('bowl', bowl_room), ('pool', pool_room)]

# ------------------------------------------------------------------ minigame art
LANE_CAM = (np.array((0.0, -2.5, 1.6), f32), math.radians(-6), 300.0, 0.30)   # camera, tilt, focal, horizon
LANE_HW, PIN_Y = 0.533, 18.29
PIN_CAM = (np.array((0.0, 18.29 - 3.4, 1.0), f32), math.radians(-18), 250.0, 0.30)   # the close-up on the pin deck                                                 # lane half-width, head pin distance (m)

def lane_room(pincam=False):
    r = Room(7)
    wood = (0.80, 0.58, 0.32)
    r.box((-4, -6, -0.3), (4, 30, -0.06), (0.10, 0.08, 0.10))                       # pit floor / under
    for lx in (-1.75, 0.0, 1.75):                                                     # this lane and its neighbours
        r.box((lx - LANE_HW, -6, -0.06), (lx + LANE_HW, 19.4, 0.0), wood, 'boards')
        for sgn in (-1, 1):                                                           # gutters
            r.box((lx + sgn * LANE_HW - (0.12 if sgn > 0 else 0), -6, -0.12), (lx + sgn * LANE_HW + (0.12 if sgn < 0 else 0), 19.4, -0.08), (0.28, 0.28, 0.30))
        if lx: r.box((lx - 0.2, PIN_Y + 1.0, 0.0), (lx + 0.2, PIN_Y + 1.1, 0.2), (0.9, 0.9, 0.9))
    for lx in (-0.875, 0.875):                                                        # ball-return caps between lanes
        r.box((lx - 0.21, -6, 0.0), (lx + 0.21, 19.4, 0.15), (0.30, 0.25, 0.40))
    for k in range(7):                                                                # aiming arrows
        x = (k - 3) * 0.13
        r.box((x - 0.012, 4.6 - abs(k - 3) * 0.12, 0.0), (x + 0.012, 4.75 - abs(k - 3) * 0.12, 0.002), (0.25, 0.12, 0.06))
    r.box((-0.53, -6, 0.0), (0.53, 0.0, 0.002), (0.70, 0.52, 0.30), 'wood')           # approach
    r.box((-0.53, -0.01, 0.0), (0.53, 0.01, 0.003), (0.10, 0.10, 0.12))               # foul line
    r.box((-4, PIN_Y + 1.3, -0.3), (4, PIN_Y + 1.5, 2.5), (0.06, 0.05, 0.07))         # pit back
    if not pincam:
        r.box((-4, PIN_Y - 1.2, 0.85), (4, PIN_Y - 0.9, 2.5), (0.20, 0.10, 0.40))     # masking unit
        for k, c in enumerate([(0.95, 0.30, 0.70), (0.30, 0.85, 0.95), (1.0, 0.80, 0.20)]):
            r.box((-3 + k * 2.1, PIN_Y - 1.21, 1.2), (-1.4 + k * 2.1, PIN_Y - 1.2, 1.7), c, 'emit')
    r.box((-4.5, -6, -0.3), (-4.2, 30, 4), (0.18, 0.12, 0.24))                        # walls, ceiling
    r.box((4.2, -6, -0.3), (4.5, 30, 4), (0.18, 0.12, 0.24))
    r.box((-4.5, -6, 3.5), (4.5, 30, 3.8), (0.08, 0.07, 0.10))
    for y in range(0, 18, 3):
        r.box((-3.5, y, 3.45), (3.5, y + 0.3, 3.5), (0.85, 0.80, 0.95), 'emit')
    return r

def pin_sprite(h_px, fallen=0):
    """a bowling pin, h_px tall (rendered 4x and box-filtered); fallen = -1/+1 lies to that side"""
    S = 4; hh = h_px * S; ww = int(hh * 0.45) | 1
    z = (hh - 1 - np.arange(hh)) / hh                        # 0 bottom .. 1 top
    prof = np.interp(z, [0, 0.05, 0.3, 0.45, 0.62, 0.70, 0.82, 0.93, 1.0], [0.42, 0.55, 1.0, 0.85, 0.40, 0.38, 0.55, 0.45, 0.10])
    xs = (np.arange(ww) - ww / 2 + 0.5) / (ww / 2)
    r = prof[:, None] * 0.88
    m = np.abs(xs[None, :]) < r
    t = np.clip(xs[None, :] / np.maximum(r, 1e-3), -1, 1)
    sh = 0.62 + 0.38 * np.sqrt(np.clip(1 - t ** 2, 0, 1)) - 0.12 * t
    rgb = np.ones((hh, ww, 3), f32) * sh[..., None] * 0.97
    stripe = ((z > 0.63) & (z < 0.67)) | ((z > 0.69) & (z < 0.72))
    rgb[stripe] = rgb[stripe] * np.array((0.85, 0.12, 0.10), f32)
    if fallen:
        im = Image.fromarray((np.dstack([rgb, m[..., None]]) * 255).astype(np.uint8).reshape(hh, ww, 4), 'RGBA')
        im = im.rotate(-82 * fallen, expand=True, resample=Image.BICUBIC)
        a = np.asarray(im).astype(f32) / 255; rgb, m = a[..., :3], a[..., 3] > 0.5
        hh, ww = m.shape
    hh2, ww2 = hh // S * S, ww // S * S
    rgb = rgb[:hh2, :ww2].reshape(hh2 // S, S, ww2 // S, S, 3).mean((1, 3))
    m = m[:hh2, :ww2].reshape(hh2 // S, S, ww2 // S, S).mean((1, 3)) > 0.45
    return rgb, m

def ball_sprite(d_px, base, stripe=None, number_dot=True, gloss=0.5, marble=False):
    S = 4; n = max(2, int(round(d_px))) * S
    yy, xx = (np.mgrid[0:n, 0:n] + 0.5) / n * 2 - 1
    rr = xx ** 2 + yy ** 2; m = rr < 1
    nz = np.sqrt(np.clip(1 - rr, 0, 1))
    L = np.array((-0.45, -0.55, 0.70)); L /= np.linalg.norm(L)
    lam = np.clip(xx * L[0] + yy * L[1] + nz * L[2], 0, 1)
    col = np.tile(np.array(base, f32), (n, n, 1))
    if marble:
        v = 0.5 + 0.5 * np.sin(xx * 9 + np.sin(yy * 7) * 2.5)
        col = col * (0.75 + 0.45 * v[..., None])
    if stripe is not None:
        band = np.abs(yy) < 0.42
        col = np.where(band[..., None], np.array(stripe, f32), np.array((0.95, 0.95, 0.92), f32))
    if number_dot:
        dot = (xx + 0.05) ** 2 + (yy + 0.05) ** 2 < 0.13
        col = np.where(dot[..., None], np.array((0.97, 0.97, 0.95), f32), col)
    rgb = col * (0.35 + 0.75 * lam[..., None])
    spec = np.clip((xx + 0.4) ** 2 + (yy + 0.45) ** 2, 0, 1) < 0.04
    rgb = np.where(spec[..., None], np.array((1, 1, 1), f32) * gloss + rgb * (1 - gloss), rgb)
    k = n // S
    rgb = rgb.reshape(k, S, k, S, 3).mean((1, 3)); m = m.reshape(k, S, k, S).mean((1, 3)) > 0.45
    return np.clip(rgb, 0, 1), m

POOL = dict(x0=28, y0=34, x1=212, y1=126)       # felt rectangle on the 240x160 screen
def pool_table():
    yy, xx = np.mgrid[0:H, 0:W].astype(f32)
    plank = np.floor(yy / 9)
    rgb = np.zeros((H, W, 3), f32)
    rgb[:] = np.array((0.26, 0.17, 0.10), f32)
    rgb *= (0.85 + 0.15 * np.sin(xx * 0.35 + plank * 2.1 + np.sin(yy * 0.2) * 1.5))[..., None]
    rgb[(yy % 9) < 1] *= 0.6
    x0, y0, x1, y1 = POOL['x0'], POOL['y0'], POOL['x1'], POOL['y1']
    rail = (xx >= x0 - 12) & (xx < x1 + 12) & (yy >= y0 - 12) & (yy < y1 + 12)
    rgb[rail] = np.array((0.42, 0.22, 0.10), f32) * (0.85 + 0.15 * np.sin(xx[rail] * 0.6 + yy[rail] * 0.15))[:, None]
    edge = rail & ~((xx >= x0 - 10) & (xx < x1 + 10) & (yy >= y0 - 10) & (yy < y1 + 10))
    rgb[edge] *= 0.7
    cush = (xx >= x0 - 4) & (xx < x1 + 4) & (yy >= y0 - 4) & (yy < y1 + 4)
    rgb[cush] = (0.06, 0.32, 0.15)
    felt = (xx >= x0) & (xx < x1) & (yy >= y0) & (yy < y1)
    cx, cy = (x0 + x1) / 2, (y0 + y1) / 2
    lamp = 1.0 - 0.35 * np.clip(((xx - cx) / 110) ** 2 + ((yy - cy) / 70) ** 2, 0, 1)
    rgb[felt] = np.array((0.10, 0.52, 0.25), f32) * lamp[felt][:, None]
    for i in range(1, 8):                                    # diamonds on the rails
        for (px, py) in ((x0 + (x1 - x0) * i / 8, y0 - 8), (x0 + (x1 - x0) * i / 8, y1 + 8)):
            if i == 4: continue
            rgb[(np.abs(xx - px) + np.abs(yy - py)) < 1.6] = (0.92, 0.88, 0.75)
    for i in range(1, 4):
        for (px, py) in ((x0 - 8, y0 + (y1 - y0) * i / 4), (x1 + 8, y0 + (y1 - y0) * i / 4)):
            rgb[(np.abs(xx - px) + np.abs(yy - py)) < 1.6] = (0.92, 0.88, 0.75)
    for (px, py) in POCKETS:
        rgb[((xx - px) ** 2 + (yy - py) ** 2) < 6.5 ** 2] = (0.02, 0.02, 0.02)
        ring = (((xx - px) ** 2 + (yy - py) ** 2) < 7.5 ** 2) & (((xx - px) ** 2 + (yy - py) ** 2) >= 6.5 ** 2)
        rgb[ring] = (0.55, 0.50, 0.40)
    rgb[(np.abs(xx - (x0 + (x1 - x0) * 0.25)) < 0.6) & felt] *= 1.25            # head string
    rgb[((xx - (x0 + (x1 - x0) * 0.75)) ** 2 + (yy - cy) ** 2) < 1.2] = (0.85, 0.85, 0.8)   # foot spot
    return np.clip(rgb, 0, 1)
POCKETS = [(POOL['x0'] - 1, POOL['y0'] - 1), ((POOL['x0'] + POOL['x1']) / 2, POOL['y0'] - 3), (POOL['x1'] + 1, POOL['y0'] - 1),
           (POOL['x0'] - 1, POOL['y1'] + 1), ((POOL['x0'] + POOL['x1']) / 2, POOL['y1'] + 3), (POOL['x1'] + 1, POOL['y1'] + 1)]
POOL_COLS = [(0.95, 0.95, 0.92), (0.95, 0.78, 0.10), (0.10, 0.25, 0.80), (0.85, 0.12, 0.10), (0.40, 0.12, 0.60), (0.95, 0.45, 0.08),
             (0.05, 0.50, 0.25), (0.55, 0.12, 0.10), (0.04, 0.04, 0.04)]

def minigame_art():
    """returns [(name, index image)], [(name, value)] for minigame_art.c"""
    global CAM, TILT, FOCAL, HZ
    out = []; consts = []
    saved = (CAM, TILT, FOCAL, HZ)
    CAM, TILT, FOCAL, HZ = LANE_CAM
    rgb, _ = render_room(lane_room())
    out.append(('lane_img', to_idx(rgb, np.ones((H, W), bool))))
    _, pin_sy, pin_ppm = project((0, PIN_Y, 0))
    CAM, TILT, FOCAL, HZ = PIN_CAM
    rgb, _ = render_room(lane_room(True))
    out.append(('pincam_img', to_idx(rgb, np.ones((H, W), bool))))
    _, _, big_ppm = project((0, PIN_Y + 0.4, 0))
    CAM, TILT, FOCAL, HZ = saved
    bh = int(round(0.38 * big_ppm))
    for nm, fl in (('pinbig_up', 0), ('pinbig_l', -1), ('pinbig_r', 1)):
        prgb, pm = pin_sprite(bh, fl); out.append((nm, to_idx(prgb, pm)))
    ph = int(round(0.38 * pin_ppm * 1.5))                     # pins drawn 1.5x so they read on a GBA screen
    for nm, fl in (('pin_up', 0), ('pin_l', -1), ('pin_r', 1)):
        prgb, pm = pin_sprite(ph, fl); out.append((nm, to_idx(prgb, pm)))
    for k, dpx in enumerate((4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 28)):
        brgb, bm = ball_sprite(dpx, (0.18, 0.12, 0.55), number_dot=False, gloss=0.6, marble=True)
        out.append(('bball_%d' % k, to_idx(brgb, bm)))
    out.append(('pool_img', to_idx(pool_table(), np.ones((H, W), bool))))
    for n in range(16):
        if n == 0: b = ball_sprite(9, POOL_COLS[0], number_dot=False)
        elif n <= 8: b = ball_sprite(9, POOL_COLS[n])
        else: b = ball_sprite(9, None or (0.95, 0.95, 0.92), stripe=POOL_COLS[n - 8])
        out.append(('pball_%d' % n, to_idx(*b)))
    consts += [('LANE_CAMY_MM', int(LANE_CAM[0][1] * 1000)), ('LANE_CAMZ_MM', int(LANE_CAM[0][2] * 1000)),
               ('LANE_TILT_COS', int(math.cos(LANE_CAM[1]) * 16384)), ('LANE_TILT_SIN', int(math.sin(LANE_CAM[1]) * 16384)),
               ('LANE_FOCAL', int(LANE_CAM[2])), ('LANE_HZ_Y', int(H * LANE_CAM[3])),
               ('PCAM_Y_MM', int(PIN_CAM[0][1] * 1000)), ('PCAM_Z_MM', int(PIN_CAM[0][2] * 1000)),
               ('PCAM_COS', int(math.cos(PIN_CAM[1]) * 16384)), ('PCAM_SIN', int(math.sin(PIN_CAM[1]) * 16384)),
               ('PCAM_FOCAL', int(PIN_CAM[2])), ('PCAM_HZ_Y', int(H * PIN_CAM[3])),
               ('POOL_X0', POOL['x0']), ('POOL_Y0', POOL['y0']), ('POOL_X1', POOL['x1']), ('POOL_Y1', POOL['y1'])]
    return out, consts

# ------------------------------------------------------------------ clerk and the showroom car
from frame_render import render_frame
from sprites_people import Human
class Clerk(Human):
    """the shop clerk; hands_up raises both arms (a hold-up)"""
    def __init__(self, outfit, hands_up):
        super().__init__(outfit, 0.0, mode='idle')
        if not hands_up: return
        J = self.J; arms = []
        for sg in (1, -1):
            sh = np.array((0.0, sg * 0.19, 0.455 + 0.0), f32) + self.J['pelvis']
            up = np.array((0.05, sg * 0.55, 0.83), f32); up /= np.linalg.norm(up)
            el = sh + up * 0.28
            fa = np.array((0.12, sg * 0.10, 1.0), f32); fa /= np.linalg.norm(fa)
            wr = el + fa * 0.25
            arms.append((sh, el, wr, wr + fa * 0.05))
        self.arms = arms

def sprite_img(model, yaw, elev, scale, w, up, down):
    rgb, m, sm, ox, oy, upx = render_frame(model, yaw, elev, scale, w, up, down, shadow=False, ss=2, outline=True)
    return rgb, m, ox, oy

def clerk_layer(hands_up, outfit, x):
    fx, fy, ppm = project((x, CLERK_Y, 0.0))
    s = 1.0 / ppm
    rgb, m, ox, oy = sprite_img(Clerk(outfit, hands_up), math.pi, math.radians(4), s, 90, 130, 4)
    return rgb, m, int(round(fx + ox)), int(round(fy + oy))

def car_layer():
    from cars3 import Car3
    car = Car3('gt', (0.62, 0.05, 0.05))
    cx, cy, ppm = project((-1.5, 4.6, 0.0))
    rgb, m, ox, oy = sprite_img(car, math.radians(125), math.radians(6), 1.0 / ppm, 200, 110, 30)
    return rgb, m, int(round(cx + ox)), int(round(cy + oy))

if __name__ == '__main__':
    imgs = []; prev = []
    outfits = {'gun': 'green', 'store': 'grey', 'pawn': 'suit', 'cars': 'white', 'bowl': 'red', 'pool': 'navy'}
    for name, fn in ROOMS:
        rgb, fgm = render_room(fn())
        if name == 'cars':                                   # the car on the showroom floor
            crgb, cm, x0, y0 = car_layer()
            h, w = cm.shape
            for yy in range(h):
                for xx in range(w):
                    X, Y = x0 + xx, y0 + yy
                    if cm[yy, xx] and 0 <= X < W and 0 <= Y < H: rgb[Y, X] = crgb[yy, xx]
        full = np.ones((H, W), bool)
        bg = to_idx(rgb, full)
        fg = np.where(fgm, bg, 0).astype(np.uint8)
        imgs.append(('room_' + name, bg)); imgs.append(('front_' + name, fg))
        prev.append((rgb, fgm, name))
        for up in (0, 1):
            crgb, cm, x0, y0 = clerk_layer(up, outfits[name], CLERK_X[name])
            imgs.append(('clerk_%s_%d' % (name, up), to_idx(crgb, cm)))
            imgs.append(('clerkpos_%s_%d' % (name, up), (x0, y0)))
    L = ['/* generated by tools/make_interiors.py: shop rooms, counters and clerks as palette indices, 0 = transparent */',
         '#include "engine.h"', '#include "interiors.h"']
    Hh = ['#ifndef INTERIORS_H', '#define INTERIORS_H', '#include "logo.h"', '/* per shop type (0 gun, 1 store, 2 pawn, 3 cars, 4 bowling, 5 pool) */', '#define NROOMS 6',
          'extern const Img *const room_img[NROOMS], *const front_img[NROOMS], *const clerk_img[NROOMS][2];', 'extern const s16 clerk_pos[NROOMS][2][2];']
    pos = {}
    for name, a in imgs:
        if name.startswith('clerkpos'): pos[name] = a; continue
        h, w = a.shape
        ys, xs = np.nonzero(a)
        if name.startswith('front'):                   # crop the counter layer to its pixels
            y0c = ys.min(); a = a[y0c:]; pos[name] = y0c; h = a.shape[0]
        L.append('static const u8 %s_px[%d] = {' % (name, w * h))
        flat = a.reshape(-1)
        for k in range(0, len(flat), 40): L.append('  ' + ','.join(str(int(v)) for v in flat[k:k + 40]) + ',')
        L.append('};')
        L.append('static const Img %s = { %d, %d, %s_px };' % (name, w, h, name))
    names = [n for n, _ in ROOMS]
    L.append('const Img *const room_img[NROOMS] = { %s };' % ', '.join('&room_' + n for n in names))
    L.append('const Img *const front_img[NROOMS] = { %s };' % ', '.join('&front_' + n for n in names))
    L.append('const Img *const clerk_img[NROOMS][2] = { %s };' % ', '.join('{ &clerk_%s_0, &clerk_%s_1 }' % (n, n) for n in names))
    L.append('const s16 clerk_pos[NROOMS][2][2] = { %s };' % ', '.join('{ {%d,%d}, {%d,%d} }' % (*pos['clerkpos_%s_0' % n], *pos['clerkpos_%s_1' % n]) for n in names))
    L.append('const s16 front_y[NROOMS] = { %s };' % ', '.join(str(pos['front_' + n]) for n in names))
    Hh.append('extern const s16 front_y[NROOMS];')
    Hh.append('#endif')
    out = sys.argv[2]
    open(out, 'w').write('\n'.join(L) + '\n')
    open(out.replace('.c', '.h'), 'w').write('\n'.join(Hh) + '\n')
    if len(sys.argv) > 3:                               # preview: palette-mapped rooms with clerks
        lut = np.zeros((256, 3), np.uint8)
        for k, i in enumerate(IDX): lut[i] = (P[k] * 255).astype(np.uint8)
        d = dict((n, a) for n, a in imgs)
        S = Image.new('RGB', (W * 2, H * len(names)))
        for k, n in enumerate(names):
            for up in (0, 1):
                im = lut[d['room_' + n]].copy()
                c = d['clerk_%s_%d' % (n, up)]; x0, y0 = pos['clerkpos_%s_%d' % (n, up)]
                for yy in range(c.shape[0]):
                    for xx in range(c.shape[1]):
                        if c[yy, xx] and 0 <= x0 + xx < W and 0 <= y0 + yy < H: im[y0 + yy, x0 + xx] = lut[c[yy, xx]]
                f = d['front_' + n]; fy0 = pos['front_' + n]
                im[f > 0] = lut[f[f > 0]]
                S.paste(Image.fromarray(im), (up * W, k * H))
        S.resize((W * 4, H * 2 * len(names)), Image.NEAREST).save(sys.argv[3])
    # minigame art -> minigame_art.c / .h
    mg, consts = minigame_art()
    L = ['/* generated by tools/make_interiors.py: bowling and pool art, palette indices, 0 = transparent */',
         '#include "engine.h"', '#include "minigame_art.h"']
    Hh = ['#ifndef MINIGAME_ART_H', '#define MINIGAME_ART_H', '#include "logo.h"']
    for nm, a in mg:
        h, w = a.shape
        L.append('static const u8 %s_px[%d] = {' % (nm, w * h))
        flat = a.reshape(-1)
        for k in range(0, len(flat), 40): L.append('  ' + ','.join(str(int(v)) for v in flat[k:k + 40]) + ',')
        L.append('};'); L.append('const Img %s = { %d, %d, %s_px };' % (nm, w, h, nm))
        if not nm.startswith(('bball_', 'pball_')): Hh.append('extern const Img %s;' % nm)
    nb = len([1 for nm, _ in mg if nm.startswith('bball_')])
    L.append('const Img *const bball_img[%d] = { %s };' % (nb, ', '.join('&bball_%d' % k for k in range(nb))))
    L.append('const Img *const pball_img[16] = { %s };' % ', '.join('&pball_%d' % k for k in range(16)))
    Hh.append('#define NBBALL %d' % nb); Hh.append('extern const Img *const bball_img[NBBALL], *const pball_img[16];')
    Hh.append('static const unsigned char bball_d[NBBALL] = { %s };' % ', '.join(str(a.shape[1]) for nm, a in mg if nm.startswith('bball_')))
    for nm, v in consts: Hh.append('#define %s %d' % (nm, v))
    Hh.append('#endif')
    mgo = out.replace('interiors.c', 'minigame_art.c')
    open(mgo, 'w').write('\n'.join(L) + '\n'); open(mgo.replace('.c', '.h'), 'w').write('\n'.join(Hh) + '\n')
    if len(sys.argv) > 3:
        lut = np.zeros((256, 3), np.uint8)
        for k, i in enumerate(IDX): lut[i] = (P[k] * 255).astype(np.uint8)
        d = dict(mg)
        S = Image.new('RGB', (W * 2, H), (60, 60, 60))
        S.paste(Image.fromarray(lut[d['lane_img']]), (0, 0)); S.paste(Image.fromarray(lut[d['pool_img']]), (W, 0))
        x = 4
        S.paste(Image.fromarray(lut[d['pincam_img']]).resize((120, 80)), (2, H - 82))
        for nm in ['pin_up', 'pin_l', 'pin_r', 'pinbig_up', 'pinbig_l'] + ['bball_%d' % k for k in range(nb)] + ['pball_%d' % k for k in range(16)]:
            a = d[nm]; im = Image.fromarray(lut[a]); S.paste(im, (x if x < 2 * W - 30 else 2, 2), Image.fromarray(((a > 0) * 255).astype(np.uint8)))
            x += a.shape[1] + 2
        S.resize((W * 4, H * 2), Image.NEAREST).save(sys.argv[3].replace('.png', '_mg.png'))
    print('interiors done')
