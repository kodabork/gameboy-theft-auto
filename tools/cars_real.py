# Realistic original car models. Each body is lofted from smooth profiles:
# side profile (top/bottom), plan-view half width, a full-bodied cross section,
# plus a tumblehome greenhouse. Shading uses a clear-coat paint model with a
# sharp horizon reflection, which is what makes rendered paint read as real.
import numpy as np
from sprites_sdf import f32, length, nrm, sd_ycyl

def smooth_curve(xs, ys, x0, x1, n=600, sigma=0.09):
    g = np.linspace(x0, x1, n)
    v = np.interp(g, xs, ys)
    k = np.exp(-0.5 * (np.arange(-60, 61) * (g[1] - g[0]) / sigma) ** 2); k /= k.sum()
    vp = np.pad(v, 60, mode='edge')
    return g, np.convolve(vp, k, mode='valid').astype(f32)

def rounded_cyl_y(p, cx, cz, r, y0, y1, rc):
    d2 = np.sqrt((p[..., 0] - cx) ** 2 + (p[..., 2] - cz) ** 2) - (r - rc)
    dy = np.abs(p[..., 1] - (y0 + y1) / 2) - ((y1 - y0) / 2 - rc)
    return np.minimum(np.maximum(d2, dy), 0) + length(np.stack([np.maximum(d2, 0), np.maximum(dy, 0)], -1)) - rc

def sd_round_box_c(p, c, b, r):
    q = np.abs(p - np.asarray(c, f32)) - (np.asarray(b, f32) - r)
    return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0) - r

def sd_ell(p, c, r):
    r = np.asarray(r, f32); q = p - np.asarray(c, f32)
    k0 = length(q / r); k1 = length(q / (r * r))
    return k0 * (k0 - 1.0) / np.maximum(k1, 1e-6)

SPECS = {
 'gt': dict(L=4.62, xf=1.45, xr=-1.25, wr=0.345, wy=0.84, tw=0.28,
    xs=[-2.31, -2.18, -1.9, -1.3, -0.5, 0.5, 1.4, 2.0, 2.22, 2.31],
    top=[0.62, 0.79, 0.85, 0.86, 0.86, 0.80, 0.74, 0.66, 0.55, 0.44],
    bot=[0.31, 0.21, 0.15, 0.12, 0.11, 0.11, 0.12, 0.14, 0.18, 0.26],
    hw=[0.70, 0.88, 0.97, 0.99, 0.965, 0.955, 0.96, 0.91, 0.80, 0.58],
    gxs=[-1.70, -1.35, -0.85, -0.30, 0.10, 0.45], groof=[0.85, 1.06, 1.19, 1.21, 1.13, 0.84], ghw=[0.62, 0.69, 0.71, 0.71, 0.70, 0.64],
    doors=1, spoiler='lip', sporty=True, spokes=10),
 'super': dict(L=4.42, xf=1.30, xr=-1.18, wr=0.34, wy=0.82, tw=0.28,
    xs=[-2.21, -2.1, -1.8, -1.1, -0.3, 0.5, 1.2, 1.85, 2.12, 2.21],
    top=[0.66, 0.84, 0.88, 0.88, 0.85, 0.78, 0.72, 0.64, 0.54, 0.44],
    bot=[0.30, 0.21, 0.15, 0.12, 0.11, 0.11, 0.12, 0.14, 0.17, 0.25],
    hw=[0.74, 0.90, 0.95, 0.96, 0.95, 0.93, 0.93, 0.89, 0.79, 0.60],
    gxs=[-1.55, -1.10, -0.55, 0.05, 0.50, 0.85], groof=[0.88, 1.08, 1.24, 1.26, 1.17, 0.86], ghw=[0.60, 0.67, 0.70, 0.70, 0.68, 0.62],
    doors=1, spoiler='duck', sporty=True, spokes=5),
 'muscle': dict(L=4.78, xf=1.50, xr=-1.30, wr=0.36, wy=0.84, tw=0.28,
    xs=[-2.39, -2.3, -2.0, -1.3, -0.5, 0.4, 1.3, 2.05, 2.30, 2.39],
    top=[0.80, 0.93, 0.96, 0.97, 0.96, 0.95, 0.935, 0.90, 0.82, 0.68],
    bot=[0.33, 0.24, 0.18, 0.15, 0.14, 0.14, 0.15, 0.17, 0.23, 0.31],
    hw=[0.86, 0.95, 0.99, 1.00, 0.985, 0.965, 0.955, 0.935, 0.87, 0.76],
    gxs=[-1.78, -1.30, -0.70, -0.10, 0.25, 0.52], groof=[0.965, 1.13, 1.31, 1.335, 1.25, 0.96], ghw=[0.62, 0.69, 0.72, 0.72, 0.71, 0.67],
    doors=1, spoiler='duck', sporty=True, spokes=5),
 'compact': dict(L=4.62, xf=1.36, xr=-1.32, wr=0.33, wy=0.79, tw=0.23,
    xs=[-2.31, -2.2, -1.95, -1.1, 0.0, 0.9, 1.5, 2.02, 2.24, 2.31],
    top=[0.86, 0.99, 1.01, 1.015, 1.005, 0.975, 0.93, 0.84, 0.72, 0.60],
    bot=[0.34, 0.26, 0.20, 0.17, 0.16, 0.16, 0.17, 0.20, 0.26, 0.33],
    hw=[0.76, 0.86, 0.89, 0.90, 0.90, 0.90, 0.89, 0.85, 0.76, 0.62],
    gxs=[-1.60, -1.20, -0.60, 0.32, 0.76, 1.10], groof=[1.01, 1.27, 1.43, 1.445, 1.35, 1.00], ghw=[0.64, 0.70, 0.725, 0.725, 0.70, 0.64],
    doors=2, spoiler=None, sporty=False, spokes=7),
 'sedan': dict(L=4.85, xf=1.42, xr=-1.42, wr=0.33, wy=0.79, tw=0.23,
    xs=[-2.42, -2.3, -2.0, -1.0, 0.0, 1.0, 1.6, 2.15, 2.36, 2.42],
    top=[0.84, 0.98, 1.00, 1.005, 0.995, 0.965, 0.925, 0.845, 0.74, 0.62],
    bot=[0.34, 0.26, 0.20, 0.17, 0.16, 0.16, 0.17, 0.20, 0.26, 0.34],
    hw=[0.77, 0.88, 0.91, 0.92, 0.92, 0.92, 0.91, 0.87, 0.78, 0.64],
    gxs=[-1.78, -1.36, -0.70, 0.30, 0.76, 1.12], groof=[1.00, 1.28, 1.45, 1.46, 1.36, 1.00], ghw=[0.66, 0.72, 0.745, 0.745, 0.72, 0.66],
    doors=2, spoiler=None, sporty=False, spokes=7),
 'hatch5': dict(L=4.26, xf=1.32, xr=-1.26, wr=0.335, wy=0.80, tw=0.23,
    xs=[-2.13, -2.09, -1.95, -1.0, 0.0, 0.8, 1.45, 1.9, 2.07, 2.13],
    top=[0.94, 0.99, 1.00, 1.00, 0.99, 0.96, 0.90, 0.81, 0.70, 0.58],
    bot=[0.34, 0.25, 0.18, 0.15, 0.14, 0.14, 0.15, 0.18, 0.25, 0.33],
    hw=[0.78, 0.87, 0.90, 0.91, 0.91, 0.905, 0.895, 0.85, 0.76, 0.60],
    gxs=[-2.05, -1.97, -0.80, 0.25, 0.70, 1.02], groof=[1.06, 1.42, 1.47, 1.475, 1.38, 1.00], ghw=[0.64, 0.70, 0.72, 0.72, 0.70, 0.64],
    doors=1, spoiler='roof', sporty=True, spokes=5),
 'pickup': dict(L=5.75, xf=1.95, xr=-1.80, wr=0.41, wy=0.86, tw=0.28,
    xs=[-2.88, -2.82, -2.6, -1.3, -0.4, 0.5, 1.5, 2.45, 2.80, 2.88],
    top=[1.10, 1.16, 1.17, 1.17, 1.17, 1.21, 1.22, 1.21, 1.16, 1.02],
    bot=[0.55, 0.45, 0.38, 0.35, 0.34, 0.34, 0.35, 0.38, 0.45, 0.54],
    hw=[0.94, 0.99, 1.00, 1.005, 1.005, 1.005, 1.00, 0.99, 0.96, 0.88],
    gxs=[-0.72, -0.60, -0.10, 0.75, 1.30, 1.66], groof=[1.20, 1.86, 1.94, 1.945, 1.84, 1.24], ghw=[0.82, 0.86, 0.87, 0.87, 0.86, 0.82],
    doors=2, spoiler=None, sporty=False, spokes=6, bed=True),
 'van': dict(L=5.10, xf=1.70, xr=-1.62, wr=0.36, wy=0.84, tw=0.24,
    xs=[-2.55, -2.52, -2.45, -1.2, 0.0, 1.2, 1.9, 2.3, 2.5, 2.55],
    top=[1.06, 1.12, 1.13, 1.13, 1.13, 1.13, 1.12, 1.08, 1.00, 0.88],
    bot=[0.46, 0.36, 0.32, 0.30, 0.29, 0.29, 0.30, 0.33, 0.38, 0.46],
    hw=[0.94, 0.98, 0.99, 0.995, 0.995, 0.995, 0.99, 0.97, 0.92, 0.82],
    gxs=[-2.48, -2.42, -1.0, 0.8, 1.45, 1.92], groof=[2.0, 2.18, 2.22, 2.22, 2.12, 1.30], ghw=[0.92, 0.95, 0.96, 0.96, 0.95, 0.90],
    doors=1, spoiler=None, sporty=False, spokes=5, van=True),
 'suv': dict(L=4.82, xf=1.45, xr=-1.38, wr=0.37, wy=0.83, tw=0.25,
    xs=[-2.41, -2.34, -2.2, -1.0, 0.0, 1.0, 1.7, 2.2, 2.36, 2.41],
    top=[1.00, 1.08, 1.10, 1.105, 1.10, 1.08, 1.05, 0.99, 0.91, 0.80],
    bot=[0.42, 0.32, 0.27, 0.24, 0.23, 0.23, 0.24, 0.27, 0.32, 0.41],
    hw=[0.84, 0.92, 0.94, 0.95, 0.95, 0.95, 0.94, 0.92, 0.86, 0.74],
    gxs=[-2.22, -2.06, -0.90, 0.45, 0.98, 1.34], groof=[1.12, 1.68, 1.76, 1.765, 1.66, 1.12], ghw=[0.73, 0.79, 0.81, 0.81, 0.79, 0.73],
    doors=2, spoiler=None, sporty=False, spokes=6),
 'fullsize': dict(L=5.38, xf=1.62, xr=-1.45, wr=0.34, wy=0.81, tw=0.23,
    xs=[-2.69, -2.6, -2.3, -1.3, 0.0, 1.0, 1.8, 2.4, 2.62, 2.69],
    top=[0.88, 1.00, 1.02, 1.025, 1.02, 1.00, 0.97, 0.90, 0.80, 0.68],
    bot=[0.36, 0.28, 0.22, 0.19, 0.18, 0.18, 0.19, 0.22, 0.28, 0.35],
    hw=[0.82, 0.92, 0.95, 0.96, 0.96, 0.96, 0.95, 0.92, 0.84, 0.72],
    gxs=[-1.62, -1.22, -0.55, 0.50, 0.95, 1.28], groof=[1.02, 1.30, 1.47, 1.48, 1.38, 1.02], ghw=[0.70, 0.76, 0.785, 0.785, 0.76, 0.70],
    doors=2, spoiler=None, sporty=False, spokes=7),

}

BUMPER_STYLE = {'fullsize': 'chrome', 'pickup': 'chrome', 'suv': 'black', 'van': 'black'}
MOULDING = {'fullsize': True, 'compact': True, 'sedan': True, 'suv': True, 'van': True, 'hatch5': True}
ARCH_TRIM = {'suv': True, 'pickup': True, 'van': True}
LAMP_STYLE = {'fullsize': 'rect_amber', 'compact': 'rect_proj', 'sedan': 'rect_proj', 'hatch5': 'round2',
              'pickup': 'tall', 'suv': 'square', 'van': 'square', 'gt': 'angular', 'super': 'oval', 'muscle': 'quad'}
GRILLE_STYLE = {'fullsize': 'chrome', 'pickup': 'chrome_tall', 'suv': 'chrome', 'compact': 'slat', 'sedan': 'slat',
                'hatch5': 'slat', 'van': 'slat', 'muscle': 'dark', 'gt': 'none', 'super': 'none'}
TAIL_STYLE = {'fullsize': 'bar', 'compact': 'pair', 'sedan': 'pair', 'hatch5': 'vertical', 'pickup': 'vertical',
              'suv': 'vertical', 'van': 'vertical', 'gt': 'round', 'super': 'round', 'muscle': 'wide'}

ZENITH = np.array((0.52, 0.60, 0.74), f32)
HORIZ = np.array((0.90, 0.92, 0.95), f32)
GND = np.array((0.14, 0.14, 0.15), f32)
LSUN = nrm(np.array((0.30, 0.50, 0.81), f32))
SHADOW_C = np.array((0.11, 0.11, 0.12), f32)

def env_real(r):
    t = r[..., 2:3]
    sky = HORIZ + (ZENITH - HORIZ) * np.clip(t, 0, 1) ** 0.55
    # distant skyline band just above the horizon
    sk = 0.06 + 0.05 * (np.sin(np.arctan2(r[..., 1:2], r[..., 0:1]) * 23.0) > 0.2)
    sky = np.where((t > 0) & (t < sk), sky * 0.62, sky)
    gnd = GND * (1.0 + 0.5 * np.clip(1 + t * 3, 0, 1))
    return np.where(t >= 0, sky, gnd)

class RealCar:
    step, iters = 0.6, 200
    def __init__(self, kind, paint, trim='black', stripe=None, taxi=False, police=False, sport_wheels=False):
        S = SPECS[kind]
        self.k, self.S = kind, S
        self.paint = np.array(paint, f32)
        self.stripe = None if stripe is None else np.array(stripe, f32)
        self.taxi = taxi
        self.police = police
        self.sport_wheels = sport_wheels
        L = S['L']
        self.g, self.T = smooth_curve(S['xs'], S['top'], -L / 2 - 0.1, L / 2 + 0.1, sigma=0.045)
        _, self.B = smooth_curve(S['xs'], S['bot'], -L / 2 - 0.1, L / 2 + 0.1, sigma=0.045)
        _, self.W = smooth_curve(S['xs'], S['hw'], -L / 2 - 0.1, L / 2 + 0.1, sigma=0.035)
        gx = S['gxs']
        self.gg, self.GR = smooth_curve(gx, S['groof'], gx[0] - 0.2, gx[-1] + 0.2, sigma=0.05)
        _, self.GW = smooth_curve(gx, S['ghw'], gx[0] - 0.2, gx[-1] + 0.2, sigma=0.05)
        self.wr, self.wy = S['wr'], S['wy']
        if not S.get('_scaled'):                        # slightly larger, wider wheels read better at sprite size
            S['wr'] = round(S['wr'] * 1.06, 3); S['tw'] = S['tw'] + 0.03; S['_scaled'] = True
            self.wr = S['wr']

    def _prof(self, x):
        return (np.interp(x, self.g, self.T), np.interp(x, self.g, self.B), np.interp(x, self.g, self.W))

    def parts(self, p):
        S = self.S
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        T, B, W = self._prof(x)
        zm = (T + B) * 0.5; zh = np.maximum((T - B) * 0.5, 1e-3)
        t = np.clip((z - zm) / zh, -1.2, 1.2)
        # fuller section with a defined shoulder; fenders swell over the wheels
        fx = np.exp(-((x - S['xf']) / 0.55) ** 2) + np.exp(-((x - S['xr']) / 0.58) ** 2)
        Weff = W * (1.0 - 0.03 * t * t - 0.05 * np.clip(t - 0.55, 0, 1)) + 0.025 * fx * (S['sporty'] + 0.6)
        Teff = T + 0.02 * fx * (ay / np.maximum(W, 0.3)) ** 3 * S['sporty']
        # rounded superellipse cross-section: soft shoulders up top, crisper rocker below
        zm2 = (Teff + B) * 0.5; zh2 = np.maximum((Teff - B) * 0.5, 1e-3)
        qy = np.maximum(ay, 1e-4) / Weff; qz = np.maximum(np.abs(z - zm2), 1e-4) / zh2
        nexp = np.where(z > zm2, S.get('nt', 2.7), 6.0)
        fa = qy ** nexp + qz ** nexp
        fsec = fa ** (1.0 / nexp)
        gfac = fsec ** (1.0 - nexp)
        grad = np.sqrt((qy ** (nexp - 1) / Weff) ** 2 + (qz ** (nexp - 1) / zh2) ** 2) * gfac
        dsec = (fsec - 1.0) / np.maximum(grad, 1e-3)
        dsec = np.minimum(dsec, np.maximum(ay - Weff, np.maximum(z - Teff, B - z)) + 0.02)   # keep far-field sane
        rx_ = S.get('rend', 0.22)
        dx = np.abs(x) - S['L'] / 2
        q2 = np.stack([dsec + rx_, dx + rx_], -1)
        body = length(np.maximum(q2, 0)) + np.minimum(q2.max(-1), 0) - rx_
        # greenhouse with tumblehome
        GR = np.interp(x, self.gg, self.GR); GW = np.interp(x, self.gg, self.GW)
        gb = T - 0.06
        inside = (x > S['gxs'][0] - 0.05) & (x < S['gxs'][-1] + 0.05)
        gt = np.clip((z - gb) / np.maximum(GR - gb, 1e-3), 0, 1)
        GWe = GW * (1.0 - 0.24 * gt * gt - 0.06 * gt)
        rg = 0.10
        gdx = np.maximum(S['gxs'][0] - x, x - S['gxs'][-1])
        gq = np.stack([gdx + rg, ay - GWe + rg, np.maximum(z - GR, gb - z) + rg], -1)
        gh = length(np.maximum(gq, 0)) + np.minimum(gq.max(-1), 0) - rg
        gh = np.where(inside | (gdx < 0.3), gh, 9.0)
        k = 0.035
        h = np.clip(0.5 + 0.5 * (gh - body) / k, 0, 1)
        shell = gh * (1 - h) + body * h - k * h * (1 - h)
        if S.get('mid'):
            intake = sd_ell(np.stack([x, ay, z], -1), (-0.62, 1.03, 0.55), (0.50, 0.16, 0.15))
            shell = np.maximum(shell, -intake)
        if S.get('bed'):
            bx0, bx1 = -S['L'] / 2 + 0.12, S['gxs'][0] - 0.08
            bedq = np.stack([np.maximum(bx0 - x, x - bx1), ay - (W - 0.07), (T - 0.38) - z], -1)
            bedc = np.maximum(np.maximum(bedq[..., 0], bedq[..., 1]), bedq[..., 2])
            shell = np.maximum(shell, -bedc)
        # wheel openings
        wr, wy = S['wr'], S['wy']
        dwf = np.sqrt((x - S['xf']) ** 2 + (z - wr) ** 2); dwr = np.sqrt((x - S['xr']) ** 2 + (z - wr) ** 2)
        well = np.maximum(np.minimum(dwf, dwr) - (wr + 0.04), (W - 0.32) - ay)
        shell = np.maximum(shell, -well)
        # mirrors
        mx = S['gxs'][-2] + 0.05
        mir = sd_ell(np.stack([x, ay, z], -1), (mx, np.interp(mx, self.g, self.W) - 0.04, np.interp(mx, self.g, self.T) + 0.05), (0.07, 0.11, 0.05))
        shell = np.minimum(shell, mir)
        if self.taxi:
            sign = sd_ell(p, (-0.30, 0, np.interp(-0.3, self.gg, self.GR) + 0.07), (0.28, 0.16, 0.09))
            shell = np.minimum(shell, np.maximum(sign, np.interp(-0.3, self.gg, self.GR) + 0.005 - z))
        if self.police:
            rz = np.interp(-0.2, self.gg, self.GR)
            bar = np.maximum(sd_ell(p, (-0.20, 0, rz + 0.05), (0.16, 0.62, 0.07)), rz + 0.004 - z)
            shell = np.minimum(shell, bar)
        if S['spoiler'] == 'roof':
            rx_ = S['gxs'][0] + 0.10
            rsp = sd_ell(p, (rx_, 0, np.interp(rx_, self.gg, self.GR) + 0.01), (0.16, 0.66, 0.03))
            shell = np.minimum(shell, rsp)
        if S['spoiler'] == 'lip':
            lx = -S['L'] / 2 + 0.12
            lip = sd_ell(p, (lx, 0, np.interp(lx, self.g, self.T) + 0.01), (0.10, 0.78, 0.035))
            shell = np.minimum(shell, lip)
        tires = np.minimum(rounded_cyl_y(np.stack([x, ay, z], -1), S['xf'], wr, wr, wy - S['tw'] / 2, wy + S['tw'] / 2, 0.06),
                           rounded_cyl_y(np.stack([x, ay, z], -1), S['xr'], wr, wr, wy - S['tw'] / 2, wy + S['tw'] / 2, 0.06))
        # separate front / rear bumpers that stand proud of the body
        Lh = S['L'] / 2
        hwmax = max(S['hw'])
        bz = np.interp(Lh - 0.1, self.g, self.B) + 0.13
        bzr = np.interp(-Lh + 0.1, self.g, self.B) + 0.13
        hb = 0.11 if S['sporty'] else 0.13
        bf = sd_round_box_c(p, (Lh - 0.05, 0, bz), (0.10, np.interp(Lh - 0.15, self.g, self.W) - 0.02, hb), 0.04)
        br = sd_round_box_c(p, (-Lh + 0.05, 0, bzr), (0.10, np.interp(-Lh + 0.15, self.g, self.W) - 0.02, hb), 0.04)
        bump = np.minimum(bf, br)
        self._bump = bump
        shell = np.minimum(shell, bump)
        return shell, tires, gh, body, T, B, W

    def sdf(self, p):
        s, t = self.parts(p)[:2]
        return np.minimum(s, t)

    def footprint(self, gx, gy):
        S = self.S
        q = np.stack([np.abs(gx) - (S['L'] / 2 - 0.15), np.abs(gy) - (max(S['hw']) - 0.06)], -1)
        return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0)

    def shade(self, p, n, view):
        S = self.S
        shell, tire, gh, body, T, B, W = self.parts(p)
        Lh_ = S['L'] / 2
        bz_f = np.interp(Lh_ - 0.1, self.g, self.B) + 0.13
        bz_r = np.interp(-Lh_ + 0.1, self.g, self.B) + 0.13
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ay = np.abs(y)
        N = len(x)
        nx, ny, nz = n[..., 0], n[..., 1], n[..., 2]
        M = np.zeros(N, np.int32)          # 0 paint 1 glass 2 rubber 3 chrome 4 emissive 5 black plastic 6 red lens 7 clear lens 8 white
        alb = np.tile(self.paint, (N, 1))
        is_tire = tire < shell
        bodyp = ~is_tire
        gxs = S['gxs']
        # ---- glass, pillars, trim
        above_belt = z > T + 0.012
        glass = bodyp & (gh < body + 0.004) & above_belt & (nz < 0.80)
        roof = bodyp & (gh < body + 0.004) & above_belt & (nz >= 0.80)
        aP = glass & (x > gxs[-2] - 0.10) & (np.abs(ny) > 0.30) & (nx > 0.18)
        if S['doors'] == 2:
            bP = glass & (np.abs(x - (gxs[2] + gxs[3]) * 0.5) < 0.06) & (np.abs(ny) > 0.45)
            cP = glass & (x < gxs[1] + 0.12) & (np.abs(ny) > 0.45)
        else:
            bP = glass & (np.abs(x - gxs[2] + 0.05) < 0.035) & (np.abs(ny) > 0.45)
            cP = glass & (x < gxs[1] - 0.05) & (np.abs(ny) > 0.45)
        if S.get('mid'):
            cP |= glass & (nx < -0.2)                                  # engine cover behind the cabin
        if S.get('van'):
            cP |= glass & (x < gxs[3] - 0.15) & (np.abs(ny) > 0.45)     # cargo area: painted panels
        trim_line = bodyp & (gh < body + 0.004) & (z > T - 0.01) & (z <= T + 0.012) & (np.abs(ny) > 0.3)
        M[glass] = 1; alb[glass] = (0.035, 0.042, 0.055)
        pillar = aP | bP
        M[pillar] = 5; alb[pillar] = (0.03, 0.03, 0.035)
        M[cP] = 0; alb[cP] = self.paint
        M[trim_line] = 3 if not S['sporty'] else 5
        alb[trim_line] = (0.55, 0.56, 0.58) if not S['sporty'] else (0.03, 0.03, 0.035)
        if self.taxi:
            sign = roof & (z > np.interp(-0.3, self.gg, self.GR) + 0.02)
            M[sign] = 8; alb[sign] = (0.92, 0.90, 0.80)
        if self.police:
            rz = np.interp(-0.2, self.gg, self.GR)
            bar = bodyp & (z > rz + 0.01) & (np.abs(x + 0.2) < 0.2)
            M[bar] = 4
            alb[bar & (y > 0.04)] = (1.0, 0.12, 0.10)
            alb[bar & (y < -0.04)] = (0.15, 0.30, 1.0)
            alb[bar & (np.abs(y) <= 0.04)] = (0.9, 0.9, 0.9)
            dl_ = gxs[-2] - 0.02; dr_ = gxs[1] + 0.12
            doors = bodyp & (M == 0) & (x < dl_) & (x > dr_) & (z < T) & (z > B + 0.06) & (np.abs(ny) > 0.35)
            alb[doors] = (0.86, 0.87, 0.88)
        # ---- stripes
        if self.stripe is not None:
            st = bodyp & (M == 0) & (np.abs(ay - 0.14) < 0.075) & (nz > 0.25)
            alb[st] = self.stripe
        # ---- front / rear details
        Lh = S['L'] / 2
        front = bodyp & (M == 0) & (nx > 0.12) & (x > Lh - 0.45)
        rear = bodyp & (M == 0) & (nx < -0.12) & (x < -Lh + 0.45)
        fz = (z - T) * 0.78                            # height relative to the hood / deck line (features drawn ~30% taller)
        k = self.k
        lamp = LAMP_STYLE.get(k, 'rect_proj')
        grl = GRILLE_STYLE.get(k, 'slat')
        tail = TAIL_STYLE.get(k, 'pair')
        def box(m, y0, y1, z0, z1): return m & (ay >= y0) & (ay <= y1) & (fz >= z0) & (fz <= z1)
        def disc(m, yc, zc, r): return m & (((ay - yc) ** 2 + (fz - zc) ** 2) <= r * r)
        CLEAR, BULB, CHROME, AMBER = (0.30, 0.31, 0.33), (0.98, 0.98, 0.94), (0.70, 0.71, 0.74), (0.95, 0.55, 0.10)
        lens = np.zeros(N, bool); bulb = np.zeros(N, bool); ring = np.zeros(N, bool); amber = np.zeros(N, bool)
        Wr = W - 0.05
        if lamp == 'rect_amber':          # big rectangular composite lamps + amber corner markers
            lens = box(front, 0.46, Wr - 0.11, -0.15, -0.04)
            bulb = disc(front, 0.62, -0.095, 0.035)
            amber = box(front, Wr - 0.10, Wr, -0.15, -0.06)
        elif lamp == 'rect_proj':         # clear rectangular lens with two round projectors
            lens = box(front, 0.44, Wr, -0.13, -0.04)
            for yc in (0.57, 0.74):
                ring |= disc(front, yc, -0.085, 0.034); bulb |= disc(front, yc, -0.085, 0.018)
        elif lamp == 'round2':            # twin round lamps in each housing
            lens = box(front, 0.46, Wr, -0.14, -0.03)
            for yc in (0.56, 0.75):
                ring |= disc(front, yc, -0.085, 0.056); bulb |= disc(front, yc, -0.085, 0.030)
            ring &= ~bulb
        elif lamp == 'tall':              # big stacked lamps (pickup)
            lens = box(front, 0.56, Wr, -0.25, -0.04)
            bulb = disc(front, 0.72, -0.085, 0.042) | disc(front, 0.72, -0.195, 0.042)
            amber = box(front, Wr - 0.06, Wr, -0.25, -0.04)
        elif lamp == 'square':            # square lamps with an amber strip below (SUV, van)
            lens = box(front, 0.52, Wr, -0.17, -0.05)
            bulb = disc(front, 0.70, -0.10, 0.045)
            amber = box(front, 0.52, Wr, -0.20, -0.172)
        elif lamp == 'angular':           # slanted clear lenses (front-engine sports car)
            lens = front & (ay >= 0.48) & (ay <= Wr) & (fz >= -0.11 + 0.12 * (ay - 0.48)) & (fz <= -0.035 + 0.05 * (ay - 0.48))
            bulb = disc(front, 0.60, -0.085, 0.024) | disc(front, 0.74, -0.065, 0.024)
        elif lamp == 'oval':              # oval lamps (compact sports coupe)
            lens = front & ((((ay - 0.70) / 0.15) ** 2 + ((fz + 0.075) / 0.05) ** 2) <= 1)
            bulb = disc(front, 0.66, -0.075, 0.026)
            ring = front & ((((ay - 0.70) / 0.165) ** 2 + ((fz + 0.075) / 0.062) ** 2) <= 1) & ~lens
        elif lamp == 'quad':              # quad rectangular lamps (pony car)
            lens = box(front, 0.47, 0.60, -0.125, -0.045) | box(front, 0.64, 0.77, -0.125, -0.045)
            bulb = disc(front, 0.535, -0.085, 0.028) | disc(front, 0.705, -0.085, 0.028)
        # grilles
        gr = np.zeros(N, bool); bars = np.zeros(N, bool)
        if grl in ('chrome', 'chrome_tall'):
            tall = grl == 'chrome_tall'
            gr = box(front, 0.0, 0.52 if tall else 0.44, -0.33 if tall else -0.20, -0.03)
            bars = gr & ((np.sin(z * (55 if tall else 65)) > 0.55) | (ay > (0.49 if tall else 0.41)) | (fz > -0.05) | (tall & (ay < 0.025)))
        elif grl == 'slat':
            gr = box(front, 0.0, 0.40, -0.13, -0.045)
            bars = gr & (np.sin(z * 120) > 0.7)
        elif grl == 'dark':
            gr = box(front, 0.0, 0.44, -0.17, -0.03)
            bars = gr & ((np.sin(y * 140) > 0.6) & (np.sin(z * 140) > 0.6))
        intake = front & (z > B + 0.06) & (z < B + 0.20) & (ay < (0.70 if S['sporty'] else 0.50))
        mesh = intake & ((np.sin(y * 90) > 0.3) ^ (np.sin(z * 90) > 0.3))
        # taillights
        rz = fz
        tl = np.zeros(N, bool); tl_hi = np.zeros(N, bool); rev = np.zeros(N, bool); tamb = np.zeros(N, bool)
        def rbox(y0, y1, z0, z1): return rear & (ay >= y0) & (ay <= y1) & (rz >= z0) & (rz <= z1)
        if tail == 'bar':                 # full-width bar
            tl = rbox(0.10, Wr, -0.15, -0.04); tl_hi = rbox(0.10, Wr, -0.11, -0.085); rev = rbox(0.28, 0.40, -0.15, -0.04)
        elif tail == 'pair':
            tl = rbox(0.42, Wr, -0.13, -0.04); tamb = rbox(0.42, Wr, -0.13, -0.10); tl_hi = rbox(0.60, Wr - 0.04, -0.08, -0.06)
        elif tail == 'vertical':
            tl = rbox(Wr - 0.16, Wr, -0.32, -0.03); rev = rbox(Wr - 0.16, Wr, -0.20, -0.15); tl_hi = rbox(Wr - 0.12, Wr - 0.04, -0.10, -0.05)
        elif tail == 'round':
            for yc in (0.56, 0.76):
                tl |= rear & (((ay - yc) ** 2 + (rz + 0.08) ** 2) <= 0.05 ** 2)
                tl_hi |= rear & (((ay - yc) ** 2 + (rz + 0.08) ** 2) <= 0.022 ** 2)
        elif tail == 'wide':
            tl = rbox(0.30, Wr, -0.13, -0.05); tl_hi = rbox(0.34, Wr - 0.04, -0.10, -0.08)
            ring_t = rbox(0.28, Wr + 0.02, -0.145, -0.035) & ~tl
            M[ring_t] = 3; alb[ring_t] = CHROME
        plate = rear & (ay < 0.26) & (z > B + 0.13) & (z < B + 0.25)
        exh = bodyp & (x < -Lh + 0.12) & (z < B + 0.10) & (np.abs(ay - 0.45) < 0.07)
        lower = bodyp & (M == 0) & (z < B + 0.06)
        M[lower] = 5; alb[lower] = (0.04, 0.04, 0.045)
        M[intake] = 5; alb[intake] = (0.02, 0.02, 0.025); alb[mesh] = (0.06, 0.06, 0.065)
        M[gr] = 5; alb[gr] = (0.03, 0.03, 0.035)
        M[bars] = 3; alb[bars] = CHROME
        M[lens] = 7; alb[lens] = CLEAR
        M[ring] = 3; alb[ring] = CHROME
        M[bulb] = 4; alb[bulb] = BULB
        M[amber] = 6; alb[amber] = AMBER
        M[tl] = 6; alb[tl] = (0.55, 0.02, 0.03); alb[tl_hi] = (0.82, 0.07, 0.07)
        M[tamb] = 6; alb[tamb] = AMBER
        M[rev] = 7; alb[rev] = (0.75, 0.75, 0.74)
        M[plate] = 8; alb[plate] = (0.88, 0.88, 0.85)
        M[exh] = 3; alb[exh] = (0.25, 0.25, 0.27)
        # ---- bumpers, plates, mouldings, arch trim, shoulder crease
        bump = self._bump
        isb = bodyp & (bump <= body + 0.004) & (bump <= gh + 0.004)
        bstyle = BUMPER_STYLE.get(k, 'paint')
        if bstyle == 'chrome':
            M[isb] = 3; alb[isb] = (0.78, 0.79, 0.82)
        elif bstyle == 'black':
            M[isb] = 5; alb[isb] = (0.06, 0.06, 0.065)
        else:
            seam = isb & (np.abs(z - (np.where(x > 0, bz_f, bz_r) + (0.11 if S['sporty'] else 0.13) - 0.01)) < 0.012)
            M[seam] = 5; alb[seam] = (0.03, 0.03, 0.03)
        fplate = isb & (x > 0) & (nx > 0.4) & (ay < 0.25) & (np.abs(z - bz_f) < 0.06)
        M[fplate] = 8; alb[fplate] = (0.88, 0.88, 0.85)
        sideface = bodyp & ~isb & (np.abs(ny) > 0.6) & (M == 0)
        if MOULDING.get(k, False):
            zm = B + (T - B) * 0.45
            mould = sideface & (np.abs(z - zm) < 0.022)
            M[mould] = 5; alb[mould] = (0.05, 0.05, 0.055)
        crease = sideface & (np.abs(z - (T - 0.07)) < 0.008)
        alb[crease] = alb[crease] * 0.72
        if ARCH_TRIM.get(k, False):
            dwf = np.sqrt((x - S['xf']) ** 2 + (z - S['wr']) ** 2); dwr = np.sqrt((x - S['xr']) ** 2 + (z - S['wr']) ** 2)
            dw = np.minimum(dwf, dwr)
            arch = bodyp & ~isb & (dw > S['wr'] + 0.03) & (dw < S['wr'] + 0.11) & (ay > W - 0.30)
            M[arch] = 5; alb[arch] = (0.05, 0.05, 0.055)
        # ---- wheels: tyre sidewall, chrome lip, spokes, brake disc and caliper
        cx = np.where(x > 0, S['xf'], S['xr'])
        rx, rz = x - cx, z - S['wr']
        rad = np.sqrt(rx * rx + rz * rz)
        ang = np.arctan2(rz, rx)
        face = is_tire & (ay > S['wy'] + S['tw'] / 2 - 0.05)
        wr = S['wr']
        sw = S['sporty'] or self.sport_wheels
        rimr = wr * (0.80 if sw else 0.70)
        rim = face & (rad < rimr)
        lip = rim & (rad > rimr - 0.025)
        nsp = 10 if self.sport_wheels else S['spokes']
        spk = rim & ((np.cos(ang * nsp) > (0.55 if nsp >= 7 else 0.15)) | (rad < wr * 0.17))
        if nsp == 5:   # split five-spoke
            spk &= ~((np.abs(np.cos(ang * nsp * 2)) > 0.92) & (rad > wr * 0.35))
        disc = rim & ~spk & ~lip
        cal = disc & (rad > wr * 0.36) & (rad < rimr - 0.04) & (np.abs(((ang - 0.9 + np.pi) % (2 * np.pi)) - np.pi) < 0.55)
        M[is_tire] = 2; alb[is_tire] = (0.035, 0.035, 0.037)
        side = is_tire & ~rim & (ay > S['wy'])
        alb[side] = (0.055, 0.055, 0.058)
        M[disc] = 5; alb[disc] = (0.20, 0.20, 0.21)
        M[cal] = 5; alb[cal] = (0.70, 0.06, 0.05) if sw else (0.30, 0.30, 0.31)
        M[spk] = 3; alb[spk] = (0.74, 0.75, 0.78) if sw else (0.62, 0.63, 0.65)
        M[lip] = 3; alb[lip] = (0.85, 0.86, 0.88)
        # ---- lighting
        r = view - 2 * (view * n).sum(-1)[:, None] * n
        env = env_real(r)
        cosv = np.clip(-(view * n).sum(-1), 0, 1)
        F = (0.04 + 0.96 * (1 - cosv) ** 5)[:, None]
        diff = np.clip((n * LSUN).sum(-1), 0, 1)[:, None]
        amb = (0.55 + 0.45 * nz)[:, None]
        # ambient occlusion: underside, wheel arches, under mirrors
        ao = 0.55 + 0.45 * np.clip((z - B) / 0.25, 0, 1)
        inarch = np.minimum(np.sqrt((x - S['xf']) ** 2 + (z - wr) ** 2), np.sqrt((x - S['xr']) ** 2 + (z - wr) ** 2)) < wr + 0.13
        ao = np.where(inarch & bodyp & (ay > W - 0.35), ao * 0.55, ao)
        ao = ao[:, None]
        hv = nrm(LSUN - view)
        sp = np.clip((n * hv).sum(-1), 0, 1)[:, None]
        base = alb * (0.32 * amb + 0.58 * diff + 0.06) * ao
        m = M[:, None]
        paint = base * 0.90 + env * (0.14 + 0.80 * F) * (0.75 + 0.25 * alb / max(alb.max(), 1e-3)) + 0.9 * sp ** 150 + 0.16 * sp ** 14
        streak = (((x * 1.3 - z * 2.2) % 0.95) < 0.11)[:, None] * 0.10      # diagonal reflection streaks
        glass_c = alb + env * (0.20 + 0.70 * F) * (0.55 + 0.45 * np.clip(nz + 0.6, 0, 1))[:, None] + streak + 1.0 * sp ** 200
        chrome = alb * 0.15 + env * 0.80 * (0.6 + 0.4 * alb) + 0.7 * sp ** 60
        rubber = base * 0.9 + 0.05 * sp ** 6
        plastic = base + env * (0.04 + 0.25 * F) + 0.10 * sp ** 30
        emis = alb
        redl = alb * 0.8 + env * (0.10 + 0.6 * F) + 0.6 * sp ** 80
        clear = base * 0.6 + env * (0.30 + 0.6 * F) + 0.9 * sp ** 120
        white = base * 1.05 + env * 0.05
        out = np.select([m == 0, m == 1, m == 2, m == 3, m == 4, m == 5, m == 6, m == 7, m == 8],
                        [paint, glass_c, rubber, chrome, emis, plastic, redl, clear, white], paint)
        # shut lines and door handles
        dl = gxs[-2] - 0.02
        dr = gxs[2] - 0.05 if S['doors'] == 1 else gxs[1] + 0.12
        mid = (gxs[2] + gxs[3]) * 0.5
        sideface = (np.abs(ny) > 0.55) & bodyp & (M == 0)
        shut = sideface & (z < T) & (z > B + 0.08) & ((np.abs(x - dl) < 0.010) | (np.abs(x - dr) < 0.010) |
                                                       ((S['doors'] == 2) & (np.abs(x - mid) < 0.010)))
        hood = bodyp & (M == 0) & (nz > 0.55) & (np.abs(x - (gxs[-1] + 0.08)) < 0.010) & (ay < W - 0.12)
        if S.get('mid'): hood = bodyp & (M == 0) & (nz > 0.55) & (np.abs(x - 1.75) < 0.010) & (ay < W - 0.15)
        hx = (dr + 0.25) if S['doors'] == 1 else (mid - 0.25)
        handle = sideface & (np.abs(x - hx) < 0.06) & (np.abs(z - (T - 0.07)) < 0.012)
        out[shut | hood] *= 0.45
        out[handle] = out[handle] * 0.5 + 0.08
        return np.clip(out, 0, 1)
