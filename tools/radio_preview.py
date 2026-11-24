# Renders every radio station in src/sound.c to WAV with a rough model of the GBA PSG (square lead
# and stabs with their volume envelopes, wavetable bass, noise drums), A A B A, for listening on a PC.
#   python3 tools/radio_preview.py build/share/radio_preview.wav [seconds_per_station]
import re, sys, wave, numpy as np
src = open('src/sound.c').read()
tab = {m.group(1): [int(v, 0) for v in m.group(3).replace('\n', '').split(',') if v.strip()]
       for m in re.finditer(r'static const u8 (\w+)\[(\d+)\] = \{(.*?)\};', src, re.S)}
for k, v in tab.items():
    if len(v) == 1: tab[k] = v * 32
waves = [[int(x, 16) for x in re.findall(r'0x([0-9A-F]{8})', row)] for row in re.findall(r'\{ (0x[0-9A-F]{8}, 0x[0-9A-F]{8}, 0x[0-9A-F]{8}, 0x[0-9A-F]{8}) \}', src)]
body = re.search(r'stations\[NSTATIONS\] = \{(.*?)\n\};', src, re.S).group(1)
stations = []
for line in body.strip().splitlines():
    f = [x.strip() for x in line.strip().strip('{},').split(',')]
    stations.append(dict(step=int(f[0]), mask=int(f[1], 0), wave=int(f[2]), bass=f[3], lead=f[4], chords=f[5], drums=f[6],
                         lead_nr=eval(f[7]), stab_nr=eval(f[8]), bass_b=f[9], lead_b=f[10], chords_b=f[11]))
SR = 22050
PREVIEW_AB = True     # the preview plays verse then chorus so both are heard; the game plays A A B A
def freq(n): return 440.0 * 2 ** ((n - 69) / 12)
def wavetable(w):
    nib = []
    for word in waves[w]:
        for b in range(4):
            byte = (word >> (8 * b)) & 255
            nib += [byte >> 4, byte & 15]
    return (np.array(nib, np.float32) - 7.5) / 7.5
def env(nr, n):
    v0, up, st = nr >> 12, (nr >> 11) & 1, (nr >> 8) & 7
    t = np.arange(n) / SR
    if st == 0: return np.full(n, v0 / 15.0)
    steps = np.floor(t / (st / 64.0))
    v = np.clip(v0 + steps if up else v0 - steps, 0, 15)
    return v / 15.0
def square(f, n, duty):
    """band-limited square-ish pulse (a few harmonics), so it doesn't fizz like a raw 22 kHz square"""
    t = np.arange(n) / SR; out = np.zeros(n)
    for h in range(1, 12):
        if h * f > SR / 2.2: break
        out += np.sin(np.pi * h * duty) / h * np.cos(2 * np.pi * h * f * t - np.pi * h * duty)
    return out * 1.6
def declick(x):
    m = min(len(x), 40); x[:m] *= np.linspace(0, 1, m); return x
def render(S, secs):
    step_s = 65536.0 / (59.73 * S['step'])         # step is sixteenths per VBlank in 16.16
    total = int(secs * SR); out = np.zeros(total, np.float32)
    lead_ch = np.zeros(total); stab_ch = np.zeros(total); bass_ch = np.zeros(total); drum_ch = np.zeros(total)
    wt = wavetable(S['wave']); rng = np.random.default_rng(1)
    duty = {0: 0.125, 0x40: 0.25, 0x80: 0.5, 0xC0: 0.75}
    nsteps = int(secs / step_s); tog = 0
    for k in range(nsteps):
        loop, s = k // 64, k % 64
        ch = (loop & 1) == 1 if PREVIEW_AB else (loop & 3) == 2
        lead = tab[S['lead_b'] if ch else S['lead']]; bass = tab[S['bass_b'] if ch else S['bass']]; chords = tab[S['chords_b'] if ch else S['chords']]
        a = int(k * step_s * SR); n = min(int(step_s * SR * 8), total - a)
        if n <= 0: break
        if lead[s]:
            lead_ch[a:a + n] = declick(square(freq(lead[s]), n, duty[S['lead_nr'] & 0xC0]) * env(S['lead_nr'], n))
        if (S['mask'] >> (s & 7)) & 1:
            note = chords[(s >> 4) * 3 + tog] + 12; tog = (tog + 1) % 3
            stab_ch[a:a + n] = declick(square(freq(note), n, duty[S['stab_nr'] & 0xC0]) * env(S['stab_nr'], n))
        if bass[s]:
            f = freq(bass[s]); ph = (np.arange(n) * f * 32 / SR) % 32
            i0 = ph.astype(int); fr = ph - i0
            bass_ch[a:a + n] = declick(wt[i0] * (1 - fr) + wt[(i0 + 1) % 32] * fr)
        d = tab[S['drums']][s & 31]
        if d:
            m = int(SR * (0.12 if d in (1, 2) else 0.04 if d == 3 else 0.12)); m = min(m, total - a)
            t = np.arange(m) / SR; e = np.exp(-t * (30 if d == 1 else 22 if d == 2 else 60 if d == 3 else 18))
            if d == 1: sig = np.sin(2 * np.pi * (60 + 90 * np.exp(-t * 40)) * t)
            else: sig = rng.uniform(-1, 1, m) * (0.8 if d == 2 else 0.35)
            drum_ch[a:a + m] = sig * e
    out = 0.22 * lead_ch + 0.14 * stab_ch + 0.25 * bass_ch + 0.3 * drum_ch
    fade = np.minimum(1, np.minimum(np.arange(total), total - np.arange(total)) / (SR * 0.3))
    return out * fade
if __name__ == '__main__':
    secs = float(sys.argv[2]) if len(sys.argv) > 2 else 14
    allw = []
    for S in stations:
        two_loops = 128 * 65536 / S['step'] / 59.73
        allw.append(render(S, min(two_loops, 40))); allw.append(np.zeros(int(SR * 0.6)))
    a = np.concatenate(allw); a = a / max(1e-6, np.abs(a).max()) * 0.9
    w = wave.open(sys.argv[1], 'wb'); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR)
    w.writeframes((a * 32767).astype(np.int16).tobytes()); w.close()
    print(len(stations), 'stations,', round(len(a) / SR), 's')
