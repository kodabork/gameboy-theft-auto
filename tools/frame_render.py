# One sprite frame: SDF render + anti-aliasing + outline + dithered shadow. Used by make_assets.py,
# which runs these jobs on every CPU core and caches each finished frame in build/cache/ so that
# changing one model only re-renders the frames that model touches.
import os, sys, hashlib, pickle, numpy as np
sys.path.insert(0, os.path.dirname(__file__))
from sprites_sdf import render

HERE = os.path.dirname(os.path.abspath(__file__))
_src_hash = {}

def _file_hash(name):
    if name not in _src_hash:
        _src_hash[name] = hashlib.sha1(open(os.path.join(HERE, name), 'rb').read()).hexdigest()
    return _src_hash[name]

def job_key(model, args):
    mod = type(model).__module__
    files = ['frame_render.py', 'sprites_sdf.py'] + ([mod + '.py'] if os.path.exists(os.path.join(HERE, mod + '.py')) else [])
    h = hashlib.sha1()
    for f in files: h.update(_file_hash(f).encode())
    h.update(type(model).__name__.encode())
    h.update(pickle.dumps(sorted((k, v.tolist() if isinstance(v, np.ndarray) else v)
                                 for k, v in vars(model).items()), protocol=4))
    h.update(repr(args).encode())
    return h.hexdigest()

def render_frame(model, yaw, elev, s, w_sq, up, down, shadow=True, ss=1, outline=False):
    """Returns (rgb, mask, shadowmask, ox, oy, upx) cropped to the visible pixels."""
    rgb, a, sh, core = render(model, yaw, elev, s / ss, w_sq * ss, up * ss, down * ss, shadow, core=True)
    if ss > 1:
        H0, W0 = a.shape                          # anti-aliasing: average ss x ss samples
        core = core.reshape(H0 // ss, ss, W0 // ss, ss).mean((1, 3)) >= 0.5
        a4 = a.reshape(H0 // ss, ss, W0 // ss, ss).astype(np.float32)
        cov = a4.mean((1, 3))
        csum = (rgb.reshape(H0 // ss, ss, W0 // ss, ss, 3) * a4[..., None]).sum((1, 3))
        rgb = csum / np.maximum(a4.sum((1, 3)), 1)[..., None]
        sh = sh.reshape(H0 // ss, ss, W0 // ss, ss).mean((1, 3)) >= 0.5
        a = cov >= 0.5
        sh &= ~a
    H, W = a.shape
    if outline:                                   # dark silhouette edge: pre-rendered sprite look
        inner = a.copy()
        inner[1:, :] &= a[:-1, :]; inner[:-1, :] &= a[1:, :]; inner[:, 1:] &= a[:, :-1]; inner[:, :-1] &= a[:, 1:]
        edge = a & ~inner
        rgb = rgb.copy(); rgb[edge] = rgb[edge] * 0.35
    yy, xx = np.mgrid[0:H, 0:W]
    smask = sh & (((xx + yy) & 1) == 0)          # dithered = see-through shadow
    smask |= core & ~a                           # solid where the body blocks all light
    return crop(rgb, a, smask, -(w_sq // 2), -up, int(round(s * 16 * 256)))

def crop(col, mask, smask, ox, oy, upx):
    any_ = mask | smask
    if not any_.any():
        return (np.zeros((1, 1, 3), np.float32), np.zeros((1, 1), bool), np.zeros((1, 1), bool), 0, 0, upx)
    ys, xs = np.nonzero(any_)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return (col[y0:y1, x0:x1].astype(np.float32), mask[y0:y1, x0:x1], smask[y0:y1, x0:x1], int(x0 + ox), int(y0 + oy), upx)

CACHE = None
JOBS = []

def _run(i):
    model, args, kw = JOBS[i]
    key = job_key(model, (args, sorted(kw.items())))
    path = os.path.join(CACHE, key + '.pkl') if CACHE else None
    if path and os.path.exists(path):
        try: return pickle.load(open(path, 'rb'))
        except Exception: pass
    out = render_frame(model, *args, **kw)
    if path:
        tmp = path + '.%d' % os.getpid()
        pickle.dump(out, open(tmp, 'wb'), protocol=4); os.replace(tmp, path)
    return out

def run_all(jobs, cache_dir, procs=None):
    """jobs: list of (model, args, kwargs). Renders them in parallel (fork) with a disk cache."""
    global CACHE, JOBS
    CACHE, JOBS = cache_dir, jobs
    if cache_dir: os.makedirs(cache_dir, exist_ok=True)
    import multiprocessing as mp
    n = procs or os.cpu_count() or 1
    if n <= 1 or len(jobs) < 2:
        return [_run(i) for i in range(len(jobs))]
    with mp.get_context('fork').Pool(n) as pool:
        # big chunks of the job list are similar in cost; interleave so cores finish together
        return pool.map(_run, range(len(jobs)), chunksize=4)
