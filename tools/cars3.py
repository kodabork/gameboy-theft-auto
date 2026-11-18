# Cars, third generation: built from realistic generic proportions (no real make or model).
# Each class is described by a side profile (roof / hood / deck / rocker control points), a plan
# shape and a cross-section with tumblehome; details (glass, pillars, seams, lamps, grille, plates,
# mirrors, handles, wheels) are painted onto the surface in shade(), and the lighting adds
# SDF ambient occlusion and a clear-coat environment reflection.
import numpy as np
from sprites_sdf import nrm, length, f32

def smooth_table(xs, ys, x0, x1, sig):
    gx = np.linspace(x0, x1, 1200).astype(f32)
    gy = np.interp(gx, xs, ys).astype(f32)
    k = max(1, int(sig / ((x1 - x0) / 1200)))
    ker = np.exp(-0.5 * (np.arange(-3 * k, 3 * k + 1) / k) ** 2); ker /= ker.sum()
    pad = np.pad(gy, 3 * k, mode='edge')
    return gx, np.convolve(pad, ker, mode='valid').astype(f32)

# x: + front. Profiles: list of (x, z) points front to back. Lengths are real-world averages.
SPECS = {
    # boxy 90s compact sedan
    'compact': dict(L=4.45, W=1.72, wb=2.58, xf=1.30, wr=0.31, tw=0.20, zr=0.17, sharp=0.025,
                    top=[(2.23, 0.56), (2.20, 0.74), (2.10, 0.78), (1.05, 0.88), (0.95, 0.89), (0.30, 1.37), (-0.75, 1.39), (-1.30, 1.00), (-2.18, 0.97), (-2.23, 0.80)],
                    belt=[(0.95, 0.89), (-1.30, 0.99)], bot=[(2.23, 0.30), (2.00, 0.20), (-2.00, 0.20), (-2.23, 0.32)],
                    gh=0.86, nose=7.0, tail=7.0, lamp='rect', grille='slim', tail_l='wrap', sport=False),
    # long full-size sedan (cop / taxi archetype)
    'fullsize': dict(L=5.30, W=1.95, wb=2.95, xf=1.52, wr=0.33, tw=0.22, zr=0.19, sharp=0.025,
                     top=[(2.65, 0.60), (2.63, 0.80), (2.50, 0.84), (1.25, 0.92), (1.12, 0.93), (0.45, 1.40), (-0.85, 1.42), (-1.45, 1.03), (-2.60, 1.00), (-2.65, 0.82)],
                     belt=[(1.12, 0.93), (-1.45, 1.03)], bot=[(2.65, 0.32), (2.35, 0.22), (-2.30, 0.22), (-2.65, 0.34)],
                     gh=0.88, nose=9.0, tail=9.0, lamp='rect', grille='wide', tail_l='bar', sport=False),
    # boxy 5-door hatch
    'hatch5': dict(L=4.10, W=1.70, wb=2.50, xf=1.18, wr=0.30, tw=0.19, zr=0.17, sharp=0.025,
                   top=[(2.05, 0.58), (2.02, 0.74), (1.90, 0.78), (1.05, 0.88), (0.92, 0.90), (0.25, 1.42), (-1.55, 1.43), (-1.95, 1.35), (-2.05, 0.82)],
                   belt=[(0.92, 0.90), (-1.95, 0.98)], bot=[(2.05, 0.30), (1.80, 0.19), (-1.80, 0.19), (-2.05, 0.34)],
                   gh=0.86, nose=6.0, tail=10.0, lamp='rect', grille='slim', tail_l='hatch', sport=False),
    # 80s wedge sports coupe
    'gt': dict(L=4.40, W=1.78, wb=2.45, xf=1.15, wr=0.32, tw=0.23, zr=0.13, sharp=0.025,
               top=[(2.20, 0.50), (2.16, 0.62), (1.30, 0.80), (0.70, 0.86), (0.05, 1.24), (-0.55, 1.25), (-1.55, 0.98), (-2.15, 0.96), (-2.20, 0.82)],
               belt=[(0.70, 0.86), (-1.55, 0.99)], bot=[(2.20, 0.26), (1.90, 0.15), (-1.90, 0.15), (-2.20, 0.32)],
               gh=0.80, nose=4.5, tail=6.0, lamp='slit', grille='intake', tail_l='bar', sport=True),
    # mid-engine wedge exotic
    'super': dict(L=4.40, W=1.95, wb=2.60, xf=1.20, wr=0.34, tw=0.28, zr=0.11, sharp=0.025,
                  top=[(2.20, 0.42), (2.10, 0.52), (1.25, 0.70), (0.85, 0.74), (0.20, 1.10), (-0.45, 1.12), (-0.70, 1.04), (-2.10, 0.98), (-2.20, 0.86)],
                  belt=[(0.85, 0.75), (-0.70, 1.00)], bot=[(2.20, 0.22), (1.90, 0.12), (-1.90, 0.12), (-2.20, 0.30)],
                  gh=0.70, nose=3.5, tail=5.0, lamp='slit', grille='intake', tail_l='bar', sport=True),
    # classic muscle: long hood, short deck, fastback
    'muscle': dict(L=4.85, W=1.88, wb=2.75, xf=1.40, wr=0.33, tw=0.24, zr=0.16, sharp=0.025,
                   top=[(2.43, 0.66), (2.40, 0.86), (2.25, 0.90), (0.70, 0.95), (0.60, 0.96), (0.00, 1.30), (-0.60, 1.30), (-1.75, 1.00), (-2.38, 0.99), (-2.43, 0.82)],
                   belt=[(0.60, 0.96), (-1.75, 1.01)], bot=[(2.43, 0.32), (2.10, 0.19), (-2.05, 0.19), (-2.43, 0.36)],
                   gh=0.82, nose=8.0, tail=8.0, lamp='quad', grille='wide', tail_l='bar3', sport=True, scoop=True),
    # boxy 90s SUV
    'suv': dict(L=4.75, W=1.88, wb=2.80, xf=1.30, wr=0.37, tw=0.24, zr=0.30, sharp=0.025,
                top=[(2.38, 0.84), (2.35, 1.04), (2.20, 1.10), (1.15, 1.16), (1.02, 1.18), (0.40, 1.80), (-2.20, 1.82), (-2.36, 1.76), (-2.38, 1.12)],
                belt=[(1.02, 1.18), (-2.30, 1.24)], bot=[(2.38, 0.50), (2.10, 0.34), (-2.10, 0.34), (-2.38, 0.52)],
                gh=0.90, nose=8.0, tail=12.0, lamp='rect', grille='tall', tail_l='vertical', sport=False),
    # square-body pickup: regular cab + long bed
    'pickup': dict(L=5.50, W=2.00, wb=3.30, xf=1.65, wr=0.38, tw=0.24, zr=0.36, sharp=0.02,
                   top=[(2.75, 1.00), (2.73, 1.22), (2.60, 1.26), (1.35, 1.32), (1.25, 1.34), (0.75, 1.92), (0.05, 1.93), (-0.30, 1.91), (-0.40, 1.34), (-2.73, 1.34), (-2.75, 1.31)],
                   belt=[(1.25, 1.34), (-0.40, 1.36)], bot=[(2.75, 0.56), (2.40, 0.42), (-2.40, 0.42), (-2.75, 0.56)],
                   gh=0.90, nose=12.0, tail=24.0, lamp='rect', grille='truck', tail_l='vertical', sport=False, bed=True, crown=0.015, tail_xn=0.985),
    # box van
    'van': dict(L=5.20, W=2.00, wb=3.25, xf=1.05, wr=0.35, tw=0.22, zr=0.24, sharp=0.02,
                top=[(2.60, 0.80), (2.58, 1.02), (2.40, 1.12), (1.90, 1.24), (1.30, 1.84), (1.05, 2.06), (-2.55, 2.08), (-2.60, 2.00)],
                belt=[(1.90, 1.24), (0.80, 1.30)], bot=[(2.60, 0.44), (2.30, 0.30), (-2.30, 0.30), (-2.60, 0.44)],
                gh=0.95, nose=8.0, tail=16.0, lamp='rect', grille='slim', tail_l='vertical', sport=False, van=True),
    # long-roof station wagon
    'wagon': dict(L=4.95, W=1.82, wb=2.85, xf=1.40, wr=0.32, tw=0.21, zr=0.17, sharp=0.025,
                  top=[(2.48, 0.58), (2.45, 0.76), (2.33, 0.80), (1.15, 0.90), (1.02, 0.91), (0.40, 1.42), (-2.15, 1.44), (-2.40, 1.36), (-2.48, 0.84)],
                  belt=[(1.02, 0.91), (-2.40, 0.98)], bot=[(2.48, 0.30), (2.20, 0.20), (-2.20, 0.20), (-2.48, 0.34)],
                  gh=0.86, nose=8.0, tail=12.0, lamp='rect', grille='slim', tail_l='vertical', sport=False, rails=True),
    # 2-door tuner coupe with a big rear wing
    'tuner': dict(L=4.25, W=1.74, wb=2.50, xf=1.20, wr=0.32, tw=0.23, zr=0.10, sharp=0.025,
                  top=[(2.12, 0.50), (2.08, 0.66), (1.95, 0.70), (0.95, 0.80), (0.85, 0.81), (0.15, 1.26), (-0.65, 1.27), (-1.20, 0.96), (-2.08, 0.92), (-2.12, 0.76)],
                  belt=[(0.85, 0.81), (-1.20, 0.95)], bot=[(2.12, 0.20), (1.95, 0.11), (-1.95, 0.11), (-2.12, 0.24)],
                  gh=0.84, nose=5.0, tail=6.0, lamp='swept', grille='intake', tail_l='round', sport=True, wing=True),
    # long luxury sedan with a formal roofline and upright grille
    'luxury': dict(L=5.40, W=1.95, wb=3.20, xf=1.50, wr=0.35, tw=0.24, zr=0.18, sharp=0.025,
                   top=[(2.70, 0.64), (2.68, 0.86), (2.55, 0.90), (1.30, 0.96), (1.18, 0.97), (0.50, 1.45), (-1.05, 1.47), (-1.40, 1.40), (-1.65, 1.05), (-2.65, 1.03), (-2.70, 0.86)],
                   belt=[(1.18, 0.97), (-1.65, 1.06)], bot=[(2.70, 0.32), (2.40, 0.21), (-2.40, 0.21), (-2.70, 0.34)],
                   gh=0.86, nose=10.0, tail=10.0, lamp='rect', grille='tall', tail_l='bar', sport=False),
    # one-box minivan
    'minivan': dict(L=5.05, W=1.95, wb=3.00, xf=1.25, wr=0.34, tw=0.22, zr=0.18, sharp=0.025,
                    top=[(2.52, 0.66), (2.48, 0.86), (2.25, 0.94), (1.60, 1.04), (0.70, 1.70), (-2.30, 1.74), (-2.48, 1.62), (-2.52, 0.90)],
                    belt=[(1.60, 1.04), (-2.45, 1.10)], bot=[(2.52, 0.34), (2.20, 0.21), (-2.20, 0.21), (-2.52, 0.36)],
                    gh=0.88, nose=5.0, tail=12.0, lamp='swept', grille='slim', tail_l='vertical', sport=False, rails=True),
    # small box truck: cab + tall cargo box
    'boxtruck': dict(L=6.40, W=2.10, wb=3.70, xf=1.95, wr=0.40, tw=0.26, zr=0.40, sharp=0.02,
                     top=[(3.20, 1.05), (3.18, 1.30), (3.00, 1.36), (2.55, 1.42), (2.15, 2.18), (1.80, 2.20), (1.72, 1.46), (-3.18, 1.46), (-3.20, 1.40)],
                     belt=[(2.55, 1.42), (1.72, 1.46)], box_x=(-3.20, 1.70), box_z=(0.72, 2.97), bot=[(3.20, 0.60), (2.80, 0.46), (-2.90, 0.50), (-3.20, 0.60)],
                     gh=0.92, nose=10.0, tail=20.0, lamp='rect', grille='truck', tail_l='vertical', sport=False, box=True),
    # 70s two-door land yacht: enormous hood, vinyl roof
    'yacht': dict(L=5.70, W=2.00, wb=3.10, xf=1.75, wr=0.34, tw=0.23, zr=0.18, sharp=0.025,
                  top=[(2.85, 0.66), (2.83, 0.86), (2.70, 0.90), (0.95, 0.98), (0.85, 0.99), (0.20, 1.36), (-0.75, 1.37), (-1.20, 1.04), (-2.80, 1.02), (-2.85, 0.84)],
                  belt=[(0.85, 0.99), (-1.20, 1.05)], bot=[(2.85, 0.34), (2.50, 0.22), (-2.50, 0.22), (-2.85, 0.36)],
                  gh=0.84, nose=12.0, tail=12.0, lamp='quad', grille='wide', tail_l='bar', sport=False, vinyl=True),
}

class Car3:
    step = 0.7
    iters = 190
    shadow_off = (0.15, 0.0)          # left/right symmetric, so mirrored frames stay correct
    core_shadow = -0.22               # solid shadow under the body, inset from the footprint edge

    def __init__(self, kind, paint, trim=None, stripe=None, taxi=False, police=False, sport_wheels=None, ambulance=False):
        S = dict(SPECS[kind])
        self.kind, self.S = kind, S
        self.paint = np.array(paint, f32)
        self.stripe = None if stripe is None else np.array(stripe, f32)
        self.taxi, self.police, self.ambulance = taxi, police, ambulance
        self.sport = S['sport'] if sport_wheels is None else sport_wheels
        L = S['L']
        x0, x1 = -L / 2, L / 2
        tx, tz = zip(*sorted(S['top']))
        bx, bz = zip(*sorted(S['bot']))
        sh = S.get('sharp', 0.05)
        self.gx, self.gT = smooth_table(tx, tz, x0, x1, sh)
        _, self.gB = smooth_table(bx, bz, x0, x1, sh * 1.5)
        # body top (hood -> beltline -> deck): roof clipped down to the belt line between cowl and backlight
        (cx, cz), (rx, rz) = S['belt'][0], S['belt'][1]
        belt = np.interp(self.gx, [rx, cx], [rz, cz])
        inside = (self.gx < cx) & (self.gx > rx)
        tb = np.where(inside, np.minimum(self.gT, belt), self.gT)
        _, self.gTb = smooth_table(self.gx, tb, x0, x1, sh)
        self.cowl, self.blx = cx, rx
        self.wx = (S['xf'], S['xf'] - S['wb'])
        self.wr, self.tw = S['wr'], S['tw']
        self.hw = S['W'] / 2
        # Lipschitz factors: the profiles are height fields, and where one drops steeply (an upright
        # tailgate, rear window or nose) "z - T(x)" overstates the true distance by sqrt(1 + T'^2).
        # Scaling by the slope keeps the ray marcher from skipping through those faces.
        def k(t): return (1.0 / np.sqrt(1.0 + np.gradient(t, self.gx) ** 2)).astype(f32)
        self.kT, self.kTb, self.kB = k(self.gT), k(self.gTb), k(self.gB)
        self.kW = k(self.plan(self.gx).astype(f32))

    def kfac(self, x):
        return (np.interp(x, self.gx, self.kT), np.interp(x, self.gx, self.kTb),
                np.interp(x, self.gx, self.kB), np.interp(x, self.gx, self.kW))

    def mirror_pos(self):
        mx = self.cowl - 0.12
        return mx, float(np.interp(mx, self.gx, self.gTb)) + 0.06, self.hw + 0.015

    def prof(self, x):
        return (np.interp(x, self.gx, self.gT), np.interp(x, self.gx, self.gTb), np.interp(x, self.gx, self.gB))

    def plan(self, x):
        S = self.S
        L2 = S['L'] / 2
        p = np.where(x > 0, S['nose'], S['tail'])
        # a square tail keeps some width at the very end, so the tailgate plane (not the tapering
        # flank) defines the surface there and its shading is clean
        xn = np.clip(np.abs(x) / L2, 0, np.where(x > 0, 1.0, S.get('tail_xn', 1.0)))
        return self.hw * (1 - xn ** p) ** (1 / p)

    def parts(self, p):
        S = self.S
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        T0, Tb0, B = self.prof(x)
        Wp = self.plan(x)
        # wheel-arch flares: a touch wider around each axle
        for wx in self.wx:
            Wp = Wp + 0.018 * np.exp(-((x - wx) / 0.45) ** 2)
        # crowned panels: hood, deck and roof curve down toward the sides instead of being flat slabs
        Tb = Tb0 - S.get('crown', 0.035) * np.clip(ay / np.maximum(Wp, 0.05), 0, 1) ** 2
        T = T0 - S.get('roof_crown', 0.06) * np.clip(ay / np.maximum(Wp * S['gh'], 0.05), 0, 1) ** 2
        # body cross-section: tuck at the rocker, widest at the shoulder, rolled top edge
        zn = np.clip((z - B) / np.maximum(Tb - B, 0.05), 0, 1)
        # crisp shoulder line at 70 % height: flat flank below, rolled-in upper body above
        tumble = 1 - 0.04 * (1 - np.clip(zn / 0.30, 0, 1)) ** 2 - np.where(zn > 0.78, 0.010 + 0.05 * np.clip((zn - 0.78) / 0.22, 0, 1), 0)
        Wz = Wp * tumble
        r = S.get('corner', 0.032)
        kT, kTb, kB, kW = self.kfac(x)
        q = np.stack([(ay - Wz) * kW + r, np.maximum((z - Tb) * kTb, (B - z) * kB) + r], -1)
        body = (length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0) - r) * 0.85
        body = np.maximum(body, np.abs(x) - S['L'] / 2)
        # greenhouse: narrower, leaning in toward the roof
        gh_on = T0 > Tb0 + 0.02
        gt = np.clip((z - Tb) / np.maximum(T - Tb, 0.05), 0, 1)
        Wg = Wp * (S['gh'] - 0.09 * gt)
        rg = S.get('gh_round', 0.05)
        qg = np.stack([(ay - Wg) * kW + rg, np.maximum((z - T) * kT, (Tb - 0.06 - z) * kTb) + rg], -1)
        green = (length(np.maximum(qg, 0)) + np.minimum(qg.max(-1), 0) - rg) * 0.8
        green = np.where(gh_on, green, 9.0)
        if S.get('bed'):
            # open pickup bed behind the cab: hollow out the top
            # flat floor at a fixed height, walls that follow the body's plan shape (no thin spots at the corners)
            inner = np.maximum(np.maximum(np.abs(x + 1.455) - 1.205, ay - (Wp - 0.09)), (S['top'][-2][1] - 0.50) - z)
            body = np.maximum(body, -inner)
            # the cab's back wall is a true plane: a near-vertical drop in the roof profile is not a
            # usable distance field and used to render as streaky noise
            green = np.maximum(green, -(x + 0.10))
        shell = np.minimum(body, green)
        if S.get('box_x'):                                 # cargo box: its own rounded box behind the cab
            bx0, bx1 = S['box_x']; bz0, bz1 = S['box_z']; rb = 0.05
            qb = np.stack([np.abs(x - (bx0 + bx1) / 2) - ((bx1 - bx0) / 2 - rb), ay - (self.hw - 0.01 - rb),
                           np.abs(z - (bz0 + bz1) / 2) - ((bz1 - bz0) / 2 - rb)], -1)
            shell = np.minimum(shell, length(np.maximum(qb, 0)) + np.minimum(qb.max(-1), 0) - rb)
        if S.get('wing'):                                  # rear wing on two uprights
            dz = np.interp(-1.85, self.gx, self.gTb)
            qw = np.stack([np.abs(x + 1.90) - 0.13, ay - (self.hw - 0.10), np.abs(z - (dz + 0.22)) - 0.018], -1)
            wing = length(np.maximum(qw, 0)) + np.minimum(qw.max(-1), 0) - 0.01
            qu = np.stack([np.abs(x + 1.90) - 0.05, np.abs(ay - self.hw * 0.62) - 0.025, np.abs(z - (dz + 0.11)) - 0.11], -1)
            wing = np.minimum(wing, length(np.maximum(qu, 0)) + np.minimum(qu.max(-1), 0))
            shell = np.minimum(shell, wing)
        if S.get('rails'):                                 # roof rails
            rx0, rx1 = self.cowl - 0.75, self.blx + 0.25
            rzt = np.interp(np.clip(x, rx1, rx0), self.gx, self.gT)
            qr = np.stack([np.maximum(x - rx0, rx1 - x), np.abs(ay - self.hw * (S['gh'] - 0.17)) - 0.02, np.abs(z - (rzt + 0.04)) - 0.02], -1)
            shell = np.minimum(shell, length(np.maximum(qr, 0)) + np.minimum(qr.max(-1), 0))
        if S.get('scoop'):
            hz = np.interp(1.4, self.gx, self.gTb)
            qs = np.stack([np.abs(x - 1.35) - 0.30, ay - 0.22, np.abs(z - hz) - 0.06], -1)
            shell = np.minimum(shell, length(np.maximum(qs, 0)) + np.minimum(qs.max(-1), 0) - 0.01)
        # wheel arches cut out of the outer body
        tires = 9.0
        for wx in self.wx:
            rad = np.sqrt((x - wx) ** 2 + (z - self.wr) ** 2)
            arch = np.maximum(rad - (self.wr + 0.045), (self.hw - 0.34) - ay)
            shell = np.maximum(shell, -arch)
            yc = self.hw - self.tw / 2 - 0.015
            qt = np.stack([rad - self.wr + 0.04, np.abs(ay - yc) - self.tw / 2 + 0.04], -1)
            t = length(np.maximum(qt, 0)) + np.minimum(qt.max(-1), 0) - 0.04
            tires = np.minimum(tires, t)
        # side mirrors
        # side mirrors: housing on a short arm from the door's front window corner (not floating)
        mx, mz, may = self.mirror_pos()
        qm = np.stack([np.abs(x - mx) - 0.04, np.abs(ay - may) - 0.065, np.abs(z - mz) - 0.04], -1)
        mirror = length(np.maximum(qm, 0)) + np.minimum(qm.max(-1), 0) - 0.015
        a0 = self.hw * S['gh'] - 0.06
        qa = np.stack([np.abs(x - mx - 0.02) - 0.05, np.abs(ay - (a0 + may) / 2) - (may - a0) / 2, np.abs(z - (mz - 0.035)) - 0.02], -1)
        mirror = np.minimum(mirror, length(np.maximum(qa, 0)) + np.minimum(qa.max(-1), 0) - 0.005)
        shell = np.minimum(shell, mirror)
        return shell, tires, body, green, T, Tb, B, Wp

    def sdf(self, p):
        s, t = self.parts(p)[:2]
        return np.minimum(s, t)

    def footprint(self, gx, gy):
        q = np.stack([np.abs(gx) - (self.S['L'] / 2 - 0.12), np.abs(gy) - (self.hw - 0.05)], -1)
        return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0)

    # ------------------------------------------------------------ surface
    def shade(self, p, n, view):
        S = self.S
        shell, tire, body, green, T, Tb, B, Wp = self.parts(p)
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        N = len(x)
        nx, ny, nz = n[:, 0], n[:, 1], n[:, 2]
        L2 = S['L'] / 2
        alb = np.tile(self.paint, (N, 1))
        mat = np.zeros(N, np.int32)        # 0 paint 1 glass 2 rubber 3 alloy 4 lamp-white 5 black trim 6 tail-red 7 chrome 8 amber 9 plate
        wheel = tire < shell - 0.0005
        car = ~wheel
        if self.stripe is not None:
            st = car & (ay < 0.24) & (ay > 0.08) & (nz > 0.25)
            alb[st] = self.stripe
        # ---- glass: greenhouse surface above the belt, framed by pillars
        g = car & (green <= body + 0.003) & (z > Tb + 0.025) & (z < T - 0.03)
        side = np.abs(ny) > 0.55
        frame = 0.035
        # A-pillar: band along the windscreen edge; B-pillar; C-pillar behind the rear door glass
        cowl, blx = self.cowl, self.blx
        roof_f = self.gx[np.argmax(self.gT)] if False else None
        a_pillar = side & (x > cowl - 0.42 - (z - Tb) * 0.9) & (x < cowl + 0.05)
        wb = S['wb']
        bx = self.wx[0] - wb * 0.47
        b_pillar = side & (np.abs(x - bx) < 0.06)
        c_end = blx + (0.30 if not S.get('van') else 0)
        c_pillar = side & (x < c_end + 0.10) & (x > blx - 0.6)
        if S.get('van'):
            c_pillar = side & (x < 0.50)                     # cargo van: blind sides behind the cab
            b_pillar = side & (np.abs(x - 0.78) < 0.05)
        if S.get('bed'):
            c_pillar = side & (x < 0.02)
        if S.get('box'):
            c_pillar = side & (x < 1.80); b_pillar = side & (np.abs(x - 2.0) < 0.001)
        edge_top = (T - z) < frame
        edge_bot = (z - Tb) < frame + 0.01
        winframe = g & side & (edge_top | edge_bot)
        glass = g & ~a_pillar & ~b_pillar & ~c_pillar & ~winframe
        alb[g & (a_pillar | b_pillar | winframe)] = (0.03, 0.03, 0.035); mat[g & (a_pillar | b_pillar | winframe)] = 5
        # windscreen and rear window: the sloped part of the cabin top, inside a painted frame
        slope = np.interp(x, self.gx, np.gradient(self.gT, self.gx))
        roof = float(self.gT.max())
        gwid = self.plan(x) * S['gh'] * (1 - 0.09 * np.clip((z - Tb) / np.maximum(T - Tb, 0.05), 0, 1) / S['gh'])
        # rear glass on fastbacks lies much flatter than a windscreen, so the rear threshold is lower
        slope_thr = np.where(x < 0, 0.16, 0.35)
        screen = (car & (green <= body + 0.003) & (np.abs(slope) > slope_thr) & (nz > 0.15)
                  & (z > Tb + 0.045) & (z < roof - 0.045) & (ay < gwid - 0.075))
        # upright rear glass (hatchbacks, wagons, SUVs, vans, the pickup's cab): the cabin's back face
        belt_r = float(np.interp(0.0, self.gx, self.gTb))            # same window line as the side glass
        if S.get('van'): belt_r = S['belt'][1][1] + 0.05               # cargo van: windows in the rear doors
        backface = ((car & (green <= body + 0.003) & (nx < -0.45) & (z > Tb + 0.06) & (z < roof - 0.05)
                     & (ay < gwid - 0.085) & (x < 0))
                    | (car & (nz < 0.6) & (nx < 0.2) & (x < -L2 + 0.35) & (z > belt_r + 0.07) & (z < roof - 0.05)
                       & (ay < self.plan(np.maximum(x, -L2 + 0.35)) * S['gh'] - 0.10)))
        if S.get('box'): backface &= False
        screen = screen | backface
        glass = glass | screen
        fr = car & (((green <= body + 0.003) & (np.abs(slope) > slope_thr) & (nz > 0.15) & (z > Tb + 0.02)) | ((nz < 0.6) & (nx < 0.2) & (x < -L2 + 0.35) & (z > belt_r + 0.03) & (z < roof - 0.01) & (not S.get('box')))) & ~screen & ~glass
        alb[fr] = (0.03, 0.03, 0.035); mat[fr] = 5
        if S.get('box'):                                   # the cargo box is all painted panel
            boxp = g & (x < 1.80); glass &= ~boxp; c_pillar = c_pillar | boxp
        cp = g & c_pillar & ~glass
        alb[cp] = self.paint; mat[cp] = 0
        mat[glass] = 1; alb[glass] = (0.02, 0.025, 0.03)
        # ---- front and rear faces
        front = car & (nx > 0.35) & (x > L2 - 0.55)
        rear = car & (nx < -0.35) & (x < -L2 + 0.55)
        fz = z - B                                           # height above the bumper bottom
        hood_z = np.interp(L2 - 0.1, self.gx, self.gTb) - np.interp(L2 - 0.1, self.gx, self.gB)
        rz = np.interp(-L2 + 0.1, self.gx, self.gTb) - np.interp(-L2 + 0.1, self.gx, self.gB)
        lamp_y0, lamp_y1 = self.hw * 0.50, self.hw * 0.93
        hz0, hz1 = hood_z - 0.20, hood_z - 0.04
        lk = S['lamp']
        if lk == 'oval':
            hl = front & (((ay - self.hw * 0.72) / (self.hw * 0.24)) ** 2 + ((fz - (hood_z - 0.11)) / 0.08) ** 2 < 1)
        elif lk == 'slit':
            hl = front & (ay > self.hw * 0.45) & (ay < self.hw * 0.92) & (np.abs(fz - (hood_z - 0.05) + (ay - self.hw * 0.45) * 0.15) < 0.035)
        elif lk == 'quad':
            hl = front & (((((ay - self.hw * 0.58) ** 2 + (fz - (hood_z - 0.13)) ** 2) < 0.075 ** 2)) |
                          (((ay - self.hw * 0.80) ** 2 + (fz - (hood_z - 0.13)) ** 2) < 0.075 ** 2))
        elif lk == 'swept':
            hl = front & (ay > lamp_y0) & (ay < lamp_y1) & (fz > hz0 + (ay - lamp_y0) * 0.18) & (fz < hz1)
        else:
            hl = front & (ay > lamp_y0) & (ay < lamp_y1) & (fz > hz0) & (fz < hz1)
        # lamp internals: bright projector / bulbs on a chrome reflector, a light strip on top
        bulb = hl & ((((ay - (lamp_y0 + lamp_y1) / 2 - 0.04) ** 2 + (fz - (hz0 + hz1) / 2) ** 2) < 0.045 ** 2) |
                     (((ay - lamp_y0 - 0.08) ** 2 + (fz - (hz0 + hz1) / 2) ** 2) < 0.035 ** 2))
        if lk in ('oval', 'quad'): bulb = hl & (np.abs(fz - (hood_z - 0.12)) < 0.035)
        drl = hl & (fz > hz1 - 0.025)
        alb[hl] = (0.62, 0.64, 0.66); mat[hl] = 7
        alb[bulb] = (1.0, 1.0, 0.96); mat[bulb] = 4
        alb[drl & ~bulb] = (0.95, 0.97, 1.0); mat[drl & ~bulb] = 4
        ind = front & (ay > lamp_y1 - 0.02) & (ay < self.hw + 0.02) & (fz > hz0) & (fz < hz0 + 0.06) & ~hl
        alb[ind] = (1.0, 0.55, 0.08); mat[ind] = 8
        # grille and lower intake
        gk = S['grille']
        if gk == 'truck':
            gr = front & (ay < lamp_y0 - 0.03) & (fz > hood_z - 0.42) & (fz < hood_z - 0.04)
        elif gk == 'tall':
            gr = front & (ay < lamp_y0 - 0.02) & (fz > hood_z - 0.30) & (fz < hood_z - 0.05)
        elif gk == 'wide':
            gr = front & (ay < lamp_y1 - 0.02) & (fz > hood_z - 0.22) & (fz < hood_z - 0.06) & ~hl
        elif gk == 'intake':
            gr = front & (ay < self.hw * 0.40) & (fz > 0.06) & (fz < 0.18)
        else:
            gr = front & (ay < lamp_y0 - 0.04) & (fz > hood_z - 0.15) & (fz < hood_z - 0.07)
        slats = ((fz * 40).astype(np.int32) & 1) == 0
        alb[gr] = (0.035, 0.035, 0.04); mat[gr] = 5
        alb[gr & slats] = (0.20, 0.20, 0.22) if gk == 'truck' else (0.09, 0.09, 0.10)
        if gk == 'truck': mat[gr & slats] = 7
        lower = front & (ay < self.hw * 0.62) & (fz > 0.04) & (fz < 0.15) & ~gr
        alb[lower] = (0.03, 0.03, 0.035); mat[lower] = 5
        fplate = front & (ay < 0.26) & (fz > 0.16) & (fz < 0.27) & ~gr
        if not S['sport']: alb[fplate] = (0.86, 0.86, 0.82); mat[fplate] = 9
        # rear: tail lamps, plate, reverse lights, exhaust
        tk = S['tail_l']
        tz0, tz1 = rz - 0.24, rz - 0.04
        if tk == 'vertical':
            tl = rear & (ay > self.hw * 0.80) & (fz > rz * 0.40) & (fz < rz - 0.03)
        elif tk == 'bar':
            tl = rear & (ay > self.hw * 0.25) & (fz > tz0 + 0.06) & (fz < tz1)
        elif tk == 'bar3':
            tl = rear & (ay > self.hw * 0.18) & (ay < self.hw * 0.90) & (fz > tz0 + 0.05) & (fz < tz1) & ((((ay / self.hw) * 7.5).astype(np.int32) % 2) == 0)
        elif tk == 'round':
            tl = rear & ((((ay - self.hw * 0.62) ** 2 + (fz - (rz - 0.12)) ** 2) < 0.07 ** 2) | (((ay - self.hw * 0.82) ** 2 + (fz - (rz - 0.12)) ** 2) < 0.07 ** 2))
        elif tk == 'slit':
            tl = rear & (ay > self.hw * 0.10) & (np.abs(fz - (rz - 0.08)) < 0.03)
        elif tk == 'hatch':
            tl = rear & (ay > self.hw * 0.60) & (fz > rz * 0.55) & (fz < rz + 0.10)
        else:   # wrap
            tl = rear & (ay > self.hw * 0.50) & (fz > tz0) & (fz < tz1)
        wrap = car & (ny * np.sign(y) > 0.5) & (x < -L2 + 0.30) & (fz > tz0 + 0.02) & (fz < tz1) & (tk in ('wrap', 'bar', 'hatch'))
        tl = tl | wrap
        alb[tl] = (0.55, 0.03, 0.03); mat[tl] = 6
        inner = tl & (np.abs(fz - (tz0 + tz1) / 2) < 0.025)
        alb[inner] = (0.85, 0.10, 0.08)
        rev = tl & (ay > self.hw * 0.50) & (ay < self.hw * 0.62) & (tk not in ('vertical', 'round', 'slit', 'bar3'))
        alb[rev] = (0.85, 0.85, 0.85); mat[rev] = 4
        plate = rear & (ay < 0.27) & (fz > rz * 0.45) & (fz < rz * 0.45 + 0.12)
        alb[plate] = (0.88, 0.88, 0.84); mat[plate] = 9
        if self.taxi: alb[plate] = (0.95, 0.85, 0.25)
        exh = rear & (fz < 0.14) & (fz > 0.04) & (np.abs(ay - self.hw * 0.62) < 0.06)
        alb[exh] = (0.55, 0.55, 0.57); mat[exh] = 7
        # ---- bumpers: lower band in body colour with black lower lip; chrome on the big sedan / truck
        lip = car & (fz < 0.06) & (np.abs(nz) < 0.7) & ~glass
        alb[lip] = (0.04, 0.04, 0.045); mat[lip] = 5
        if self.kind in ('pickup', 'fullsize'):
            cb = (front | rear) & (fz > 0.07) & (fz < 0.22) & ~fplate & ~plate & ~exh & ~gr
            alb[cb] = (0.55, 0.56, 0.58); mat[cb] = 7
        # side: rocker in black, door seams, handles, wheel-arch liners
        sideb = car & (np.abs(ny) > 0.45) & ~g
        rock = sideb & (z < B + 0.07)
        alb[rock] = (0.05, 0.05, 0.055); mat[rock] = 5
        seams = []
        if self.kind in ('compact', 'fullsize', 'hatch5', 'suv'):
            seams = [self.cowl - 0.05, bx + 0.02, blx + 0.10]
        elif self.kind in ('gt', 'muscle', 'super'):
            seams = [self.cowl - 0.05, self.cowl - 1.15]
        elif self.kind == 'pickup':
            seams = [self.cowl - 0.05, bx, -0.17]
        elif self.kind == 'van':
            seams = [self.cowl - 0.05, 0.78, -0.40]
        for sx in seams:
            sm = sideb & (np.abs(x - sx) < 0.012) & (z > B + 0.08) & (z < T - 0.02)
            alb[sm] = alb[sm] * 0.35
        for hx in seams[:2]:
            hd = sideb & (np.abs(x - (hx - 0.22)) < 0.06) & (np.abs(z - (Tb - 0.10)) < 0.018)
            alb[hd] = (0.40, 0.40, 0.42) if self.kind != 'super' else alb[hd] * 0.4; mat[hd] = 7
        # shoulder crease highlight line
        crease = sideb & (np.abs(z - (Tb - 0.17)) < 0.010)
        alb[crease] = alb[crease] * 0.80
        if self.kind == 'super':
            sx = x + 0.45; sz = z - (B + 0.18)
            intake = sideb & (sx < 0.20) & (sx > -0.30 + sz * 0.9) & (sz > 0) & (sz < 0.22)
            alb[intake] = alb[intake] * 0.18; mat[intake] = 5
        # police / taxi liveries
        if self.police:
            doors = car & (np.abs(ny) > 0.4) & (x > blx + 0.05) & (x < self.cowl - 0.10) & (z < Tb - 0.03)
            alb[doors] = (0.86, 0.86, 0.86)
            bar = car & (z > T - 0.02) & (np.abs(x - (bx + 0.10)) < 0.11) & (ay < self.hw * 0.62)
            alb[bar & (y > 0)] = (0.95, 0.12, 0.10); alb[bar & (y <= 0)] = (0.15, 0.30, 0.98); mat[bar] = 4
        if S.get('vinyl'):
            vr = car & (z > Tb + 0.02) & ~glass & (nz > -0.2)
            alb[vr] = (0.05, 0.04, 0.035); mat[vr] = 5
        if self.ambulance:
            band = car & (np.abs(ny) > 0.4) & (np.abs(z - (Tb + (T - Tb) * 0.55 if S.get('box') else Tb - 0.25)) < 0.09)
            alb[band] = (0.85, 0.10, 0.08)
            lb = car & (z > T - 0.02) & (x > 1.0) & (x < 1.35) & (ay < self.hw * 0.75)
            alb[lb & (y > 0)] = (0.95, 0.12, 0.10); alb[lb & (y <= 0)] = (0.95, 0.95, 0.95); mat[lb] = 4
        if self.taxi:
            sign = car & (z > T - 0.02) & (np.abs(x - (bx + 0.05)) < 0.16) & (ay < 0.20)
            alb[sign] = (0.96, 0.92, 0.70); mat[sign] = 4
            chk = car & (np.abs(ny) > 0.45) & (np.abs(z - (Tb - 0.22)) < 0.03) & (((x * 10).astype(np.int32) & 1) == 0)
            alb[chk] = (0.08, 0.08, 0.08)
        # mirrors: body colour shell, black glass face
        mx_, mz, may = self.mirror_pos()
        mir = car & (ay > may - 0.07) & (np.abs(x - mx_) < 0.07) & (np.abs(z - mz) < 0.06)
        alb[mir & (nx < -0.3)] = (0.06, 0.06, 0.07); mat[mir & (nx < -0.3)] = 1
        # ---- wheels
        mat[wheel] = 2; alb[wheel] = (0.045, 0.045, 0.05)
        wxs = np.array(self.wx, f32)
        k = np.argmin(np.abs(x[:, None] - wxs[None, :]), 1)
        dx = x - wxs[k]; dz = z - self.wr
        rad = np.sqrt(dx ** 2 + dz ** 2)
        outer = wheel & (np.abs(ny) > 0.6)
        rim_r = self.wr * (0.70 if self.sport else 0.64)
        rim = outer & (rad < rim_r)
        ang = np.arctan2(dz, dx)
        nsp = 10 if self.sport else 5
        spoke = np.abs(((ang * nsp / (2 * np.pi)) % 1.0) - 0.5) < (0.16 if self.sport else 0.22)
        hub = rad < rim_r * 0.28
        lip_r = rad > rim_r * 0.88
        alloy = (0.62, 0.63, 0.66) if self.kind not in ('pickup', 'van') else (0.55, 0.56, 0.58)
        alb[rim] = (0.10, 0.10, 0.11); mat[rim] = 5                       # dark behind the spokes
        sp = rim & (spoke | hub | lip_r)
        alb[sp] = alloy; mat[sp] = 3
        disc = rim & ~sp & (rad < rim_r * 0.80) & (rad > rim_r * 0.35)
        alb[disc] = (0.30, 0.29, 0.28); mat[disc] = 3
        wall = outer & (rad > rim_r) & (rad < rim_r + 0.03)
        alb[wall] = (0.08, 0.08, 0.085)
        return self.light(p, n, view, alb, mat)

    # ------------------------------------------------------------ lighting
    def light(self, p, n, view, alb, mat):
        # light from above and behind the camera, symmetric left/right in view space: a car seen at
        # heading -a is then exactly the mirror image of heading +a, so only half the angles are stored
        vh = view[0].copy(); vh[2] = 0; vh = vh / max(np.linalg.norm(vh), 1e-6)
        Ls = nrm(-vh * 0.55 + np.array((0, 0, 0.85), f32))
        diff = np.clip((n * Ls).sum(-1), 0, 1)[:, None]
        # ambient occlusion from the distance field
        ao = np.ones(len(p), f32)
        for i, h in enumerate((0.03, 0.07, 0.13, 0.22)):
            d = self.sdf(p + n * h)
            ao -= np.clip(h - d, 0, None) * (2.2 / (i + 1))
        ao = np.clip(ao, 0.25, 1)[:, None]
        # ground contact darkening
        gnd = np.clip(p[:, 2:3] / 0.35, 0, 1)
        sky = 0.55 + 0.45 * n[:, 2:3]
        amb = (0.26 * sky + 0.04 * (1 - n[:, 2:3])) * (0.55 + 0.45 * gnd)
        base = alb * (amb * ao + 0.80 * diff * (0.6 + 0.4 * ao))
        r = view - 2 * (view * n).sum(-1)[:, None] * n
        cosv = np.clip(-(view * n).sum(-1), 0, 1)
        F = (0.04 + 0.96 * (1 - cosv) ** 5)[:, None]
        envc = self.env(r)
        h = nrm(Ls - view)
        sp = np.clip((n * h).sum(-1), 0, 1)[:, None]
        m = mat[:, None]
        paint = base * 0.92 + envc * (0.13 + 0.80 * F) * ao + (1.0 * sp ** 150 + 0.18 * sp ** 14) * ao
        # tinted glass: dark interior, a soft sky reflection that is stronger on the raked screens
        # (they face the sky) and toward their top edge, so windows read as glass, not holes
        sky_t = np.clip(r[:, 2:3] * 2.0, 0, 1)
        glass = alb + (envc * (0.10 + 0.55 * F) * (0.55 + 0.45 * sky_t) + 0.9 * sp ** 200) * np.clip(n[:, 2:3] * 1.4, 0.22, 1)
        rubber = base * 0.9 + 0.05 * sp ** 6
        alloy = base * 0.5 + envc * 0.45 * ao + 0.7 * sp ** 40
        chrome = alb * 0.15 + envc * 0.75 * ao + 0.9 * sp ** 60
        lamp = alb * 0.85 + 0.15 + 0.5 * sp ** 30
        trim = base + envc * 0.05
        tail = base * 1.15 + envc * (0.10 + 0.4 * F) + 0.6 * sp ** 60
        out = np.where(m == 0, paint, trim)
        out = np.where(m == 1, glass, out)
        out = np.where(m == 2, rubber, out)
        out = np.where(m == 3, alloy, out)
        out = np.where(m == 4, lamp, out)
        out = np.where(m == 6, tail, out)
        out = np.where(m == 7, chrome, out)
        out = np.where(m == 8, alb * 0.8 + 0.1, out)
        out = np.where(m == 9, base * 1.1 + 0.05, out)
        return np.clip(out, 0, 1)

    @staticmethod
    def env(r):
        t = r[:, 2:3]
        sky_up = np.array((0.56, 0.70, 0.90), f32)
        sky_hor = np.array((0.86, 0.88, 0.90), f32)
        ground = np.array((0.16, 0.16, 0.17), f32)
        up = sky_hor + (sky_up - sky_hor) * np.clip(t * 1.6, 0, 1)
        # a city skyline on the horizon gives reflections their character
        az = np.arctan2(r[:, 1:2], r[:, 0:1])
        bh = 0.10 + 0.10 * (np.sin(az * 7.0) > 0.3) + 0.06 * (np.sin(az * 17.0 + 1.0) > 0.6)
        up = np.where(t < bh, up * 0.45 + 0.05, up)
        dn = ground + (sky_hor * 0.6 - ground) * np.clip(1 + t * 5, 0, 1)
        return np.where(t > 0, up, dn)
