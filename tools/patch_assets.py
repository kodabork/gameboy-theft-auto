# Re-renders selected frames and splices them into the existing asset blob, keeping the
# current palette. Usage: patch_assets.py src/assets.c src/assets.h build/assets.bin
import sys, os, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render, SHADOW
from sprites_people import Human

cfile, hfile, bfile = sys.argv[1:4]
csrc = open(cfile).read()
rows = re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', csrc.split('spr_frames')[1].split('};')[0])
table = [tuple(int(v) for v in r) for r in rows]
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
pal = np.array([((c & 31) / 31.0, ((c >> 5) & 31) / 31.0, ((c >> 10) & 31) / 31.0) for c in pal15], np.float32)
NCOL = len(pal)
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)
pal_lab = tolab(pal)
sets = dict(re.findall(r'#define (PED_\w+) (\d+)', open(hfile).read()))
blob = open(bfile, 'rb').read()

def quantize(rgb, mask, smask):
    lab = np.zeros(mask.shape, np.uint8)
    if mask.any():
        L_ = tolab(rgb[mask])
        lab[mask] = ((L_[:, None, :] - pal_lab[None, :NCOL - 1]) ** 2).sum(-1).argmin(1) + 1
    lab[smask] = NCOL
    return lab

new = {}
def redo(idx, model, yaw, elev, s, w, up, dn):
    rgb, a, sh = render(model, yaw, np.radians(elev), s, w, up, dn, True)
    H, W = a.shape
    yy, xx = np.mgrid[0:H, 0:W]
    smask = sh & (((xx + yy) & 1) == 0)
    any_ = a | smask
    ys, xs = np.nonzero(any_)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    lab = quantize(rgb[y0:y1, x0:x1], a[y0:y1, x0:x1], smask[y0:y1, x0:x1])
    new[idx] = (lab, int(x0 - w // 2), int(y0 - up), int(round(s * 16 * 256)))

outfits = {'PED_PLAYER': 'player', 'PED_GREY': 'grey', 'PED_RED': 'red', 'PED_NAVY': 'navy', 'PED_SUIT': 'suit',
           'PED_GREEN': 'green', 'PED_WHITE': 'white', 'PED_COP': 'cop'}
for name, outfit in outfits.items():
    base = int(sets[name])
    blocks = [(0, 17, 0.022, 56, 92, 8)] if name == 'PED_PLAYER' else [(0, 15, 0.032, 40, 64, 6), (152, 6, 0.060, 22, 34, 4)]
    for off, elev, s, w, up, dn in blocks:
        for d in range(8):
            # lying bodies need a canvas as long as a person is tall
            redo(base + off + 144 + d, Human(outfit, 0, mode='down', down_seed=d % 3), d * 2 * np.pi / 8, elev, s,
                 int(2.3 / s) // 2 * 2, int(1.1 / s), int(1.1 / s))
    print(name, flush=True)

out = bytearray(); ntab = []
for i, (w, h, ox, oy, upx, off) in enumerate(table):
    if i in new:
        lab, ox, oy, upx = new[i]
        h, w = lab.shape
        data = lab.T.tobytes()
    else:
        data = blob[off:off + w * h]
    noff = len(out); out += data
    while len(out) % 4: out.append(0)
    ntab.append((w, h, ox, oy, upx, noff))
open(bfile, 'wb').write(out)
head, rest = csrc.split('const SprFrame spr_frames', 1)
tail = rest.split('};', 1)[1]
lines = ['const SprFrame spr_frames[%d] = {' % len(ntab)] + ['  {%d,%d,%d,%d,%d,%d},' % t for t in ntab]
open(cfile, 'w').write(head + '\n'.join(lines) + '\n};' + tail)
print('patched', len(new), 'frames; blob', len(out))
