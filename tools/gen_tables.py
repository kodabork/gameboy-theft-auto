# Generates lookup tables for the engine (sin, reciprocal depth, row distance, ground tile, atan).
import math, sys
FX, FY, HORIZON = 86, 172, 80
out = []
def arr(ctype, name, vals, per=16):
    out.append(f"const {ctype} {name}[{len(vals)}] = {{")
    for i in range(0, len(vals), per):
        out.append("  " + ",".join(str(v) for v in vals[i:i+per]) + ",")
    out.append("};")
arr("short", "sin_tab", [round(math.sin(i*2*math.pi/4096)*16384) for i in range(4096)])
arr("int", "recip_z", [0x7fffffff if z == 0 else round(FY*65536/z) for z in range(4096)], 8)
arr("int", "row_k", [round(FY*65536/(y+0.5-HORIZON)) if y > HORIZON else 0 for y in range(160)], 8)
# atan for ratio 0..256 (Q8) -> angle in 4096-per-turn units (0..512)
arr("short", "atan_tab", [round(math.atan(i/256)*4096/(2*math.pi)) for i in range(257)])
# ground tile for one 64m city block, 0.5m texels.
# 0=lot 1=asphalt 2=sidewalk 3=yellow 4=crosswalk 5=asphalt dark 6=sidewalk seam 7=curb
import random
random.seed(7)
patch = {(i, j): random.random() < 0.07 for i in range(64) for j in range(64)}
t = []
for ly in range(128):
    for lx in range(128):
        rx, ry = lx < 24, ly < 24
        if rx or ry:
            v = 5 if patch[(lx >> 1, ly >> 1)] else 1
            if rx and not ry:
                if lx in (4, 8, 15, 19) and v == 1 and (ly // 3) % 4 == 0: v = 5
                if lx in (11, 12): v = 3
                if (24 <= ly < 30 or ly >= 122) and lx % 4 < 2: v = 4
            if ry and not rx:
                if ly in (4, 8, 15, 19) and v == 1 and (lx // 3) % 4 == 0: v = 5
                if ly in (11, 12): v = 3
                if (24 <= lx < 30 or lx >= 122) and ly % 4 < 2: v = 4
        elif lx < 30 or ly < 30 or lx >= 122 or ly >= 122:
            v = 2
            if lx in (24, 127) or ly in (24, 127): v = 7
            elif (lx % 3 == 0) or (ly % 3 == 0): v = 6
        else:
            v = 0 if (lx % 4 and ly % 4) else 8      # 8 = lot seam / texture
        t.append(v)
# manhole covers mid-block in the lanes (9) and sidewalk tree pits (10)
def disc(cx, cy, r, v):
    for yy in range(int(cy - r - 1), int(cy + r + 2)):
        for xx in range(int(cx - r - 1), int(cx + r + 2)):
            if 0 <= xx < 128 and 0 <= yy < 128 and (xx - cx) ** 2 + (yy - cy) ** 2 <= r * r:
                t[yy * 128 + xx] = v
disc(6, 74, 1.6, 9); disc(18, 44, 1.6, 9); disc(74, 6, 1.6, 9); disc(44, 18, 1.6, 9)
for k in (40, 72, 104):
    for a in (25, 26, 27):
        for b in (k - 1, k, k + 1):
            t[b * 128 + a] = 10; t[a * 128 + b] = 10            # strips along both roads
            t[b * 128 + (a + 99)] = 10; t[(a + 99) * 128 + b] = 10
# clouds: soft fbm-ish noise for the upper sky, 0 clear / 1 thin / 2 thick
import math
rnd = random.Random(5)
ph = [(rnd.uniform(0, 6.28), rnd.uniform(0, 6.28)) for _ in range(6)]
cl = []
for row in range(64):
    for col in range(256):
        a = col * 2 * math.pi / 256
        v = 0.0
        for i, (p1, p2) in enumerate(ph):
            f = 2 ** i
            v += math.sin(a * f * 2 + p1 + math.sin(row * 0.08 * f + p2)) * math.cos(row * 0.11 * f + a * f + p2) / f
        v += 0.35 - row / 110.0                             # thinner toward the horizon
        cl.append(2 if v > 0.75 else 1 if v > 0.45 else 0)
arr("unsigned char", "clouds", cl, 64)
arr("unsigned char", "ground_tile", t, 32)
arr("int", "recip_atan", [0] + [round((256 << 16) / n) for n in range(1, 512)], 8)
# PSG frequency register values per MIDI note
def f(n): return 440.0 * 2 ** ((n - 69) / 12)
arr("unsigned short", "psg_sq", [max(0, min(2047, round(2048 - 131072 / f(n)))) for n in range(128)])
arr("unsigned short", "psg_wave", [max(0, min(2047, round(2048 - 65536 / f(n)))) for n in range(128)])
open(sys.argv[1], "w").write('#include "tables.h"\n' + "\n".join(out) + "\n")
