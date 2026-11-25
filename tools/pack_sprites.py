"""Pack the sprite pixels for the renderer: each column keeps only the run from its first to its last
non-transparent pixel (inner holes stay as 0), so the blitter never reads or steps over the empty top
and bottom of a column. Pixels drawn are unchanged.

  python3 tools/pack_sprites.py src/assets.c build/assets.bin build/sprpack.bin

Layout (little endian):
  u32 off[NFRAMES]                       byte offset of each frame from the start of the file
  frame: w column records { u16 rel, u8 top, u8 len }   rel = offset of the run from the frame start
         then the runs (len bytes each; len 0 = empty column)
Frames are 4-byte aligned. Identical frames share one copy."""
import re, sys, struct
import numpy as np
cfile, binfile, out = sys.argv[1:4]
src = open(cfile).read()
tab = [tuple(int(v) for v in r) for r in re.findall(r'\{(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+),(-?\d+)\}', src.split('spr_frames')[1].split('};')[0])]
pix = np.fromfile(binfile, np.uint8)
n = len(tab)
blob = bytearray(4 * n)
seen = {}
for i, (w, h, ox, oy, upx, off) in enumerate(tab):
    a = pix[off:off + w * h].reshape(w, h) if w * h else np.zeros((0, 0), np.uint8)
    hdr = bytearray(); runs = bytearray()
    for col in a:
        nz = np.nonzero(col)[0]
        if len(nz):
            t, e = int(nz[0]), int(nz[-1]) + 1
            hdr += struct.pack('<HBB', 4 * w + len(runs), t, e - t)
            runs += col[t:e].tobytes()
        else:
            hdr += struct.pack('<HBB', 0, 0, 0)
    fr = bytes(hdr + runs)
    assert len(fr) < 65536
    if fr in seen:
        struct.pack_into('<I', blob, 4 * i, seen[fr]); continue
    while len(blob) & 3: blob.append(0)
    seen[fr] = len(blob)
    struct.pack_into('<I', blob, 4 * i, len(blob))
    blob += fr
while len(blob) & 3: blob.append(0)
open(out, 'wb').write(blob)
print('%d frames (%d unique): %.2f MB -> %.2f MB' % (n, len(seen), len(pix) / 1e6, len(blob) / 1e6))
