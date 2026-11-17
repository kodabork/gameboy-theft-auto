# contact sheet of sprite frames from src/assets.c + build/assets.bin
import re, sys
from PIL import Image
src = open('src/assets.c').read()
fr = [tuple(map(int, m)) for m in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(\d+)\}', src.split('spr_palette')[0])]
pal = [int(x, 0) for x in re.findall(r'0x[0-9A-Fa-f]+|\d+', src.split('spr_palette[148] = {')[1].split('}')[0])]
blob = open('build/assets.bin', 'rb').read()
def frame(i):
    w, h, ox, oy, upx, off = fr[i]
    im = Image.new('RGB', (w, h), (90, 90, 96))
    px = im.load()
    for x in range(w):
        for y in range(h):
            c = blob[off + x * h + y]
            if c:
                v = pal[c - 1]
                px[x, y] = ((v & 31) << 3, ((v >> 5) & 31) << 3, ((v >> 10) & 31) << 3)
    return im
hdr = open('src/assets.h').read()
sets = re.findall(r'#define (CAR_\w+) (\d+)', hdr)
angles = [int(a) for a in sys.argv[2].split(',')] if len(sys.argv) > 2 else [0, 4, 8, 12, 16, 20, 24]
ims = [[frame(int(b) + a) for a in angles] for n, b in sets]
cw = max(im.width for row in ims for im in row) + 4; ch = max(im.height for row in ims for im in row) + 4
S = Image.new('RGB', (cw * len(angles), ch * len(sets)), (90, 90, 96))
for j, row in enumerate(ims):
    for i, im in enumerate(row): S.paste(im, (i * cw, j * ch))
S = S.resize((S.width * 2, S.height * 2), Image.NEAREST)
S.save(sys.argv[1])
print(len(sets), 'sets', [n for n, b in sets])
