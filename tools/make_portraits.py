# Cutscene portraits: original 48x48 pixel-art busts of the six speaking characters, drawn from
# simple shapes, shaded, outlined and mapped onto the game's existing palette.
# Usage: make_portraits.py src/assets.c src/portraits.c [preview.png]
import sys, re, numpy as np
from PIL import Image, ImageDraw

S = 48
csrc = open(sys.argv[1]).read()
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
cols = [(i + 104, c) for i, c in enumerate(pal15) if i + 104 < 252]
cols += [(252, 0x10DF), (253, 0x03FF), (254, 0x7FFF), (255, 0x0000)]
P = np.array([((c & 31) / 31, ((c >> 5) & 31) / 31, ((c >> 10) & 31) / 31) for _, c in cols], np.float32)
IDX = np.array([i for i, _ in cols], np.uint8)
def lab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.3, 0.8), np.float32)
PL = lab(P)

def shade(c, k): return tuple(min(1.0, v * k) for v in c)

class Canvas:
    """Paint with named colours on a 48x48 grid; colours are RGB tuples."""
    def __init__(self, bg):
        self.rgb = np.zeros((S, S, 3), np.float32); self.rgb[:] = bg
        self.fig = np.zeros((S, S), bool)       # figure mask (for the outline)
        yy, xx = np.mgrid[0:S, 0:S]
        self.yy, self.xx = yy, xx
        # soft vignette background
        r = np.hypot(xx - 24, yy - 18) / 34
        self.rgb *= (1.15 - 0.45 * r)[..., None]
    def mask_ellipse(self, cx, cy, rx, ry):
        return ((self.xx + 0.5 - cx) / rx) ** 2 + ((self.yy + 0.5 - cy) / ry) ** 2 <= 1
    def mask_poly(self, pts):
        im = Image.new('L', (S, S), 0); ImageDraw.Draw(im).polygon(pts, fill=1)
        return np.array(im) > 0
    def mask_rect(self, x0, y0, x1, y1):
        return (self.xx >= x0) & (self.xx < x1) & (self.yy >= y0) & (self.yy < y1)
    def fill(self, m, c, fig=True):
        self.rgb[m] = c
        if fig: self.fig |= m
    def px(self, pts, c):
        for x, y in pts: self.rgb[y, x] = c

def to_idx(rgb):
    L = lab(rgb.reshape(-1, 3))
    best = ((L[:, None, :] - PL[None]) ** 2).sum(-1).argmin(1)
    return IDX[best].reshape(S, S)

def bust(c, skin, cloth, female=False, chin=13.0, neck_w=5):
    """torso, neck, head, ears; light comes from the upper left"""
    sd = shade(skin, 0.78)
    c.fill(c.mask_ellipse(24, 53, 23, 15), cloth)                              # shoulders
    c.fill(c.mask_rect(24 - neck_w, 30, 24 + neck_w, 42), skin)                # neck
    c.fill(c.mask_rect(24, 30, 24 + neck_w, 42), sd)
    c.fill(c.mask_rect(24 - neck_w, 33, 24 + neck_w, 35), shade(skin, 0.68))   # chin shadow on the neck
    head = c.mask_ellipse(24, 20, 10.5 if not female else 10, chin)
    c.fill(head, skin)
    c.fill(c.mask_ellipse(13.8, 22, 2.2, 3.2), skin)                           # ears
    c.fill(c.mask_ellipse(34.2, 22, 2.2, 3.2), sd)
    right = head & (c.xx >= 29)                                                # shaded side of the face
    c.rgb[right] = sd
    c.rgb[head & (c.xx == 28)] = shade(skin, 0.88)
    return head

def eyes(c, iris=(0.20, 0.13, 0.08), brow=(0.10, 0.07, 0.05), lashes=False, brow_y=17, worried=False, angry=False):
    for x0 in (17, 26):
        c.fill(c.mask_rect(x0, 20, x0 + 4, 22), (0.92, 0.92, 0.90))            # whites
        ix = x0 + 2 if x0 == 17 else x0 + 1
        c.fill(c.mask_rect(ix, 20, ix + 2, 22), iris)
        c.px([(ix, 20)], (0.05, 0.05, 0.06))
        if lashes: c.fill(c.mask_rect(x0 - 1, 19, x0 + 5, 20), (0.08, 0.06, 0.06))
        else: c.fill(c.mask_rect(x0, 19, x0 + 4, 20), shade(iris, 0.5) if False else (0.35, 0.25, 0.20))
    for x0, side in ((16, 0), (26, 1)):
        for k in range(5):
            dy = 0
            if worried: dy = (k - 2) * (1 if side == 0 else -1) // 2 * -1
            if angry: dy = (k - 2) * (1 if side == 0 else -1) // 2
            c.px([(x0 + k, brow_y + dy)], brow)
            c.px([(x0 + k, brow_y + 1 + dy)], brow) if k in (1, 2, 3) else None

def nose_mouth(c, skin, lip=None, frown=False, smile=False, mouth_y=29):
    sd = shade(skin, 0.70)
    c.px([(24, 23), (24, 24), (25, 25), (23, 26), (24, 26), (25, 26)], sd)     # nose ridge + nostrils
    c.px([(23, 25)], shade(skin, 1.08))
    lip = lip or shade(skin, 0.62)
    xs = range(21, 28)
    for x in xs:
        y = mouth_y
        if frown and x in (21, 27): y += 1
        if smile and x in (21, 27): y -= 1
        c.px([(x, y)], lip)
    c.px([(24, mouth_y + 2)], shade(skin, 0.85))

def outline(c, col=(0.04, 0.04, 0.05)):
    f = c.fig
    edge = np.zeros_like(f)
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
        edge |= np.roll(np.roll(f, dy, 0), dx, 1) & ~f
    edge[-1, :] = False; edge[0, :] = False; edge[:, 0] = False; edge[:, -1] = False   # no wrap-around
    c.rgb[edge] = col

# ------------------------------------------------------------------ the cast
def eddie():
    c = Canvas((0.30, 0.34, 0.42)); skin = (0.80, 0.60, 0.46); hair = (0.08, 0.07, 0.07)
    jacket = (0.15, 0.16, 0.19)
    head = bust(c, skin, jacket)
    c.fill(c.mask_poly([(19, 36), (29, 36), (24, 47)]), (0.62, 0.18, 0.15))    # red shirt in the open jacket
    c.fill(c.mask_poly([(14, 40), (19, 36), (24, 47), (12, 47)]), shade(jacket, 1.35))   # lapels
    c.fill(c.mask_poly([(34, 40), (29, 36), (24, 47), (36, 47)]), shade(jacket, 1.1))
    c.fill(c.mask_ellipse(24, 13, 11.5, 7) & ~c.mask_rect(0, 16, 48, 48), hair)  # short crop
    c.fill(c.mask_rect(13, 12, 16, 19), hair); c.fill(c.mask_rect(32, 12, 35, 19), hair)
    eyes(c, brow=hair)
    nose_mouth(c, skin)
    c.px([(x, y) for x in range(18, 31) for y in range(30, 33) if (x + y) % 2 == 0 and head[y, x] and abs(x - 24) > 2], shade(skin, 0.86))  # light stubble on the jaw
    outline(c); return c

def marta():
    c = Canvas((0.45, 0.28, 0.22)); skin = (0.74, 0.53, 0.39); hair = (0.16, 0.09, 0.06)
    overall = (0.55, 0.11, 0.10)
    c.fill(c.mask_ellipse(24, 26, 15, 20) & (c.yy > 10), hair)                 # hair falls to the shoulders
    head = bust(c, skin, overall, female=True, chin=12.5, neck_w=4)
    c.fill(c.mask_rect(20, 38, 28, 48), (0.85, 0.82, 0.75))                    # tank top under the overalls
    c.fill(c.mask_rect(16, 38, 19, 48), shade(overall, 0.8)); c.fill(c.mask_rect(29, 38, 32, 48), shade(overall, 0.8))   # straps
    band = c.mask_ellipse(24, 13, 11.5, 6) & (c.yy < 14)                       # red bandana
    c.fill(band, (0.78, 0.13, 0.10))
    c.px([(19, 11), (23, 10), (27, 12), (30, 11)], (0.95, 0.85, 0.80))         # polka dots
    c.fill(c.mask_rect(13, 14, 16, 26), hair); c.fill(c.mask_rect(32, 14, 35, 26), hair)
    eyes(c, iris=(0.25, 0.15, 0.08), brow=hair, lashes=True)
    nose_mouth(c, skin, lip=(0.62, 0.22, 0.20), smile=True)
    c.px([(30, 25), (31, 25), (31, 26)], (0.30, 0.25, 0.22))                   # grease smudge
    outline(c); return c

def jinx():
    c = Canvas((0.32, 0.22, 0.40)); skin = (0.88, 0.72, 0.60); hair = (0.55, 0.20, 0.62)
    hoodie = (0.20, 0.46, 0.26)
    c.fill(c.mask_ellipse(24, 40, 17, 9), shade(hoodie, 0.75))                 # hood bunched behind the neck
    head = bust(c, skin, hoodie, female=True, chin=12, neck_w=4)
    c.px([(21, y) for y in range(40, 47)] + [(27, y) for y in range(40, 47)], (0.85, 0.85, 0.80))   # drawstrings
    top = c.mask_ellipse(23, 12, 12, 7) & (c.yy < 17)                          # short dyed crop, swept over
    c.fill(top, hair)
    c.fill(c.mask_poly([(12, 12), (20, 9), (16, 20), (12, 22)]), hair)         # long fringe on the left
    c.fill(c.mask_rect(33, 13, 35, 18), shade(hair, 0.6))                      # shaved side
    eyes(c, iris=(0.15, 0.35, 0.20), brow=shade(hair, 0.5), lashes=True)
    nose_mouth(c, skin, lip=(0.78, 0.40, 0.45))
    c.px([(34, 25), (34, 26)], (0.95, 0.80, 0.25)); c.px([(13, 25)], (0.95, 0.80, 0.25))   # earrings
    outline(c); return c

def moran():
    c = Canvas((0.40, 0.12, 0.12)); skin = (0.84, 0.68, 0.56); hair = (0.62, 0.62, 0.65)
    suit = (0.07, 0.07, 0.09)
    head = bust(c, skin, suit, chin=13.5, neck_w=6)
    c.fill(c.mask_poly([(18, 36), (30, 36), (24, 46)]), (0.90, 0.90, 0.88))    # white shirt
    c.fill(c.mask_poly([(23, 37), (25, 37), (26, 46), (24, 47), (22, 46)]), (0.65, 0.08, 0.08))   # red tie
    c.fill(c.mask_poly([(13, 41), (18, 36), (24, 47), (11, 47)]), shade(suit, 2.0))
    c.fill(c.mask_poly([(35, 41), (30, 36), (24, 47), (37, 47)]), shade(suit, 1.6))
    slick = c.mask_ellipse(24, 12, 11.5, 6) & (c.yy < 13)                      # slicked back, receding
    c.fill(slick, hair); c.fill(c.mask_rect(13, 12, 16, 22), hair); c.fill(c.mask_rect(32, 12, 35, 22), shade(hair, 0.8))
    c.px([(x, 14) for x in range(19, 24)], shade(skin, 0.80)); c.px([(x, 15) for x in range(24, 30)], shade(skin, 0.80))   # forehead lines
    eyes(c, iris=(0.30, 0.30, 0.35), brow=(0.45, 0.45, 0.48), brow_y=17, angry=True)
    nose_mouth(c, skin, frown=True)
    c.px([(19, 26), (19, 27), (29, 26), (29, 27)], shade(skin, 0.75))          # deep lines beside the mouth
    c.px([(30, 18), (31, 19), (31, 20)], (0.65, 0.45, 0.40))                   # old scar
    outline(c); return c

def shaw():
    c = Canvas((0.16, 0.20, 0.34)); skin = (0.72, 0.52, 0.40); hair = (0.30, 0.20, 0.12)
    coat = (0.42, 0.36, 0.26)
    head = bust(c, skin, coat, neck_w=5)
    c.fill(c.mask_poly([(19, 36), (29, 36), (24, 45)]), (0.80, 0.80, 0.76))    # shirt
    c.fill(c.mask_poly([(23, 38), (26, 38), (27, 47), (22, 47)]), (0.14, 0.16, 0.30))   # loosened navy tie
    c.fill(c.mask_poly([(12, 40), (19, 35), (23, 47), (10, 47)]), shade(coat, 1.25))   # trench-coat collar
    c.fill(c.mask_poly([(36, 40), (29, 35), (25, 47), (38, 47)]), shade(coat, 1.05))
    part = c.mask_ellipse(24, 13, 11.5, 7) & (c.yy < 16)                       # side-parted hair
    c.fill(part, hair); c.px([(19, y) for y in range(8, 13)], shade(hair, 1.6))
    c.fill(c.mask_rect(13, 12, 16, 20), hair); c.fill(c.mask_rect(32, 12, 35, 20), hair)
    eyes(c, iris=(0.25, 0.18, 0.10), brow=hair)
    nose_mouth(c, skin)
    c.fill(c.mask_rect(20, 27, 29, 29), hair)                                  # thick moustache
    c.px([(19, 28), (29, 28)], hair)
    c.px([(x, y) for x in range(17, 32) for y in range(29, 33) if (x * 3 + y) % 4 == 0 and head[y, x]], shade(skin, 0.75))
    c.px([(15, 44), (16, 44), (15, 45), (16, 45)], (0.95, 0.80, 0.25))         # badge peeking out
    outline(c); return c

def rudy():
    c = Canvas((0.14, 0.34, 0.36)); skin = (0.55, 0.38, 0.27); cap = (0.86, 0.86, 0.84)
    jacket = (0.36, 0.37, 0.40)
    head = bust(c, skin, jacket, neck_w=5)
    c.fill(c.mask_poly([(18, 36), (30, 36), (31, 48), (17, 48)]), (0.88, 0.88, 0.86))   # white tee
    c.fill(c.mask_ellipse(24, 13, 11.8, 7.5) & (c.yy < 15), cap)               # cap, worn backwards
    c.fill(c.mask_rect(13, 13, 35, 15), shade(cap, 0.75))                      # band
    c.fill(c.mask_rect(31, 9, 38, 12), (0.20, 0.20, 0.24))                     # the bill sticks out behind
    c.fill(c.mask_rect(13, 15, 15, 20), (0.08, 0.06, 0.05)); c.fill(c.mask_rect(33, 15, 35, 20), (0.08, 0.06, 0.05))
    eyes(c, iris=(0.15, 0.10, 0.06), brow=(0.08, 0.06, 0.05), worried=True)
    nose_mouth(c, skin)
    c.px([(23, 32), (24, 32), (25, 32), (23, 33), (24, 33), (25, 33), (24, 34)], (0.16, 0.10, 0.07))   # small chin beard
    c.px([(x, 28) for x in range(21, 28)], (0.16, 0.10, 0.07))   # thin moustache
    outline(c); return c

CAST = [('EDDIE', eddie), ('MARTA', marta), ('JINX', jinx), ('MORAN', moran), ('SHAW', shaw), ('RUDY', rudy)]
def rgb15(c):
    r, g, b = [min(31, int(round(v * 31))) for v in np.clip(c, 0, 1)]
    return r | (g << 5) | (b << 10)
out = ['/* generated by tools/make_portraits.py: 48x48 cutscene portraits */',
       '#include "engine.h"', '#include "logo.h"', '#include "portraits.h"',
       '/* hardware: 6x6 tiles (4bpp, row-major) and a 16-colour OBJ palette each (0 = transparent) */']
host_idx, pals, tiles, prev = [], [], [], []
for name, fn in CAST:
    c = fn()
    host_idx.append(to_idx(c.rgb))
    q = Image.fromarray((np.clip(c.rgb, 0, 1) * 255).astype(np.uint8)).quantize(colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    qi = np.array(q) + 1
    qp = np.array(q.getpalette()[:45], np.float32).reshape(15, 3) / 255
    pals.append([0] + [rgb15(v) for v in qp])
    prev.append(np.concatenate([np.zeros((1, 3)), qp])[qi])
    words = []
    for ty in range(6):
        for tx in range(6):
            for y in range(8):
                row = 0
                for x in range(8): row |= int(qi[ty * 8 + y, tx * 8 + x]) << (4 * x)
                words.append(row)
    tiles.append(words)
out.append('const u16 face_pal[NFACES][16] = {')
for p_ in pals: out.append('  { ' + ','.join(str(v) for v in p_) + ' },')
out.append('};')
out.append('const u32 face_tiles[NFACES][36 * 8] = {')
for w in tiles:
    out.append('  {')
    for i in range(0, len(w), 8): out.append('    ' + ','.join('0x%08X' % v for v in w[i:i + 8]) + ',')
    out.append('  },')
out.append('};')
out.append('#ifdef HOST   /* the PC build draws them into the framebuffer with the game palette */')
for (name, _), idx in zip(CAST, host_idx):
    out.append('static const u8 face_%s_px[%d] = {' % (name.lower(), S * S))
    flat = idx.reshape(-1)
    for i in range(0, len(flat), 48): out.append('  ' + ','.join(str(v) for v in flat[i:i + 48]) + ',')
    out.append('};')
out.append('const Img img_faces[NFACES] = {')
for name, _ in CAST: out.append('  { %d, %d, face_%s_px },' % (S, S, name.lower()))
out.append('};')
out.append('#endif')
out.append('const char *const face_names[NFACES] = { %s };' % ', '.join('"%s"' % n for n, _ in CAST))
open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
if len(sys.argv) > 3:
    img = (np.concatenate(prev, 1) * 255).astype(np.uint8)
    Image.fromarray(img).resize((img.shape[1] * 4, img.shape[0] * 4), Image.NEAREST).save(sys.argv[3])
print('portraits:', ', '.join(n for n, _ in CAST))
