# Cutscene portraits: original anime-style pixel art, 64x64, one per speaking character.
# Each face is built from shapes (drawn 4x larger, then reduced by majority vote so every pixel is
# crisp), cel-shaded with 2-3 tones per material, given coloured outlines, and written for the GBA as
# an 8bpp sprite (palette entries 96..255 of the OBJ palette, uploaded when the speaker changes).
# Usage: make_portraits.py src/assets.c src/portraits.c [preview.png]
import sys, re, numpy as np
from PIL import Image, ImageDraw

S, K = 64, 4                     # final size, supersampling factor
W = S * K

def catmull(pts, n=8, closed=True):
    P = list(pts)
    out = []
    m = len(P)
    rng = range(m) if closed else range(m - 1)
    for i in rng:
        p0 = np.array(P[(i - 1) % m] if closed or i > 0 else P[0], float)
        p1 = np.array(P[i], float); p2 = np.array(P[(i + 1) % m], float)
        p3 = np.array(P[(i + 2) % m] if closed or i + 2 < m else P[-1], float)
        for t in np.linspace(0, 1, n, endpoint=False):
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    if not closed: out.append(np.array(P[-1], float))
    return [tuple(p) for p in out]

class Art:
    """A label canvas: shapes are painted with material names, later coloured."""
    def __init__(self):
        self.im = Image.new('L', (W, W), 0)
        self.d = ImageDraw.Draw(self.im)
        self.names = ['bg']
    def lab(self, name):
        if name not in self.names: self.names.append(name)
        return self.names.index(name)
    def poly(self, pts, name, smooth=True, closed=True):
        pts = catmull(pts) if smooth else pts
        self.d.polygon([(x * K, y * K) for x, y in pts], fill=self.lab(name))
    def ellipse(self, cx, cy, rx, ry, name):
        self.d.ellipse([(cx - rx) * K, (cy - ry) * K, (cx + rx) * K, (cy + ry) * K], fill=self.lab(name))
    def rect(self, x0, y0, x1, y1, name):
        self.d.rectangle([x0 * K, y0 * K, x1 * K - 1, y1 * K - 1], fill=self.lab(name))
    def line(self, pts, name, w=1.0, smooth=True):
        pts = catmull(pts, closed=False) if smooth else pts
        self.d.line([(x * K, y * K) for x, y in pts], fill=self.lab(name), width=max(1, int(round(w * K))), joint='curve')
    def reduce(self):
        a = np.array(self.im).reshape(S, K, S, K).transpose(0, 2, 1, 3).reshape(S, S, K * K)
        out = np.zeros((S, S), np.uint8)
        for y in range(S):
            for x in range(S):
                v, c = np.unique(a[y, x], return_counts=True)
                out[y, x] = v[c.argmax()]
        return out

def shade(c, k): return tuple(float(min(1.0, max(0.0, v * k))) for v in c)
def hue(c, d):   # darker and a little cooler/warmer: hue-shifted shadow like pixel artists use
    return tuple(float(min(1, max(0, v))) for v in (c[0] * d[0], c[1] * d[1], c[2] * d[2]))

# ------------------------------------------------------------------ common anatomy
def face_base(a, chin=47.0, jaw=21.0, wide=0.0):
    """neck, face and ears; light from the upper left"""
    a.poly([(27, 41), (37, 41), (38, 56), (26, 56)], 'skin', smooth=False)                          # neck
    a.poly([(26, 44), (38, 44), (38, 50), (32, 52), (26, 49)], 'skin_sh', smooth=False)             # shadow under the chin
    a.poly([(19 - wide, 18), (45 + wide, 18), (45.5 + wide, 28), (44 + wide * 0.5, 36), (32 + jaw * 0.25, chin - 2.5),
            (32, chin), (32 - jaw * 0.25, chin - 2.5), (20 - wide * 0.5, 36), (18.5 - wide, 28)], 'skin')
    a.ellipse(18.6 - wide, 31, 1.8, 3.2, 'skin'); a.ellipse(45.4 + wide, 31, 1.8, 3.2, 'skin_sh')   # ears
    # cel shadow down the far (right) side of the face and under the jaw
    a.poly([(44.5 + wide, 22), (45.5 + wide, 28), (44 + wide * 0.5, 36), (32 + jaw * 0.25, chin - 2.5), (35, chin - 3.0),
            (41.5, 36), (42.5, 28)], 'skin_sh')

def eye(a, cx, cy, side, iris='iris', style='normal', lid=0.0):
    """big anime eye. side -1 = left eye (on our left), +1 = right. style: normal, sharp, soft, tired"""
    w = 4.6 if style != 'sharp' else 4.8
    h = 3.6 if style == 'normal' else (2.7 if style == 'sharp' else (3.9 if style == 'soft' else 2.6))
    top = cy - h + lid
    # white
    a.poly([(cx - w, cy + 0.2), (cx - w * 0.6, top + 0.6), (cx + w * 0.5, top + 0.4), (cx + w, cy - 0.4 + (0.6 if style == 'sharp' else 0)),
            (cx + w * 0.6, cy + h * 0.7), (cx - w * 0.5, cy + h * 0.75)], 'eyewhite')
    # iris + pupil + highlights
    ir_x = cx - side * 0.4
    a.ellipse(ir_x, cy + 0.6, 2.6, h * 0.95 + 0.4, iris)
    a.ellipse(ir_x, cy + 1.6, 2.0, h * 0.5, iris + '_lt')                                           # lighter lower half
    a.ellipse(ir_x, cy + 0.2, 1.2, h * 0.5, 'pupil')
    a.rect(ir_x - 1.8, cy - h * 0.55, ir_x - 0.2, cy - h * 0.55 + 1.6, 'shine')
    a.rect(ir_x + 1.0, cy + h * 0.55, ir_x + 2.0, cy + h * 0.55 + 1.0, 'shine')
    # upper lash line: thick, dark, flicking out at the outer corner
    outer = cx + side * (w + 1.2)
    inner = cx - side * w
    a.line([(inner, cy + 0.2), (cx - side * w * 0.4, top - 0.1), (cx + side * w * 0.5, top - 0.2), (outer, cy - 0.8 - (0.8 if style == 'sharp' else 0))], 'lash', 1.3)
    if style == 'soft':
        a.line([(cx + side * w * 0.6, top - 0.2), (outer + side * 0.6, top - 0.8)], 'lash', 0.9)    # lashes
    # lower lash, outer part only
    a.line([(cx + side * w * 0.2, cy + h * 0.8), (cx + side * w * 0.9, cy + h * 0.5)], 'lash_lo', 0.8)

def brow(a, cx, cy, side, angle=0.0, thick=1.0, name='brow'):
    """angle > 0: inner end lowered (angry); < 0: raised (worried)"""
    inner, outer = (cx - side * 4.5, cy + angle), (cx + side * 4.5, cy - 0.3 - angle * 0.3)
    mid = (cx, cy - 1.0)
    a.line([inner, mid, outer], name, thick)

def nose(a, x=33.2, y=37.5):
    a.line([(x, y - 2.0), (x + 0.6, y)], 'skin_sh', 0.8, smooth=False)
    a.rect(x - 0.4, y, x + 1.2, y + 0.9, 'skin_dk')

def mouth(a, kind='flat', x=32.5, y=42.5, w=3.2):
    if kind == 'flat':  a.line([(x - w, y), (x + w, y + 0.2)], 'mouth', 0.9, smooth=False)
    if kind == 'smile': a.line([(x - w, y - 0.6), (x - w * 0.3, y + 0.6), (x + w * 0.4, y + 0.6), (x + w, y - 0.8)], 'mouth', 0.9)
    if kind == 'smirk': a.line([(x - w, y + 0.3), (x + w * 0.2, y + 0.4), (x + w * 1.1, y - 1.0)], 'mouth', 0.9)
    if kind == 'frown': a.line([(x - w, y + 0.8), (x - w * 0.3, y - 0.2), (x + w * 0.4, y - 0.2), (x + w, y + 0.9)], 'mouth', 0.9)
    if kind == 'open':  a.ellipse(x, y + 0.6, w * 0.6, 1.4, 'mouth'); a.ellipse(x, y + 1.3, w * 0.4, 0.6, 'tongue')

def lock(a, x0, x1, ytop, tx, ty, name, bend=0.0, shaded=True):
    """a pointed lock of hair hanging from the band [x0, x1] at ytop to the tip (tx, ty); the shadow
    side (right, away from the light) gets a darker sliver so neighbouring clumps read apart"""
    mx = (x0 + x1) / 2 + bend
    pts = [(x0, ytop), (x1, ytop), (mx + (tx - mx) * 0.4 + 1.0, (ytop + ty) / 2), (tx, ty), (mx + (tx - mx) * 0.4 - 1.0, (ytop + ty) / 2 + 0.5)]
    if shaded and name == 'hair':
        a.poly(pts, 'hair_sh')
        sh = [(x - 1.1 + (0.5 if i == 3 else 0), y - (0.6 if i == 3 else 0)) for i, (x, y) in enumerate(pts)]
        a.poly(sh, 'hair')
    else:
        a.poly(pts, name)

def strands(a, pts_list):
    for pts in pts_list: a.line(pts, 'hair_sh', 0.7)

def blush(a):
    for x in (22.0, 40.5): a.line([(x - 1.6, 36.2), (x + 1.6, 35.6)], 'blush', 0.8, smooth=False)

def torso(a, kind, **k):
    if kind == 'jacket':
        a.poly([(0, 64), (2, 57), (12, 52), (24, 50), (32, 53), (40, 50), (52, 52), (62, 57), (64, 64)], 'cloth', smooth=False)
        a.poly([(26, 50), (32, 53), (38, 50), (35, 64), (29, 64)], 'shirt', smooth=False)
        a.poly([(23, 50), (26, 50), (31, 60), (29, 64), (25, 64), (22, 55)], 'lapel', smooth=False)
        a.poly([(41, 50), (38, 50), (33, 60), (35, 64), (39, 64), (42, 55)], 'lapel_sh', smooth=False)
        a.poly([(46, 52), (52, 52), (62, 57), (64, 64), (48, 64)], 'cloth_sh', smooth=False)
    if kind == 'suit':
        torso(a, 'jacket')
        a.poly([(31, 53), (33, 53), (34, 62), (32, 64), (30, 62)], 'tie', smooth=False)
        a.poly([(30.5, 53), (33.5, 53), (33, 55), (31, 55)], 'tie_sh', smooth=False)
    if kind == 'coat':
        a.poly([(0, 64), (2, 56), (12, 51), (24, 49), (32, 53), (40, 49), (52, 51), (62, 56), (64, 64)], 'cloth', smooth=False)
        a.poly([(26, 49), (32, 53), (38, 49), (35, 64), (29, 64)], 'shirt', smooth=False)
        a.poly([(31, 54), (33.5, 54), (34.5, 63), (31.5, 64), (30, 63)], 'tie', smooth=False)
        a.poly([(20, 48), (27, 49), (30, 58), (24, 64), (18, 56)], 'lapel', smooth=False)       # big trench collar
        a.poly([(44, 48), (37, 49), (34, 58), (40, 64), (46, 56)], 'lapel_sh', smooth=False)
        a.poly([(46, 51), (52, 51), (62, 56), (64, 64), (48, 64)], 'cloth_sh', smooth=False)
    if kind == 'hoodie':
        a.poly([(0, 64), (3, 56), (14, 51), (24, 49), (32, 51), (40, 49), (50, 51), (61, 56), (64, 64)], 'cloth', smooth=False)
        a.poly([(17, 50), (24, 47), (32, 50), (40, 47), (47, 50), (42, 55), (32, 56), (22, 55)], 'cloth_sh')   # hood around the neck
        a.line([(28, 55), (27.5, 62)], 'string', 0.9, smooth=False); a.line([(36, 55), (36.5, 62)], 'string', 0.9, smooth=False)
        a.poly([(46, 52), (50, 51), (61, 56), (64, 64), (48, 64)], 'cloth_sh', smooth=False)
    if kind == 'overalls':
        a.poly([(0, 64), (3, 57), (14, 52), (25, 50), (32, 52), (39, 50), (50, 52), (61, 57), (64, 64)], 'shirt', smooth=False)   # tank top
        a.poly([(46, 52), (50, 52), (61, 57), (64, 64), (49, 64)], 'shirt_sh', smooth=False)
        a.rect(19, 51, 24, 64, 'cloth'); a.rect(40, 51, 45, 64, 'cloth_sh')                         # straps
        a.rect(19, 59, 45, 64, 'cloth'); a.rect(36, 59, 45, 64, 'cloth_sh')                         # bib
        a.rect(20, 52, 23, 54, 'button'); a.rect(41, 52, 44, 54, 'button')
    if kind == 'tee':
        a.poly([(0, 64), (3, 56), (13, 52), (24, 50), (32, 52), (40, 50), (51, 52), (61, 56), (64, 64)], 'cloth', smooth=False)   # open jacket
        a.poly([(23, 50), (32, 53), (41, 50), (40, 64), (24, 64)], 'shirt', smooth=False)
        a.poly([(35, 53), (41, 50), (40, 64), (36, 64)], 'shirt_sh', smooth=False)
        a.line([(26, 51.5), (32, 54), (38, 51.5)], 'shirt_sh', 0.8)
        a.poly([(46, 52), (51, 52), (61, 56), (64, 64), (48, 64)], 'cloth_sh', smooth=False)

# ------------------------------------------------------------------ the cast (all original)
def eddie():
    a = Art()
    a.poly([(15, 14), (22, 6), (33, 3), (44, 6), (50, 15), (49, 30), (15, 30)], 'hair_sh')        # back mass
    torso(a, 'jacket'); face_base(a)
    a.poly([(16, 24), (16, 13), (23, 6), (33, 4), (43, 7), (48, 14), (48, 24), (44, 18), (32, 15), (20, 18)], 'hair')
    a.poly([(20, 20), (44, 20), (43, 23.5), (21, 23.5)], 'skin_sh', smooth=False)                  # fringe shadow on the forehead
    for x0, x1, tx, ty in ((16, 22, 16.5, 30), (41, 48, 47.5, 30)): lock(a, x0, x1, 13, tx, ty, 'hair')   # side locks
    for x0, x1, tx, ty in ((19, 26, 21, 27), (24, 31, 26, 25.5), (29, 36, 32, 27), (34, 41, 37, 25), (38, 45, 43, 27.5)):
        lock(a, x0, x1, 12, tx, ty, 'hair')
    strands(a, [[(24, 8), (22, 12), (21, 15)], [(33, 6), (32, 10), (30, 14)], [(40, 8), (42, 12), (43, 15)]])
    a.line([(21, 9.5), (27, 7), (34, 7.2)], 'hair_hi', 1.4)                                         # shine band
    brow(a, 25.5, 26.5, -1, 0.8, 1.1); brow(a, 38.5, 26.5, 1, 0.8, 1.1)
    eye(a, 25.5, 31, -1, style='sharp'); eye(a, 38.5, 31, 1, style='sharp')
    nose(a); mouth(a, 'flat')
    return a, dict(skin=(0.96, 0.80, 0.68), hair=(0.12, 0.12, 0.16), iris=(0.45, 0.28, 0.14), cloth=(0.17, 0.18, 0.22),
                   shirt=(0.72, 0.18, 0.16), bg=((0.24, 0.30, 0.42), (0.10, 0.12, 0.20)))

def marta():
    a = Art()
    a.poly([(13, 18), (18, 8), (32, 3), (46, 8), (51, 18), (53, 52), (46, 58), (18, 58), (11, 52)], 'hair_sh')   # long hair behind
    torso(a, 'overalls'); face_base(a, chin=46.5, jaw=19)
    a.poly([(15, 30), (15, 15), (22, 7), (32, 4.5), (42, 7), (49, 15), (49, 30), (45, 19), (32, 16), (19, 19)], 'hair')
    for x0, x1, tx, ty in ((16, 24, 17, 34), (22, 30, 25, 25), (28, 36, 31, 24), (34, 42, 39, 26), (40, 48, 47, 34)):
        lock(a, x0, x1, 14, tx, ty, 'hair')
    strands(a, [[(14, 34), (13.5, 42), (14, 50)], [(50, 34), (50.5, 42), (50, 50)]])
    a.poly([(14, 30), (18, 30), (19, 48), (15, 52), (12, 46)], 'hair')                              # side locks framing the face
    a.poly([(50, 30), (46, 30), (45, 48), (49, 52), (52, 46)], 'hair_sh')
    a.poly([(14, 14), (20, 8), (32, 5.5), (44, 8), (50, 14), (49, 17), (32, 12), (15, 17)], 'band')   # red bandana
    a.poly([(42, 9), (50, 14), (49, 17), (40, 12)], 'band_sh')
    for x, y in ((22, 10), (29, 8.5), (36, 8.5), (43, 11)): a.rect(x, y, x + 1, y + 1, 'dot')
    a.line([(20, 19), (27, 16.5)], 'hair_hi', 1.2)
    a.poly([(21, 20), (43, 20), (42, 22.5), (22, 22.5)], 'skin_sh', smooth=False)
    brow(a, 25.5, 26.6, -1, -0.2, 0.8); brow(a, 38.5, 26.6, 1, -0.2, 0.8)
    eye(a, 25.5, 31.2, -1, style='soft'); eye(a, 38.5, 31.2, 1, style='soft')
    nose(a); mouth(a, 'smirk', w=3.0); blush(a)
    a.rect(41, 37, 44, 38, 'smudge')
    return a, dict(skin=(0.86, 0.64, 0.48), hair=(0.30, 0.16, 0.10), iris=(0.70, 0.42, 0.12), cloth=(0.66, 0.15, 0.13),
                   shirt=(0.92, 0.88, 0.80), band=(0.82, 0.16, 0.14), bg=((0.48, 0.30, 0.22), (0.20, 0.10, 0.08)))

def jinx():
    a = Art()
    a.poly([(15, 16), (21, 7), (33, 4), (45, 8), (50, 17), (49, 38), (15, 38)], 'hair_sh')
    torso(a, 'hoodie'); face_base(a, chin=46.5, jaw=19)
    a.poly([(15, 34), (15, 15), (22, 7), (33, 5), (44, 8), (49, 16), (49, 24), (44, 18), (32, 15), (19, 19)], 'hair')
    # long swept fringe over the left eye side, short on the right
    a.poly([(16, 14), (33, 12), (40, 15), (34, 18), (27, 24), (21, 30), (17, 36), (15, 28)], 'hair')
    a.poly([(38, 14), (47, 16), (48, 24), (45, 22), (41, 19)], 'hair')
    a.rect(46, 20, 49, 30, 'hair_dk')                                                               # shaved side
    strands(a, [[(30, 6), (27, 10), (24, 14)], [(38, 7), (37, 11), (35, 14)], [(44, 10), (45, 14)]])
    a.line([(19, 11), (27, 7.5), (34, 7.5)], 'hair_hi', 1.4)
    a.poly([(28, 20), (44, 20), (43, 22.5), (27, 23)], 'skin_sh', smooth=False)
    brow(a, 38.5, 26.4, 1, 0.3, 0.8)
    eye(a, 25.5, 31.3, -1, iris='iris', style='soft'); eye(a, 38.5, 31.3, 1, style='soft')
    a.poly([(16, 14), (33, 12), (36, 15), (28, 22), (23, 27.5), (20, 31), (17, 34), (15, 26)], 'hair')   # fringe over the eye
    a.line([(20, 27), (28, 18)], 'hair_sh', 0.7); a.line([(23, 25), (31, 17)], 'hair_hi', 0.8)
    nose(a); mouth(a, 'smirk', w=2.8); blush(a)
    a.rect(17, 34, 18, 36, 'gold'); a.rect(45, 34, 46, 36, 'gold')                                   # earrings
    a.rect(29, 41, 30, 42, 'gold')                                                                  # lip ring
    return a, dict(skin=(0.98, 0.84, 0.74), hair=(0.62, 0.28, 0.74), iris=(0.20, 0.62, 0.36), cloth=(0.22, 0.50, 0.30),
                   bg=((0.36, 0.22, 0.46), (0.14, 0.08, 0.20)))

def moran():
    a = Art()
    a.poly([(15, 18), (20, 9), (32, 6), (44, 9), (49, 18), (49, 30), (15, 30)], 'hair_sh')
    torso(a, 'suit'); face_base(a, chin=48.5, jaw=24, wide=0.6)
    # slicked back, receding: no fringe, high forehead
    a.poly([(16, 26), (16, 15), (21, 9), (32, 6.5), (43, 9), (48, 15), (48, 26), (46, 18), (40, 13), (32, 12), (24, 13), (18, 18)], 'hair')
    for y in (9, 11.5): a.line([(21, y + 1), (32, y - 1.5), (43, y + 1)], 'hair_sh', 0.7)            # combed lines
    a.line([(22, 8.5), (30, 7)], 'hair_hi', 1.1)
    a.line([(26, 17), (38, 17)], 'skin_sh', 0.6, smooth=False); a.line([(27, 19), (37, 19)], 'skin_sh', 0.6, smooth=False)   # forehead lines
    brow(a, 25.5, 26.8, -1, 1.6, 1.4, 'brow'); brow(a, 38.5, 26.8, 1, 1.6, 1.4, 'brow')
    eye(a, 25.5, 31.2, -1, style='sharp', lid=0.8); eye(a, 38.5, 31.2, 1, style='sharp', lid=0.8)
    a.line([(22, 35), (25, 34.5)], 'skin_sh', 0.6, smooth=False); a.line([(42, 35), (39, 34.5)], 'skin_sh', 0.6, smooth=False)   # under-eye lines
    nose(a); mouth(a, 'frown', w=3.6)
    a.line([(27.5, 39), (28.5, 43)], 'skin_sh', 0.6, smooth=False); a.line([(37.5, 39), (36.5, 43)], 'skin_sh', 0.6, smooth=False)
    a.line([(41, 26), (43, 33)], 'scar', 0.8, smooth=False)
    return a, dict(skin=(0.92, 0.76, 0.64), hair=(0.66, 0.66, 0.70), brow=(0.42, 0.42, 0.46), iris=(0.36, 0.38, 0.44),
                   cloth=(0.10, 0.10, 0.13), shirt=(0.92, 0.92, 0.90), tie=(0.72, 0.10, 0.10), bg=((0.46, 0.12, 0.12), (0.16, 0.04, 0.05)))

def shaw():
    a = Art()
    a.poly([(15, 16), (21, 7), (33, 4.5), (45, 8), (50, 17), (49, 30), (15, 30)], 'hair_sh')
    torso(a, 'coat'); face_base(a, chin=48, jaw=23, wide=0.4)
    a.poly([(16, 25), (16, 14), (22, 7), (33, 5), (44, 8), (48, 15), (48, 25), (44, 18), (32, 15), (20, 18)], 'hair')
    # messy side part: locks falling to the right
    for x0, x1, tx, ty in ((18, 25, 19, 24), (23, 31, 28, 22), (30, 38, 37, 23), (37, 45, 45, 26)):
        lock(a, x0, x1, 12, tx, ty, 'hair', bend=1.0)
    strands(a, [[(25, 7), (23, 11)], [(35, 6), (36, 10)]])
    a.line([(24, 7.5), (33, 6.5)], 'hair_hi', 1.2)
    a.poly([(20, 19.5), (44, 19.5), (43, 22), (21, 22)], 'skin_sh', smooth=False)
    brow(a, 25.5, 26.6, -1, 0.4, 1.2); brow(a, 38.5, 26.6, 1, 0.4, 1.2)
    eye(a, 25.5, 31.4, -1, style='tired'); eye(a, 38.5, 31.4, 1, style='tired')
    nose(a)
    a.poly([(27, 40), (32.5, 39.2), (38, 40), (38.5, 42), (35, 41.2), (32.5, 41.6), (30, 41.2), (26.5, 42)], 'stache')   # moustache
    mouth(a, 'flat', y=43.2, w=2.4)
    for x, y in ((24, 42), (26, 44), (39, 43), (41, 41), (29, 46), (35, 46)): a.rect(x, y, x + 1, y + 1, 'skin_sh')   # stubble
    a.rect(16, 59, 19, 62, 'gold')                                                                  # badge
    return a, dict(skin=(0.88, 0.68, 0.54), hair=(0.40, 0.26, 0.15), iris=(0.38, 0.30, 0.18), cloth=(0.62, 0.52, 0.36),
                   shirt=(0.82, 0.84, 0.86), tie=(0.16, 0.20, 0.40), bg=((0.18, 0.24, 0.40), (0.06, 0.08, 0.16)))

def rudy():
    a = Art()
    torso(a, 'tee'); face_base(a, chin=47.5, jaw=22)
    a.poly([(16, 22), (16, 13), (22, 6.5), (32, 4.5), (42, 6.5), (48, 13), (48, 22), (32, 19)], 'cap')   # cap, worn backwards
    a.poly([(16, 22), (48, 22), (48, 18), (16, 18)], 'cap_sh', smooth=False)                         # band
    a.poly([(42, 9), (49, 6.5), (55, 8.5), (55, 11), (48, 12), (44, 12)], 'bill')                    # visor sticking out behind, right side
    a.line([(44, 11.5), (54.5, 10.5)], 'bill_hi', 0.7)
    a.poly([(25, 11), (39, 11), (38, 13), (26, 13)], 'cap_sh', smooth=False)                         # strap opening
    a.line([(21, 9), (29, 6.5)], 'cap_hi', 1.2)
    a.rect(16, 22, 19, 28, 'hair'); a.rect(45, 22, 48, 28, 'hair')                                    # short hair at the temples
    brow(a, 25.5, 26.5, -1, -1.4, 1.1); brow(a, 38.5, 26.5, 1, -1.4, 1.1)                          # worried
    eye(a, 25.5, 31.2, -1, style='normal'); eye(a, 38.5, 31.2, 1, style='normal')
    nose(a); mouth(a, 'open', y=42.2, w=3.0)
    a.poly([(30.5, 45.5), (34.5, 45.5), (33.5, 47.5), (31.5, 47.5)], 'hair', smooth=False)           # small goatee
    return a, dict(skin=(0.62, 0.42, 0.30), hair=(0.12, 0.09, 0.08), iris=(0.30, 0.20, 0.12), cloth=(0.40, 0.42, 0.46),
                   shirt=(0.94, 0.94, 0.92), cap=(0.92, 0.92, 0.90), bg=((0.14, 0.38, 0.40), (0.04, 0.14, 0.16)))

CAST = [('EDDIE', eddie), ('MARTA', marta), ('JINX', jinx), ('MORAN', moran), ('SHAW', shaw), ('RUDY', rudy)]

# ------------------------------------------------------------------ colouring
def colours(c):
    sk = c['skin']; hr = c['hair']
    if max(hr) < 0.3:                 # very dark hair has no room for shading: lift it to a blue-black
        hr = (hr[0] * 0.8 + 0.12, hr[1] * 0.8 + 0.13, hr[2] * 0.8 + 0.20)
    sk_sh = hue(sk, (0.86, 0.72, 0.74)); sk_dk = hue(sk, (0.70, 0.52, 0.55))
    hr_sh = hue(hr, (0.50, 0.50, 0.62)); hr_hi = tuple(min(1, v * 1.6 + 0.22) for v in hr)
    cl = c.get('cloth', (0.2, 0.2, 0.2)); sh = c.get('shirt', (0.9, 0.9, 0.9))
    iris = c['iris']
    col = {
        'skin': sk, 'skin_sh': sk_sh, 'skin_dk': sk_dk,
        'hair': hr, 'hair_sh': hr_sh, 'hair_hi': hr_hi, 'hair_dk': hue(hr, (0.45, 0.45, 0.55)),
        'brow': c.get('brow', hue(hr, (0.6, 0.6, 0.65))), 'lash': (0.10, 0.06, 0.07), 'lash_lo': hue(sk, (0.55, 0.38, 0.40)),
        'eyewhite': (0.98, 0.97, 0.95), 'iris': hue(iris, (0.75, 0.75, 0.80)), 'iris_lt': tuple(min(1, v * 1.35 + 0.08) for v in iris),
        'pupil': hue(iris, (0.25, 0.22, 0.25)), 'shine': (1.0, 1.0, 1.0),
        'mouth': hue(sk, (0.48, 0.30, 0.32)), 'tongue': (0.86, 0.42, 0.42),
        'cloth': cl, 'cloth_sh': hue(cl, (0.68, 0.68, 0.78)), 'lapel': tuple(min(1, v * 1.25 + 0.03) for v in cl), 'lapel_sh': hue(cl, (0.55, 0.55, 0.65)),
        'shirt': sh, 'shirt_sh': hue(sh, (0.78, 0.78, 0.86)), 'tie': c.get('tie', (0.6, 0.1, 0.1)), 'tie_sh': hue(c.get('tie', (0.6, 0.1, 0.1)), (0.6, 0.6, 0.7)),
        'string': (0.92, 0.92, 0.88), 'button': (0.85, 0.80, 0.55), 'gold': (0.98, 0.82, 0.30), 'scar': hue(sk, (0.80, 0.55, 0.58)),
        'stache': hue(hr, (0.9, 0.9, 0.9)), 'smudge': hue(sk, (0.55, 0.50, 0.50)),
        'band': c.get('band', (0.8, 0.15, 0.12)), 'band_sh': hue(c.get('band', (0.8, 0.15, 0.12)), (0.65, 0.65, 0.75)), 'dot': (0.98, 0.94, 0.90),
        'cap': c.get('cap', (0.9, 0.9, 0.9)), 'cap_sh': hue(c.get('cap', (0.9, 0.9, 0.9)), (0.72, 0.74, 0.82)), 'cap_hi': (1.0, 1.0, 1.0),
        'bill': (0.18, 0.18, 0.22), 'bill_hi': (0.36, 0.36, 0.42), 'blush': (0.98, 0.58, 0.58),
    }
    return col

# outline colour for each material: a dark, saturated version of it (coloured lineart)
def line_of(c): return hue(c, (0.30, 0.24, 0.28))

def finish(a, c):
    lab = a.reduce()
    names = a.names
    col = colours(c)
    img = np.zeros((S, S, 3), np.float32)
    # background: vertical gradient with a soft diagonal light streak
    top, bot = np.array(c['bg'][0]), np.array(c['bg'][1])
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32)
    t = (yy / (S - 1))[..., None]
    img[:] = top * (1 - t) + bot * t
    streak = (np.abs((xx - yy * 0.6) - 8) < 5) | (np.abs((xx - yy * 0.6) - 18) < 2)
    img[streak] = img[streak] * 1.18 + 0.03
    fig = lab != 0
    for i, n in enumerate(names):
        if i == 0: continue
        img[lab == i] = col.get(n, (1, 0, 1))
    # outer silhouette outline (coloured by the material inside)
    out = img.copy()
    for y in range(S):
        for x in range(S):
            if not fig[y, x]: continue
            edge = False
            for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                yy2, xx2 = y + dy, x + dx
                if 0 <= yy2 < S and 0 <= xx2 < S and not fig[yy2, xx2]: edge = True
            if edge and y < S - 1: out[y, x] = line_of(img[y, x])
    # inner lines: where the face meets the hair or the neck meets the clothes
    group = {}
    for i, n in enumerate(names):
        g = n.split('_')[0]
        group[i] = 'skin' if g in ('skin',) else 'hair' if g in ('hair', 'band', 'dot', 'cap', 'bill') else g
    for y in range(1, S - 1):
        for x in range(1, S - 1):
            gi = group.get(lab[y, x], '')
            if gi != 'skin': continue
            for dy, dx in ((-1, 0), (0, -1), (0, 1), (1, 0)):
                gj = group.get(lab[y + dy, x + dx], '')
                if gj in ('hair', 'cloth', 'shirt', 'lapel', 'stache'):
                    out[y, x] = line_of(col['skin']) if gj != 'hair' else hue(col['skin_sh'], (0.75, 0.7, 0.75)); break
    return np.clip(out, 0, 1)

# ------------------------------------------------------------------ output
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
    out = ['/* generated by tools/make_portraits.py: 64x64 anime-style cutscene portraits */',
           '#include "engine.h"', '#include "logo.h"', '#include "portraits.h"',
           '/* hardware: 8bpp tiles (64 tiles, row-major, 1D mapping) and up to 160 colours each,',
           '   for OBJ palette entries 96..255 */']
    pals, tiles, hosts, prev = [], [], [], []
    for name, fn in CAST:
        a, c = fn()
        img = finish(a, c)
        prev.append(img)
        q15 = np.array([[rgb15(img[y, x]) for x in range(S)] for y in range(S)])
        uniq = sorted(set(q15.reshape(-1).tolist()))
        if len(uniq) > 160: raise SystemExit('%s: %d colours (max 160)' % (name, len(uniq)))
        idx = {v: 96 + k for k, v in enumerate(uniq)}
        pal = uniq + [0] * (160 - len(uniq))
        pals.append(pal)
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
        hosts.append(GI[((L[:, None, :] - lab_(GP)[None]) ** 2).sum(-1).argmin(1)].reshape(S, S))
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
        out.append('static const u8 face_%s_px[%d] = {' % (name.lower(), S * S))
        flat = idx_.reshape(-1)
        for i in range(0, len(flat), 64): out.append('  ' + ','.join(str(v) for v in flat[i:i + 64]) + ',')
        out.append('};')
    out.append('const Img img_faces[NFACES] = {')
    for name, _ in CAST: out.append('  { %d, %d, face_%s_px },' % (S, S, name.lower()))
    out.append('};')
    out.append('#endif')
    out.append('const char *const face_names[NFACES] = { %s };' % ', '.join('"%s"' % n for n, _ in CAST))
    open(sys.argv[2], 'w').write('\n'.join(out) + '\n')
    if len(sys.argv) > 3:
        im = (np.concatenate(prev, 1) * 255).astype(np.uint8)
        Image.fromarray(im).resize((im.shape[1] * 4, im.shape[0] * 4), Image.NEAREST).save(sys.argv[3])
