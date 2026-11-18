# Original street props: traffic light, fire hydrant, trash can, bench, street tree,
# rooftop water tower. Local frame: x forward, y left, z up (metres).
import numpy as np
from sprites_sdf import (f32, length, nrm, sd_round_box, sd_capsule, sd_sphere, sd_zcyl,
                         sd_ycyl, smin, lighting, plane)

def sd_xcyl(p, cy, cz, r, x0, x1):
    d2 = np.sqrt((p[..., 1] - cy) ** 2 + (p[..., 2] - cz) ** 2) - r
    dx = np.abs(p[..., 0] - (x0 + x1) / 2) - (x1 - x0) / 2
    return np.minimum(np.maximum(d2, dx), 0) + length(np.stack([np.maximum(d2, 0), np.maximum(dx, 0)], -1))

class Base:
    step, iters = 0.8, 120
    def sdf(self, p): return np.min(np.stack(self.parts(p)), 0)
    def shade(self, p, n, view):
        ps = np.stack(self.parts(p))
        idx = ps.argmin(0)
        alb = np.array(self.cols, f32)[idx]
        kind = np.array(self.kinds, np.int32)[idx]
        return lighting(alb, kind, n, view, p[..., 2] + 0.6)

class TrafficLight(Base):
    """Pole on the corner with a mast arm reaching over the road (+x) and a signal head."""
    cols = [(0.16, 0.17, 0.16), (0.16, 0.17, 0.16), (0.10, 0.10, 0.09), (0.15, 0.95, 0.35), (0.25, 0.05, 0.05), (0.10, 0.10, 0.09)]
    kinds = [3, 3, 5, 4, 5, 5]
    def parts(self, p):
        pole = np.minimum(sd_zcyl(p, 0, 0, 0.10, 0, 5.6), sd_zcyl(p, 0, 0, 0.16, 0, 0.5))
        arm = sd_capsule(p, (0, 0, 5.35), (3.6, 0, 5.45), 0.06)
        head = sd_round_box(p, (3.45, 0, 4.95), (0.15, 0.17, 0.45), 0.04)
        lit = sd_sphere(p, (3.30, 0, 4.68), 0.10)            # green lamp facing traffic (-x side)
        red = sd_sphere(p, (3.30, 0, 5.22), 0.09)
        ped = sd_round_box(p, (0.18, 0, 2.6), (0.10, 0.14, 0.16), 0.03)
        return [pole, arm, head, lit, red, ped]
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 0.16

class Hydrant(Base):
    cols = [(0.62, 0.08, 0.06), (0.62, 0.08, 0.06), (0.55, 0.55, 0.55), (0.62, 0.08, 0.06)]
    kinds = [0, 0, 3, 0]
    def parts(self, p):
        body = sd_zcyl(p, 0, 0, 0.12, 0, 0.58)
        cap = sd_sphere(p, (0, 0, 0.58), 0.12)
        noz = np.minimum(sd_ycyl(p, 0, 0.38, 0.06, -0.20, 0.20), sd_xcyl(p, 0, 0.36, 0.07, 0, 0.19))
        base = sd_zcyl(p, 0, 0, 0.17, 0, 0.07)
        return [body, cap, noz, base]
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 0.16

class TrashCan(Base):
    cols = [(0.10, 0.22, 0.13), (0.12, 0.13, 0.12), (0.10, 0.22, 0.13)]
    kinds = [0, 5, 0]
    def parts(self, p):
        body = sd_zcyl(p, 0, 0, 0.27, 0.0, 0.92)
        body = np.maximum(body, -sd_zcyl(p, 0, 0, 0.23, 0.2, 1.0))
        rim = sd_zcyl(p, 0, 0, 0.30, 0.88, 0.95)
        bag = sd_sphere(p, (0, 0, 0.86), 0.21)
        return [body, rim, bag]
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 0.28

class Bench(Base):
    cols = [(0.42, 0.27, 0.15), (0.42, 0.27, 0.15), (0.14, 0.14, 0.14)]
    kinds = [5, 5, 3]
    def parts(self, p):
        seat = np.full(p.shape[:-1], 9, f32)
        for k in range(3):
            seat = np.minimum(seat, sd_round_box(p, (0.0, 0.10 - k * 0.12, 0.45), (0.90, 0.045, 0.02), 0.01))
        back = np.full(p.shape[:-1], 9, f32)
        for k in range(2):
            back = np.minimum(back, sd_round_box(p, (0.0, 0.22, 0.62 + k * 0.14), (0.90, 0.02, 0.045), 0.01))
        legs = np.full(p.shape[:-1], 9, f32)
        for lx in (-0.75, 0.75):
            legs = np.minimum(legs, sd_round_box(p, (lx, 0.04, 0.24), (0.03, 0.22, 0.24), 0.01))
            legs = np.minimum(legs, sd_round_box(p, (lx, 0.22, 0.60), (0.03, 0.02, 0.20), 0.01))
        return [seat, back, legs]
    def footprint(self, gx, gy):
        q = np.stack([np.abs(gx) - 0.9, np.abs(gy - 0.05) - 0.25], -1)
        return length(np.maximum(q, 0)) + np.minimum(q.max(-1), 0)

class StreetTree:
    """A younger, narrower tree for sidewalk tree pits."""
    step, iters = 0.8, 120
    def __init__(self, seed=11):
        rng = np.random.default_rng(seed)
        self.blobs = []
        for i in range(18):
            a = rng.uniform(0, 2 * np.pi); r = rng.uniform(0, 1.0); h = rng.uniform(3.0, 5.6)
            rr = rng.uniform(0.55, 0.85) * (1.15 - abs(h - 4.3) / 3.5)
            self.blobs.append((np.cos(a) * r, np.sin(a) * r, h, rr))
        self.blobs = np.array(self.blobs, f32)
    def parts(self, p):
        trunk = sd_capsule(p, (0, 0, 0), (0.03, 0.01, 3.4), 0.09)
        leaf = np.full(p.shape[:-1], 9, f32)
        for bx, by, bz, br in self.blobs:
            leaf = smin(leaf, sd_sphere(p, (bx, by, bz), br), 0.25)
        noise = 0.08 * np.sin(p[..., 0] * 11.1 + np.sin(p[..., 1] * 9.3)) * np.sin(p[..., 2] * 10.7 + p[..., 1] * 3.7)
        return trunk, leaf + noise
    def sdf(self, p):
        t, l = self.parts(p); return np.minimum(t, l)
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 1.1
    def shade(self, p, n, view):
        t, l = self.parts(p)
        z = p[..., 2]
        isleaf = l < t
        var = 0.85 + 0.15 * np.sin(p[..., 0] * 13 + p[..., 1] * 11 + z * 7)
        alb = np.where(isleaf[:, None], np.array((0.22, 0.34, 0.13), f32) * var[:, None], np.array((0.24, 0.18, 0.12), f32))
        kind = np.full(len(z), 5, np.int32)
        inner = np.clip((z - 2.9) / 2.6, 0.3, 1.0)[:, None]
        return lighting(alb, kind, n, view, z) * np.where(isleaf[:, None], inner * 0.9 + 0.25, 1.0)

class WaterTower(Base):
    """Wooden barrel tank on a steel frame, the classic city rooftop silhouette."""
    cols = [(0.38, 0.27, 0.18), (0.20, 0.20, 0.21), (0.14, 0.14, 0.15), (0.30, 0.22, 0.15), (0.12, 0.12, 0.12)]
    kinds = [5, 3, 3, 5, 3]
    def parts(self, p):
        tank = sd_zcyl(p, 0, 0, 1.55, 2.2, 5.2)
        hoops = np.full(p.shape[:-1], 9, f32)
        for hz in (2.6, 3.4, 4.2, 4.9):
            hoops = np.minimum(hoops, sd_zcyl(p, 0, 0, 1.59, hz - 0.035, hz + 0.035))
        legs = np.full(p.shape[:-1], 9, f32)
        for a in range(4):
            ang = a * np.pi / 2 + np.pi / 4
            lx, ly = np.cos(ang) * 1.2, np.sin(ang) * 1.2
            legs = np.minimum(legs, sd_capsule(p, (lx * 1.1, ly * 1.1, 0), (lx, ly, 2.25), 0.07))
        brace = np.minimum(sd_round_box(p, (0, 0, 1.1), (1.3, 0.03, 0.03), 0.01), sd_round_box(p, (0, 0, 1.1), (0.03, 1.3, 0.03), 0.01))
        legs = np.minimum(legs, brace)
        r = np.sqrt(p[..., 0] ** 2 + p[..., 1] ** 2)
        roof = np.maximum((r * 0.55 + (p[..., 2] - 6.1)) / np.sqrt(1 + 0.55 ** 2), 5.15 - p[..., 2])
        roof = np.maximum(roof, r - 1.65)
        ladder = sd_round_box(p, (1.62, 0, 3.2), (0.02, 0.18, 2.4), 0.01)
        return [tank, hoops, legs, roof, ladder]
    def shade(self, p, n, view):
        col = Base.shade(self, p, n, view)
        # vertical wood staves
        ang = np.arctan2(p[..., 1], p[..., 0])
        stave = (np.sin(ang * 40) > 0.85) & (np.sqrt(p[..., 0] ** 2 + p[..., 1] ** 2) > 1.5) & (p[..., 2] > 2.2) & (p[..., 2] < 5.2)
        col[stave] *= 0.7
        return col
    def footprint(self, gx, gy): return np.sqrt(gx ** 2 + gy ** 2) - 1.7

class Cat(Base):
    """A street cat. mode 'walk' (phase 0..2pi) or 'sit'. coat: 'tabby' (orange, striped),
    'black', 'grey' (grey with a white chest and paws)."""
    step, iters = 0.7, 140
    COATS = {'tabby': ((0.80, 0.45, 0.16), (0.50, 0.24, 0.07), (0.92, 0.80, 0.62)),
             'black': ((0.07, 0.07, 0.08), (0.04, 0.04, 0.05), (0.10, 0.10, 0.11)),
             'grey':  ((0.50, 0.51, 0.54), (0.36, 0.37, 0.40), (0.90, 0.90, 0.88))}
    def __init__(self, coat, mode='walk', phase=0.0):
        self.coat, self.mode, self.phase = coat, mode, float(phase)
        fur, dark, light = self.COATS[coat]
        eye = (0.70, 0.85, 0.20) if coat != 'black' else (0.95, 0.80, 0.15)
        #         body  head  legs   tail  ears  eyes  nose
        self.cols = [fur, fur, fur, dark if coat == 'tabby' else fur, fur, eye, (0.85, 0.50, 0.50)]
        self.kinds = [5, 5, 5, 5, 5, 4, 5]
    def parts(self, p):
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        ph = self.phase
        legs = np.full(p.shape[:-1], 9, f32)
        if self.mode == 'sit':
            body = smin(sd_capsule(p, (-0.10, 0, 0.11), (0.06, 0, 0.22), 0.085), sd_sphere(p, (-0.09, 0, 0.09), 0.10), 0.05)
            hc = np.array((0.11, 0, 0.36), f32)
            for s in (-1, 1):
                legs = np.minimum(legs, sd_capsule(p, (0.07, s * 0.035, 0.20), (0.09, s * 0.035, 0.02), 0.022))
                legs = np.minimum(legs, sd_sphere(p, (-0.07, s * 0.07, 0.07), 0.055))      # folded haunches
            tail = np.minimum(sd_capsule(p, (-0.17, 0, 0.03), (-0.05, 0.13, 0.025), 0.02),
                              sd_capsule(p, (-0.05, 0.13, 0.025), (0.08, 0.10, 0.02), 0.018))
        else:
            bob = 0.008 * np.cos(2 * ph)
            body = smin(sd_capsule(p, (-0.16, 0, 0.20 + bob), (0.12, 0, 0.21 + bob), 0.075),
                        sd_sphere(p, (0.10, 0, 0.20 + bob), 0.08), 0.04)
            hc = np.array((0.23, 0, 0.27 + bob), f32)
            for i, (lx, s) in enumerate(((0.11, 1), (0.11, -1), (-0.13, 1), (-0.13, -1))):
                sw = 0.07 * np.sin(ph + (0 if i in (0, 3) else np.pi))                    # diagonal pairs
                lift = 0.03 * max(0.0, np.cos(ph + (0 if i in (0, 3) else np.pi)))
                legs = np.minimum(legs, sd_capsule(p, (lx, s * 0.04, 0.18 + bob), (lx + sw, s * 0.04, 0.025 + lift), 0.022))
            sway = 0.05 * np.sin(ph * 0.5)
            t0, t1, t2 = (-0.20, 0, 0.22), (-0.30, sway, 0.30), (-0.32, sway * 1.8, 0.43)
            tail = np.minimum(sd_capsule(p, t0, t1, 0.02), sd_capsule(p, t1, t2, 0.017))
        head = smin(sd_sphere(p, hc, 0.07), sd_sphere(p, hc + (0.05, 0, -0.018), 0.034), 0.02)
        ears = np.minimum(sd_capsule(p, hc + (0.0, 0.032, 0.045), hc + (-0.004, 0.042, 0.075), 0.02),
                          sd_capsule(p, hc + (0.0, -0.032, 0.045), hc + (-0.004, -0.042, 0.075), 0.02))
        eyes = np.minimum(sd_sphere(p, hc + (0.055, 0.026, 0.012), 0.012), sd_sphere(p, hc + (0.055, -0.026, 0.012), 0.012))
        nose = sd_sphere(p, hc + (0.083, 0, -0.012), 0.009)
        return [body, head, legs, tail, ears, eyes, nose]
    def shade(self, p, n, view):
        ps = np.stack(self.parts(p))
        idx = ps.argmin(0)
        alb = np.array(self.cols, f32)[idx].copy()
        fur, dark, light = self.COATS[self.coat]
        x, y, z = p[..., 0], p[..., 1], p[..., 2]
        furry = idx <= 4
        if self.coat == 'tabby':                                    # stripes across the back and tail
            st = (np.sin(x * 70 + np.abs(y) * 20) > 0.35) & furry & (z > 0.12)
            alb[st] = dark
        if self.coat in ('grey', 'tabby'):                          # pale chest, muzzle and paws
            pale = furry & (((idx == 0) & (z < 0.17) & (x > 0.02)) | ((idx == 2) & (z < 0.05)) | ((idx == 1) & (x > p[..., 0].min() * 0 + 0.0) & (z < 0.27) & (x > 0.24)))
            alb[pale] = light
        kind = np.array(self.kinds, np.int32)[idx]
        return lighting(alb, kind, n, view, z * 3 + 0.4)
    def footprint(self, gx, gy): return np.sqrt((gx / 1.8) ** 2 + gy ** 2) - 0.09
