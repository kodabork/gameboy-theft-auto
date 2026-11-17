# v0.3 models: natural-looking people (anatomical, smooth), knocked-down pose,
# blood decals, and original tuner/sports cars in a street-racing style.
import numpy as np
from sprites_sdf import (f32, length, nrm, sd_round_box, sd_capsule, sd_sphere, sd_ycyl,
                         plane, smin, lighting, env, Car, SHADOW)

def dot(a, b): return (a * b).sum(-1)

def sd_round_cone(p, a, b, r1, r2):
    a = np.asarray(a, f32); b = np.asarray(b, f32)
    ba = b - a; l2 = float((ba * ba).sum()); rr = r1 - r2; a2 = l2 - rr * rr; il2 = 1.0 / l2
    pa = p - a; y = dot(pa, ba); z = y - l2
    q = pa * l2 - ba * y[..., None]
    x2 = dot(q, q); y2 = y * y * l2; z2 = z * z * l2
    k = np.sign(rr) * rr * rr * x2
    d3 = (np.sqrt(np.maximum(x2 * a2 * il2, 0)) + y * rr) * il2 - r1
    d1 = np.sqrt(x2 + z2) * il2 - r2
    d2 = np.sqrt(x2 + y2) * il2 - r1
    return np.where(np.sign(z) * a2 * z2 > k, d1, np.where(np.sign(y) * a2 * y2 < k, d2, d3))

def sd_ellipsoid(p, c, r):
    r = np.asarray(r, f32)
    q = (p - np.asarray(c, f32))
    k0 = length(q / r); k1 = length(q / (r * r))
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)

def rot_y(v, a):  # pitch about the y (left) axis: positive tips +x downward
    c, s = np.cos(a), np.sin(a)
    return np.array((v[0] * c + v[2] * s, v[1], -v[0] * s + v[2] * c), f32)

# ------------------------------------------------------------------ people
OUT = {
    # top, top_len ('jacket'|'coat'|'shirt'), legs, leg_kind ('pants'|'skirt'), shoes, skin, hair, hair_style, accent
    'player': dict(top=(0.13, 0.14, 0.16), top_len='jacket', legs=(0.30, 0.28, 0.24), leg='pants', shoes=(0.28, 0.17, 0.10),
                   skin=(0.68, 0.50, 0.38), hair=(0.06, 0.05, 0.05), style='short', accent=(0.60, 0.18, 0.15), female=False),
    'man_coat': dict(top=(0.33, 0.34, 0.36), top_len='coat', legs=(0.10, 0.10, 0.11), leg='pants', shoes=(0.05, 0.05, 0.05),
                     skin=(0.46, 0.32, 0.23), hair=(0.05, 0.04, 0.04), style='short', accent=(0.80, 0.80, 0.78), female=False),
    'woman_red': dict(top=(0.58, 0.10, 0.11), top_len='coat', legs=(0.10, 0.09, 0.09), leg='skirt', shoes=(0.08, 0.05, 0.05),
                      skin=(0.86, 0.69, 0.57), hair=(0.42, 0.26, 0.13), style='long', accent=(0.58, 0.10, 0.11), female=True),
    'man_hoodie': dict(top=(0.12, 0.16, 0.29), top_len='shirt', legs=(0.24, 0.30, 0.44), leg='pants', shoes=(0.85, 0.85, 0.85),
                       skin=(0.72, 0.54, 0.42), hair=(0.70, 0.56, 0.32), style='short', accent=(0.12, 0.16, 0.29), female=False),
    'woman_white': dict(top=(0.86, 0.85, 0.82), top_len='shirt', legs=(0.18, 0.22, 0.34), leg='pants', shoes=(0.12, 0.10, 0.10),
                        skin=(0.55, 0.38, 0.28), hair=(0.05, 0.04, 0.04), style='pony', accent=(0.86, 0.85, 0.82), female=True),
    'man_suit': dict(top=(0.08, 0.09, 0.11), top_len='jacket', legs=(0.08, 0.09, 0.11), leg='pants', shoes=(0.04, 0.03, 0.03),
                     skin=(0.80, 0.63, 0.52), hair=(0.30, 0.30, 0.30), style='short', accent=(0.85, 0.85, 0.88), female=False),
    'man_brown': dict(top=(0.36, 0.25, 0.16), top_len='jacket', legs=(0.38, 0.35, 0.28), leg='pants', shoes=(0.20, 0.13, 0.08),
                      skin=(0.62, 0.45, 0.33), hair=(0.18, 0.12, 0.08), style='short', accent=(0.55, 0.52, 0.46), female=False),
}

class Human:
    """Smooth anatomical figure. phase in radians (walk cycle); lying=True for knocked-down pose."""
    def __init__(self, outfit, phase=0.0, moving=True, lying=False, run=False):
        self.o = {k: (np.array(v, f32) if isinstance(v, tuple) else v) for k, v in OUT[outfit].items()}
        self.lying = lying
        fem = self.o['female']
        self.sc = 0.95 if fem else 1.0
        amp = 0.62 if run else 0.42
        s = np.sin(phase) if moving else 0.0
        bob = (0.022 * np.cos(2 * phase) - 0.01) if moving else 0.0
        hz = 0.93 + bob
        self.hz = hz
        lean = (0.12 if run else 0.04) if moving else 0.0
        self.lean = lean
        self.legs, self.arms = [], []
        hipw = 0.085 if not fem else 0.09
        for sg, off in ((1, 0.0), (-1, np.pi)):
            ph = phase + off
            th = amp * np.sin(ph) if moving else 0.03 * sg
            knee = (0.08 + (1.0 if run else 0.75) * max(0.0, np.cos(ph)) ** 1.5) if moving else 0.03
            hip = np.array((0, sg * hipw, hz), f32)
            kn = hip + np.array((np.sin(th), 0, -np.cos(th)), f32) * 0.45
            sh = th - knee
            an = kn + np.array((np.sin(sh), 0, -np.cos(sh)), f32) * 0.43
            foot_pitch = -min(0.0, sh) * 0.3 + (0.25 * max(0.0, -np.sin(ph)) if moving else 0)
            self.legs.append((hip, kn, an, foot_pitch))
            asw = -(0.65 if run else 0.38) * np.sin(ph) if moving else 0.04
            shp = np.array((0.0, sg * (0.185 if not fem else 0.165), hz + 0.47), f32)
            el = shp + np.array((np.sin(asw), sg * 0.03, -np.cos(asw)), f32) * 0.28
            fa = asw + (0.9 if run else 0.25 + 0.2 * max(0.0, np.sin(-ph)))
            wr = el + np.array((np.sin(fa), sg * 0.01, -np.cos(fa)), f32) * 0.25
            self.arms.append((shp, el, wr))

    def _xf(self, p):
        if self.lying:   # on the back, head toward +x, face up
            return np.stack([p[..., 2] - 0.12, p[..., 1], p[..., 0] + 0.9], -1)
        if self.lean:
            c, s = np.cos(self.lean), np.sin(self.lean)
            z = p[..., 2]
            # lean the upper body forward about the hips
            up = z > self.hz
            x2 = np.where(up, (p[..., 0]) * c - (z - self.hz) * s, p[..., 0])
            z2 = np.where(up, (p[..., 0]) * s + (z - self.hz) * c + self.hz, z)
            return np.stack([x2, p[..., 1], z2], -1)
        return p

    def parts(self, p):
        p = self._xf(p) / self.sc
        o = self.o; hz = self.hz; fem = o['female']
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        # torso
        chest = sd_ellipsoid(p, (0.01, 0, hz + 0.36), (0.12, 0.172 if not fem else 0.15, 0.15))
        if fem:
            chest = smin(chest, sd_ellipsoid(p, (0.07, 0, hz + 0.34), (0.07, 0.13, 0.06)), 0.04)
        abd = sd_ellipsoid(p, (0.0, 0, hz + 0.18), (0.115, 0.155 if not fem else 0.14, 0.14))
        pel = sd_ellipsoid(p, (0.0, 0, hz + 0.02), (0.115, 0.165 if not fem else 0.175, 0.11))
        dz = np.array((0, 0, -0.035), f32)
        shl = np.minimum(sd_sphere(p, self.arms[0][0] + dz, 0.056), sd_sphere(p, self.arms[1][0] + dz, 0.056))
        torso = smin(smin(chest, abd, 0.07), shl, 0.05)
        neck = sd_round_cone(p, (0.0, 0, hz + 0.50), (0.015, 0, hz + 0.62), 0.052, 0.047)
        head = sd_ellipsoid(p, (0.02, 0, hz + 0.73), (0.102, 0.083, 0.114))
        jaw = sd_ellipsoid(p, (0.045, 0, hz + 0.665), (0.072, 0.066, 0.055))
        nose = sd_ellipsoid(p, (0.118, 0, hz + 0.725), (0.022, 0.014, 0.03))
        ears = np.minimum(sd_sphere(p, (0.0, 0.085, hz + 0.725), 0.022), sd_sphere(p, (0.0, -0.085, hz + 0.725), 0.022))
        headf = smin(smin(head, jaw, 0.04), np.minimum(nose, ears), 0.012)
        # hair
        hs = sd_ellipsoid(p, (0.005, 0, hz + 0.75), (0.112, 0.092, 0.122))
        hair = np.maximum(hs, plane(p, (0.06, 0, hz + 0.735), (0.75, 0, -1)))
        hair = np.maximum(hair, -plane(p, (0, 0, hz + 0.67), (0, 0, -1)) * 0 + plane(p, (0, 0, hz + 0.68), (0.25, 0, -1)))
        if o['style'] == 'long':
            hair = smin(hair, sd_round_box(p, (-0.07, 0, hz + 0.58), (0.05, 0.10, 0.16), 0.05), 0.04)
        elif o['style'] == 'pony':
            hair = smin(hair, sd_round_cone(p, (-0.10, 0, hz + 0.76), (-0.16, 0, hz + 0.56), 0.045, 0.025), 0.03)
        # limbs
        legs = np.full(x.shape, 9, f32); shoes = legs.copy(); arms = legs.copy(); hands = legs.copy()
        thighs = legs.copy()
        for hip, kn, an, fp in self.legs:
            th = sd_round_cone(p, hip, kn, 0.082, 0.056)
            sh = sd_round_cone(p, kn, an, 0.056, 0.040)
            thighs = np.minimum(thighs, th)
            legs = np.minimum(legs, smin(th, sh, 0.02))
            toe = an + rot_y(np.array((0.18, 0, -0.035), f32), fp)
            heel = an + rot_y(np.array((-0.035, 0, -0.04), f32), fp)
            shoes = np.minimum(shoes, sd_round_cone(p, heel, toe, 0.048, 0.042))
        for sp, el, wr in self.arms:
            ua = sd_round_cone(p, sp, el, 0.052 if not fem else 0.045, 0.042 if not fem else 0.037)
            fa = sd_round_cone(p, el, wr, 0.042 if not fem else 0.037, 0.031 if not fem else 0.028)
            arms = np.minimum(arms, smin(ua, fa, 0.015))
            dirv = (wr - el) / max(np.linalg.norm(wr - el), 1e-6)
            hands = np.minimum(hands, sd_ellipsoid(p, wr + dirv * 0.065, (0.034, 0.024, 0.05)))
        extra = np.full(x.shape, 9, f32)
        if o['top_len'] == 'coat':
            extra = sd_round_cone(p, (0, 0, hz + 0.05), (0.0, 0, hz - 0.25), 0.155, 0.19 if o['leg'] == 'skirt' else 0.17)
            extra = np.maximum(extra, -plane(p, (0, 0, hz + 0.1), (0, 0, 1)))
        elif o['leg'] == 'skirt':
            extra = sd_round_cone(p, (0, 0, hz + 0.05), (0.0, 0, hz - 0.30), 0.15, 0.20)
        body = smin(torso, pel, 0.05)
        return dict(body=body, neck=neck, head=headf, hair=hair, legs=legs, thighs=thighs, shoes=shoes,
                    arms=arms, hands=hands, extra=extra, p=p)

    def sdf(self, p):
        d = self.parts(p)
        m = np.minimum
        return m(m(m(m(d['body'], d['neck']), m(d['head'], d['hair'])), m(d['legs'], d['shoes'])),
                 m(m(d['arms'], d['hands']), d['extra'])) * self.sc

    def footprint(self, gx, gy):
        if self.lying: return np.sqrt((gx * 0.55) ** 2 + gy ** 2) - 0.25
        return np.sqrt(gx ** 2 * 0.8 + gy ** 2) - 0.22

    def shade(self, p, n, view):
        d = self.parts(p)
        q = d['p']; o = self.o; hz = self.hz
        names = ['body', 'neck', 'head', 'hair', 'legs', 'shoes', 'arms', 'hands', 'extra']
        st = np.stack([d[k] for k in names]); idx = st.argmin(0)
        skin, top, legc = o['skin'], o['top'], o['legs']
        cols = np.array([top, skin, skin, o['hair'], legc, o['shoes'], top, skin, top], f32)
        alb = cols[idx]
        x, y, z = q[..., 0], q[..., 1], q[..., 2]
        # pelvis / hips wear trousers colour below the belt for short tops
        belt = hz + (0.06 if o['top_len'] != 'shirt' else 0.10)
        lowbody = (idx == 0) & (z < belt) & (o['top_len'] != 'coat')
        alb[lowbody] = legc
        if o['leg'] == 'skirt':
            alb[idx == 8] = legc if o['top_len'] != 'coat' else top
            # bare legs / dark tights below the hem
            alb[(idx == 4)] = skin * 0.55 + np.array((0.05, 0.04, 0.04), f32)
        # jacket opening / shirt front
        if o['top_len'] in ('jacket', 'coat'):
            front = (idx == 0) & (x > 0.07) & (np.abs(y) < 0.045) & (z > hz + 0.15)
            alb[front] = o['accent']
        # sleeves end at wrists (already), short sleeves for white top
        if o['top_len'] == 'shirt' and o['female']:
            arm = (idx == 6)
            lower = arm & (z < hz + 0.33)
            alb[lower] = skin
        # face: brows/eyes and mouth hints
        face = (idx == 2) & (x > 0.07)
        eyes = face & (np.abs(z - (hz + 0.745)) < 0.013) & (np.abs(np.abs(y) - 0.035) < 0.018)
        alb[eyes] = alb[eyes] * 0.35
        mouth = face & (np.abs(z - (hz + 0.655)) < 0.008) & (np.abs(y) < 0.025)
        alb[mouth] = alb[mouth] * 0.6
        # cloth folds / texture
        cloth = (idx == 0) | (idx == 4) | (idx == 6) | (idx == 8)
        fold = 0.90 + 0.10 * np.sin(z * 55 + np.sin(y * 31) * 2) * np.sin(x * 23 + z * 9)
        alb[cloth] *= fold[cloth][:, None]
        # hair strands
        hairm = idx == 3
        alb[hairm] *= (0.85 + 0.25 * np.sin(y * 140 + z * 60))[hairm][:, None]
        kind = np.full(len(z), 5, np.int32)
        if self.lying:
            kind[:] = 5
        return lighting(alb, kind, n, view, np.clip(z, 0.4, 2) + (1.0 if self.lying else 0))

# ------------------------------------------------------------------ blood
class Blood:
    def __init__(self, seed, size=1.0, streak=0.0):
        rng = np.random.default_rng(seed)
        self.blobs = [(0.0, 0.0, 0.55 * size)]
        for i in range(9):
            a = rng.uniform(0, 2 * np.pi); r = rng.uniform(0.2, 0.75) * size
            self.blobs.append((np.cos(a) * r + streak * rng.uniform(0, 1), np.sin(a) * r * 0.7, rng.uniform(0.08, 0.28) * size))
        for i in range(10):   # droplets
            a = rng.uniform(0, 2 * np.pi); r = rng.uniform(0.7, 1.3) * size
            self.blobs.append((np.cos(a) * r + streak, np.sin(a) * r * 0.6, rng.uniform(0.03, 0.07)))
    def sdf(self, p):
        d = np.full(p.shape[:-1], 9, f32)
        for bx, by, br in self.blobs:
            d = smin(d, np.sqrt((p[..., 0] - bx) ** 2 + (p[..., 1] - by) ** 2) - br, 0.12)
        return np.maximum(d, np.abs(p[..., 2] - 0.004) - 0.004)
    def footprint(self, gx, gy): return gx * 0 + 9
    def shade(self, p, n, view):
        N = len(p)
        alb = np.tile(np.array((0.32, 0.02, 0.02), f32), (N, 1))
        r = view - 2 * (view * n).sum(-1)[:, None] * n
        h = nrm(np.array((0.35, 0.55, 0.76), f32) - view)
        spec = np.clip((n * h).sum(-1), 0, 1)[:, None]
        edge = np.clip(1.0 - n[:, 2], 0, 1)[:, None]
        return np.clip(alb * (0.95 - 0.4 * edge) + env(r) * 0.06 + 0.45 * spec ** 60, 0, 1)

# ------------------------------------------------------------- tuner cars
class Tuner(Car):
    """Original street-racing designs: 'coupe' (wide-body with wing), 'muscle', 'exotic'."""
    def __init__(self, kind, paint, stripe=None, glow=None, rim=(0.62, 0.63, 0.66)):
        Car.__init__(self, 'sedan', paint)
        self.kind2 = kind; self.stripe = None if stripe is None else np.array(stripe, f32)
        self.glow = glow; self.rimc = np.array(rim, f32)
        self.wr = 0.33; self.wx = 1.36 if kind != 'muscle' else 1.48; self.wy = 0.84 if kind != 'muscle' else 0.82

    def parts(self, p):
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y); pm = np.stack([x, ay, z], -1)
        k = self.kind2
        if k == 'coupe':
            body = sd_round_box(p, (0, 0, 0.53), (2.25, 0.93, 0.27), 0.13)
            body = np.maximum(body, plane(p, (1.30, 0, 0.80), (0.33, 0, 1)))
            body = np.maximum(body, plane(p, (2.25, 0, 0.34), (0.75, 0, -0.65)))
            fend = np.minimum(sd_round_box(pm, (self.wx, 0.86, 0.50), (0.48, 0.12, 0.20), 0.10),
                              sd_round_box(pm, (-self.wx, 0.86, 0.52), (0.50, 0.13, 0.21), 0.10))
            body = smin(body, fend, 0.08)
            cab = sd_round_box(p, (-0.35, 0, 0.99), (1.15, 0.72, 0.25), 0.10)
            cab = np.maximum(cab, plane(p, (0.62, 0, 0.80), (0.70, 0, 0.71)))
            cab = np.maximum(cab, plane(p, (-0.95, 0, 0.82), (-0.42, 0, 0.91)))   # fastback
            cab = np.maximum(cab, plane(pm, (0, 0.72, 0.8), (0, 1, 0.35)))
            wing = sd_round_box(p, (-2.05, 0, 1.06), (0.17, 0.86, 0.022), 0.012)
            wing = np.minimum(wing, sd_round_box(pm, (-2.0, 0.55, 0.93), (0.05, 0.02, 0.13), 0.01))
            wing = np.minimum(wing, sd_round_box(pm, (-2.05, 0.86, 1.06), (0.18, 0.012, 0.06), 0.01))
            lip = sd_round_box(p, (2.20, 0, 0.22), (0.10, 0.90, 0.03), 0.02)
            skirt = sd_round_box(pm, (0, 0.93, 0.26), (1.0, 0.05, 0.05), 0.03)
            extra = np.minimum(np.minimum(wing, lip), skirt)
            belt = 0.80
        elif k == 'muscle':
            body = sd_round_box(p, (0, 0, 0.60), (2.40, 0.93, 0.31), 0.10)
            body = np.maximum(body, plane(p, (2.40, 0, 0.40), (0.8, 0, -0.6)))
            cab = sd_round_box(p, (-0.60, 0, 1.10), (1.05, 0.74, 0.25), 0.07)
            cab = np.maximum(cab, plane(p, (0.20, 0, 0.92), (0.62, 0, 0.78)))
            cab = np.maximum(cab, plane(p, (-1.0, 0, 0.92), (-0.45, 0, 0.89)))
            cab = np.maximum(cab, plane(pm, (0, 0.74, 0.9), (0, 1, 0.25)))
            scoop = sd_round_box(p, (1.0, 0, 0.94), (0.35, 0.20, 0.06), 0.03)
            spoil = sd_round_box(p, (-2.30, 0, 0.94), (0.06, 0.85, 0.035), 0.015)
            extra = np.minimum(scoop, spoil)
            belt = 0.92
        else:  # exotic, mid-engine wedge
            body = sd_round_box(p, (0, 0, 0.47), (2.25, 0.99, 0.24), 0.12)
            body = np.maximum(body, plane(p, (1.20, 0, 0.62), (0.30, 0, 1)))
            body = np.maximum(body, plane(p, (2.25, 0, 0.30), (0.7, 0, -0.7)))
            cab = sd_round_box(p, (0.05, 0, 0.88), (1.10, 0.70, 0.22), 0.10)
            cab = np.maximum(cab, plane(p, (0.75, 0, 0.66), (0.55, 0, 0.83)))
            cab = np.maximum(cab, plane(p, (-0.60, 0, 0.70), (-0.30, 0, 0.95)))
            cab = np.maximum(cab, plane(pm, (0, 0.70, 0.7), (0, 1, 0.45)))
            deck = sd_round_box(p, (-1.25, 0, 0.72), (0.80, 0.80, 0.08), 0.06)
            intake = sd_round_box(pm, (-0.30, 1.02, 0.50), (0.35, 0.03, 0.12), 0.02)
            extra = np.minimum(deck, intake)
            belt = 0.66
        self.belt = belt
        well = np.maximum(np.minimum(np.sqrt((x - self.wx) ** 2 + (z - self.wr) ** 2),
                                     np.sqrt((x + self.wx) ** 2 + (z - self.wr) ** 2)) - (self.wr + 0.05), 0.60 - ay)
        shell = smin(body, cab, 0.05)
        shell = np.minimum(shell, extra)
        shell = np.maximum(shell, -well)
        wheel = np.minimum(sd_ycyl(pm, self.wx, self.wr, self.wr, self.wy - 0.13, self.wy + 0.13),
                           sd_ycyl(pm, -self.wx, self.wr, self.wr, self.wy - 0.13, self.wy + 0.13))
        return shell, wheel, cab, body

    def shade(self, p, n, view):
        shell, wheel, cab, body = self.parts(p)
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y); N = len(x)
        alb = np.tile(self.paint, (N, 1)); kind = np.zeros(N, np.int32)
        isw = wheel < shell
        glass = (~isw) & (cab <= body + 0.004) & (z > self.belt) & (n[..., 2] < 0.85)
        kind[glass] = 1; alb[glass] = (0.03, 0.035, 0.045)
        if self.stripe is not None:
            st = (~isw) & (~glass) & (ay > 0.10) & (ay < 0.22) & (n[..., 2] > 0.3)
            alb[st] = self.stripe
            side = (~isw) & (~glass) & (np.abs(n[..., 1]) > 0.7) & (np.abs(z - 0.55) < 0.05) & (x > -1.6) & (x < 1.9)
            alb[side] = self.stripe
        front = (~isw) & (x > 2.02); rear = (~isw) & (x < -2.10)
        hl = front & (z > 0.48) & (z < 0.62) & (ay > 0.45) & (ay < 0.88)
        grille = front & (ay < 0.42) & (z > 0.28) & (z < 0.50)
        tl = rear & (z > 0.55) & (z < 0.75) & (ay > 0.40)
        low = (~isw) & (~glass) & (z < 0.30)
        alb[low] = alb[low] * 0.35 + 0.02
        alb[grille] = (0.04, 0.04, 0.04); kind[grille] = 5
        alb[hl] = (0.92, 0.94, 1.0); kind[hl] = 4
        alb[tl] = (0.75, 0.03, 0.03); kind[tl] = 6
        wingm = (~isw) & (z > 0.98) & (x < -1.8) if self.kind2 == 'coupe' else np.zeros(N, bool)
        alb[wingm] = (0.06, 0.06, 0.07); kind[wingm] = 5
        rad = np.minimum(np.sqrt((x - self.wx) ** 2 + (z - self.wr) ** 2), np.sqrt((x + self.wx) ** 2 + (z - self.wr) ** 2))
        cx = np.where(x > 0, self.wx, -self.wx)
        ang = np.arctan2(z - self.wr, x - cx)
        rim = isw & (rad < self.wr * 0.74) & (ay > self.wy - 0.02)
        spoke = rim & ((np.cos(ang * 5) > 0.15) | (rad < self.wr * 0.25) | (rad > self.wr * 0.66))
        alb[isw] = (0.05, 0.05, 0.05); kind[isw] = 2
        alb[rim] = (0.10, 0.10, 0.11); kind[rim] = 5
        alb[spoke] = self.rimc; kind[spoke] = 3
        return lighting(alb, kind, n, view, z + 0.15)
