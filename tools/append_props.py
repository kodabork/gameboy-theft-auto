# Renders the street props and appends them to the existing sprite set using the current
# palette (no full re-render). Usage: append_props.py src/assets.c src/assets.h build/assets.bin
import sys, os, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render
from props import TrafficLight, Hydrant, TrashCan, Bench, StreetTree, WaterTower, Cat

cfile, hfile, bfile = sys.argv[1:4]
csrc = open(cfile).read()
hsrc = open(hfile).read()
if 'PROP_TLIGHT' in hsrc:
    print('props already appended'); sys.exit(0)
table = [tuple(int(v) for v in r) for r in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', csrc.split('spr_frames')[1].split('};')[0])]
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
pal = np.array([((c & 31) / 31.0, ((c >> 5) & 31) / 31.0, ((c >> 10) & 31) / 31.0) for c in pal15], np.float32)
NCOL = len(pal)
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)
pal_lab = tolab(pal)
blob = bytearray(open(bfile, 'rb').read())

def add(model, yaw, elev, s, w, up, dn, ss=2):
    rgb, a, sh = render(model, yaw, np.radians(elev), s / ss, w * ss, up * ss, dn * ss, True)
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

defs = []
def begin(name): defs.append((name, len(table)))
begin('PROP_TLIGHT')                      # 8 view angles; arm reaches along +x
for i in range(8): add(TrafficLight(), i * 2 * np.pi / 8, 10, 0.03, 260, 200, 10)
begin('PROP_HYDRANT'); add(Hydrant(), 0.4, 14, 0.015, 40, 46, 8)
begin('PROP_TRASH'); add(TrashCan(), 0.0, 14, 0.015, 48, 68, 8)
begin('PROP_BENCH')                       # 4 view angles
for i in range(4): add(Bench(), i * 2 * np.pi / 4, 14, 0.02, 110, 50, 20)
begin('PROP_STREETTREE'); add(StreetTree(), 0.0, 10, 0.04, 100, 158, 12)
begin('PROP_WATERTOWER'); add(WaterTower(), 0.3, 6, 0.05, 80, 130, 10)
# cats: per coat, 8 view directions x (sit, walk 0..3) = 40 frames
for coat, name in (('tabby', 'CAT_TABBY'), ('black', 'CAT_BLACK'), ('grey', 'CAT_GREY')):
    begin(name)
    for d in range(8):
        yaw = d * 2 * np.pi / 8
        add(Cat(coat, 'sit'), yaw, 15, 0.016, 56, 40, 8)
        for f in range(4): add(Cat(coat, 'walk', f * 2 * np.pi / 4), yaw, 15, 0.016, 56, 40, 8)
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
