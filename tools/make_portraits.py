# Cutscene portraits: original SNES-style pixel art, one per speaking character.
# Drawn at 32x32 (soft, rounded shapes rasterised 8x and reduced by majority vote, so every pixel
# is clean), shaded with simple 2-3 tone ramps (light from the upper left), outlined, and doubled
# to 64x64. Each character has their own face shape, eyes, brows, mouth, hair and clothes.
# Written for the GBA as an 8bpp 64x64 sprite (OBJ palette entries 96..255).
# Usage: make_portraits.py src/assets.c src/portraits.c [preview.png]
import sys, re, numpy as np
from PIL import Image, ImageDraw

S, K = 32, 8                      # art size, supersampling
OUT = 64                          # displayed size (2x)

class Art:
    def __init__(self):
        self.im = Image.new('L', (S * K, S * K), 0)
        self.d = ImageDraw.Draw(self.im)
        self.names = ['bg']
        self.over = []
    def lab(self, n):
        if n not in self.names: self.names.append(n)
        return self.names.index(n)
    def poly(self, pts, n):
        self.d.polygon([(x * K, y * K) for x, y in pts], fill=self.lab(n))
    def ell(self, cx, cy, rx, ry, n):
        self.d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], fill=self.lab(n))
    def rect(self, x0, y0, x1, y1, n):
        self.d.rectangle([x0 * K, y0 * K, x1 * K - 1, y1 * K - 1], fill=self.lab(n))
    def px(self, x, y, n): self.over.append((x, y, n))
    def pxs(self, pts, n):
        for x, y in pts: self.px(x, y, n)
    def labels(self):
        a = np.array(self.im).reshape(S, K, S, K).transpose(0, 2, 1, 3).reshape(S, S, K * K)
        out = np.zeros((S, S), np.uint8)
        for y in range(S):
            for x in range(S):
                v, c = np.unique(a[y, x], return_counts=True)
                out[y, x] = v[c.argmax()]
        for x, y, n in self.over: out[y, x] = self.lab(n)
        return out

# ------------------------------------------------------------------ building blocks
def T(pts, dy=3.4): return [(x, y + dy) for x, y in pts]       # clothes sit below the bigger head

def body(a, kind):
    a.poly(T([(0, 32), (0.5, 28.5), (5, 25.8), (11.5, 24.6), (16, 25.4), (20.5, 24.6), (27, 25.8), (31.5, 28.5), (32, 32)]), 'cloth')
    if kind in ('jacket', 'suit', 'coat'):
        a.poly(T([(12.5, 24.6), (16, 26.2), (19.5, 24.6), (18.2, 32), (13.8, 32)]), 'shirt')
        a.poly(T([(11.2, 24.6), (12.8, 24.6), (15.4, 29.5), (14.2, 32), (12.4, 32), (10.6, 27.5)]), 'lapel')
        a.poly(T([(20.8, 24.6), (19.2, 24.6), (16.6, 29.5), (17.8, 32), (19.6, 32), (21.4, 27.5)]), 'lapel')
    if kind == 'coat':
        a.poly(T([(9.5, 24.2), (12.8, 24.6), (15, 29), (12.5, 32), (9, 28)]), 'lapel')
        a.poly(T([(22.5, 24.2), (19.2, 24.6), (17, 29), (19.5, 32), (23, 28)]), 'lapel')
    if kind in ('suit', 'coat'):
        a.poly(T([(15.3, 26.2), (16.7, 26.2), (17.3, 31), (16, 32), (14.7, 31)]), 'tie')
    if kind == 'hoodie':
        a.poly(T([(9.5, 25.6), (12.5, 23.8), (16, 25.4), (19.5, 23.8), (22.5, 25.6), (20, 27.6), (16, 28.2), (12, 27.6)]), 'hood')
        a.pxs([(14, 31), (18, 31)], 'string')
    if kind == 'tee':
        a.poly(T([(11.6, 24.8), (16, 26.6), (20.4, 24.8), (20, 32), (12, 32)]), 'shirt')
    if kind == 'overalls':
        a.poly(T([(0, 32), (0.5, 28.5), (5, 25.8), (11.5, 24.6), (16, 25.4), (20.5, 24.6), (27, 25.8), (31.5, 28.5), (32, 32)]), 'shirt')
        a.rect(10, 28.4, 12.6, 32, 'cloth'); a.rect(19.4, 28.4, 22, 32, 'cloth')
        a.px(11, 30, 'gold'); a.px(20, 30, 'gold')

def head(a, fw=1.0, jaw=1.0, chin=0.0, neck=1.0):
    a.rect(16 - 3.4 * neck, 22, 16 + 3.4 * neck, 30, 'skin')                            # neck
    a.ell(16, 15.4, 8.6 * fw, 10.0, 'skin')                                              # cranium + face
    a.ell(16, 20.6 + chin * 0.5, 6.6 * jaw, 5.3 + chin * 0.5, 'skin')                    # jaw, rounded chin
    a.ell(16 - 8.3 * fw, 17.2, 1.4, 2.2, 'skin'); a.ell(16 + 8.3 * fw, 17.2, 1.4, 2.2, 'skin')   # ears

EYES = {    # 4 wide (the eye on our left; the other is its mirror). k lash, w white, I iris, i glint, s lid shade
    'normal': ["kkkk", "wiIw", "wIIw"],
    'narrow': ["kkkk", ".iI."],
    'soft':   ["kkkk", "wiIw", "wIIw"],
    'tired':  ["ssss", "kkkk", "wiIw"],
    'round':  ["kkkk", "wiIw", "wIIw", ".ww."],
}
def face(a, eyes='normal', brows='normal', mouth='flat', lashes=False):
    ey = 17                                                     # the row with the whites and irises
    rows = EYES[eyes]
    lash_row = rows.index("kkkk")
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            if ch == '.': continue
            n = {'k': 'lash', 'w': 'white', 'I': 'iris', 'i': 'shine', 's': 'skin_sh'}[ch]
            y = ey - 1 + (j - lash_row)
            a.px(10 + i, y, n)
            a.px(21 - i, y, {'i': 'iris', 'I': 'shine'}.get(ch, n))   # mirrored eye keeps its glint on the left too
    if lashes: a.px(9, ey - 1, 'lash'); a.px(22, ey - 1, 'lash')
    by = ey - 3
    B = {'normal':  [(10, by), (11, by), (12, by), (13, by)],
         'angry':   [(10, by - 1), (11, by - 1), (12, by), (13, by)],
         'worried': [(10, by), (11, by), (12, by - 1), (13, by - 1)],
         'arched':  [(10, by), (11, by - 1), (12, by - 1), (13, by)],
         'heavy':   [(10, by), (11, by), (12, by), (13, by), (11, by - 1), (12, by - 1)]}[brows]
    for x, y in B: a.px(x, y, 'brow'); a.px(31 - x, y, 'brow')
    a.pxs([(17, 19), (16, 20), (17, 20)], 'skin_sh')                      # nose
    my = 22
    M = {'flat':  [(14, my), (15, my), (16, my), (17, my)],
         'smile': [(13, my - 1), (14, my), (15, my), (16, my), (17, my), (18, my - 1)],
         'smirk': [(14, my), (15, my), (16, my), (17, my - 1)],
         'frown': [(13, my + 1), (14, my), (15, my), (16, my), (17, my), (18, my + 1)],
         'open':  [(14, my), (15, my), (16, my), (17, my), (15, my + 1), (16, my + 1)]}[mouth]
    a.pxs(M, 'mouth')

# ------------------------------------------------------------------ the cast (all original)
def eddie():
    a = Art(); body(a, 'jacket'); head(a)
    a.poly([(7.4, 17), (7.2, 11), (9, 6.5), (13, 4), (16, 3.6), (20, 4), (23, 6.5), (24.8, 11), (24.6, 17), (23.8, 17.4), (22.4, 13.2),
            (21.2, 10.8), (19.8, 12.8), (18.4, 10.6), (17, 12.4), (15.6, 10.4), (14.2, 12.6), (12.8, 10.6), (11.4, 12.8), (10, 10.8),
            (9.4, 13.2), (8.2, 17.4)], 'hair')                                                       # short, spiky black hair
    a.poly([(10.4, 6.6), (14, 4.8), (13, 7.2)], 'hair_hi'); a.poly([(14.6, 4.8), (18, 5), (15.6, 6.6)], 'hair_hi')
    face(a, 'normal', 'angry', 'flat')
    return a, dict(skin=(0.93, 0.74, 0.60), hair=(0.16, 0.16, 0.22), iris=(0.36, 0.22, 0.12), cloth=(0.18, 0.19, 0.24),
                   shirt=(0.70, 0.18, 0.16), bg=(0.25, 0.32, 0.46))

def marta():
    a = Art()
    a.poly([(6, 14), (7.5, 7), (12, 4), (16, 3.6), (20, 4), (24.5, 7), (26, 14), (27, 30), (23, 32), (9, 32), (5, 30)], 'hair')   # long hair behind
    body(a, 'overalls'); head(a, fw=0.95, jaw=0.92, chin=-0.4, neck=0.85)
    a.poly([(7.4, 15), (7.6, 9), (10, 5.5), (16, 4), (22, 5.5), (24.4, 9), (24.6, 15), (23, 12.5), (20, 11.2), (16, 10.8), (12.5, 11.6), (9.6, 13.4)], 'hair')
    a.poly([(7.6, 14), (9.8, 12.6), (9.6, 23), (7.2, 25.5)], 'hair'); a.poly([(24.4, 14), (22.2, 12.6), (22.4, 23), (24.8, 25.5)], 'hair')
    a.poly([(7.2, 10.6), (9.6, 6.4), (16, 4.8), (22.4, 6.4), (24.8, 10.6), (24, 12), (16, 9.2), (8, 12)], 'band')   # red bandana
    a.pxs([(11, 8), (16, 6), (21, 8)], 'dot')
    face(a, 'soft', 'arched', 'smirk', lashes=True)
    a.px(21, 21, 'smudge'); a.px(22, 21, 'smudge')
    return a, dict(skin=(0.82, 0.60, 0.44), hair=(0.34, 0.18, 0.11), iris=(0.48, 0.28, 0.10), cloth=(0.70, 0.18, 0.15),
                   shirt=(0.93, 0.90, 0.82), band=(0.86, 0.20, 0.16), lips=True, bg=(0.50, 0.34, 0.24))

def jinx():
    a = Art(); body(a, 'hoodie'); head(a, fw=0.94, jaw=0.9, chin=-0.5, neck=0.85)
    a.poly([(7.4, 15), (7.4, 9), (10, 5.4), (16, 4), (22, 5.4), (24.6, 9), (24.6, 13), (23, 11), (18, 9.8), (13, 10.2), (9.5, 12)], 'hair')
    a.rect(23.2, 11, 24.8, 19, 'hair_sh')                                                                   # shaved side
    face(a, 'soft', 'normal', 'smirk', lashes=True)
    a.poly([(7.2, 10), (18, 8.6), (20.5, 10.4), (16, 12.6), (14, 17.6), (11.6, 20.8), (7.6, 20)], 'hair')   # long fringe over one eye
    a.poly([(10.6, 7), (16, 5), (13.6, 8)], 'hair_hi')
    a.px(7, 21, 'gold'); a.px(24, 21, 'gold'); a.px(15, 23, 'gold')                                        # earrings, lip ring
    return a, dict(skin=(0.96, 0.82, 0.72), hair=(0.60, 0.30, 0.78), iris=(0.18, 0.55, 0.32), cloth=(0.24, 0.52, 0.32),
                   hood=(0.18, 0.40, 0.25), lips=True, bg=(0.38, 0.26, 0.50))

def moran():
    a = Art(); body(a, 'suit'); head(a, fw=1.05, jaw=1.16, chin=0.6, neck=1.15)
    a.poly([(7.4, 16.4), (7.2, 10), (9.4, 6), (16, 4.2), (22.6, 6), (24.8, 10), (24.6, 16.4), (23.2, 16), (22.6, 11.2), (19, 8.4),
            (16, 8), (13, 8.4), (9.4, 11.2), (8.8, 16)], 'hair')                                      # slicked back, receding
    a.poly([(10.6, 6.6), (16, 5), (14, 6.8)], 'hair_hi')
    face(a, 'narrow', 'heavy', 'frown')
    a.pxs([(13, 11), (14, 11), (17, 11), (18, 11), (14, 9), (15, 9), (16, 9)], 'skin_sh')                 # forehead lines
    a.pxs([(12, 21), (19, 21)], 'skin_sh')                                                                  # lines by the mouth
    a.pxs([(21, 15), (21, 16), (22, 17)], 'scar')
    return a, dict(skin=(0.90, 0.74, 0.62), hair=(0.70, 0.70, 0.74), brow=(0.44, 0.44, 0.48), iris=(0.32, 0.34, 0.40),
                   cloth=(0.12, 0.12, 0.15), shirt=(0.92, 0.92, 0.90), tie=(0.74, 0.12, 0.12), bg=(0.46, 0.14, 0.14))

def shaw():
    a = Art(); body(a, 'coat'); head(a, fw=1.0, jaw=1.06, chin=0.4)
    a.poly([(7.4, 17), (7.2, 10.5), (9, 6.4), (13, 4.2), (16, 3.8), (20, 4.2), (23, 6.4), (24.8, 10.5), (24.6, 17), (23.6, 17.2), (23, 12.8),
            (20.5, 11.6), (17, 13.4), (13.6, 11.2), (10.4, 12.4), (9.2, 17.2)], 'hair')              # side part, a lock falling forward
    a.pxs([(12, 6), (12, 7), (12, 8)], 'hair_sh')
    a.poly([(13.4, 6), (18, 4.8), (15.4, 7)], 'hair_hi')
    face(a, 'tired', 'normal', 'flat')
    a.pxs([(13, 21), (14, 21), (15, 21), (16, 21), (17, 21), (18, 21), (12, 22), (19, 22)], 'stache')     # moustache
    a.pxs([(11, 22), (20, 23), (12, 24), (19, 24)], 'skin_sh')                                              # stubble
    a.px(5, 31, 'gold'); a.px(6, 31, 'gold')                                                                # badge
    return a, dict(skin=(0.86, 0.66, 0.52), hair=(0.44, 0.29, 0.17), iris=(0.34, 0.26, 0.16), cloth=(0.66, 0.56, 0.40),
                   shirt=(0.84, 0.86, 0.88), tie=(0.18, 0.22, 0.44), bg=(0.20, 0.27, 0.42))

def rudy():
    a = Art(); body(a, 'tee'); head(a, fw=1.0, jaw=1.0, chin=0.2)
    a.poly([(7.2, 13.4), (7.8, 8), (11, 5), (16, 4), (21, 5), (24.2, 8), (24.8, 13.4)], 'cap')           # cap worn backwards
    a.rect(7.2, 11.6, 24.8, 13.6, 'cap_band')
    a.poly([(21, 7.6), (26.5, 6.4), (29, 7.8), (28.2, 9.6), (22.6, 9.8)], 'bill')                           # visor poking out behind
    a.pxs([(8, 14), (8, 15), (23, 14), (23, 15)], 'hair')                                                   # short hair at the temples
    a.poly([(10.6, 7.6), (15.4, 5.4), (13.4, 8.2)], 'cap_hi')
    face(a, 'round', 'worried', 'open')
    a.pxs([(15, 24), (16, 24), (15, 25), (16, 25)], 'hair')                                                 # small goatee
    return a, dict(skin=(0.58, 0.40, 0.29), hair=(0.12, 0.10, 0.09), iris=(0.24, 0.16, 0.10), cloth=(0.44, 0.46, 0.50),
                   shirt=(0.95, 0.95, 0.93), cap=(0.93, 0.93, 0.91), bg=(0.16, 0.40, 0.42))

CAST = [('TARAS', eddie), ('MARTA', marta), ('JINX', jinx), ('MORAN', moran), ('SHAW', shaw), ('RUDY', rudy)]

# ------------------------------------------------------------------ colour and shading
def mul(c, k): return tuple(float(np.clip(v * m, 0, 1)) for v, m in zip(c, k))
def ramp(c):            # shadow (cooler), base, light (warmer)
    c = tuple(c)
    return (mul(c, (0.70, 0.66, 0.80)), c, tuple(float(min(1, v * 1.12 + w)) for v, w in zip(c, (0.06, 0.05, 0.02))))

SHADED = ('skin', 'hair', 'cloth', 'shirt', 'lapel', 'tie', 'band', 'hood', 'cap', 'stache')

def finish(a, c):
    lab = a.labels()
    names = a.names
    sk = c['skin']; hr = c['hair']; cl = c.get('cloth', (0.3, 0.3, 0.3))
    base = {'skin': sk, 'hair': hr, 'cloth': cl, 'shirt': c.get('shirt', (0.9, 0.9, 0.9)),
            'lapel': mul(cl, (1.18, 1.18, 1.18)), 'tie': c.get('tie', (0.6, 0.1, 0.1)), 'band': c.get('band', (0.8, 0.2, 0.15)),
            'hood': c.get('hood', mul(cl, (0.8, 0.8, 0.8))), 'cap': c.get('cap', (0.9, 0.9, 0.9)), 'stache': mul(hr, (0.95, 0.95, 0.95))}
    flat = {'white': (0.97, 0.96, 0.93), 'iris': c['iris'], 'shine': (1.0, 1.0, 1.0), 'lash': mul(hr, (0.35, 0.35, 0.40)) if max(hr) > 0.3 else (0.08, 0.06, 0.07),
            'brow': c.get('brow', mul(hr, (0.75, 0.75, 0.80))), 'mouth': mul(sk, (0.50, 0.34, 0.36)), 'gold': (0.98, 0.84, 0.32),
            'string': (0.92, 0.92, 0.88), 'dot': (0.98, 0.95, 0.90), 'smudge': mul(sk, (0.6, 0.55, 0.55)), 'scar': mul(sk, (0.82, 0.62, 0.62)),
            'cap_band': mul(c.get('cap', (0.9, 0.9, 0.9)), (0.72, 0.74, 0.84)), 'bill': (0.20, 0.20, 0.24),
            'hair_hi': ramp(hr)[2], 'cap_hi': (1.0, 1.0, 1.0)}
    img = np.zeros((S, S, 3), np.float32)
    bg = np.array(c['bg'], np.float32)
    yy, xx = np.mgrid[0:S, 0:S]
    img[:] = bg
    img[yy >= 11] = bg * 0.86; img[yy >= 22] = bg * 0.74                         # three soft bands
    fig = lab != 0
    for i, n in enumerate(names):
        if i == 0: continue
        m = lab == i
        b = n.split('_')[0]
        if n in flat: img[m] = flat[n]; continue
        if b in base:
            r = ramp(base[b])
            img[m] = r[0] if n.endswith('_sh') else r[1]
    # light from the upper left: the lower-right rim of each shaded part falls into shadow,
    # and its upper-left edge catches a little light
    FEATURES = ('lash', 'white', 'iris', 'shine', 'brow', 'mouth', 'skin_sh', 'scar', 'smudge', 'gold', 'stache')
    for b in SHADED:
        ids = [i for i, n in enumerate(names) if n == b]
        if not ids: continue
        m = np.isin(lab, ids)
        # the part's full extent: for skin that includes the features painted on it (so no shade
        # speckles around eyes and mouth); for hair, its highlights
        ext_names = [n for n in names if n == b or n.startswith(b + '_') or (b == 'skin' and n in FEATURES)]
        ext = np.isin(lab, [names.index(n) for n in ext_names])
        r = ramp(base[b])
        same = lambda dx, dy: np.roll(np.roll(ext, -dy, 0), -dx, 1)
        rim = m & ~(same(2, 1) & same(1, 2)) if b != 'skin' else m & ~same(2, 1)
        img[rim] = r[0]
        lit = m & ~same(-1, -1) & ~rim
        if b in ('hair', 'cap', 'cloth', 'band'): img[lit] = r[2]
    sm = np.isin(lab, [i for i, n in enumerate(names) if n == 'skin'])
    img[sm & (np.abs(xx - 11.5) <= 1) & (yy == 20)] = ramp(sk)[2]                 # light on the near cheek
    if c.get('lips'): img[(lab == names.index('mouth'))] = mul(sk, (0.78, 0.40, 0.40))
    out = img.copy()                                                              # dark outline on the silhouette
    for y in range(S):
        for x in range(S):
            if not fig[y, x] or y == S - 1: continue
            if any(0 <= y + dy < S and 0 <= x + dx < S and not fig[y + dy, x + dx] for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1))):
                out[y, x] = mul(img[y, x], (0.32, 0.28, 0.36))
    return np.clip(out, 0, 1)

def rgb15(v):
    r, g, b = [min(31, int(round(x * 31))) for x in np.clip(v, 0, 1)]
    return r | (g << 5) | (b << 10)

if __name__ == '__main__':
    csrc = open(sys.argv[1]).read()
    pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
    gcols = [(i + 104, cc) for i, cc in enumerate(pal15) if i + 104 < 252] + [(252, 0x10DF), (253, 0x03FF), (254, 0x7FFF), (255, 0x0000)]
    GP = np.array([((cc & 31) / 31, ((cc >> 5) & 31) / 31, ((cc >> 10) & 31) / 31) for _, cc in gcols], np.float32)
    GI = np.array([i for i, _ in gcols], np.uint8)
    def lab_(v): return np.sqrt(np.clip(v, 0, 1)) * np.array((1.0, 1.3, 0.8), np.float32)
    out = ['/* generated by tools/make_portraits.py: SNES-style cutscene portraits (32x32 art shown at 64x64) */',
           '#include "engine.h"', '#include "logo.h"', '#include "portraits.h"',
           '/* hardware: 8bpp tiles (64 tiles, row-major, 1D mapping) and up to 160 colours each,',
           '   for OBJ palette entries 96..255 */']
    pals, tiles, hosts, prev = [], [], [], []
    for name, fn in CAST:
        a, c = fn()
        img = np.kron(finish(a, c), np.ones((OUT // S, OUT // S, 1)))
        prev.append(img)
        q15 = np.array([[rgb15(img[y, x]) for x in range(OUT)] for y in range(OUT)])
        uniq = sorted(set(q15.reshape(-1).tolist()))
        if len(uniq) > 160: raise SystemExit('%s: %d colours (max 160)' % (name, len(uniq)))
        idx = {v: 96 + k for k, v in enumerate(uniq)}
        pals.append(uniq + [0] * (160 - len(uniq)))
        px = np.vectorize(idx.get)(q15).astype(np.uint8)
        words = []
        for ty in range(8):
            for tx in range(8):
                for y in range(8):
                    row = px[ty * 8 + y, tx * 8: tx * 8 + 8]
                    words.append(int(row[0]) | int(row[1]) << 8 | int(row[2]) << 16 | int(row[3]) << 24)
                    words.append(int(row[4]) | int(row[5]) << 8 | int(row[6]) << 16 | int(row[7]) << 24)
        tiles.append(words)
        L = lab_(img.reshape(-1, 3))
        hosts.append(GI[((L[:, None, :] - lab_(GP)[None]) ** 2).sum(-1).argmin(1)].reshape(OUT, OUT))
        print('%-6s %3d colours' % (name, len(uniq)))
    out.append('const u16 face_pal[NFACES][160] = {')
    for p_ in pals: out.append('  { ' + ','.join(str(v) for v in p_) + ' },')
    out.append('};')
    out.append('const u32 face_tiles[NFACES][64 * 16] = {')
    for w in tiles:
        out.append('  {')
        for i in range(0, len(w), 8): out.append('    ' + ','.join('0x%08X' % v for v in w[i:i + 8]) + ',')
        out.append('  },')
    out.append('};')
    out.append('#ifdef HOST   /* the PC build draws them into the framebuffer with the game palette */')
    for (name, _), idx_ in zip(CAST, hosts):
        out.append('static const u8 face_%s_px[%d] = {' % (name.lower(), OUT * OUT))
        flat_ = idx_.reshape(-1)
        for i in range(0, len(flat_), 64): out.append('  ' + ','.join(str(v) for v in flat_[i:i + 64]) + ',')
        out.append('};')
    out.append('const Img img_faces[NFACES] = {')
    for name, _ in CAST: out.append('  { %d, %d, face_%s_px },' % (OUT, OUT, name.lower()))
    out.append('};')
    out.append('#endif')
    out.append('const char *const face_names[NFACES] = { %s };' % ', '.join('"%s"' % n for n, _ in CAST))
    open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
    if len(sys.argv) > 3:
        im = (np.concatenate(prev, 1) * 255).astype(np.uint8)
        Image.fromarray(im).resize((im.shape[1] * 3, im.shape[0] * 3), Image.NEAREST).save(sys.argv[3])
