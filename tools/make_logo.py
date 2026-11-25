# Title-screen artwork: the owner's logo (art/title_logo.png) and stylized menu labels,
# rendered with a free font (FreeSans Bold Oblique), mapped onto the game's existing palette.
# Usage: make_logo.py src/assets.c src/logo.c
import sys, re, numpy as np
from PIL import Image, ImageDraw, ImageFont, ImageFilter

csrc = open(sys.argv[1]).read()
pal15 = [int(v) for v in re.search(r'spr_palette\[\d+\] = \{\s*(.*?)\};', csrc, re.S).group(1).replace('\n', '').split(',') if v.strip()]
# palette indices 104.. = sprite colours, then the HUD colours 252..255
cols = [(i + 104, c) for i, c in enumerate(pal15) if i + 104 < 252]
cols += [(252, 0x10DF), (253, 0x03FF), (254, 0x7FFF), (255, 0x0000)]
P = np.array([((c & 31) / 31, ((c >> 5) & 31) / 31, ((c >> 10) & 31) / 31) for _, c in cols], np.float32)
IDX = np.array([i for i, _ in cols], np.uint8)
def lab(c): return np.sqrt(np.clip(c, 0, 1)) * np.array((1.0, 1.3, 0.8), np.float32)
PL = lab(P)
BAY = (np.array([[0, 8, 2, 10], [12, 4, 14, 6], [3, 11, 1, 9], [15, 7, 13, 5]]) + 0.5) / 16 - 0.5

FONT = '/usr/share/fonts/truetype/freefont/FreeSansBoldOblique.ttf'

def to_idx(rgb, mask):
    h, w, _ = rgb.shape
    out = np.zeros((h, w), np.uint8)
    yy, xx = np.mgrid[0:h, 0:w]
    rgbd = np.clip(rgb + BAY[yy & 3, xx & 3][..., None] * 0.06, 0, 1)        # light ordered dither
    L = lab(rgbd[mask])
    best = ((L[:, None, :] - PL[None]) ** 2).sum(-1).argmin(1)
    out[mask] = IDX[best]
    return out

def text_layer(txt, size, pad):
    f = ImageFont.truetype(FONT, size)
    l, t, r, b = f.getbbox(txt)
    W, H = r - l + pad * 2, b - t + pad * 2
    im = Image.new('L', (W, H), 0)
    ImageDraw.Draw(im).text((pad - l, pad - t), txt, font=f, fill=255)
    return np.array(im) > 127

def dilate(m, k):
    im = Image.fromarray((m * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(k))
    return np.array(im) > 127

def logo(txt, size):
    pad = 8
    core = text_layer(txt, size, pad)
    H, W = core.shape
    outline = dilate(core, 7)
    shadow = np.zeros_like(core); shadow[4:, 4:] = outline[:-4, :-4]
    yy = np.mgrid[0:H, 0:W][0].astype(np.float32)
    ys = np.nonzero(core.any(1))[0]; y0, y1 = ys.min(), ys.max()
    t = np.clip((yy - y0) / max(1, y1 - y0), 0, 1)
    # sunset chrome: pale gold sky, bright horizon line, deep orange-red ground
    top = np.stack([1.0 - 0.0 * t, 0.97 - 0.35 * t, 0.70 - 0.55 * t], -1)
    bot = np.stack([0.98 - 0.25 * (t - 0.5), 0.50 - 0.45 * (t - 0.5), 0.08 + 0.0 * t], -1)
    fill = np.where((t < 0.5)[..., None], top, bot)
    fill[np.abs(t - 0.5) < 0.035] = (1.0, 1.0, 0.95)
    # inner bevel: lighter near the upper-left edge of each stroke
    inner = core & ~dilate(~core, 3)
    edge = core & ~inner
    fill[edge] = np.minimum(fill[edge] * 1.25 + 0.1, 1)
    rgb = np.zeros((H, W, 3), np.float32)
    rgb[shadow] = (0.22, 0.03, 0.05)
    rgb[outline] = (0.02, 0.02, 0.03)
    rgb[core] = fill[core]
    mask = shadow | outline
    return to_idx(rgb, mask)

def label(txt, size, color, outline_px=3):
    pad = 4
    core = text_layer(txt, size, pad)
    out = dilate(core, outline_px)
    H, W = core.shape
    yy = np.mgrid[0:H, 0:W][0].astype(np.float32)
    ys = np.nonzero(core.any(1))[0]; y0, y1 = ys.min(), ys.max()
    t = np.clip((yy - y0) / max(1, y1 - y0), 0, 1)[..., None]
    c = np.array(color, np.float32)
    fill = np.clip(c * (1.15 - 0.35 * t), 0, 1)
    rgb = np.zeros((H, W, 3), np.float32)
    rgb[out] = (0.02, 0.02, 0.03)
    rgb[core] = fill[core]
    return to_idx(rgb, out)

def shrink_rgba(im, w, h):
    """premultiplied-alpha box filter, then palette match (no dither); returns indices"""
    pm = im.copy(); pm[..., :3] *= pm[..., 3:4]
    sm = np.stack([np.array(Image.fromarray(pm[..., c]).resize((w, h), Image.BOX)) for c in range(4)], -1)
    a = sm[..., 3]
    rgb = sm[..., :3] / np.maximum(a, 1e-6)[..., None]
    mask = a > 0.5
    out = np.zeros((h, w), np.uint8)
    best = ((lab(rgb[mask])[:, None, :] - PL[None]) ** 2).sum(-1).argmin(1)
    out[mask] = IDX[best]
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    out[mask & (r > 0.3) & (r > g + 0.2) & (r > b + 0.2)] = 252   # the red lettering: the HUD's pure red
    return out

def crop_alpha(im):
    ys, xs = np.nonzero(im[..., 3] > 0.5)
    return im[ys.min():ys.max() + 1, xs.min():xs.max() + 1]

def logo_from_art(path, words_h, ny_mode, ny_w):
    """The owner's title logo (art/title_logo.png, transparent background). The stacked words are shrunk
    to words_h pixels high. "New York" (the red lettering and its outline) is taken out and either
    shrunk separately to ny_w pixels wide ('art'), or redrawn as crisp pixel lettering ('font'),
    so it stays readable at Game Boy size. Returns palette indices."""
    im = np.array(Image.open(path).convert('RGBA')).astype(np.float32) / 255
    r, g, b, a = im[..., 0], im[..., 1], im[..., 2], im[..., 3]
    red = (r > 0.6) & (g < 0.32) & (b < 0.32) & (a > 0.5)
    ys, xs = np.nonzero(red)
    near = np.array(Image.fromarray((red * 255).astype(np.uint8)).filter(ImageFilter.MaxFilter(31))) > 127
    near[:ys.min() - 16] = False
    ny = im.copy(); ny[~near] = 0
    words = im.copy(); words[near] = 0
    words = crop_alpha(words)
    wh0, ww0 = words.shape[:2]
    ww = int(round(ww0 * words_h / wh0))
    top = shrink_rgba(words, ww, words_h)
    if ny_mode == 'font':
        bot = label_red('New York', 17)
    else:
        ny = crop_alpha(ny)
        nh0, nw0 = ny.shape[:2]
        bot = shrink_rgba(ny, ny_w, int(round(nh0 * ny_w / nw0)))
    # stack: New York tucked 3 px up under the words, centred
    H = words_h + bot.shape[0] - 3
    W = max(ww, bot.shape[1])
    out = np.zeros((H, W), np.uint8)
    out[:words_h, (W - ww) // 2:(W - ww) // 2 + ww] = top
    y0 = words_h - 3; x0 = (W - bot.shape[1]) // 2
    sub = out[y0:y0 + bot.shape[0], x0:x0 + bot.shape[1]]
    sub[bot > 0] = bot[bot > 0]
    return out

def label_red(txt, size):
    """crisp bold italic lettering in HUD red with a 2-pixel black outline"""
    pad = 4
    core = text_layer(txt, size, pad)
    out = dilate(core, 5)
    res = np.zeros(core.shape, np.uint8)
    res[out] = 255
    res[core] = 252
    ys, xs = np.nonzero(out)
    return res[ys.min():ys.max() + 1, xs.min():xs.max() + 1]

import os
ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'art', 'title_logo.png')
imgs = [
    ('img_logo', logo_from_art(ART, 80, os.environ.get('LOGO_NY', 'art'), 128) if os.path.exists(ART) else logo('GTA', 70)),
    ('img_sub', label('GAMEBOY THEFT AUTO', 13, (1.0, 1.0, 1.0))),
    ('img_city', label('NEW YORK', 11, (1.0, 0.80, 0.25))),
    ('img_new', label('NEW GAME', 15, (0.92, 0.92, 0.95))),
    ('img_new_s', label('NEW GAME', 15, (1.0, 0.82, 0.20))),
    ('img_cont', label('CONTINUE', 15, (0.92, 0.92, 0.95))),
    ('img_cont_s', label('CONTINUE', 15, (1.0, 0.82, 0.20))),
    ('img_press', label('PRESS A', 10, (0.92, 0.92, 0.95), 3)),
]
L = ['/* generated by tools/make_logo.py: title artwork as palette indices, 0 = transparent */',
     '#include "engine.h"', '#include "logo.h"']
H = ['#ifndef LOGO_H', '#define LOGO_H', 'typedef struct { u16 w, h; const u8 *px; } Img;']
for name, a in imgs:
    h, w = a.shape
    L.append('static const u8 %s_px[%d] = {' % (name, w * h))
    flat = a.reshape(-1)
    for k in range(0, len(flat), 32): L.append('  ' + ','.join(str(int(v)) for v in flat[k:k + 32]) + ',')
    L.append('};')
    L.append('const Img %s = { %d, %d, %s_px };' % (name, w, h, name))
    H.append('extern const Img %s;' % name)
H.append('#endif')
open(sys.argv[2], 'w').write('\n'.join(L) + '\n')
open(sys.argv[2].replace('.c', '.h'), 'w').write('\n'.join(H) + '\n')
# preview
prev = Image.new('RGB', (240, 160), (60, 70, 90))
y = 4
for name, a in imgs:
    h, w = a.shape
    rgb = np.zeros((h, w, 3), np.uint8)
    lut = {int(i): (P[k] * 255).astype(np.uint8) for k, i in enumerate(IDX)}
    for yy in range(h):
        for xx in range(w):
            v = a[yy, xx]
            if v: rgb[yy, xx] = lut[int(v)]
    m = Image.fromarray((a > 0).astype(np.uint8) * 255)
    prev.paste(Image.fromarray(rgb), ((240 - w) // 2, y), m)
    y += h - 2
    if y > 150: break
prev.resize((720, 480), Image.NEAREST).save(sys.argv[3] if len(sys.argv) > 3 else 'build/logo_preview.png')
print('logo images', [(n, a.shape) for n, a in imgs])
