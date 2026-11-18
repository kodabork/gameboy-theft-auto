# Pre-renders original 3D models (cars, people, trees, lamps) into sprites with an
# orthographic SDF raymarcher. Local frame: x forward, y left, z up (metres).
import numpy as np

f32 = np.float32

def length(v): return np.sqrt((v * v).sum(-1))
def nrm(v): return v / np.maximum(length(v)[..., None], 1e-6)

def sd_round_box(p, c, b, r):
    q = np.abs(p - c) - (np.array(b, f32) - r)
    return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0) - r

def sd_capsule(p, a, b, r):
    a = np.asarray(a, f32); b = np.asarray(b, f32)
    pa = p - a; ba = b - a
    h = np.clip((pa * ba).sum(-1) / max((ba * ba).sum(), 1e-9), 0, 1)
    return length(pa - ba * h[..., None]) - r

def sd_sphere(p, c, r): return length(p - np.asarray(c, f32)) - r

def sd_ycyl(p, cx, cz, r, y0, y1):
    d2 = np.sqrt((p[..., 0] - cx) ** 2 + (p[..., 2] - cz) ** 2) - r
    dy = np.abs(p[..., 1] - (y0 + y1) / 2) - (y1 - y0) / 2
    return np.minimum(np.maximum(d2, dy), 0) + length(np.stack([np.maximum(d2, 0), np.maximum(dy, 0)], -1))

def sd_zcyl(p, cx, cy, r, z0, z1):
    d2 = np.sqrt((p[..., 0] - cx) ** 2 + (p[..., 1] - cy) ** 2) - r
    dz = np.abs(p[..., 2] - (z0 + z1) / 2) - (z1 - z0) / 2
    return np.minimum(np.maximum(d2, dz), 0) + length(np.stack([np.maximum(d2, 0), np.maximum(dz, 0)], -1))

def plane(p, o, n):
    n = np.asarray(n, f32); n = n / np.linalg.norm(n)
    return ((p - np.asarray(o, f32)) * n).sum(-1)

def smin(a, b, k):
    h = np.clip(0.5 + 0.5 * (b - a) / k, 0, 1)
    return b * (1 - h) + a * h - k * h * (1 - h)

# ------------------------------------------------------------------ cars
class Car:
    def __init__(self, kind, paint, taxi=False):
        self.kind, self.paint, self.taxi = kind, np.array(paint, f32), taxi
        if kind == 'sedan':
            self.wr, self.wx, self.wy = 0.33, 1.40, 0.79
        else:
            self.wr, self.wx, self.wy = 0.38, 1.45, 0.82

    def parts(self, p):
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        pm = np.stack([x, ay, z], -1)
        if self.kind == 'sedan':
            body = sd_round_box(p, (0, 0, 0.60), (2.33, 0.90, 0.31), 0.15)
            body = np.maximum(body, plane(p, (1.55, 0, 0.90), (0.30, 0, 1)))       # hood slopes to nose
            body = np.maximum(body, plane(p, (-1.70, 0, 0.93), (-0.22, 0, 1)))     # trunk
            body = np.maximum(body, plane(p, (2.33, 0, 0.36), (0.8, 0, -0.6)))     # chin
            cab = sd_round_box(p, (-0.30, 0, 1.13), (1.30, 0.76, 0.30), 0.10)
            cab = np.maximum(cab, plane(p, (0.80, 0, 0.92), (0.64, 0, 0.77)))      # windshield
            cab = np.maximum(cab, plane(p, (-1.25, 0, 0.92), (-0.62, 0, 0.78)))    # rear glass
            cab = np.maximum(cab, plane(pm, (0, 0.76, 0.92), (0, 1, 0.30)))        # tumblehome
            if self.taxi:
                cab = np.minimum(cab, sd_round_box(p, (-0.35, 0, 1.50), (0.26, 0.16, 0.08), 0.03))
        else:  # SUV
            body = sd_round_box(p, (0, 0, 0.78), (2.36, 0.95, 0.42), 0.16)
            body = np.maximum(body, plane(p, (1.75, 0, 1.15), (0.25, 0, 1)))
            body = np.maximum(body, plane(p, (2.36, 0, 0.45), (0.8, 0, -0.6)))
            cab = sd_round_box(p, (-0.30, 0, 1.47), (1.95, 0.86, 0.36), 0.10)
            cab = np.maximum(cab, plane(p, (1.05, 0, 1.18), (0.55, 0, 0.84)))
            cab = np.maximum(cab, plane(pm, (0, 0.86, 1.2), (0, 1, 0.18)))
        well = np.maximum(np.minimum(
            np.sqrt((x - self.wx) ** 2 + (z - self.wr) ** 2), np.sqrt((x + self.wx) ** 2 + (z - self.wr) ** 2)) - (self.wr + 0.07),
            0.55 - ay)
        shell = smin(body, cab, 0.05)
        shell = np.maximum(shell, -well)
        mirror = sd_round_box(pm, (0.70 if self.kind == 'sedan' else 0.95, 0.98, 1.02 if self.kind == 'sedan' else 1.30), (0.06, 0.09, 0.05), 0.02)
        shell = np.minimum(shell, mirror)
        wheel = np.minimum(sd_ycyl(pm, self.wx, self.wr, self.wr, self.wy - 0.12, self.wy + 0.12),
                           sd_ycyl(pm, -self.wx, self.wr, self.wr, self.wy - 0.12, self.wy + 0.12))
        return shell, wheel, cab, body

    def sdf(self, p):
        s, w, _, _ = self.parts(p)
        return np.minimum(s, w)

    def footprint(self, gx, gy):
        q = np.stack([np.abs(gx) - 2.25, np.abs(gy) - 0.85], -1)
        return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0)

    def shade(self, p, n, view):
        shell, wheel, cab, body = self.parts(p)
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        N = len(x)
        alb = np.tile(self.paint, (N, 1))
        kind = np.zeros(N, np.int32)        # 0 paint 1 glass 2 tire 3 rim 4 light 5 plastic 6 tail
        is_wheel = wheel < shell
        sedan = self.kind == 'sedan'
        belt = 0.92 if sedan else 1.16
        taxisign = np.zeros(N, bool)
        if self.taxi:
            taxisign = (~is_wheel) & (z > 1.40) & (np.abs(x + 0.35) < 0.31) & (ay < 0.2)
        glass = (~is_wheel) & (cab <= body + 0.004) & (z > belt) & (n[..., 2] < 0.80) & ~taxisign
        bpillar = np.abs(x + (0.25 if sedan else 0.15)) < 0.07
        cpillar = (x < (-1.05 if sedan else -1.95)) & (ay > 0.55) & (n[..., 0] > -0.5)
        glass &= ~(bpillar & (np.abs(n[..., 1]) > 0.5)) & ~cpillar
        kind[glass] = 1

        front = (~is_wheel) & (x > (2.05 if sedan else 2.10))
        rear = (~is_wheel) & (x < (-2.12 if sedan else -2.18))
        hl = front & (z > (0.62 if sedan else 0.82)) & (z < (0.78 if sedan else 0.98)) & (ay > 0.42) & (ay < 0.86)
        grille = front & (ay < 0.40) & (z > (0.44 if sedan else 0.60)) & (z < (0.68 if sedan else 0.95))
        tl = rear & (z > (0.70 if sedan else 0.95)) & (z < (0.86 if sedan else 1.22)) & (ay > 0.48)
        plate = rear & (ay < 0.24) & (z > (0.46 if sedan else 0.62)) & (z < (0.60 if sedan else 0.78))
        bumper = (~is_wheel) & (~glass) & (z < (0.40 if sedan else 0.52))
        alb[bumper] = (0.09, 0.09, 0.10); kind[bumper] = 5
        alb[grille] = (0.05, 0.05, 0.05); kind[grille] = 5
        alb[hl] = (0.92, 0.92, 0.86); kind[hl] = 4
        alb[tl] = (0.70, 0.04, 0.04); kind[tl] = 6
        alb[plate] = (0.85, 0.85, 0.80); kind[plate] = 5
        alb[glass] = (0.03, 0.04, 0.05)
        alb[taxisign] = (0.95, 0.90, 0.62); kind[taxisign] = 4
        rad = np.minimum(np.sqrt((x - self.wx) ** 2 + (z - self.wr) ** 2), np.sqrt((x + self.wx) ** 2 + (z - self.wr) ** 2))
        rim = is_wheel & (rad < self.wr * 0.62) & (ay > self.wy)
        alb[is_wheel] = (0.05, 0.05, 0.05); kind[is_wheel] = 2
        alb[rim] = (0.55, 0.56, 0.58); kind[rim] = 3
        return lighting(alb, kind, n, view, z)

SKY_UP = np.array((0.78, 0.82, 0.88), f32)
SKY_HOR = np.array((0.62, 0.64, 0.67), f32)
GROUND = np.array((0.24, 0.24, 0.25), f32)
L = nrm(np.array((0.35, 0.55, 0.76), f32))
SHADOW = np.array((0.11, 0.11, 0.12), f32)

def env(r):
    t = r[..., 2:3]
    up = SKY_HOR + (SKY_UP - SKY_HOR) * np.clip(t, 0, 1)
    # soft building silhouettes on the horizon make paint reflections read as "city"
    bld = (np.sin(r[..., 1:2] * 11.0) * 0.5 + 0.5) * 0.18
    up = np.where(t < bld, up * 0.55, up)
    return np.where(t > 0, up, GROUND + (SKY_HOR - GROUND) * np.clip(1 + t * 4, 0, 1))

def lighting(alb, kind, n, view, z):
    diff = np.clip((n * L).sum(-1), 0, 1)[:, None]
    sky = (0.5 + 0.5 * n[..., 2:3])
    ao = (0.55 + 0.45 * np.clip(z / 0.55, 0, 1))[:, None]
    col = alb * (0.30 * sky + 0.62 * diff + 0.10) * ao
    r = view - 2 * (view * n).sum(-1)[:, None] * n
    fres = (0.04 + 0.96 * (1 - np.clip(-(view * n).sum(-1), 0, 1)) ** 5)[:, None]
    h = nrm(L - view)
    spec = np.clip((n * h).sum(-1), 0, 1)[:, None]
    k = kind[:, None]
    refl = env(r)
    paint = col + refl * (0.10 + 0.55 * fres) + 0.55 * spec ** 40
    glassc = alb + refl * (0.35 + 0.55 * fres) + 0.8 * spec ** 60
    chrome = alb * 0.4 + refl * 0.5 + 0.6 * spec ** 30
    carbon = col + refl * 0.12 + 0.25 * spec ** 25
    out = np.where(k == 0, paint, col)
    out = np.where(k == 1, glassc, out)
    out = np.where(k == 3, chrome, out)
    out = np.where(k == 4, alb * 0.9 + 0.1 + 0.4 * spec ** 20, out)
    out = np.where(k == 6, col + refl * 0.25 + 0.5 * spec ** 30, out)
    out = np.where(k == 2, col + 0.08 * spec ** 8, out)
    out = np.where(k == 5, col + refl * 0.06, out)
    out = np.where(k == 7, carbon, out)
    return np.clip(out, 0, 1)

# ---------------------------------------------------------------- people
OUTFITS = {
    'player': dict(top=(0.14, 0.15, 0.17), top2=(0.14, 0.15, 0.17), legs=(0.32, 0.30, 0.26), shoes=(0.30, 0.18, 0.10), skin=(0.66, 0.48, 0.36), hair=(0.06, 0.05, 0.05), shirt=(0.62, 0.20, 0.16)),
    'grey':   dict(top=(0.36, 0.37, 0.39), top2=(0.36, 0.37, 0.39), legs=(0.09, 0.09, 0.10), shoes=(0.05, 0.05, 0.05), skin=(0.45, 0.31, 0.22), hair=(0.05, 0.04, 0.04), shirt=(0.80, 0.80, 0.80)),
    'red':    dict(top=(0.55, 0.10, 0.10), top2=(0.55, 0.10, 0.10), legs=(0.55, 0.48, 0.34), shoes=(0.30, 0.22, 0.15), skin=(0.85, 0.68, 0.56), hair=(0.45, 0.30, 0.15), shirt=(0.55, 0.10, 0.10)),
    'navy':   dict(top=(0.12, 0.15, 0.28), top2=(0.12, 0.15, 0.28), legs=(0.25, 0.30, 0.42), shoes=(0.85, 0.85, 0.85), skin=(0.70, 0.52, 0.40), hair=(0.70, 0.58, 0.35), shirt=(0.12, 0.15, 0.28)),
}

class Person:
    def __init__(self, outfit, phase, moving=True):
        self.o = {k: np.array(v, f32) for k, v in OUTFITS[outfit].items()}
        s = np.sin(phase) if moving else 0.0
        c = np.cos(phase) if moving else 0.0
        hip_z = 0.93 + (0.02 * abs(c) if moving else 0)
        self.bob = hip_z - 0.93
        legs, arms = [], []
        for side, sg in ((0.10, 1), (-0.10, -1)):
            th = 0.45 * s * sg                       # thigh swing
            knee_bend = 0.85 * max(0.0, sg * np.cos(phase)) + 0.1 if moving else 0.04
            hip = np.array((0, side, hip_z), f32)
            knee = hip + np.array((np.sin(th), 0, -np.cos(th)), f32) * 0.46
            sh = th - knee_bend
            ankle = knee + np.array((np.sin(sh), 0, -np.cos(sh)), f32) * 0.44
            legs.append((hip, knee, ankle))
            ar = -0.35 * s * sg
            shoulder = np.array((0, side * 2.0, hip_z + 0.50), f32)
            elbow = shoulder + np.array((np.sin(ar), 0, -np.cos(ar)), f32) * 0.28
            wrist = elbow + np.array((np.sin(ar + 0.25), 0, -np.cos(ar + 0.25)), f32) * 0.26
            arms.append((shoulder, elbow, wrist))
        self.legs, self.arms, self.hip_z = legs, arms, hip_z

    def parts(self, p):
        hz = self.hip_z
        torso = sd_round_box(p, (0, 0, hz + 0.27), (0.10, 0.17, 0.24), 0.08)
        pelvis = sd_round_box(p, (0, 0, hz + 0.02), (0.09, 0.15, 0.08), 0.06)
        neck = sd_capsule(p, (0, 0, hz + 0.52), (0.01, 0, hz + 0.62), 0.05)
        head = sd_sphere(p, (0.015, 0, hz + 0.71), 0.105)
        hair = np.maximum(sd_sphere(p, (-0.012, 0, hz + 0.722), 0.114), plane(p, (0.02, 0, hz + 0.69), (0.55, 0, -1)))
        legd = np.full(p.shape[:-1], 9, f32); shoe = legd.copy(); arm = legd.copy(); hand = legd.copy()
        for hip, knee, ankle in self.legs:
            legd = np.minimum(legd, np.minimum(sd_capsule(p, hip, knee, 0.075), sd_capsule(p, knee, ankle, 0.06)))
            shoe = np.minimum(shoe, sd_round_box(p, ankle + np.array((0.06, 0, -0.03), f32), (0.11, 0.05, 0.04), 0.03))
        for sh, el, wr in self.arms:
            arm = np.minimum(arm, np.minimum(sd_capsule(p, sh, el, 0.055), sd_capsule(p, el, wr, 0.047)))
            hand = np.minimum(hand, sd_sphere(p, wr + (wr - el) * 0.25, 0.045))
        top = smin(torso, smin(arm, pelvis * 0 + 9, 0.01), 0.04)
        return dict(top=top, pelvis=pelvis, neck=neck, head=head, hair=hair, legs=legd, shoes=shoe, hands=hand, arms=arm)

    def sdf(self, p):
        d = self.parts(p)
        return np.min(np.stack([d['top'], d['pelvis'], d['neck'], d['head'], d['hair'], d['legs'], d['shoes'], d['hands']]), 0)

    def footprint(self, gx, gy):
        return np.sqrt(gx ** 2 * 0.8 + gy ** 2) - 0.22

    def shade(self, p, n, view):
        d = self.parts(p)
        names = ['top', 'pelvis', 'neck', 'head', 'hair', 'legs', 'shoes', 'hands']
        cols = [self.o['top'], self.o['legs'], self.o['skin'], self.o['skin'], self.o['hair'], self.o['legs'], self.o['shoes'], self.o['skin']]
        st = np.stack([d[k] for k in names])
        idx = st.argmin(0)
        alb = np.array(cols, f32)[idx]
        # open jacket: show shirt on chest front
        z = p[..., 2]
        chest = (idx == 0) & (p[..., 0] > 0.10) & (np.abs(p[..., 1]) < 0.05) & (z > self.hip_z + 0.15)
        alb[chest] = self.o['shirt']
        kind = np.full(len(z), 5, np.int32)
        return lighting(alb, kind, n, view, z + 0.4)

# ------------------------------------------------------------- scenery
class Tree:
    def __init__(self, seed=3):
        rng = np.random.default_rng(seed)
        self.blobs = []
        for i in range(26):
            a = rng.uniform(0, 2 * np.pi); r = rng.uniform(0, 1.6); h = rng.uniform(3.6, 6.4)
            rr = rng.uniform(0.7, 1.15) * (1.2 - abs(h - 5) / 4)
            self.blobs.append((np.cos(a) * r, np.sin(a) * r, h, rr))
        self.blobs = np.array(self.blobs, f32)
    def parts(self, p):
        trunk = sd_capsule(p, (0, 0, 0), (0.05, 0.02, 4.2), 0.14)
        br1 = sd_capsule(p, (0.02, 0.01, 3.0), (0.8, 0.5, 4.6), 0.07)
        br2 = sd_capsule(p, (0.03, 0.0, 3.3), (-0.7, -0.4, 4.9), 0.07)
        trunk = np.minimum(trunk, np.minimum(br1, br2))
        leaf = np.full(p.shape[:-1], 9, f32)
        for bx, by, bz, br in self.blobs:
            leaf = smin(leaf, sd_sphere(p, (bx, by, bz), br), 0.3)
        noise = 0.10 * np.sin(p[..., 0] * 9.1 + np.sin(p[..., 1] * 7.3)) * np.sin(p[..., 2] * 8.7 + p[..., 1] * 3.1)
        return trunk, leaf + noise
    def sdf(self, p):
        t, l = self.parts(p); return np.minimum(t, l)
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 1.5
    def shade(self, p, n, view):
        t, l = self.parts(p)
        z = p[..., 2]
        isleaf = l < t
        var = 0.85 + 0.15 * np.sin(p[..., 0] * 13 + p[..., 1] * 11 + z * 7)
        alb = np.where(isleaf[:, None], np.array((0.20, 0.32, 0.12), f32) * var[:, None], np.array((0.22, 0.16, 0.11), f32))
        kind = np.full(len(z), 5, np.int32)
        inner = np.clip((z - 3.4) / 3.0, 0.3, 1.0)[:, None]
        return lighting(alb, kind, n, view, z) * np.where(isleaf[:, None], inner * 0.9 + 0.25, 1.0)

class Lamp:
    def parts(self, p):
        pole = sd_capsule(p, (0, 0, 0), (0, 0, 6.6), 0.07)
        base = sd_zcyl(p, 0, 0, 0.13, 0, 0.6)
        arm = sd_capsule(p, (0, 0, 6.5), (1.7, 0, 6.85), 0.04)
        head = sd_round_box(p, (1.85, 0, 6.82), (0.28, 0.13, 0.06), 0.04)
        return np.minimum(np.minimum(pole, base), arm), head
    def sdf(self, p):
        a, b = self.parts(p); return np.minimum(a, b)
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 0.12
    def shade(self, p, n, view):
        a, h = self.parts(p)
        alb = np.where((h < a)[:, None] & (n[..., 2:3] < -0.5), np.array((0.95, 0.92, 0.78), f32), np.array((0.20, 0.22, 0.21), f32))
        kind = np.where(h < a, 4, 3).astype(np.int32)
        return lighting(alb, kind, n, view, p[..., 2] + 1)

BAYER4 = np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]], f32)

# ------------------------------------------------------------- renderer
SHADOW = np.array((0.11, 0.11, 0.12), f32)

def render(model, yaw, elev, s, w_sq, rows_up, rows_down, shadow=True, max_t=30.0, core=False):
    """Orthographic render. yaw = model heading relative to view direction (0 = facing away).
    Returns (rgb[h, w_sq], alpha mask, shadow mask) with anchor (ground centre) at column w_sq/2, row rows_up."""
    H = rows_up + rows_down
    j, i = np.mgrid[0:H, 0:w_sq].astype(f32)
    u = (i - w_sq / 2 + 0.5) * s
    v = (rows_up - j - 0.5) * s
    ce, se = np.cos(elev), np.sin(elev)
    d = np.array((ce, 0, -se), f32)
    right = np.array((0, -1, 0), f32)
    up = np.array((se, 0, ce), f32)
    o = u[..., None] * right + v[..., None] * up - d * 15
    # rotate into model frame
    cy, sy = np.cos(-yaw), np.sin(-yaw)
    R = np.array(((cy, -sy, 0), (sy, cy, 0), (0, 0, 1)), f32)
    o = o.reshape(-1, 3) @ R.T
    dl = R @ d
    t = np.zeros(len(o), f32)
    hit = np.zeros(len(o), bool)
    alive = np.ones(len(o), bool)
    stepk = getattr(model, 'step', 0.9)
    for _ in range(getattr(model, 'iters', 110)):
        idx = np.nonzero(alive)[0]
        if len(idx) == 0: break
        p = o[idx] + dl * t[idx, None]
        dist = model.sdf(p)
        t[idx] += dist * stepk
        done = dist < 0.0015
        hit[idx[done]] = True
        far = (t[idx] > max_t) | (p[:, 2] < -0.05)
        alive[idx[done | far]] = False
    rgb = np.zeros((len(o), 3), f32)
    hi = np.nonzero(hit)[0]
    if len(hi):
        p = o[hi] + dl * t[hi, None]
        e = 0.002
        n = np.stack([model.sdf(p + (e, 0, 0)) - model.sdf(p - (e, 0, 0)),
                      model.sdf(p + (0, e, 0)) - model.sdf(p - (0, e, 0)),
                      model.sdf(p + (0, 0, e)) - model.sdf(p - (0, 0, e))], -1)
        n = nrm(n)
        rgb[hi] = model.shade(p, n, np.tile(dl, (len(hi), 1)))
    shad = np.zeros(len(o), bool)
    if shadow:
        tg = o[:, 2] / max(-dl[2], 1e-4)
        g = o + dl * tg[:, None]
        so = getattr(model, 'shadow_off', (0.25, -0.15))
        fd = model.footprint(g[:, 0] + so[0], g[:, 1] + so[1])
        shad = (~hit) & (fd < 0.12)
        cshad = (~hit) & (fd < getattr(model, 'core_shadow', -9.0))   # solid contact shadow right under the body
    if hasattr(model, 'ground_light'):
        tg = o[:, 2] / max(-dl[2], 1e-4)
        g = o + dl * tg[:, None]
        gcol, gint = model.ground_light(g[:, 0], g[:, 1])
        jj, ii = np.mgrid[0:H, 0:w_sq]
        thr = (BAYER4[jj & 3, ii & 3].reshape(-1) + 0.5) / 16.0
        glow = (~hit) & (gint > thr)
        rgb[glow] = gcol[glow]
        hit = hit | glow
        shad &= ~glow
    if core:
        if not shadow: cshad = np.zeros(len(o), bool)
        return rgb.reshape(H, w_sq, 3), hit.reshape(H, w_sq), shad.reshape(H, w_sq), (cshad & ~hit).reshape(H, w_sq)
    return rgb.reshape(H, w_sq, 3), hit.reshape(H, w_sq), shad.reshape(H, w_sq)
