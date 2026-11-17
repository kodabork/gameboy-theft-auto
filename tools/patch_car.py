# Re-render one car set and splice it into the existing blob, keeping the palette.
import sys, os, re, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render
from cars_real import RealCar
cfile, hfile, bfile, setname = sys.argv[1:5]
car = RealCar('compact', (0.08, 0.22, 0.62), sport_wheels=True)
ELEV = [(16, 0.040, 136, 62, 24), (11, 0.050, 112, 46, 20), (7, 0.065, 86, 30, 14), (4, 0.090, 62, 18, 10)]
csrc = open(cfile).read()
table = [tuple(int(v) for v in r) for r in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', csrc.split('spr_frames')[1].split('};')[0])]
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
pal = np.array([((c & 31) / 31.0, ((c >> 5) & 31) / 31.0, ((c >> 10) & 31) / 31.0) for c in pal15], np.float32)
NCOL = len(pal)
def tolab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.25, 0.8), np.float32)
pal_lab = tolab(pal)
base = int(re.search(r'#define %s (\d+)' % setname, open(hfile).read()).group(1))
blob = open(bfile, 'rb').read()
new = {}; idx = base
for e, s, w, up, dn in ELEV:
    for i in range(32):
        ss = 2
        rgb, a, sh = render(car, i * 2 * np.pi / 32, np.radians(e), s / ss, w * ss, up * ss, dn * ss, True)
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
        lab[m] = ((L_[:, None, :] - pal_lab[None, :NCOL - 1]) ** 2).sum(-1).argmin(1) + 1
        lab[smask[y0:y1, x0:x1]] = NCOL
        new[idx] = (lab, int(x0 - w // 2), int(y0 - up), int(round(s * 16 * 256)))
        idx += 1
out = bytearray(); ntab = []
for i, (w, h, ox, oy, upx, off) in enumerate(table):
    if i in new:
        lab, ox, oy, upx = new[i]; h, w = lab.shape; data = lab.T.tobytes()
    else:
        data = blob[off:off + w * h]
    noff = len(out); out += data
    while len(out) % 4: out.append(0)
    ntab.append((w, h, ox, oy, upx, noff))
open(bfile, 'wb').write(out)
head, rest = csrc.split('const SprFrame spr_frames', 1)
tail = rest.split('};', 1)[1]
open(cfile, 'w').write(head + '\n'.join(['const SprFrame spr_frames[%d] = {' % len(ntab)] + ['  {%d,%d,%d,%d,%d,%d},' % t for t in ntab]) + '\n};' + tail)
print('patched', len(new))
