# Renders the street props and appends them to the existing sprite set using the current
# palette (no full re-render). Usage: append_props.py src/assets.c src/assets.h build/assets.bin
import sys, os, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render
from cars3 import Car3

cfile, hfile, bfile = sys.argv[1:4]
csrc = open(cfile).read()
hsrc = open(hfile).read()
if 'CAR_WRECK' in hsrc:
    print('fx already appended'); sys.exit(0)
table = [tuple(int(v) for v in r) for r in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', csrc.split('spr_frames')[1].split('};')[0])]
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
pal = np.array([((c & 31) / 31.0, ((c >> 5) & 31) / 31.0, ((c >> 10) & 31) / 31.0) for c in pal15], np.float32)
NCOL = len(pal)
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)
pal_lab = tolab(pal)
blob = bytearray(open(bfile, 'rb').read())

def add(model, yaw, elev, s, w, up, dn, ss=2, post=None):
    rgb, a, sh = render(model, yaw, np.radians(elev), s / ss, w * ss, up * ss, dn * ss, True)
    if post is not None: rgb = post(rgb)
    H0, W0 = a.shape
    a4 = a.reshape(H0 // ss, ss, W0 // ss, ss).astype(np.float32)
    csum = (rgb.reshape(H0 // ss, ss, W0 // ss, ss, 3) * a4[..., None]).sum((1, 3))
    rgb = csum / np.maximum(a4.sum((1, 3)), 1)[..., None]
    sh = sh.reshape(H0 // ss, ss, W0 // ss, ss).mean((1, 3)) >= 0.5
    a = a4.mean((1, 3)) >= 0.5; sh &= ~a
    H, W = a.shape
    yy, xx = np.mgrid[0:H, 0:W]
    smask = sh & (((xx + yy) & 1) == 0)
    ys, xs = np.nonzero(a | smask)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    lab = np.zeros((y1 - y0, x1 - x0), np.uint8)
    m = a[y0:y1, x0:x1]
    L_ = tolab(rgb[y0:y1, x0:x1][m])
    for k in range(0, len(L_), 20000):
        sub = L_[k:k + 20000]
        idx = ((sub[:, None, :] - pal_lab[None, :NCOL - 1]) ** 2).sum(-1).argmin(1) + 1
        mm = np.flatnonzero(m.reshape(-1))[k:k + 20000]
        lab.reshape(-1)[mm] = idx
    lab[smask[y0:y1, x0:x1]] = NCOL
    h, ww = lab.shape
    off = len(blob); blob.extend(lab.T.tobytes())
    while len(blob) % 4: blob.append(0)
    table.append((ww, h, int(x0 - w // 2), int(y0 - up), int(round(s * 16 * 256)), off))


def add_image(col, mask, ox, oy, upx):
    """col HxWx3 float, mask HxW bool; ox,oy: offset of the top-left from the anchor (pixels)"""
    H, W = mask.shape
    lab = np.zeros((H, W), np.uint8)
    L_ = tolab(col[mask])
    if len(L_):
        idx = ((L_[:, None, :] - pal_lab[None, :NCOL - 1]) ** 2).sum(-1).argmin(1) + 1
        lab[mask] = idx
    off = len(blob); blob.extend(lab.T.tobytes())
    while len(blob) % 4: blob.append(0)
    table.append((W, H, int(ox), int(oy), int(upx), off))

defs = []
def begin(name): defs.append((name, len(table)))
ELEV = [(16, 0.040, 136, 62, 24), (11, 0.050, 112, 46, 20), (7, 0.065, 86, 30, 14), (4, 0.090, 62, 18, 10)]
begin('CAR_WRECK')                        # burnt-out shell, same layout as the car sets
wreck = Car3('fullsize', (0.055, 0.048, 0.042), sport_wheels=False)
_rw = np.random.default_rng(9)
def burnt(rgb):
    # charred: no lit lamps or bright glass, sooty grey-black with rust-brown blotches
    H, W, _ = rgb.shape
    lum = rgb.mean(-1, keepdims=True)
    g = np.minimum(lum, 0.30) * 0.75
    n = _rw.random((H // 6 + 2, W // 6 + 2))
    yy, xx = np.mgrid[0:H, 0:W]
    nn = n[yy // 6, xx // 6][..., None]
    rust = np.array((0.30, 0.15, 0.07), np.float32)
    ash = np.array((0.32, 0.31, 0.29), np.float32)
    out = g * np.array((1.0, 0.97, 0.93), np.float32)
    out = np.where(nn > 0.70, out * 0.4 + rust * (0.5 + lum), out)
    out = np.where(nn < 0.10, out * 0.5 + ash * 0.6, out)
    return out.astype(np.float32)
for e, s_, w, up, dn in ELEV:
    for i in range(32): add(wreck, i * 2 * np.pi / 32, e, s_, w, up, dn, post=burnt)
print('wreck done', flush=True)

rng = np.random.default_rng(5)
def noise2(h, w, sc):
    g = rng.random((h // sc + 2, w // sc + 2))
    yy, xx = np.mgrid[0:h, 0:w] / sc
    y0 = yy.astype(int); x0 = xx.astype(int); fy = yy - y0; fx = xx - x0
    a = g[y0, x0] * (1 - fx) + g[y0, x0 + 1] * fx; b = g[y0 + 1, x0] * (1 - fx) + g[y0 + 1, x0 + 1] * fx
    return a * (1 - fy) + b * fy
BAY = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) + 0.5) / 16

begin('FX_SMOKE')                         # 4 frames: puff growing and thinning
for k in range(4):
    S = 20 + k * 6
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    r = np.hypot(xx - S / 2 + 0.5, (yy - S / 2 + 0.5) * 1.1) / (S / 2)
    n = noise2(S, S, 4)
    dens = np.clip((1 - r) * 1.6 + (n - 0.5) * 0.9, 0, 1) * (1.0 - k * 0.17)
    mask = dens > BAY[(yy.astype(int) & 3), (xx.astype(int) & 3)] * 0.9 + 0.08
    v = 0.25 + 0.35 * n + 0.15 * (1 - yy / S)
    col = np.stack([v, v, v * 1.04], -1).astype(np.float32)
    add_image(col, mask, -S // 2, -S, int(0.03 * 16 * 256))
begin('FX_FIRE')                          # 4 flicker frames
for k in range(4):
    W_, H_ = 22, 30
    yy, xx = np.mgrid[0:H_, 0:W_].astype(np.float32)
    n = noise2(H_, W_, 3)
    t = yy / H_                                   # 0 top .. 1 bottom
    half = (0.15 + 0.85 * t ** 0.6) * W_ / 2 * (0.75 + 0.35 * n)
    shape = np.abs(xx - W_ / 2 + 0.5) < half
    shape &= (n * 0.5 + t) > 0.35 + 0.05 * k
    heat = np.clip(t * 1.1 + (1 - np.abs(xx - W_ / 2) / (W_ / 2)) * 0.5 - 0.3 + (n - 0.5) * 0.3, 0, 1)
    col = np.stack([0.85 + 0.15 * heat, 0.12 + 0.8 * heat ** 1.6, 0.02 + 0.7 * np.clip(heat - 0.7, 0, 1) * 3], -1).astype(np.float32)
    rng.random(7)
    add_image(col, shape, -W_ // 2, -H_, int(0.035 * 16 * 256))
begin('FX_EXPL')                          # 6 frames: flash, fireball, rolling smoke
for k in range(6):
    S = 24 + k * 10
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    r = np.hypot(xx - S / 2 + 0.5, yy - S / 2 + 0.5) / (S / 2)
    n = noise2(S, S, 4 + k)
    edge = 1 - r + (n - 0.5) * 0.5
    mask = edge > 0.02 + (0.25 * BAY[(yy.astype(int) & 3), (xx.astype(int) & 3)] if k >= 4 else 0)
    hot = np.clip(edge * (2.2 - k * 0.38), 0, 1)
    fire = np.stack([np.ones_like(hot), 0.3 + 0.7 * hot, 0.1 + 0.85 * np.clip(hot - 0.5, 0, 1) * 2], -1)
    smoke = np.stack([0.18 + 0.2 * n] * 3, -1)
    mixw = np.clip((k - 1.5) / 3.5 + (1 - hot) * 0.6, 0, 1)[..., None] if k > 1 else np.zeros_like(hot)[..., None]
    col = (fire * (1 - mixw) + smoke * mixw).astype(np.float32)
    add_image(col, mask, -S // 2, -S * 3 // 4, int(0.05 * 16 * 256))
print('rendered', len(table), flush=True)
open(bfile, 'wb').write(blob)
head, rest = csrc.split('const SprFrame spr_frames', 1)
tail = rest.split('};', 1)[1]
open(cfile, 'w').write(head + '\n'.join(['const SprFrame spr_frames[%d] = {' % len(table)] + ['  {%d,%d,%d,%d,%d,%d},' % t for t in table]) + '\n};' + tail)
extra = ''.join('#define %s %d\n' % d for d in defs)
hsrc = re.sub(r'#define SPR_NFRAMES \d+', '#define SPR_NFRAMES %d' % len(table), hsrc)
hsrc = hsrc.replace('#endif', extra + '#endif')
open(hfile, 'w').write(hsrc)
print('appended', [d[0] for d in defs])
