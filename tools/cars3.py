# Cars, third generation: built from realistic generic proportions (no real make or model).
# Each class is described by a side profile (roof / hood / deck / rocker control points), a plan
# shape and a cross-section with tumblehome; details (glass, pillars, seams, lamps, grille, plates,
# mirrors, handles, wheels) are painted onto the surface in shade(), and the lighting adds
# SDF ambient occlusion and a clear-coat environment reflection.
import numpy as np
from sprites_sdf import nrm, length, f32, smin

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
    'compact': dict(L=4.50, W=1.76, wb=2.65, xf=1.32, wr=0.32, tw=0.21, zr=0.15, sharp=0.07,
                    top=[(2.25, 0.58), (2.22, 0.72), (2.05, 0.80), (1.30, 0.90), (0.85, 0.95), (0.40, 1.24), (0.05, 1.40), (-0.40, 1.44), (-0.85, 1.38),
                         (-1.40, 1.09), (-1.75, 1.07), (-2.15, 1.04), (-2.25, 0.86)],
                    belt=[(0.85, 0.95), (-1.40, 1.08)], bot=[(2.25, 0.32), (2.00, 0.21), (-2.00, 0.21), (-2.25, 0.34)],
                    gh=0.88, nose=4.0, tail=5.0, lamp='swept', grille='slim', tail_l='wrap', sport=False),
    # long full-size sedan (cop / taxi archetype)
    'fullsize': dict(L=5.30, W=1.95, wb=2.95, xf=1.52, wr=0.33, tw=0.22, zr=0.19, sharp=0.06,
                     top=[(2.65, 0.60), (2.63, 0.80), (2.50, 0.84), (1.25, 0.92), (1.12, 0.93), (0.45, 1.40), (-0.85, 1.42), (-1.45, 1.03), (-2.60, 1.00), (-2.65, 0.82)],
                     belt=[(1.12, 0.93), (-1.45, 1.03)], bot=[(2.65, 0.32), (2.35, 0.22), (-2.30, 0.22), (-2.65, 0.34)],
                     gh=0.88, nose=5.5, tail=6.0, lamp='rect', grille='wide', tail_l='bar', sport=False),
    # boxy 5-door hatch
    'hatch5': dict(L=4.10, W=1.70, wb=2.50, xf=1.18, wr=0.30, tw=0.19, zr=0.17, sharp=0.07,
                   top=[(2.05, 0.58), (2.02, 0.74), (1.90, 0.78), (1.05, 0.88), (0.92, 0.90), (0.25, 1.42), (-1.55, 1.43), (-1.95, 1.35), (-2.05, 0.82)],
                   belt=[(0.92, 0.90), (-1.95, 0.98)], bot=[(2.05, 0.30), (1.80, 0.19), (-1.80, 0.19), (-2.05, 0.34)],
                   gh=0.88, nose=4.0, tail=7.0, lamp='rect', grille='slim', tail_l='hatch', sport=False),
    # 80s wedge sports coupe
    'gt': dict(L=4.40, W=1.78, wb=2.45, xf=1.15, wr=0.32, tw=0.23, zr=0.13, sharp=0.08,
               top=[(2.20, 0.50), (2.16, 0.62), (1.30, 0.80), (0.70, 0.86), (0.05, 1.24), (-0.55, 1.25), (-1.55, 0.98), (-2.15, 0.96), (-2.20, 0.82)],
               belt=[(0.70, 0.86), (-1.55, 0.99)], bot=[(2.20, 0.26), (1.90, 0.15), (-1.90, 0.15), (-2.20, 0.32)],
               gh=0.82, nose=3.2, tail=4.5, lamp='slit', grille='intake', tail_l='bar', sport=True),
    # mid-engine wedge exotic
    'super': dict(L=4.40, W=1.95, wb=2.60, xf=1.20, wr=0.31, tw=0.28, zr=0.11, sharp=0.05,
                  top=[(2.20, 0.42), (2.10, 0.52), (1.25, 0.70), (0.85, 0.74), (0.20, 1.10), (-0.45, 1.12), (-0.70, 1.04), (-2.10, 0.98), (-2.20, 0.86)],
                  belt=[(0.85, 0.75), (-0.70, 1.00)], bot=[(2.20, 0.22), (1.90, 0.12), (-1.90, 0.12), (-2.20, 0.30)],
                  gh=0.72, nose=3.0, tail=4.0, lamp='slit', grille='intake', tail_l='bar', sport=True),
    # classic muscle: long hood, short deck, fastback
    'muscle': dict(L=4.85, W=1.88, wb=2.75, xf=1.40, wr=0.33, tw=0.24, zr=0.16, sharp=0.045,
                   top=[(2.43, 0.66), (2.40, 0.86), (2.25, 0.90), (0.70, 0.95), (0.60, 0.96), (0.00, 1.30), (-0.60, 1.30), (-1.75, 1.00), (-2.38, 0.99), (-2.43, 0.82)],
                   belt=[(0.60, 0.96), (-1.75, 1.01)], bot=[(2.43, 0.32), (2.10, 0.19), (-2.05, 0.19), (-2.43, 0.36)],
                   gh=0.84, nose=6.0, tail=6.0, lamp='quad', grille='wide', tail_l='bar3', sport=True, scoop=True),
    # boxy 90s SUV
    'suv': dict(L=4.75, W=1.88, wb=2.80, xf=1.30, wr=0.37, tw=0.24, zr=0.30, sharp=0.06,
                top=[(2.38, 0.84), (2.35, 1.04), (2.20, 1.10), (1.15, 1.16), (1.02, 1.18), (0.40, 1.80), (-2.20, 1.82), (-2.36, 1.76), (-2.38, 1.12)],
                belt=[(1.02, 1.18), (-2.30, 1.24)], bot=[(2.38, 0.50), (2.10, 0.34), (-2.10, 0.34), (-2.38, 0.52)],
                gh=0.92, nose=5.0, tail=8.0, lamp='rect', grille='tall', tail_l='vertical', sport=False),
    # square-body pickup: regular cab + long bed
    'pickup': dict(L=5.50, W=2.00, wb=3.30, xf=1.65, wr=0.38, tw=0.24, zr=0.36, sharp=0.045,
                   top=[(2.75, 1.00), (2.73, 1.22), (2.60, 1.26), (1.35, 1.32), (1.25, 1.34), (0.75, 1.92), (0.05, 1.93), (-0.30, 1.91), (-0.40, 1.34), (-2.73, 1.34), (-2.75, 1.31)],
                   belt=[(1.25, 1.34), (-0.40, 1.36)], bot=[(2.75, 0.56), (2.40, 0.42), (-2.40, 0.42), (-2.75, 0.56)],
                   gh=0.92, nose=8.0, tail=12.0, lamp='rect', grille='truck', tail_l='vertical', sport=False, bed=True, crown=0.015, tail_xn=0.985),
    # box van
    'van': dict(L=5.20, W=2.00, wb=3.25, xf=1.05, wr=0.35, tw=0.22, zr=0.24, sharp=0.05,
                top=[(2.60, 0.80), (2.58, 1.02), (2.40, 1.12), (1.90, 1.24), (1.30, 1.84), (1.05, 2.06), (-2.55, 2.08), (-2.60, 2.00)],
                belt=[(1.90, 1.24), (0.80, 1.30)], bot=[(2.60, 0.44), (2.30, 0.30), (-2.30, 0.30), (-2.60, 0.44)],
                gh=0.96, nose=5.0, tail=12.0, lamp='rect', grille='slim', tail_l='vertical', sport=False, van=True),
    # long-roof station wagon
    'wagon': dict(L=4.95, W=1.82, wb=2.85, xf=1.40, wr=0.32, tw=0.21, zr=0.17, sharp=0.07,
                  top=[(2.48, 0.58), (2.45, 0.76), (2.33, 0.80), (1.15, 0.90), (1.02, 0.91), (0.40, 1.42), (-2.15, 1.44), (-2.40, 1.36), (-2.48, 0.84)],
                  belt=[(1.02, 0.91), (-2.40, 0.98)], bot=[(2.48, 0.30), (2.20, 0.20), (-2.20, 0.20), (-2.48, 0.34)],
                  gh=0.88, nose=4.0, tail=8.0, lamp='rect', grille='slim', tail_l='vertical', sport=False, rails=True),
    # 2-door tuner coupe with a big rear wing
    'tuner': dict(L=4.25, W=1.74, wb=2.50, xf=1.20, wr=0.32, tw=0.23, zr=0.10, sharp=0.06,
                  top=[(2.12, 0.50), (2.08, 0.66), (1.95, 0.70), (0.95, 0.80), (0.85, 0.81), (0.15, 1.26), (-0.65, 1.27), (-1.20, 0.96), (-2.08, 0.92), (-2.12, 0.76)],
                  belt=[(0.85, 0.81), (-1.20, 0.95)], bot=[(2.12, 0.20), (1.95, 0.11), (-1.95, 0.11), (-2.12, 0.24)],
                  gh=0.86, nose=4.0, tail=5.0, lamp='swept', grille='intake', tail_l='round', sport=True, wing=True),
    # long luxury sedan with a formal roofline and upright grille
    'luxury': dict(L=5.40, W=1.95, wb=3.20, xf=1.50, wr=0.35, tw=0.24, zr=0.18, sharp=0.065,
                   top=[(2.70, 0.64), (2.68, 0.86), (2.55, 0.90), (1.30, 0.96), (1.18, 0.97), (0.50, 1.45), (-1.05, 1.47), (-1.40, 1.40), (-1.65, 1.05), (-2.65, 1.03), (-2.70, 0.86)],
                   belt=[(1.18, 0.97), (-1.65, 1.06)], bot=[(2.70, 0.32), (2.40, 0.21), (-2.40, 0.21), (-2.70, 0.34)],
                   gh=0.88, nose=5.0, tail=6.0, lamp='rect', grille='tall', tail_l='bar', sport=False),
    # one-box minivan
    'minivan': dict(L=5.05, W=1.95, wb=3.00, xf=1.25, wr=0.34, tw=0.22, zr=0.18, sharp=0.07,
                    top=[(2.52, 0.66), (2.48, 0.86), (2.25, 0.94), (1.60, 1.04), (0.70, 1.70), (-2.30, 1.74), (-2.48, 1.62), (-2.52, 0.90)],
                    belt=[(1.60, 1.04), (-2.45, 1.10)], bot=[(2.52, 0.34), (2.20, 0.21), (-2.20, 0.21), (-2.52, 0.36)],
                    gh=0.9, nose=3.5, tail=9.0, lamp='swept', grille='slim', tail_l='vertical', sport=False, rails=True),
    # small box truck: cab + tall cargo box
    'boxtruck': dict(L=6.40, W=2.10, wb=3.70, xf=1.95, wr=0.40, tw=0.26, zr=0.40, sharp=0.02,
                     top=[(3.20, 1.05), (3.18, 1.30), (3.00, 1.36), (2.55, 1.42), (2.15, 2.18), (1.80, 2.20), (1.72, 1.46), (-3.18, 1.46), (-3.20, 1.40)],
                     belt=[(2.55, 1.42), (1.72, 1.46)], box_x=(-3.20, 1.70), box_z=(0.72, 2.97), bot=[(3.20, 0.60), (2.80, 0.46), (-2.90, 0.50), (-3.20, 0.60)],
                     gh=0.92, nose=10.0, tail=20.0, lamp='rect', grille='truck', tail_l='vertical', sport=False, box=True),
    # 70s two-door land yacht: enormous hood, vinyl roof
    'yacht': dict(L=5.70, W=2.00, wb=3.10, xf=1.75, wr=0.34, tw=0.23, zr=0.18, sharp=0.04,
                  top=[(2.85, 0.66), (2.83, 0.86), (2.70, 0.90), (0.95, 0.98), (0.85, 0.99), (0.20, 1.36), (-0.75, 1.37), (-1.20, 1.04), (-2.80, 1.02), (-2.85, 0.84)],
                  belt=[(0.85, 0.99), (-1.20, 1.05)], bot=[(2.85, 0.34), (2.50, 0.22), (-2.50, 0.22), (-2.85, 0.36)],
                  gh=0.86, nose=9.0, tail=9.0, lamp='quad', grille='wide', tail_l='bar', sport=False, vinyl=True),
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
        if kind not in ('pickup', 'boxtruck', 'van', 'suv'): bz = tuple(v - 0.025 for v in bz)   # sit lower
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
        self.wr, self.tw = S['wr'] * 1.10, S['tw'] * 1.12   # v0.23: bigger wheels, filling the arches
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
            Wp = Wp + 0.030 * np.exp(-((x - wx) / 0.42) ** 2)      # pronounced wheel-arch flares
        # crowned panels: hood, deck and roof curve down toward the sides instead of being flat slabs
        # crowned hood and deck, flattening out at the very nose and tail so the front edge (and the
        # headlights under it) stays dead level instead of sagging toward the corners
        ends = np.clip((S['L'] / 2 - np.abs(x)) / 0.45, 0, 1)
        Tb = Tb0 - S.get('crown', 0.035) * ends * np.clip(ay / np.maximum(Wp, 0.05), 0, 1) ** 2
        # fenders rise over the wheels where the body is low (sports cars): no hole above the tyre
        for wx in self.wx:
            need = (self.wr * 2 + 0.06) - np.interp(wx, self.gx, self.gTb)
            if need > -0.02:
                edge = np.clip((ay / np.maximum(Wp, 0.05) - 0.45) / 0.35, 0, 1)
                Tb = Tb + (need + 0.04) * np.exp(-((x - wx) / 0.42) ** 2) * edge * edge * (3 - 2 * edge)
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
            ry = self.hw * (S['gh'] - 0.17)
            rzt = rzt - S.get('roof_crown', 0.06) * min(1.0, (S['gh'] - 0.17) / S['gh']) ** 2      # sit on the curved roof
            qr = np.stack([np.maximum(x - rx0, rx1 - x), np.abs(ay - ry) - 0.016, np.abs(z - (rzt + 0.025)) - 0.016], -1)
            shell = np.minimum(shell, length(np.maximum(qr, 0)) + np.minimum(qr.max(-1), 0))
        if S.get('scoop'):
            hz = np.interp(1.4, self.gx, self.gTb)
            qs = np.stack([np.abs(x - 1.25) - 0.26, ay - 0.16, np.abs(z - hz) - 0.022], -1)       # low power bulge, not a brick
            shell = smin(shell, length(np.maximum(qs, 0)) + np.minimum(qs.max(-1), 0) - 0.02, 0.06)
        # wheel arches cut out of the outer body
        tires = 9.0
        for wx in self.wx:
            rad = np.sqrt((x - wx) ** 2 + (z - self.wr) ** 2)
            arch = np.maximum(rad - (self.wr + 0.028), (self.hw - 0.34) - ay)
            shell = np.maximum(shell, -arch)
            yc = self.hw - self.tw / 2 - 0.004                  # flush with the body side
            qt = np.stack([rad - self.wr + 0.04, np.abs(ay - yc) - self.tw / 2 + 0.04], -1)
            t = length(np.maximum(qt, 0)) + np.minimum(qt.max(-1), 0) - 0.04
            tires = np.minimum(tires, t)
        # side mirrors
        # side mirrors: housing on a short arm from the door's front window corner (not floating)
        mx, mz, may = self.mirror_pos()
        qm = np.stack([np.abs(x - mx) - 0.028, np.abs(ay - may) - 0.050, np.abs(z - mz) - 0.030], -1)   # slimmer mirror housing
        mirror = length(np.maximum(qm, 0)) + np.minimum(qm.max(-1), 0) - 0.015
        a0 = max(self.hw * S['gh'] - 0.06, self.hw - 0.16)
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
        # ---- headlights: a distinct, period-correct design per vehicle. Top edges stay level (set
        # below the lowest point of the crowned hood so nothing droops), lower edges level or rising
        # toward the outside, no light bars.
        lk = {'compact': 'teardrop', 'luxury': 'teardrop', 'minivan': 'teardrop', 'fullsize': 'composite',
              'pickup': 'composite', 'wagon': 'composite', 'van': 'composite', 'hatch5': 'twinround',
              'gt': 'popup', 'super': 'popup', 'muscle': 'quad', 'suv': 'stacked', 'boxtruck': 'stacked',
              'tuner': 'slim', 'yacht': 'quadrect'}.get(self.kind, 'composite')
        crown = S.get('crown', 0.035)
        top_z = hood_z - 0.085                             # inner top edge; the outer end rises 7 cm toward the hood line
        LH = {'composite': 0.15, 'teardrop': 0.17, 'twinround': 0.16, 'quad': 0.15, 'quadrect': 0.12,
              'stacked': 0.24, 'slim': 0.09, 'popup': 0.06}[lk]
        lamp_y0, lamp_y1 = self.hw * (0.38 if lk != 'stacked' else 0.46), self.hw * (0.84 if lk != 'stacked' else 0.82)   # flat part of the nose only
        hz0, hz1 = top_z - LH, top_z
        if lk == 'popup': hz0, hz1 = 0.10, 0.10 + LH        # pop-ups stay closed: only slim bumper lamps show
        lw, lh = lamp_y1 - lamp_y0, hz1 - hz0
        u = (ay - lamp_y0) / lw                             # 0 inner .. 1 outer
        v = (fz - hz0 - 0.07 * np.clip(u, 0, 1)) / lh        # the whole lamp sweeps 7 cm up toward the outside, so from the
                                                             # game's raised 3/4 camera it never reads as drooping (measured)
        def rrect(m=0.0, p=6):
            return (np.abs((u - 0.5) / (0.5 + m)) ** p + np.abs((v - 0.5) / (0.5 + m * lw / lh)) ** p) < 1
        def disc(cu, cv, r):
            return np.sqrt(((u - cu) * lw) ** 2 + ((v - cv) * lh) ** 2) < r
        CHROME, LENS, BULB, AMBER, DARK = (0.80, 0.81, 0.84), (0.20, 0.22, 0.26), (1.0, 1.0, 0.96), (1.0, 0.56, 0.10), (0.035, 0.035, 0.04)
        hl = np.zeros_like(front); bezel = np.zeros_like(front); wrap = np.zeros_like(front)
        def paint(m, col, mt):
            alb[m] = col; mat[m] = mt
        if lk == 'composite':                              # flat-topped rectangle, ribbed reflector, amber corner
            hl = front & rrect(); bezel = front & rrect(0.03) & ~hl
            paint(hl, LENS, 1)
            refl = hl & (u < 0.80)
            paint(refl, CHROME, 7)
            ribs = refl & ((((fz * 55).astype(np.int32)) & 1) == 0)
            alb[ribs] = (0.62, 0.63, 0.66)
            paint(hl & disc(0.38, 0.5, 0.032), BULB, 4)
            paint(hl & (u >= 0.82), AMBER, 8)
            wrap = car & (np.abs(ny) > 0.45) & (x > L2 - 0.16) & (fz > hz0 + 0.02) & (fz < hz1 - 0.01) & ~front
            paint(wrap, AMBER, 8)
        elif lk == 'teardrop':                             # rises toward the outside, twin projectors
            bot = 0.02 + 0.42 * np.clip(u, 0, 1)
            inner_round = np.sqrt(np.clip(1 - (np.clip(0.15 - u, 0, 1) / 0.15) ** 2, 0, 1))
            hl = front & (u > 0) & (u < 1.0) & (v > bot + (1 - inner_round) * 0.45) & (v < 1)
            bezel = front & (u > -0.04) & (u < 1.03) & (v > bot - 0.10) & (v < 1.10) & ~hl
            paint(hl, LENS, 1)
            for cu in (0.22, 0.47):
                cv = 0.5 + 0.21 * cu
                paint(hl & disc(cu, cv, 0.055), CHROME, 7); paint(hl & disc(cu, cv, 0.030), BULB, 4)
            paint(hl & (u > 0.80), AMBER, 8)
            wrap = car & (np.abs(ny) > 0.45) & (x > L2 - 0.18) & (fz > hz0 + 0.07) & (fz < hz1 - 0.01) & ~front
            paint(wrap, (0.92, 0.92, 0.88), 4)
        elif lk == 'twinround':                            # two round reflectors behind one lens
            hl = front & rrect(0, 4); bezel = front & rrect(0.03, 4) & ~hl
            paint(hl, LENS, 1)
            for cu in (0.30, 0.70):
                paint(hl & disc(cu, 0.5, 0.42 * lh), CHROME, 7); paint(hl & disc(cu, 0.5, 0.15 * lh), BULB, 4)
            ind = front & (u > 0.75) & (u < 1.02) & (v > -0.55) & (v < -0.15)
            paint(ind, AMBER, 8)
        elif lk == 'quad':                                 # classic quad round lamps set in the grille
            for cu in (0.27, 0.73):
                d1 = front & disc(cu, 0.5, 0.40 * lh)
                hl |= d1; bezel |= front & disc(cu, 0.5, 0.52 * lh) & ~d1
            paint(bezel, CHROME, 7); paint(hl, LENS, 1)
            for cu in (0.27, 0.73):
                paint(front & disc(cu, 0.5, 0.30 * lh), (0.86, 0.87, 0.88), 7); paint(front & disc(cu, 0.5, 0.12 * lh), BULB, 4)
            bezel = np.zeros_like(front)
        elif lk == 'quadrect':                             # 70s: side-by-side rectangular pairs
            for c0, c1 in ((0.0, 0.47), (0.53, 1.0)):
                m_ = front & (u > c0) & (u < c1) & (v > 0) & (v < 1)
                hl |= m_
                paint(m_, CHROME, 7)
                paint(m_ & disc((c0 + c1) / 2, 0.5, 0.25 * lh), BULB, 4)
            bezel = front & (u > -0.04) & (u < 1.04) & (v > -0.18) & (v < 1.18) & ~hl
        elif lk == 'stacked':                              # tall unit: headlamp above, amber below
            hl = front & rrect(0, 8); bezel = front & rrect(0.03, 8) & ~hl
            paint(hl, LENS, 1)
            up = hl & (v > 0.36)
            paint(up, CHROME, 7)
            alb[up & ((((fz * 55).astype(np.int32)) & 1) == 0)] = (0.62, 0.63, 0.66)
            paint(hl & disc(0.45, 0.68, 0.04), BULB, 4)
            paint(hl & (v <= 0.30), AMBER, 8)
        elif lk == 'slim':                                 # tuner: slim, angular, twin projectors
            bot = 0.0 + 0.55 * np.clip(u, 0, 1)
            hl = front & (u > 0) & (u < 1) & (v > bot * 0.6) & (v < 1)
            bezel = front & (u > -0.04) & (u < 1.03) & (v > bot * 0.6 - 0.2) & (v < 1.2) & ~hl
            paint(hl, LENS, 1)
            for cu in (0.20, 0.45):
                paint(hl & disc(cu, 0.55, 0.036), CHROME, 7); paint(hl & disc(cu, 0.55, 0.020), BULB, 4)
            paint(hl & (u > 0.82), AMBER, 8)
        elif lk == 'popup':                                # closed pop-ups: panel seams on the nose, bumper lamps
            seam_x0, seam_x1 = L2 - 0.48, L2 - 0.12
            top_face = car & (nz > 0.55) & (x > seam_x0 - 0.01) & (x < seam_x1 + 0.01) & (ay > self.hw * 0.42) & (ay < self.hw * 0.86)
            inside = (x > seam_x0 + 0.012) & (x < seam_x1 - 0.012) & (ay > self.hw * 0.42 + 0.012) & (ay < self.hw * 0.86 - 0.012)
            paint(top_face & ~inside, DARK, 5)
            hl = front & rrect(0, 8); bezel = front & rrect(0.04, 8) & ~hl
            paint(hl, (0.85, 0.86, 0.88), 4)
            paint(hl & (u > 0.55), AMBER, 8)
        alb[bezel] = DARK; mat[bezel] = 5
        hl = hl | bezel | wrap
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
        if gk != 'intake':                                  # bright surround frames the grille: the car's "face"
            ring = np.zeros_like(gr)
            for dy_, dz_ in ((0.022, 0), (0, 0.022), (0, -0.022)):
                if gk == 'truck':  g2 = (ay < lamp_y0 - 0.03 + dy_) & (fz > hood_z - 0.42 - abs(dz_)) & (fz < hood_z - 0.04 + abs(dz_))
                elif gk == 'tall': g2 = (ay < lamp_y0 - 0.02 + dy_) & (fz > hood_z - 0.30 - abs(dz_)) & (fz < hood_z - 0.05 + abs(dz_))
                elif gk == 'wide': g2 = (ay < lamp_y1 - 0.02 + dy_) & (fz > hood_z - 0.22 - abs(dz_)) & (fz < hood_z - 0.06 + abs(dz_))
                else:              g2 = (ay < lamp_y0 - 0.04 + dy_) & (fz > hood_z - 0.15 - abs(dz_)) & (fz < hood_z - 0.07 + abs(dz_))
                ring |= front & g2
            ring &= ~gr & ~hl
            alb[ring] = (0.70, 0.71, 0.73); mat[ring] = 7
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
        rim_r = self.wr * (0.74 if self.sport else 0.68)
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
