# Natural-looking original human models, original street-racing cars, blood art.
import numpy as np
from sprites_sdf import (f32, length, nrm, sd_round_box, sd_capsule, sd_sphere, sd_ycyl,
                         plane, smin, lighting, env, L)

def sd_ellipsoid(p, c, r):
    r = np.asarray(r, f32)
    q = p - np.asarray(c, f32)
    k0 = length(q / r)
    k1 = length(q / (r * r))
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)

def sd_round_cone(p, a, b, ra, rb):
    """Tapered capsule from a (radius ra) to b (radius rb)."""
    a = np.asarray(a, f32); b = np.asarray(b, f32)
    ba = b - a
    l2 = float((ba * ba).sum())
    rr = ra - rb
    a2 = l2 - rr * rr
    il2 = 1.0 / l2
    pa = p - a
    y = (pa * ba).sum(-1)
    z = y - l2
    xv = pa * l2 - y[..., None] * ba
    x2 = (xv * xv).sum(-1)
    y2 = y * y * l2
    z2 = z * z * l2
    k = np.sign(rr) * rr * rr * x2
    d1 = np.sqrt(x2 + z2) * il2 - rb
    d2 = np.sqrt(x2 + y2) * il2 - ra
    d3 = (np.sqrt(x2 * a2 * il2) + y * rr) * il2 - ra
    return np.where(np.sign(z) * a2 * z2 > k, d1, np.where(np.sign(y) * a2 * y2 < k, d2, d3))

def rot_y(v, a):
    c, s = np.cos(a), np.sin(a)
    return np.array((v[0] * c + v[2] * s, v[1], -v[0] * s + v[2] * c), f32)

OUTFITS = {
    # top, shirt, legs, shoes, skin, hair, long hair, cap
    'player': dict(top=(0.13, 0.14, 0.16), shirt=(0.60, 0.18, 0.15), legs=(0.33, 0.31, 0.26), shoes=(0.28, 0.17, 0.09),
                   skin=(0.68, 0.49, 0.37), hair=(0.06, 0.05, 0.05), long=False),
    'grey':   dict(top=(0.38, 0.39, 0.41), shirt=(0.82, 0.82, 0.80), legs=(0.10, 0.10, 0.11), shoes=(0.05, 0.05, 0.05),
                   skin=(0.42, 0.29, 0.21), hair=(0.05, 0.04, 0.04), long=False),
    'red':    dict(top=(0.58, 0.11, 0.12), shirt=(0.58, 0.11, 0.12), legs=(0.14, 0.17, 0.27), shoes=(0.55, 0.40, 0.30),
                   skin=(0.86, 0.68, 0.57), hair=(0.40, 0.24, 0.12), long=True),
    'navy':   dict(top=(0.12, 0.15, 0.27), shirt=(0.12, 0.15, 0.27), legs=(0.26, 0.31, 0.43), shoes=(0.85, 0.85, 0.84),
                   skin=(0.72, 0.53, 0.41), hair=(0.70, 0.57, 0.33), long=False),
    'suit':   dict(top=(0.18, 0.18, 0.20), shirt=(0.85, 0.86, 0.88), legs=(0.18, 0.18, 0.20), shoes=(0.04, 0.04, 0.04),
                   skin=(0.55, 0.38, 0.28), hair=(0.03, 0.03, 0.03), long=True),
    'green':  dict(top=(0.24, 0.28, 0.16), shirt=(0.70, 0.66, 0.55), legs=(0.20, 0.24, 0.34), shoes=(0.35, 0.25, 0.16),
                   skin=(0.62, 0.44, 0.32), hair=(0.20, 0.13, 0.08), long=False),
    'white':  dict(top=(0.84, 0.84, 0.82), shirt=(0.84, 0.84, 0.82), legs=(0.06, 0.06, 0.07), shoes=(0.80, 0.78, 0.75),
                   skin=(0.80, 0.62, 0.50), hair=(0.82, 0.70, 0.42), long=True),
    'cop':    dict(top=(0.08, 0.10, 0.22), shirt=(0.08, 0.10, 0.22), legs=(0.06, 0.07, 0.15), shoes=(0.03, 0.03, 0.03),
                   skin=(0.64, 0.46, 0.34), hair=(0.05, 0.04, 0.04), long=False, cap=True),
    # an older man (v0.42): older=True gives a heavier middle, a slight stoop, thinning hair and a moustache
    'bohdan': dict(top=(0.44, 0.33, 0.22), shirt=(0.74, 0.70, 0.60), legs=(0.24, 0.22, 0.20), shoes=(0.22, 0.14, 0.08),
                   skin=(0.80, 0.62, 0.52), hair=(0.74, 0.73, 0.71), long=False, older=True),
    # women (v0.40): female=True gives the women's body; skirt=True a knee-length skirt with bare legs below
    'dee':    dict(top=(0.10, 0.13, 0.26), shirt=(0.88, 0.88, 0.86), legs=(0.12, 0.12, 0.14), shoes=(0.05, 0.04, 0.04),
                   skin=(0.36, 0.23, 0.16), hair=(0.04, 0.03, 0.03), long=False, female=True, bob=True),
    'marta':  dict(top=(0.60, 0.12, 0.12), shirt=(0.15, 0.14, 0.15), legs=(0.18, 0.24, 0.40), shoes=(0.30, 0.20, 0.12),
                   skin=(0.85, 0.66, 0.54), hair=(0.45, 0.20, 0.10), long=True, female=True),
    'woman_a': dict(top=(0.80, 0.62, 0.15), shirt=(0.80, 0.62, 0.15), legs=(0.12, 0.10, 0.14), shoes=(0.45, 0.10, 0.10),
                   skin=(0.62, 0.43, 0.31), hair=(0.12, 0.08, 0.05), long=True, female=True, skirt=True),
    'woman_b': dict(top=(0.15, 0.48, 0.50), shirt=(0.92, 0.92, 0.90), legs=(0.36, 0.37, 0.40), shoes=(0.85, 0.84, 0.80),
                   skin=(0.88, 0.71, 0.60), hair=(0.80, 0.66, 0.38), long=True, female=True, skirt=True),
    # club dancers (v0.49): bikini=True leaves the body bare apart from a small top and bottom (in 'top') and heels
    'dancer_a': dict(top=(0.90, 0.10, 0.45), shirt=(0.90, 0.10, 0.45), legs=(0.90, 0.10, 0.45), shoes=(0.85, 0.08, 0.30),
                   skin=(0.84, 0.64, 0.52), hair=(0.14, 0.08, 0.05), long=True, female=True, bikini=True),
    'dancer_b': dict(top=(0.15, 0.80, 0.85), shirt=(0.15, 0.80, 0.85), legs=(0.15, 0.80, 0.85), shoes=(0.92, 0.92, 0.95),
                   skin=(0.50, 0.33, 0.23), hair=(0.05, 0.04, 0.04), long=True, female=True, bikini=True),
    'dancer_c': dict(top=(0.10, 0.08, 0.10), shirt=(0.10, 0.08, 0.10), legs=(0.10, 0.08, 0.10), shoes=(0.08, 0.06, 0.08),
                   skin=(0.90, 0.74, 0.64), hair=(0.92, 0.80, 0.50), long=True, female=True, bikini=True),
}

def wrapa(a): return (a + np.pi) % (2 * np.pi) - np.pi
def bump(phi, c, w): return np.exp(-(wrapa(phi - c) / w) ** 2)

GAIT = {   # hip flexion, knee flexion curves (radians) for one leg over its cycle
    'walk': dict(hip=lambda f: 0.36 * np.cos(f) + 0.03,
                 knee=lambda f: 0.20 * bump(f, 0.55, 0.45) + 1.05 * bump(f, 4.35, 0.55) + 0.05,
                 push=lambda f: 0.35 * bump(f, 3.45, 0.40),
                 sh=lambda f: -0.34 * np.cos(f) + 0.02, el=lambda f: 0.22 + 0.38 * max(0.0, -np.cos(f)),
                 tw=0.07, lean=0.04, lift=0.0),
    'run':  dict(hip=lambda f: 0.58 * np.cos(f) + 0.12,
                 knee=lambda f: 0.38 * bump(f, 0.55, 0.35) + 1.50 * bump(f, 4.05, 0.75) + 0.12,
                 push=lambda f: 0.50 * bump(f, 3.0, 0.40),
                 sh=lambda f: -0.60 * np.cos(f) + 0.12, el=lambda f: 1.15 + 0.20 * max(0.0, -np.cos(f)),
                 tw=0.13, lean=0.14, lift=0.03),
}

class Human:
    def __init__(self, outfit, phase=0.0, moving=True, down=False, down_seed=0, mode=None, gun=False, tumble=0.0, weapon='pistol'):
        o = OUTFITS[outfit]
        self.o = {k: np.array(v, f32) for k, v in o.items() if k not in ('long', 'cap', 'female', 'skirt', 'bob', 'older', 'bikini')}
        self.older = o.get('older', False)
        self.long = o['long']; self.cap = o.get('cap', False)
        self.fem = o.get('female', False); self.skirt = o.get('skirt', False); self.bob = o.get('bob', False)
        self.bikini = o.get('bikini', False)
        sway = 0.0
        if mode is None: mode = 'down' if down else ('walk' if moving else 'idle')
        self.down = down or mode == 'down'
        self.mode = mode
        self.tumble = tumble if mode == 'tumble' else None   # ragdoll: limbs flung out, body pitched by this angle
        self.gun = gun or mode == 'aim'
        self.weapon = weapon if mode == 'aim' else None
        long_gun = self.weapon in ('smg', 'shotgun', 'rifle', 'rocket')
        legs_rel, arms_spec = [], []
        lean = 0.0; lift = 0.0; twist = 0.0
        for sg in (1, -1):                         # sg=+1 left side (y>0), -1 right
            if mode == 'down':
                rng = np.random.default_rng(down_seed * 7 + (sg > 0))
                th = rng.uniform(-0.15, 0.35); kn = rng.uniform(0.0, 0.9); push = 0.0
                spread = 0.10 + rng.uniform(0, 0.12)
                ar_out = rng.uniform(0.5, 1.3); ar_fwd = rng.uniform(-0.6, 0.6); eb = rng.uniform(0.1, 1.0)
            elif mode == 'tumble':                    # flung through the air: limbs splayed and flailing
                th = (0.55 if sg > 0 else -0.25); kn = (0.9 if sg > 0 else 0.35); push = 0.0
                spread = 0.16 if sg > 0 else 0.22
                ar_out = 1.35 if sg > 0 else 1.0; ar_fwd = (2.3 if sg > 0 else -0.9); eb = 0.5
            elif mode in GAIT:
                g = GAIT[mode]
                f = phase + (0.0 if sg > 0 else np.pi)
                th = g['hip'](f); kn = g['knee'](f); push = g['push'](f)
                spread = 0.0; ar_out = 0.10
                ar_fwd = g['sh'](f); eb = g['el'](f)        # arm swings opposite to the same-side leg
                lean = g['lean']; lift = g['lift'] * max(0.0, np.cos(2 * phase))
                twist = g['tw'] * np.cos(phase)             # hips turn with the left leg, shoulders counter-turn
            elif mode == 'aim' and long_gun:     # two-handed: bladed stance, left foot forward
                th = 0.20 if sg > 0 else -0.10; kn = 0.18 if sg > 0 else 0.08; push = 0.0
                spread = 0.09 if self.weapon == 'rocket' else 0.06
                ar_fwd, ar_out, eb = 0.5, 0.1, 0.5           # replaced by the stance below
                twist = {'smg': 0.12, 'shotgun': 0.22, 'rifle': 0.24, 'rocket': 0.16}[self.weapon]
            elif mode == 'aim':
                th = 0.10 * sg; kn = 0.10; push = 0.0; spread = 0.05
                if sg < 0:  ar_fwd, ar_out, eb = 1.50, -0.12, 0.02      # right arm straight out
                else:       ar_fwd, ar_out, eb = 1.15, -0.55, 0.45      # left hand supports
            elif mode == 'dance':     # club dance: weight shifts side to side, knees dip in turn, arms up and round
                ph = phase + (0.0 if sg > 0 else np.pi)
                th = 0.05 * np.sin(phase) * sg; kn = 0.10 + 0.22 * max(0.0, np.sin(ph)); push = 0.0; spread = 0.075
                if sg > 0:  ar_fwd, ar_out, eb = 2.55 + 0.30 * np.sin(phase), 0.35 + 0.15 * np.cos(phase), 0.55 + 0.25 * np.sin(phase)   # left arm up
                else:       ar_fwd, ar_out, eb = 1.20 + 1.10 * max(0.0, np.sin(phase + 1.2)), 0.45, 1.10 - 0.5 * max(0.0, np.sin(phase + 1.2))  # right: hip to overhead
                sway = 0.055 * np.sin(phase); twist = 0.10 * np.sin(phase * 2.0); lean = -0.03
            else:   # idle: a slow breath and weight shift over the cycle (phase 0 = the old still pose)
                bs = np.sin(phase)
                th = 0.03 * sg + 0.025 * bs; kn = (0.06 if sg > 0 else 0.02) + 0.03 * max(0.0, sg * bs); push = 0.0; spread = 0.045
                ar_fwd, ar_out, eb = 0.02 + 0.03 * bs * sg, 0.11 + 0.02 * bs, 0.24 + 0.04 * bs
                lean = 0.018 * bs; twist = 0.045 * bs
            if self.older and mode not in ('down', 'tumble'): lean += 0.06 if sg > 0 else 0.0   # a slight stoop
            hip = np.array((sg * 0.098 * np.sin(twist), sg * 0.098 * np.cos(twist), -0.045), f32)
            knee = hip + np.array((np.sin(th), sg * spread, -np.cos(th)), f32) * 0.43
            sha = th - kn
            ankle = knee + np.array((np.sin(sha), sg * spread * 0.5, -np.cos(sha)), f32) * 0.415
            fa = sha + np.pi / 2 - push                      # foot roughly perpendicular to the shin
            if mode == 'down': fa = np.radians(60)
            fdir = np.array((np.sin(fa), 0, -np.cos(fa)), f32)
            heel = ankle - fdir * 0.05 + np.array((0, 0, -0.045), f32)
            toe = ankle + fdir * 0.17 + np.array((0, 0, -0.05), f32)
            legs_rel.append([hip, knee, ankle, heel, toe])
            arms_spec.append((sg, ar_fwd, ar_out, eb))
        self.twist = twist
        if sway:                                     # hips shift sideways over planted feet
            for l in legs_rel:
                l[1] = l[1] - np.array((0, sway * 0.5, 0), f32)
                for k in (2, 3, 4): l[k] = l[k] - np.array((0, sway, 0), f32)
        # pelvis height: put the lowest foot on the ground (plus flight lift when running)
        if mode in ('down', 'tumble'):
            hz = 0.95
        else:
            lowest = min(min(l[3][2], l[4][2]) for l in legs_rel)
            hz = -lowest + 0.035 + lift
        self.hz = hz
        P = np.array((0, sway, hz), f32)
        def leaned(v):
            x, z = v[0], v[2]
            return np.array((x * np.cos(lean) + z * np.sin(lean), v[1] - sway * 0.6 * z / 0.7, -x * np.sin(lean) + z * np.cos(lean)), f32) + P
        J = {}
        J['pelvis'] = P.copy()
        J['waist'] = leaned(np.array((0.005, 0, 0.18), f32))
        J['chest'] = leaned(np.array((0.01, 0, 0.355), f32))
        J['neck0'] = leaned(np.array((0.0, 0, 0.53), f32))
        J['neck1'] = leaned(np.array((0.018, 0, 0.60), f32))
        J['head'] = leaned(np.array((0.012, 0, 0.705), f32))
        legs = [[v + P for v in l] for l in legs_rel]
        arms = []
        for sg, ar_fwd, ar_out, eb in arms_spec:
            ts = -twist * 1.2                             # shoulders counter-rotate against the hips
            sw = 0.165 if self.fem else 0.19              # women: narrower shoulders
            shoulder = leaned(np.array((-sg * sw * np.sin(ts), sg * sw * np.cos(ts), 0.455), f32))
            up_dir = np.array((np.sin(ar_fwd) * np.cos(ar_out), sg * np.sin(ar_out), -np.cos(ar_fwd) * np.cos(ar_out)), f32)
            up_dir /= np.linalg.norm(up_dir)
            elbow = shoulder + up_dir * 0.28
            fa = ar_fwd + eb
            fa_dir = np.array((np.sin(fa) * np.cos(ar_out), sg * np.sin(ar_out) * 0.8, -np.cos(fa) * np.cos(ar_out)), f32)
            fa_dir /= np.linalg.norm(fa_dir)
            wrist = elbow + fa_dir * 0.25
            hand = wrist + fa_dir * 0.05
            arms.append((shoulder, elbow, wrist, hand))
        self.J, self.legs, self.arms = J, legs, arms
        # pistol in the right hand, pointing along the forearm
        sh, el, wr, hd = arms[1]
        d = (wr - el) / np.linalg.norm(wr - el)
        self.gun_a = hd - d * 0.02
        self.gun_b = hd + d * 0.15
        if long_gun: self._stance(P)

    # ---- two-handed weapon stances: hands placed on the weapon, elbows solved with 2-bone IK
    GUNS = {   # weapon: (right-hand grip, left-hand support, right elbow pole, left elbow pole)
        'smg':     ((0.30, -0.07, 0.43), (0.46, -0.03, 0.42), (-0.2, -0.7, -0.7), (0.0, 0.4, -1.0)),
        'shotgun': ((0.21, -0.11, 0.48), (0.54, -0.06, 0.48), (-0.3, -0.8, -0.5), (0.0, 0.3, -1.0)),
        'rifle':   ((0.22, -0.11, 0.49), (0.53, -0.06, 0.50), (-0.3, -0.8, -0.5), (0.0, 0.3, -1.0)),
        'rocket':  ((0.14, -0.17, 0.47), (0.40, -0.13, 0.49), (-0.2, -0.6, -0.8), (0.0, 0.3, -1.0)),
    }

    def _stance(self, P):
        rg, lg, rp, lp = self.GUNS[self.weapon]
        arms = []
        for (sg, (shoulder, _, _, _)) in zip((1, -1), self.arms):
            H = np.array(lg if sg > 0 else rg, f32) + P
            pole = np.array(lp if sg > 0 else rp, f32)
            l1, l2 = 0.28, 0.30
            d = H - shoulder; L = float(np.linalg.norm(d))
            if L > l1 + l2 - 0.005:
                H = shoulder + d / L * (l1 + l2 - 0.005); d = H - shoulder; L = l1 + l2 - 0.005
            u = d / L
            ca = np.clip((l1 * l1 + L * L - l2 * l2) / (2 * l1 * L), -1, 1)
            pv = pole - (pole @ u) * u; pv /= np.linalg.norm(pv)
            elbow = shoulder + u * l1 * ca + pv * l1 * np.sqrt(1 - ca * ca)
            fd = (H - elbow) / np.linalg.norm(H - elbow)
            arms.append((shoulder, elbow, H - fd * 0.05, H))
        self.arms = arms

    def _gun_sdf(self, p):
        P = np.array((0, 0, self.hz), f32)
        def box(c, b, r=0.008): return sd_round_box(p, np.array(c, f32) + P, b, r)
        def rod(a, b, ra, rb): return sd_round_cone(p, np.array(a, f32) + P, np.array(b, f32) + P, ra, rb)
        w = self.weapon
        if w == 'smg':
            g = box((0.40, -0.05, 0.45), (0.12, 0.022, 0.034))
            g = np.minimum(g, box((0.37, -0.05, 0.38), (0.018, 0.014, 0.05)))            # magazine
            g = np.minimum(g, rod((0.52, -0.05, 0.455), (0.62, -0.05, 0.455), 0.014, 0.012))
            g = np.minimum(g, box((0.24, -0.06, 0.44), (0.05, 0.012, 0.022)))            # folded stock
        elif w == 'rifle':
            g = box((0.07, -0.11, 0.505), (0.075, 0.022, 0.042))                          # stock
            g = np.minimum(g, box((0.30, -0.09, 0.53), (0.17, 0.024, 0.034)))             # receiver
            g = np.minimum(g, rod((0.46, -0.08, 0.535), (1.00, -0.07, 0.54), 0.015, 0.012))   # barrel
            g = np.minimum(g, rod((0.20, -0.09, 0.585), (0.40, -0.088, 0.585), 0.021, 0.021)) # scope
            g = np.minimum(g, box((0.34, -0.09, 0.465), (0.02, 0.014, 0.045)))            # magazine
        elif w == 'shotgun':
            g = box((0.07, -0.11, 0.50), (0.075, 0.024, 0.044))
            g = np.minimum(g, box((0.27, -0.09, 0.515), (0.11, 0.025, 0.036)))
            g = np.minimum(g, rod((0.37, -0.08, 0.53), (0.84, -0.065, 0.535), 0.021, 0.020))  # wide barrel
            g = np.minimum(g, box((0.55, -0.07, 0.50), (0.075, 0.027, 0.027)))            # pump
        else:   # rocket launcher: a long tube over the right shoulder
            g = rod((-0.46, -0.17, 0.565), (0.60, -0.17, 0.565), 0.058, 0.058)
            g = np.minimum(g, rod((0.56, -0.17, 0.565), (0.67, -0.17, 0.565), 0.070, 0.070))   # muzzle flare
            g = np.minimum(g, rod((-0.52, -0.17, 0.565), (-0.44, -0.17, 0.565), 0.066, 0.066)) # rear flare
            g = np.minimum(g, box((0.14, -0.17, 0.49), (0.02, 0.016, 0.05)))              # grip
            g = np.minimum(g, box((0.22, -0.17, 0.64), (0.03, 0.01, 0.025)))              # sight
        return g

    def _local(self, p):
        if self.tumble is not None:
            # pitch the whole body head-over-heels about its middle; the middle sits 1 m up so every
            # angle stays above the ground plane (the game draws it at the right height)
            c, s_ = np.cos(self.tumble), np.sin(self.tumble)
            x, z = p[..., 0], p[..., 2] - 1.0
            q = np.empty_like(p)
            q[..., 0] = x * c - z * s_
            q[..., 1] = p[..., 1]
            q[..., 2] = x * s_ + z * c + 0.95
            return q
        if not self.down:
            return p
        # lying on the back along +x (head towards +x), back resting on the ground
        q = np.empty_like(p)
        q[..., 0] = p[..., 2] - 0.115          # body forward  <- world up
        q[..., 1] = p[..., 1]
        q[..., 2] = p[..., 0] + 0.90           # body up       <- world x
        return q

    def parts(self, p):
        p = self._local(p)
        J = self.J
        if self.fem:                                 # women: a defined waist, fuller hips, a bust
            pelvis = sd_ellipsoid(p, J['pelvis'], (0.118, 0.186, 0.122))
            if self.bikini:                          # club dancers: curvier, with wider hips and a full, round rear
                pelvis = sd_ellipsoid(p, J['pelvis'], (0.124, 0.205, 0.126))
                glutes = np.minimum(sd_ellipsoid(p, J['pelvis'] + np.array((-0.072, 0.064, -0.035), f32), (0.092, 0.084, 0.098)),
                                    sd_ellipsoid(p, J['pelvis'] + np.array((-0.072, -0.064, -0.035), f32), (0.092, 0.084, 0.098)))
                pelvis = smin(pelvis, glutes, 0.035)
            waist = sd_ellipsoid(p, J['waist'], (0.094, 0.132, 0.12))
            chest = sd_ellipsoid(p, J['chest'], (0.116, 0.158, 0.150))
            if self.bikini:                          # club dancers: a fuller bust
                bust = np.minimum(sd_ellipsoid(p, J['chest'] + np.array((0.082, 0.058, -0.022), f32), (0.074, 0.068, 0.070)),
                                  sd_ellipsoid(p, J['chest'] + np.array((0.082, -0.058, -0.022), f32), (0.074, 0.068, 0.070)))
            else:
                bust = np.minimum(sd_ellipsoid(p, J['chest'] + np.array((0.068, 0.052, -0.012), f32), (0.058, 0.056, 0.056)),
                                  sd_ellipsoid(p, J['chest'] + np.array((0.068, -0.052, -0.012), f32), (0.058, 0.056, 0.056)))
            self._bust = bust
            chest = smin(chest, bust, 0.04)
            torso = smin(smin(waist, chest, 0.06), pelvis, 0.06)
        else:
            pelvis = sd_ellipsoid(p, J['pelvis'], (0.112, 0.172, 0.115))
            waist = sd_ellipsoid(p, J['waist'], (0.108, 0.160, 0.13))
            chest = sd_ellipsoid(p, J['chest'], (0.128, 0.188, 0.160))
            torso = smin(smin(waist, chest, 0.07), pelvis, 0.06)
        if self.older:                               # a comfortable middle
            belly = sd_ellipsoid(p, J['waist'] + np.array((0.045, 0, 0.0), f32), (0.115, 0.150, 0.125))
            torso = smin(torso, belly, 0.05)
        neck = sd_round_cone(p, J['neck0'], J['neck1'], 0.056, 0.050)
        h = J['head']
        head = sd_ellipsoid(p, h, (0.103, 0.085, 0.118))
        jaw = sd_ellipsoid(p, h + (0.022, 0, -0.050), (0.070, 0.060, 0.062))
        nose = sd_ellipsoid(p, h + (0.090, 0, -0.012), (0.022, 0.012, 0.024))
        ears = np.minimum(sd_ellipsoid(p, h + (-0.005, 0.077, -0.005), (0.018, 0.010, 0.028)),
                          sd_ellipsoid(p, h + (-0.005, -0.077, -0.005), (0.018, 0.010, 0.028)))
        face = smin(smin(head, jaw, 0.03), np.minimum(nose, ears), 0.01)
        hair = sd_ellipsoid(p, h + (-0.012, 0, 0.022), (0.102, 0.085, 0.108))
        hair = np.maximum(hair, plane(p, h + (0.055, 0, 0.012), (0.85, 0, -0.55)))
        if self.older:                               # receding: a bare crown, grey at the sides and back, and a moustache
            hair = np.maximum(hair, -np.maximum(sd_ellipsoid(p, h + (0.03, 0, 0.07), (0.075, 0.060, 0.06)), h[2] + 0.035 - p[..., 2]))
            tash = sd_ellipsoid(p, h + (0.088, 0, -0.035), (0.018, 0.034, 0.011))
            hair = np.minimum(hair, tash)
        if self.cap:
            crown = np.maximum(sd_ellipsoid(p, h + (-0.005, 0, 0.045), (0.108, 0.092, 0.085)), h[2] + 0.015 - p[..., 2])
            brim = sd_ellipsoid(p, h + (0.085, 0, 0.03), (0.075, 0.085, 0.012))
            hair = np.minimum(crown, brim)
        if self.long:
            if self.fem:                             # shoulder-length hair, falling behind the shoulders
                back = sd_round_box(p, h + (-0.055, 0, -0.15), (0.05, 0.095, 0.17), 0.045)
                back = np.maximum(back, -sd_ellipsoid(p, h + (0.03, 0, -0.06), (0.08, 0.072, 0.13)))
            else:
                back = sd_round_box(p, h + (-0.06, 0, -0.10), (0.045, 0.085, 0.12), 0.04)
                back = np.maximum(back, -sd_ellipsoid(p, h + (0.02, 0, -0.04), (0.08, 0.07, 0.12)))
            hair = smin(hair, back, 0.03)
        if self.bob:                                 # a chin-length bob
            bob = sd_ellipsoid(p, h + (-0.015, 0, -0.015), (0.108, 0.098, 0.120))
            bob = np.maximum(bob, -sd_ellipsoid(p, h + (0.06, 0, -0.03), (0.075, 0.075, 0.10)))
            bob = np.maximum(bob, h[2] - 0.085 - p[..., 2])
            hair = smin(hair, bob, 0.02)
        legd = np.full(p.shape[:-1], 9, f32); shoe = legd.copy(); arm = legd.copy(); hand = legd.copy()
        lk = 0.92 if self.fem else 1.0               # women: slimmer limbs
        for hip, knee, ankle, heel, toe in self.legs:
            thigh = sd_round_cone(p, hip, knee, (0.106 if self.bikini else 0.096) * lk, 0.064 * lk)
            shin = sd_round_cone(p, knee, ankle, 0.060 * lk, 0.044 * lk)
            calf = sd_ellipsoid(p, knee + (ankle - knee) * 0.30 + np.array((-0.012, 0, 0), f32), (0.056, 0.055, 0.12))
            legd = np.minimum(legd, smin(smin(thigh, shin, 0.03), calf, 0.04))
            sh = sd_round_cone(p, heel, toe, 0.046, 0.038)
            sh = np.maximum(sh, -(p[..., 2] - (heel[2] - 0.035)) if not self.down else sh)
            shoe = np.minimum(shoe, sh)
        for shoulder, elbow, wrist, hnd in self.arms:
            up = sd_round_cone(p, shoulder, elbow, 0.064 * lk, 0.050 * lk)
            fo = sd_round_cone(p, elbow, wrist, 0.049 * lk, 0.040 * lk)
            arm = np.minimum(arm, smin(up, fo, 0.025))
            # a fist: one rounded lump at the end of the sleeve, never a thin paddle sticking out
            hand = np.minimum(hand, sd_ellipsoid(p, hnd + (hnd - wrist) * 0.15, (0.047, 0.044, 0.047)))
        # deltoids: low, wide ellipsoids that melt into the chest and the upper arm (no bumps on top)
        delts = 9
        for sh_, el_, _, _ in self.arms:
            c = sh_ + (el_ - sh_) * 0.15 + np.array((0, 0, -0.010), f32)
            delts = np.minimum(delts, sd_ellipsoid(p, c, (0.066 * lk, 0.064 * lk, 0.066 * lk)))
            # trapezius: a sloping ridge from the base of the neck down to the shoulder
            base = J['neck0'] + np.array((-0.01, 0, 0.0), f32)
            delts = np.minimum(delts, sd_round_cone(p, base, sh_ + np.array((-0.005, 0, 0.0), f32), 0.058, 0.050))
        top = smin(smin(torso, delts, 0.06), arm, 0.04)
        bare = np.full(p.shape[:-1], 9, f32)
        if self.skirt:                               # a flared skirt to just above the knee; bare legs below
            knees = (self.legs[0][1] + self.legs[1][1]) * 0.5
            hem = knees + np.array((0, 0, 0.07), f32)
            skirt = sd_round_cone(p, J['pelvis'] + np.array((0, 0, 0.02), f32), hem, 0.17, 0.215)
            skirt = np.maximum(skirt, hem[2] - p[..., 2])
            hips_legs = smin(pelvis, skirt, 0.03)
            bare = np.maximum(legd, p[..., 2] - hem[2] - 0.01)
        else:
            hips_legs = smin(pelvis, legd, 0.04)
        gun = sd_round_cone(p, self.gun_a, self.gun_b, 0.022, 0.018) if self.gun else np.full(p.shape[:-1], 9, f32)
        if self.weapon not in (None, 'pistol'):
            gun = self._gun_sdf(p)
        elif self.gun:
            grip = sd_round_cone(p, self.gun_a, self.gun_a + np.array((0, 0, -0.08), f32), 0.018, 0.016)
            gun = np.minimum(gun, grip)
        bustd = self._bust if self.fem else np.full(p.shape[:-1], 9, f32)
        return dict(bust=bustd, top=top, legs=hips_legs, shoes=np.minimum(shoe, gun) if False else shoe, gun=gun, skin=np.minimum(np.minimum(smin(face, neck, 0.03), hand), bare),
                    hair=hair, torso=torso, pelvis=pelvis, arm=arm, local=p, bare=bare)

    def sdf(self, p):
        d = self.parts(p)
        return np.minimum(np.minimum(np.minimum(np.minimum(d['top'], d['legs']), np.minimum(d['shoes'], d['skin'])), d['hair']), d['gun'])

    def footprint(self, gx, gy):
        if self.down:
            return np.sqrt((gx / 0.75) ** 2 + (gy / 0.28) ** 2) * 0.28 - 0.28
        return np.sqrt(gx ** 2 * 0.9 + gy ** 2) - 0.21

    def shade(self, p, n, view):
        d = self.parts(p)
        q = d['local']
        names = ['top', 'legs', 'shoes', 'skin', 'hair', 'gun']
        st = np.stack([d[k] for k in names])
        idx = st.argmin(0)
        o = self.o
        hc = o['top'] if self.cap else o['hair']
        gcol = (0.20, 0.25, 0.13) if self.weapon == 'rocket' else (0.07, 0.07, 0.075)
        cols = np.array([o['top'], o['legs'], o['shoes'], o['skin'], hc, gcol], f32)
        alb = cols[idx].copy()
        hz = self.hz
        x, y, z = q[..., 0], q[..., 1], q[..., 2]
        # torso is pelvis-coloured below the jacket hem
        hem = (idx == 0) & (d['pelvis'] < d['top'] + 0.002) & (z < hz + 0.06)
        alb[hem] = o['legs']
        # open jacket shows the shirt; collar; belt
        chest = (idx == 0) & (d['torso'] <= d['arm'] + 0.004) & (x > 0.07) & (np.abs(y) < 0.045) & (z > hz + 0.12) & (z < hz + 0.50)
        alb[chest] = o['shirt']
        belt = (idx == 1) & (z > hz + 0.03) & (z < hz + 0.065)
        alb[belt] = (0.07, 0.06, 0.05)
        # face: brows/eyes and mouth
        h = self.J['head']
        fr = (idx == 3) & (x > h[0] + 0.07)
        eyes = fr & (np.abs(np.abs(y) - 0.032) < 0.015) & (np.abs(z - (h[2] + 0.008)) < 0.009)
        brow = fr & (np.abs(np.abs(y) - 0.032) < 0.022) & (np.abs(z - (h[2] + 0.027)) < 0.007)
        mouth = fr & (np.abs(y) < 0.022) & (np.abs(z - (h[2] - 0.058)) < 0.005)
        alb[eyes] = alb[eyes] * 0.35
        alb[brow] = o['hair'] * 0.9 + alb[brow] * 0.1
        alb[mouth] = alb[mouth] * 0.62
        if self.bikini:                              # bare skin apart from a small top round the bust and a bottom at the hips
            cz = self.J['chest'][2]
            body = (idx == 0) | (idx == 1)
            # a narrow bandeau over the bust and a small low-cut bottom
            onbody = body & (d['torso'] <= d['arm'] + 0.004)
            cups = onbody & (d['bust'] < 0.012) & (z > cz - 0.095) & (z < cz + 0.040)       # bikini cups over the bust
            strap = onbody & (z > cz + 0.004) & (z < cz + 0.020)                               # thin strap round the back
            topband = cups | strap
            bottom = body & (d['pelvis'] < 0.010) & (z < hz + 0.010) & (z > hz - 0.115) & ((x > -0.05) | (z < hz - 0.025) | (np.abs(y) < 0.03))
            covered = topband | bottom
            alb[body & ~covered] = o['skin']
            alb[covered] = o['top']
            idx = np.where(body & ~covered, 3, idx)
        # subtle cloth folds
        cloth = (idx == 0) | (idx == 1)
        fold = 0.93 + 0.07 * np.sin(z * 55 + np.sin(y * 30) * 2) * np.sin(x * 40 + z * 13)
        alb[cloth] *= fold[cloth, None]
        kind = np.full(len(z), 5, np.int32)
        col = lighting(alb, kind, n, view, p[..., 2] + 0.45)
        # skin & hair get a soft sheen, everything a cool rim light
        hv = nrm(L - view)
        spec = np.clip((n * hv).sum(-1), 0, 1)
        if self.cap:
            brimm = (idx == 4) & (p[..., 2] < self.J['head'][2] + 0.045) & (d['local'][..., 0] > self.J['head'][0] + 0.03)
            col[brimm] = col[brimm] * 0.4
        sk = (idx == 3) | ((idx == 4) & (not self.cap))
        col[sk] += (0.10 * spec[sk] ** 16)[:, None]
        rim = (1 - np.abs((n * view).sum(-1))) ** 3
        col += rim[:, None] * np.array((0.10, 0.11, 0.13), f32)
        return np.clip(col, 0, 1)

# ------------------------------------------------------------ street cars
# Original street-racing designs built from smooth-blended volumes: a low hull,
# flared wheel arches, a curved glass canopy, splitter, skirts, diffuser and wing.
CAR_SPECS = {
    #          hull centre z, half extents,        r,    wheel r, wheel x, wheel y, canopy centre,     canopy radii,        belt
    'exotic': dict(hz=0.44, hb=(2.22, 0.98, 0.21), hr=0.17, wr=0.35, wx=1.40, wy=0.86, cc=(-0.10, 0, 0.70), cr=(1.30, 0.68, 0.33), belt=0.70,
                   nose=((2.20, 0, 0.32), (0.42, 0, 1.0)), tail=((-1.9, 0, 0.70), (-0.10, 0, 1.0)), spokes=5, wing=(-2.02, 0.96)),
    'gt':     dict(hz=0.50, hb=(2.28, 0.96, 0.24), hr=0.17, wr=0.35, wx=1.42, wy=0.85, cc=(-0.45, 0, 0.80), cr=(1.40, 0.70, 0.34), belt=0.76,
                   nose=((1.55, 0, 0.76), (0.30, 0, 1.0)), tail=((-1.95, 0, 0.78), (-0.15, 0, 1.0)), spokes=10, wing=(-2.10, 1.10)),
    'muscle': dict(hz=0.56, hb=(2.40, 0.98, 0.29), hr=0.12, wr=0.36, wx=1.50, wy=0.86, cc=(-0.60, 0, 0.96), cr=(1.05, 0.72, 0.32), belt=0.86,
                   nose=((2.00, 0, 0.86), (0.08, 0, 1.0)), tail=((-2.05, 0, 0.86), (-0.05, 0, 1.0)), spokes=5, wing=None),
    'hatch':  dict(hz=0.52, hb=(2.06, 0.93, 0.28), hr=0.16, wr=0.33, wx=1.27, wy=0.82, cc=(-0.30, 0, 0.98), cr=(1.60, 0.74, 0.44), belt=0.82,
                   nose=((1.30, 0, 0.80), (0.33, 0, 1.0)), tail=((-1.95, 0, 0.95), (-0.9, 0, 0.45)), spokes=6, wing=(-1.92, 1.42)),
}

class SportCar:
    def __init__(self, kind, paint, accent, vinyl):
        self.kind = kind
        self.S = CAR_SPECS[kind]
        self.paint = np.array(paint, f32); self.accent = np.array(accent, f32); self.vinyl = vinyl
        S = self.S
        self.wr, self.wx, self.wy = S['wr'], S['wx'], S['wy']

    def parts(self, p):
        S = self.S
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        pm = np.stack([x, ay, z], -1)
        hb = S['hb']
        hull = sd_round_box(p, (0, 0, S['hz']), hb, S['hr'])
        hull = np.maximum(hull, plane(p, *S['nose']))
        hull = np.maximum(hull, plane(p, *S['tail']))
        hull = np.maximum(hull, plane(p, (hb[0], 0, 0.22), (0.7, 0, -0.7)))          # chin
        wr, wx, wy = S['wr'], S['wx'], S['wy']
        # flared arches blend into the hull
        arch = np.minimum(sd_ellipsoid(pm, (wx, wy - 0.06, wr + 0.10), (0.62, 0.20, 0.30)),
                          sd_ellipsoid(pm, (-wx, wy - 0.04, wr + 0.12), (0.66, 0.22, 0.32)))
        hull = smin(hull, arch, 0.12)
        if self.kind == 'muscle':
            cab = sd_round_box(p, S['cc'], S['cr'], 0.10)
            cab = np.maximum(cab, plane(p, (0.12, 0, 0.86), (0.62, 0, 0.78)))
            cab = np.maximum(cab, plane(p, (-1.30, 0, 0.86), (-0.55, 0, 0.83)))
            cab = np.maximum(cab, plane(pm, (0, 0.72, 0.86), (0, 1, 0.28)))
            scoop = sd_round_box(p, (1.05, 0, 0.87), (0.55, 0.22, 0.05), 0.04)
            hull = smin(hull, scoop, 0.05)
        elif self.kind == 'hatch':
            cab = sd_round_box(p, S['cc'], S['cr'], 0.13)
            cab = np.maximum(cab, plane(p, (0.70, 0, 0.82), (0.66, 0, 0.75)))
            cab = np.maximum(cab, plane(p, (-1.90, 0, 0.9), (-0.85, 0, 0.53)))
            cab = np.maximum(cab, plane(pm, (0, 0.76, 0.82), (0, 1, 0.30)))
        else:
            cab = sd_ellipsoid(p, S['cc'], S['cr'])
            cab = np.maximum(cab, -(z - S['belt'] + 0.12))
            cab = np.maximum(cab, z - (S['cc'][2] + S['cr'][2] * 0.72))       # flat roof
        body = smin(hull, cab, 0.10)
        if self.kind == 'exotic':
            intake = sd_ellipsoid(pm, (-0.55, 1.00, 0.55), (0.48, 0.14, 0.17))
            body = np.maximum(body, -intake)
            fin = sd_round_box(p, (-1.5, 0, 0.80), (0.45, 0.02, 0.06), 0.01)            # engine-cover spine
            body = np.minimum(body, fin)
        # splitter, skirts, diffuser
        split = sd_round_box(p, (hb[0] - 0.02, 0, 0.13), (0.16, hb[1] - 0.04, 0.025), 0.015)
        skirt = sd_round_box(pm, (0, hb[1] - 0.02, 0.17), (wx - wr - 0.12, 0.05, 0.05), 0.02)
        diff = sd_round_box(p, (-hb[0] + 0.05, 0, 0.18), (0.12, hb[1] - 0.15, 0.05), 0.02)
        aero = np.minimum(np.minimum(split, skirt), diff)
        if S['wing'] is not None:
            wxp, wz = S['wing']
            blade = sd_round_box(p, (wxp, 0, wz), (0.17, hb[1] - 0.06, 0.022), 0.018)
            blade = np.maximum(blade, plane(p, (wxp, 0, wz + 0.01), (0.25, 0, 1)))
            plates = sd_round_box(pm, (wxp, hb[1] - 0.07, wz - 0.02), (0.20, 0.012, 0.07), 0.01)
            struts = sd_round_box(pm, (wxp + 0.05, 0.55, (wz + S['hz'] + hb[2]) / 2), (0.05, 0.02, (wz - S['hz'] - hb[2]) / 2 + 0.02), 0.01)
            aero = np.minimum(aero, np.minimum(np.minimum(blade, plates), struts))
        well = np.maximum(np.minimum(np.sqrt((x - wx) ** 2 + (z - wr) ** 2), np.sqrt((x + wx) ** 2 + (z - wr) ** 2)) - (wr + 0.045),
                          0.52 - ay)
        body = np.maximum(body, -well)
        mirror = sd_round_box(pm, (S['cc'][0] + S['cr'][0] * 0.55, S['cr'][1] + 0.22, S['belt'] + 0.10), (0.07, 0.07, 0.04), 0.025)
        body = np.minimum(body, mirror)
        tire = np.minimum(sd_ycyl(pm, wx, wr, wr, wy - 0.15, wy + 0.13), sd_ycyl(pm, -wx, wr, wr, wy - 0.15, wy + 0.13)) - 0.0
        tire = tire  # rounded via the shading of sidewalls
        return body, aero, tire, cab, hull

    def sdf(self, p):
        b, a, t, _, _ = self.parts(p)
        return np.minimum(np.minimum(b, a), t)

    def footprint(self, gx, gy):
        hb = self.S['hb']
        q = np.stack([np.abs(gx) - (hb[0] - 0.1), np.abs(gy) - (hb[1] - 0.05)], -1)
        return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0)

    def shade(self, p, n, view):
        S = self.S
        body, aero, tire, cab, hull = self.parts(p)
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        N = len(x)
        hb = S['hb']
        alb = np.tile(self.paint, (N, 1))
        kind = np.zeros(N, np.int32)
        is_tire = (tire < body) & (tire < aero)
        is_aero = (aero < body) & ~is_tire
        is_body = ~is_tire & ~is_aero
        # glass: canopy surface above the belt line, not the roof top
        glass = is_body & (cab < hull + 0.01) & (z > S['belt'] + 0.025) & (n[..., 2] < 0.55)
        if self.kind == 'hatch': glass &= z < S['cc'][2] + S['cr'][2] - 0.04
        if self.kind in ('exotic', 'gt'):
            # windshield + side windows; leave a painted roof panel and rear buttress
            glass &= ~((x < S['cc'][0] - S['cr'][0] * 0.62) & (np.abs(n[..., 1]) > 0.3))   # rear buttress
            if self.kind == 'exotic': glass &= ~(n[..., 0] < -0.25)                        # engine cover, no rear glass
        if self.kind == 'muscle': glass &= ~((np.abs(x + 0.55) < 0.06) & (np.abs(n[..., 1]) > 0.5))
        apillar = glass & (np.abs(n[..., 1]) > 0.35) & (np.abs(n[..., 0]) > 0.35) & (n[..., 0] > 0)
        glass &= ~apillar
        # vinyl / graphics on paint
        paint = is_body & ~glass
        if self.vinyl == 'stripes':
            st = (np.abs(ay - 0.13) < 0.075) & (n[..., 2] > 0.2)
            alb[paint & st] = self.accent
        elif self.vinyl == 'blade':
            side = np.abs(n[..., 1]) > 0.5
            c0 = 0.30 + 0.30 * np.clip((x + 2.0) / 4.0, 0, 1)
            band = side & (z > c0) & (z < c0 + 0.09 + 0.10 * np.clip((x + 0.5) / 2.5, 0, 1)) & (x > -1.6)
            alb[paint & band] = self.accent
        elif self.vinyl == 'twotone':
            top = (n[..., 2] > 0.45) & (z > S['belt'] - 0.02)
            alb[paint & top] = self.accent
        elif self.vinyl == 'flames':
            side = np.abs(n[..., 1]) > 0.45
            fl = side & (x > 0.0) & (z > 0.30) & (z < 0.30 + 0.30 * np.clip(x / 1.8, 0, 1) * (0.6 + 0.4 * np.sin(x * 15) ** 2))
            alb[paint & fl] = self.accent
        # hood vents (dark slots) for exotic / gt
        if self.kind in ('exotic', 'gt'):
            vent = paint & (n[..., 2] > 0.7) & (x > 0.9) & (x < 1.5) & (np.abs(ay - 0.45) < 0.12) & (np.sin(x * 60) > 0.2)
            alb[vent] = (0.03, 0.03, 0.03); kind[vent] = 5
        if self.kind == 'exotic':
            intk = paint & (ay > 0.86) & (np.abs(x + 0.55) < 0.48) & (z > 0.40) & (z < 0.70) & (n[..., 1] * np.sign(y) < 0.3)
            alb[intk] = (0.03, 0.03, 0.035); kind[intk] = 5
        front = is_body & (x > hb[0] - 0.30)
        rear = is_body & (x < -hb[0] + 0.28)
        zl = S['hz'] + 0.04
        hl = front & (z > zl) & (z < zl + 0.10) & (ay > 0.42) & (ay < 0.88) & (n[..., 0] > 0.15)
        proj = hl & ((np.abs(ay - 0.58) < 0.05) | (np.abs(ay - 0.74) < 0.05))
        grille = front & (ay < 0.55) & (z > 0.20) & (z < zl - 0.02) & (n[..., 0] > 0.4)
        tl = rear & (z > zl) & (z < zl + 0.09) & (ay > 0.25) & (ay < 0.92) & (n[..., 0] < -0.15)
        exh = rear & (z < 0.30) & (np.abs(ay - 0.40) < 0.09) & (n[..., 0] < -0.5)
        alb[grille] = (0.03, 0.03, 0.035); kind[grille] = 5
        alb[hl] = (0.70, 0.72, 0.75); kind[hl] = 3
        alb[proj] = (0.97, 0.97, 0.99); kind[proj] = 4
        alb[tl] = (0.62, 0.02, 0.03); kind[tl] = 6
        alb[exh] = (0.55, 0.55, 0.58); kind[exh] = 3
        alb[glass] = (0.015, 0.02, 0.03); kind[glass] = 1
        alb[is_aero] = (0.07, 0.07, 0.08); kind[is_aero] = 7                         # carbon fibre
        # wheels: rounded tyre sidewall, chrome lip, spokes, red calipers
        rx = np.where(x > 0, x - S['wx'], x + S['wx'])
        rz = z - S['wr']
        rad = np.sqrt(rx ** 2 + rz ** 2)
        ang = np.arctan2(rz, rx)
        outer = is_tire & (ay > S['wy'] + 0.02)
        rim = outer & (rad < S['wr'] * 0.80)
        lip = rim & (rad > S['wr'] * 0.72)
        ns = S['spokes']
        sp_w = 0.55 if ns >= 10 else 0.25
        spoke = rim & ((np.cos(ang * ns) > sp_w) | (rad < S['wr'] * 0.20))
        caliper = rim & ~spoke & (rad > S['wr'] * 0.35) & (rad < S['wr'] * 0.62) & (np.abs(ang - (np.pi * 0.35 if True else 0)) < 0.7)
        alb[is_tire] = (0.05, 0.05, 0.05); kind[is_tire] = 2
        side_hl = is_tire & ~rim & (ay > S['wy']) & (rad > S['wr'] * 0.80)
        alb[side_hl] = (0.09, 0.09, 0.09)
        alb[rim] = (0.03, 0.03, 0.035); kind[rim] = 5
        alb[spoke] = (0.66, 0.67, 0.70); kind[spoke] = 3
        alb[lip] = (0.80, 0.81, 0.84); kind[lip] = 3
        alb[caliper] = (0.75, 0.06, 0.05); kind[caliper] = 5
        col = lighting(alb, kind, n, view, z)
        # deep metallic clear coat
        hv = nrm(L - view)
        spec = np.clip((n * hv).sum(-1), 0, 1)
        pm_ = kind == 0
        col[pm_] += (0.22 * spec[pm_] ** 8)[:, None] * (self.paint * 0.6 + 0.4)
        # subtle panel lines (doors, hood, bumper seams)
        seam = paint & ((np.abs(x - (S['cc'][0] + 0.35)) < 0.012) | (np.abs(x - (S['cc'][0] - 0.75)) < 0.012)) & (np.abs(n[..., 1]) > 0.5) & (z < S['belt'])
        col[seam] *= 0.6
        return np.clip(col, 0, 1)

# ------------------------------------------------------------------ blood
def blood_pool(size_m, elev, s, seed):
    """Flat blood pool on the ground as seen from elevation `elev`. Returns rgb, mask, anchor offsets."""
    rng = np.random.default_rng(seed)
    se = np.sin(elev)
    W = int(size_m * 2.4 / s) | 1
    H = int(size_m * 2.4 * se / s) + 3
    j, i = np.mgrid[0:H, 0:W].astype(f32)
    u = (i - W / 2 + 0.5) * s
    v = (H / 2 - j - 0.5) * s
    gy = -u
    gx = v / se
    r = np.sqrt(gx ** 2 + gy ** 2)
    a = np.arctan2(gy, gx)
    edge = size_m * (1 + 0.18 * np.sin(a * 3 + rng.uniform(0, 6)) + 0.10 * np.sin(a * 7 + rng.uniform(0, 6)))
    # a couple of satellite splats
    m = r < edge
    for _ in range(4):
        aa = rng.uniform(0, 6.28); rr = size_m * rng.uniform(1.05, 1.35); sr = size_m * rng.uniform(0.10, 0.22)
        m |= np.sqrt((gx - np.cos(aa) * rr) ** 2 + (gy - np.sin(aa) * rr) ** 2) < sr
    t = np.clip(r / np.maximum(edge, 1e-3), 0, 1)
    base = np.array((0.36, 0.02, 0.03), f32)
    col = base[None, None] * (0.75 + 0.35 * t[..., None])
    gloss = np.exp(-(((gx + size_m * 0.25) ** 2 + (gy - size_m * 0.2) ** 2) / (size_m * 0.18) ** 2))
    col = col + gloss[..., None] * np.array((0.30, 0.10, 0.10), f32)
    return np.clip(col, 0, 1), m, -(W // 2), -(H // 2)
