/* New York renderer. ARM code, copied to IWRAM on GBA.
   - Building walls: column by column, near to far, per-column clip (no overdraw).
     Facades get floors, window bays with varied glass, storefronts, cornices.
   - Ground: flat, floor-cast row by row into every pixel no wall covers. Inside the grid it
     uses the street tile texture; outside, the island material map (coast road, beach, sea).
   - Fog: 4 palette levels blended with a 2x2 ordered dither (looks like 16).
   - Cars, people, trees, lamps: pre-rendered sprites, scaled, occluded by walls. */
#include "engine.h"
#include "assets.h"
#include "terrain.h"
#include "world.h"

typedef long long s64;

static s16 col_ymax[SCR_W];
static s16 col_wtop[SCR_W];
static s32 col_wiz[SCR_W];   /* inverse depth of nearest wall (0 = none) */
static int cols_left;

static const Camera *C;
static u16 *FB;

#define PIX2(i) ((u16)((i) | ((i) << 8)))

static const u8 bayer[4] = { 2, 10, 14, 6 };   /* index (x&1) | (y&1)<<1 */

static inline void to_cam(int wx, int wy, int *ox, int *oz)
{
    int dx = wx - C->x, dy = wy - C->y;
    *oz = (dx * C->c + dy * C->s) >> 14;
    *ox = (dx * C->s - dy * C->c) >> 14;
}

/* fog in 1/16 steps: 0..48 */
static inline int fog16(int z)
{
    int f = z < 480 ? 0 : ((z - 480) * 3) / 170;   /* clean up to 30 m, 48 at FARZ */
    return f > 47 ? 47 : f;
}
static inline int fog_pick(int f16, int dither)
{
    int l = f16 >> 4;
    if ((f16 & 15) > dither && l < 3) l++;
    return l;
}

/* ---------------------------------------------------------------- walls */
/* Inverse depth (iz = FY*65536/z) is linear across a wall in screen space, so heights come
   from an add per column; texture u and depth z are divided out once every 4 columns. */
static u8 fog_iz[128];
static u8 col_ang[SCR_W];          /* view angle of each column, 256 per turn */                 /* fog16 indexed by iz >> 8 (only near-ish values matter) */
static inline int fog_from_iz(int iz) { return iz >= (127 << 8) ? 0 : fog_iz[iz >> 8]; }

void vfill(u16 *dst, int rows, u16 c0, u16 c1);
/* fill rows [r0, r1] of one column with a 2-colour vertical dither */
static inline void vspan(u16 *col, int r0, int r1, u16 c0, u16 c1)
{
    if (r0 > r1) return;
    u16 *p = col + r0 * SCR_W;
    if (r0 & 1) { u16 t = c0; c0 = c1; c1 = t; }
#ifdef HOST
    int n = r1 - r0 + 1;
    while (n >= 2) { p[0] = c0; p[SCR_W] = c1; p += 2 * SCR_W; n -= 2; }
    if (n) *p = c0;
#else
    vfill(p, r1 - r0 + 1, c0, c1);         /* hand-written ARM: 8 rows a pass, one instruction per pixel */
#endif
}
/* fill from row cr down to (but not including) row 'until' (clipped to re); returns next row */
static inline int band(u16 *col, int cr, int re, int until, u16 c0, u16 c1)
{
    int last = until - 1;
    if (last > re) last = re;
    if (last >= cr) { vspan(col, cr, last, c0, c1); return last + 1; }
    return cr;
}

typedef struct { u8 bay, w0, w1, v0, v1, storey, lintel, sill; } StyleDef;
static const StyleDef styles[4] = {
    { 48, 10, 38, 14, 40, 56, 1, 1 },     /* brick tenement: paired sash windows, lintels, sills */
    { 40, 12, 28, 12, 44, 56, 1, 2 },     /* brownstone: tall narrow windows, stone lintels and sills */
    { 48,  0, 48, 10, 48, 64, 0, 0 },     /* office: ribbon glazing with mullions */
    { 64,  6, 58, 12, 42, 56, 0, 0 },     /* modern: wide windows */
};
static const u16 bay_rcp[4] = { 21846, 26215, 21846, 16384 };   /* ceil(2^20 / bay) */

#define ZK (FY * 65536)
static s16 col_gfl[SCR_W];               /* first row of ground below the nearest wall */

static void draw_wall(int x0, int z0, int u0, int x1, int z1, int u1,
                      int L, int H, int B, int mat, int wmat, int light, int seed, int style)
{
    if (z0 < NEARZ && z1 < NEARZ) return;
    if (z0 < NEARZ) {
        int t = ((NEARZ - z0) << 12) / (z1 - z0);
        x0 += ((x1 - x0) * t) >> 12; u0 += ((u1 - u0) * t) >> 12; z0 = NEARZ;
    } else if (z1 < NEARZ) {
        int t = ((NEARZ - z1) << 12) / (z0 - z1);
        x1 += ((x0 - x1) * t) >> 12; u1 += ((u0 - u1) * t) >> 12; z1 = NEARZ;
    }
    if (z0 > 4095) z0 = 4095;
    if (z1 > 4095) z1 = 4095;
    /* x*FX*256/z == x*recip_z[z]/512 (FY = 2*FX): a multiply instead of two divisions */
    int sx0 = (60 << 8) + (int)(((s64)x0 * recip_z[z0]) >> 9);
    int sx1 = (60 << 8) + (int)(((s64)x1 * recip_z[z1]) >> 9);
    if (sx0 > sx1) {
        int t;
        t = sx0; sx0 = sx1; sx1 = t; t = x0; x0 = x1; x1 = t;
        t = z0; z0 = z1; z1 = t;     t = u0; u0 = u1; u1 = t;
    }
    int c0 = (sx0 + 127) >> 8, c1 = (sx1 + 127) >> 8;
    if (c0 < 0) c0 = 0;
    if (c1 > SCR_W) c1 = SCR_W;
    if (c0 >= c1) return;

    int camh = C->h; (void)B;            /* flat ground: every building stands at height 0 */
    int hc = H - camh;
    int iz0 = recip_z[z0], iz1 = recip_z[z1];
    /* fully hidden behind nearer walls? */
    int yt0 = HORIZON - ((hc * iz0) >> 16), yt1 = HORIZON - ((hc * iz1) >> 16);
    int ytmin = yt0 < yt1 ? yt0 : yt1;
    int vis = 0;
    for (int c = c0; c < c1; c++) if (col_ymax[c] >= ytmin) { vis = 1; break; }
    if (!vis) return;

    int dx16 = (sx1 - sx0) >> 4; if (dx16 < 1) dx16 = 1;
    int izstep = ((iz1 - iz0) << 4) / dx16;                    /* per column */
    int uz0 = u0 * (iz0 >> 4), uz1 = u1 * (iz1 >> 4);           /* u/z, scaled */
    int uzstep = ((uz1 - uz0) / dx16) << 4;
    int frac = (c0 << 8) + 128 - sx0;
    int iz = iz0 + (int)(((s64)izstep * frac) >> 8);
    int uz = uz0 + (int)(((s64)uzstep * frac) >> 8);
    int ua = 0, ub = 0, blk = -1, cstart = 0;
    /* storeys above the shop level: the same for every column of this wall */
    int nfl = (H - 16 - 64) / styles[style].storey;
    if (nfl < 0) nfl = 0;
    const int hb = 64 + nfl * styles[style].storey;

    for (int c = c0; c < c1; c++, iz += izstep, uz += uzstep) {
        int ym = col_ymax[c];
        if (ym < 0) continue;
        int izc = iz < 64 ? 64 : iz;
        int yt = HORIZON - ((hc * izc) >> 16);
        if (yt > ym) continue;                                  /* hidden in this column */
        int yb = HORIZON + ((camh * izc) >> 16);
        if (col_wiz[c] == 0) { col_wiz[c] = izc; col_wtop[c] = yt; col_gfl[c] = (s16)(yb < 0 ? 0 : yb); }
        int rs = yt < 0 ? 0 : yt;
        int re = yb - 1 < ym ? yb - 1 : ym;
        int nym = yt - 1;
        col_ymax[c] = nym;
        if (nym < 0) cols_left--;
        if (rs > re) continue;

        int f16 = fog_from_iz(izc);
        int fe = fog_pick(f16, bayer[(c & 1)]), fo = fog_pick(f16, bayer[(c & 1) | 2]);
        u16 *col = FB + c;
        int detail = f16 < 40;
        u16 cw0 = PIX2(WPAL(mat, fe, light)), cw1 = PIX2(WPAL(mat, fo, light));
        if (!detail) { vspan(col, rs, re, cw0, cw1); continue; }
        u16 ct0 = PIX2(WPAL(MAT_TRIM, fe, light)), ct1 = PIX2(WPAL(MAT_TRIM, fo, light));
        /* texture u: exact every 4 columns, linear in between */
        int b = c >> 2;
        if (b != blk) {
            int izb = iz + izstep * 4; if (izb < 64) izb = 64;
            /* consecutive blocks share an edge: reuse the previous block's end instead of dividing again */
            if (b == blk + 1 && c == cstart + 4) ua = ub;
            else ua = uz / (izc >> 4 > 0 ? izc >> 4 : 1);
            ub = (uz + uzstep * 4) / (izb >> 4 > 0 ? izb >> 4 : 1);
            blk = b; cstart = c;
        }
        int u = ua + (((ub - ua) * (c - cstart)) >> 2);
        if (u < 0) u = 0;
        /* facade style: 0 brick tenement, 1 brownstone, 2 office ribbon glass, 3 modern concrete */
        const StyleDef *S = &styles[style];
        int bq = (int)(((unsigned)u * bay_rcp[style]) >> 20);   /* u / bay without a division (unsigned: long walls) */
        int um = u - bq * S->bay;
        int edge = (u >= 16 && u <= L - 16);
        int wincol = edge && um >= S->w0 && um < S->w1;
        int mullion = (style == 2) && (um % 16) < 2;
        int shopcol = edge && (u % 48) >= 4 && (u % 48) < 44;
        u16 cs0 = PIX2(WPAL(MAT_SILVER, fe, light)), cs1 = PIX2(WPAL(MAT_SILVER, fo, light));
        /* the facade is a stack of horizontal bands; each maps to a run of rows */
        #define ROW(hh) (HORIZON - (((hh) - camh) * izc >> 16))
        int cr = rs;
        cr = band(col, cr, re, ROW(H - 16), ct0, ct1);                 /* cornice */
        if (cr <= re) {
            int SH = S->storey;
            cr = band(col, cr, re, ROW(hb), cw0, cw1);
            int bay = bq + seed;
            for (int fl = nfl - 1; fl >= 0 && cr <= re; fl--) {
                int base = 64 + fl * SH;
                if (wincol) {
                    int hsh = (bay * 7 + fl * 13) * 2654435761u >> 29;
                    int gl = hsh < 3;
                    u16 g0 = PIX2(WPAL(wmat, fe, gl)), g1 = PIX2(WPAL(wmat, fo, gl));
                    if (mullion) { g0 = ct0; g1 = ct1; }
                    if (S->lintel) {
                        cr = band(col, cr, re, ROW(base + S->v1 + 3), cw0, cw1);
                        cr = band(col, cr, re, ROW(base + S->v1), cs0, cs1);       /* stone lintel */
                    } else cr = band(col, cr, re, ROW(base + S->v1), cw0, cw1);
                    cr = band(col, cr, re, ROW(base + S->v0), g0, g1);
                    if (S->sill) cr = band(col, cr, re, ROW(base + S->v0 - 2), S->sill == 2 ? cs0 : ct0, S->sill == 2 ? cs1 : ct1);
                } else if (style == 2) {
                    /* spandrel line between ribbon windows */
                    cr = band(col, cr, re, ROW(base + S->v1), cw0, cw1);
                    cr = band(col, cr, re, ROW(base + S->v0), ct0, ct1);
                }
                cr = band(col, cr, re, ROW(base), cw0, cw1);
            }
            /* storefront level: optional striped awning, band, shop glass, plinth */
            int awning = (style < 2) && ((seed >> 3) & 1) && edge;
            if (awning) {
                static const u8 awn_mat[3] = { MAT_BRICK, MAT_GRASS, MAT_GLASS };
                int am = awn_mat[(seed >> 4) % 3];
                int stripe = (u / 6) & 1;
                u16 a0 = stripe ? cs0 : PIX2(WPAL(am, fe, 1)), a1 = stripe ? cs1 : PIX2(WPAL(am, fo, 1));
                cr = band(col, cr, re, ROW(58), ct0, ct1);
                cr = band(col, cr, re, ROW(48), a0, a1);
                cr = band(col, cr, re, ROW(46), PIX2(WPAL(MAT_TRIM, fe, 0)), PIX2(WPAL(MAT_TRIM, fo, 0)));   /* shade under it */
            } else cr = band(col, cr, re, ROW(56), ct0, ct1);
            if (shopcol) {
                int gl = (seed >> 1) & 1;
                u16 g0 = PIX2(WPAL(wmat, fe, gl)), g1 = PIX2(WPAL(wmat, fo, gl));
                if (!awning) cr = band(col, cr, re, ROW(48), cw0, cw1);
                cr = band(col, cr, re, ROW(6), g0, g1);
            }
            cr = band(col, cr, re, ROW(4), cw0, cw1);
            vspan(col, cr, re, ct0, ct1);                                  /* dark plinth at street level */
        }
        #undef ROW
    }
}

typedef struct { s16 idx; s16 key; } SortEnt;
EWRAM_BSS static SortEnt vis[MAX_BUILDINGS];

static void draw_buildings(void)
{
    const World *w = &g_world;
    int nv = 0;
    int cx = C->x, cy = C->y;
    /* only the blocks within FARZ of the camera */
    int bx0 = (cx - FARZ) >> 10, bx1 = (cx + FARZ) >> 10, by0 = (cy - FARZ) >> 10, by1 = (cy + FARZ) >> 10;
    if (bx0 < 0) bx0 = 0; if (by0 < 0) by0 = 0; if (bx1 > NB - 1) bx1 = NB - 1; if (by1 > NB - 1) by1 = NB - 1;
    for (int bj = by0; bj <= by1; bj++)
    for (int i = blk_start[bj * NB + bx0], ie = blk_start[bj * NB + bx1 + 1]; i < ie; i++) {
        const Building *b = &w->b[i];
        int qx = cx < b->x0 ? b->x0 : (cx > b->x1 ? b->x1 : cx);
        int qy = cy < b->y0 ? b->y0 : (cy > b->y1 ? b->y1 : cy);
        int ax = cx - qx, ay = cy - qy;
        if (ax < 0) ax = -ax;
        if (ay < 0) ay = -ay;
        int d = ax > ay ? ax + (ay >> 1) : ay + (ax >> 1);
        if (d > FARZ) continue;
        int xs[4], zs[4];
        to_cam(b->x0, b->y0, &xs[0], &zs[0]); to_cam(b->x1, b->y0, &xs[1], &zs[1]);
        to_cam(b->x1, b->y1, &xs[2], &zs[2]); to_cam(b->x0, b->y1, &xs[3], &zs[3]);
        int behind = 0, right = 0, left = 0;
        for (int k = 0; k < 4; k++) {
            if (zs[k] < NEARZ) behind++;
            if (xs[k] * FX > zs[k] * 62) right++;
            if (-xs[k] * FX > zs[k] * 62) left++;
        }
        if (behind == 4 || right == 4 || left == 4) continue;
        int j = nv++;
        while (j > 0 && vis[j - 1].key > d) { vis[j] = vis[j - 1]; j--; }
        vis[j].idx = i; vis[j].key = d;
    }
    for (int k = 0; k < nv && cols_left > 0; k++) {
        int bi = vis[k].idx;
        const Building *b = &w->b[bi];
        int H = b->h;
        int style = (b->mat == MAT_GLASS) ? 2 : (b->mat == MAT_BROWN) ? 1 :
                    (b->mat == MAT_CONCRETE) ? ((bi & 1) ? 3 : 2) : (bi % 3 == 0 ? 3 : 0);
        int xa, za, xb, zb;
        if (cx < b->x0) {
            to_cam(b->x0, b->y1, &xa, &za); to_cam(b->x0, b->y0, &xb, &zb);
            draw_wall(xa, za, 0, xb, zb, b->y1 - b->y0, b->y1 - b->y0, H, b->base, b->mat, b->wmat, 0, bi * 3, style);
        } else if (cx > b->x1) {
            to_cam(b->x1, b->y0, &xa, &za); to_cam(b->x1, b->y1, &xb, &zb);
            draw_wall(xa, za, 0, xb, zb, b->y1 - b->y0, b->y1 - b->y0, H, b->base, b->mat, b->wmat, 0, bi * 5, style);
        }
        if (cy < b->y0) {
            to_cam(b->x0, b->y0, &xa, &za); to_cam(b->x1, b->y0, &xb, &zb);
            draw_wall(xa, za, 0, xb, zb, b->x1 - b->x0, b->x1 - b->x0, H, b->base, b->mat, b->wmat, 1, bi * 7, style);
        } else if (cy > b->y1) {
            to_cam(b->x1, b->y1, &xa, &za); to_cam(b->x0, b->y1, &xb, &zb);
            draw_wall(xa, za, 0, xb, zb, b->x1 - b->x0, b->x1 - b->x0, H, b->base, b->mat, b->wmat, 1, bi * 11, style);
        }
    }
}

/* ------------------------------------------------------ streets and sky */
/* ---------------------------------------------------------------- ground */
EWRAM_BSS static s32 col_k[SCR_W];                     /* column slope (2x-119)/(2FX), 16.16 */
EWRAM_BSS static u8 fogpair[128][2][2];                  /* palette fog offsets [z>>5][column parity][row parity] */
#define GRID_LIM GRID_U
#define LOTW (NB + 1)

/* palette base (8 + mat*8, no light) per terrain material code */
static const u8 tm_b8[10] = { 8 + MAT_GRASS * 8, 8 + MAT_WATER * 8, 8 + MAT_SAND * 8, 8 + MAT_GRASS * 8, 8 + MAT_BROWN * 8,
                              8 + MAT_ASPHALT * 8, 8 + MAT_YELLOW * 8, 8 + MAT_SIDEWALK * 8, 8 + MAT_SIDEWALK * 8, 8 + MAT_CONCRETE * 8 };

/* palette base (8 + mat*8 + light) per street tile code; 0xFF/0xFE = the block's lot surface */
static const u8 tile_base[11] = {
    0xFF, 8 + MAT_ASPHALT * 8 + 1, 8 + MAT_SIDEWALK * 8 + 1, 8 + MAT_YELLOW * 8 + 1, 8 + MAT_SILVER * 8 + 1,
    8 + MAT_ASPHALT * 8, 8 + MAT_SIDEWALK * 8, 8 + MAT_CONCRETE * 8, 0xFE,
    8 + MAT_TRIM * 8 + 1, 8 + MAT_GRASS * 8
};
static u8 lot_base[LOTW * LOTW];                /* palette base (no light) of each block's lot */
EWRAM_BSS static u8 tile_pal[128 * 128];            /* tile_base[ground_tile[i]], resolved once at start-up */
#define GRID_TX (GRID_LIM >> 3)

/* on-grid ground texel -> palette base (lots resolved per block) */
static inline __attribute__((always_inline)) int grid_px(int gu, int gv, const u8 *tp, const u8 *lb)
{
    int b = tp[((gv >> 9) & (127 << 7)) | ((gu >> 16) & 127)];
    if (b >= 0xFE) b = lb[(gv >> 23) * LOTW + (gu >> 23)] + (b & 1);
    return b;
}
/* Off-grid ground spans. Everything except the lane lines is constant inside one terrain cell
   (1 m; the street grid's edges and the map's edges fall on cell edges), so a run of pixels in
   the same cell reuses the last result: the per-pixel work is two adds and a compare. Lane
   lines are worked out per pixel, with the four road samples cached per sample block. The
   result is exactly gen_base's. u, v: terrain-map units (cell = 1 << 20). */
static struct { u32 cu, cv; int lanes; } gc;
#define GRID_U0 ((u32)(-TMAP_ORIGIN) << 16)          /* u of street-grid texel 0 */
/* Lane lines: t, the road coordinate (1/64 m, 512 at the centre line) bilinearly interpolated from the
   four road samples around the pixel (terrain_r, every metre, offset by half a cell), picks the colour:
   zone 0 concrete, 1 asphalt, 2 edge line, 3 asphalt, 4 centre line (dashed cells only), 5 asphalt,
   6 edge line, 7 asphalt, 8 concrete. */
static inline int lane_zone(int t)
{
    return (t >= 218) + (t >= 230) + (t >= 262) + (t >= 499) + (t >= 526) + (t >= 763) + (t >= 795) + (t >= 807);
}
EWRAM_BSS static u8 zone_of_t[128 * 8];          /* lane_zone(t) for every t the samples can give */
static const u8 zone_base[9] = { 0, 8 + MAT_ASPHALT * 8 + 1, 8 + MAT_SIDEWALK * 8 + 1, 8 + MAT_ASPHALT * 8 + 1, 8 + MAT_ASPHALT * 8 + 1,
                                 8 + MAT_ASPHALT * 8 + 1, 8 + MAT_SIDEWALK * 8 + 1, 8 + MAT_ASPHALT * 8 + 1, 0 };
/* the sample block (2x2 road samples) of the last lane pixel, as interpolation coefficients:
   la = a0 + fx * da, lb = b0 + fx * db, t = (16 la + fy (lb - la)) >> 5. zone >= 0: the block has
   one zone throughout; ok = 0: not every sample is on a road (plain terrain colour). */
typedef struct { int rk, ok, zone, a0, da, b0, db; } LaneBlock;
static LaneBlock lk = { 0x7FFFFFFF, 0, 0, 0, 0, 0, 0 };
static __attribute__((noinline)) void lane_block(int rk)
{
    LaneBlock *L = &lk;
    const u8 *r = terrain_r + rk;
    int r0 = r[0], r1 = r[1], r2 = r[TMAP_W], r3 = r[TMAP_W + 1];
    L->rk = rk; L->ok = r0 && r1 && r2 && r3;
    r0 &= 127; r1 &= 127; r2 &= 127; r3 &= 127;
    L->a0 = r0 << 4; L->da = r1 - r0; L->b0 = r2 << 4; L->db = r3 - r2;
    int lo = r0, hi = r0;
    if (r1 < lo) lo = r1; if (r1 > hi) hi = r1;
    if (r2 < lo) lo = r2; if (r2 > hi) hi = r2;
    if (r3 < lo) lo = r3; if (r3 > hi) hi = r3;
    int z = zone_of_t[lo * 8];                     /* the interpolated t always lies in [8 lo, 8 hi] */
    L->zone = z == zone_of_t[hi * 8] ? z : -1;
}
/* a road pixel close enough for lane lines: u, v map units, idx its cell, m the cell's terrain_m */
static inline __attribute__((always_inline)) int lane_px(u32 u, u32 v, int idx, int m)
{
    u32 su = u - (8u << 16), sv = v - (8u << 16);   /* sample-block coordinates */
    int rk = ((int)sv >> 20) * TMAP_W + ((int)su >> 20);
    const LaneBlock *L = &lk;
    if (rk != L->rk) lane_block(rk);
    if (!L->ok) return tm_b8[m & 127] + (m >> 7);
    int z = L->zone;
    if (z < 0) {                                   /* lines cross this block: per pixel */
        int fx = (su >> 16) & 15, fy = (sv >> 16) & 15;
        int la = L->a0 + fx * L->da, lb = L->b0 + fx * L->db;
        z = zone_of_t[((la << 4) + fy * (lb - la)) >> 5];
    }
    if (z == 0 || z == 8) return 8 + MAT_CONCRETE * 8 + (m >> 7);
    if (z == 4 && (terrain_r[idx] >> 7)) return 8 + MAT_YELLOW * 8 + 1;
    return zone_base[z];
}
/* an off-grid texel (callers handle the street grid); sets the reuse cell gc.cu, gc.cv */
static __attribute__((noinline)) int gen_slow(u32 u, u32 v, int lanes)
{
    const u32 lim = (TMAP_W - 2) << 20;
    gc.cu = u; gc.cv = v;
    if (u >= lim || v >= lim) return 8 + MAT_WATER * 8 + (((u ^ v) >> 21) & 1);
    int idx = (v >> 20) * TMAP_W + (u >> 20);
    int m = terrain_m[idx], mt = m & 127;
    if (!(lanes && mt >= TM_ROAD && mt != TM_PATH)) return tm_b8[mt] + (m >> 7);
    gc.cu = ~u;                                        /* lane lines: per pixel */
    return lane_px(u, v, idx, m);
}
/* grid texel (the street grid's texels are smaller than a terrain cell, so no reuse) */
static inline __attribute__((always_inline)) int grid_uv(u32 g, u32 h)
{
    int b = tile_pal[((h >> 12) & (127 << 7)) | ((g >> 19) & 127)];
    if (b >= 0xFE) b = lot_base[(h >> 26) * LOTW + (g >> 26)] + (b & 1);
    return b;
}
#define ROR16(x) (((x) >> 16) | ((x) << 16))
/* one texel that is not in the current reuse cell: street grid, open sea, plain terrain, or a lane-line
   pixel (gen_slow). Updates the reuse cell (*cu, *cv) and returns the palette base. */
static inline __attribute__((always_inline)) u32 gen_miss(u32 u, u32 v, u32 *cu, u32 *cv)
{
    u32 g = u - GRID_U0, h = v - GRID_U0;
    if (g < ((u32)GRID_TX << 19) && h < ((u32)GRID_TX << 19)) { *cu = ~u; return (u32)grid_uv(g, h); }
    const u32 lim = (TMAP_W - 2) << 20;
    *cu = u; *cv = v;
    if (u >= lim || v >= lim) return 8 + MAT_WATER * 8 + (((u ^ v) >> 21) & 1);
    int idx = (v >> 20) * TMAP_W + (u >> 20);
    int m = terrain_m[idx], mt = m & 127;
    if (mt < TM_ROAD || mt == TM_PATH || !gc.lanes) return (u32)(tm_b8[mt] + (m >> 7));
    *cu = ~u;                                          /* lane lines: no whole-cell reuse */
    return (u32)lane_px(u, v, idx, m);
}
/* off-grid ground: logical pixels p..e-1 of one row (pp: fog offsets of this pixel and the next) */
static __attribute__((noinline)) void gen_run(u16 *p, u16 *e, u32 u, u32 v, u32 su8, u32 sv8, u32 pp)
{
    u32 cu = ~u, cv = v, cb = 0;
    for (; p < e; p++, u += su8, v += sv8, pp = ROR16(pp)) {
        if (((u ^ cu) | (v ^ cv)) >= (1u << 20)) cb = gen_miss(u, v, &cu, &cv) * 0x0101;
        *p = (u16)(cb + pp);
    }
}
/* the same into two rows (pp0, pp1: fog offsets of the two rows) */
static __attribute__((noinline)) void gen_run2(u16 *p, u16 *e, u32 u, u32 v, u32 su8, u32 sv8, u32 pp0, u32 pp1)
{
    u32 cu = ~u, cv = v, cb = 0;
    for (; p < e; p++, u += su8, v += sv8, pp0 = ROR16(pp0), pp1 = ROR16(pp1)) {
        if (((u ^ cu) | (v ^ cv)) >= (1u << 20)) cb = gen_miss(u, v, &cu, &cv) * 0x0101;
        p[SCR_W] = (u16)(cb + pp1);
        p[0] = (u16)(cb + pp0);
    }
}
/* off-grid ground, columns x..x1-1 of one row */
static void gen_span(u16 *row, int x, int x1, int gu, int gv, int su, int sv, int fe, int fo, int lanes)
{
    gc.lanes = lanes;
    u32 pe = (u32)fe * 0x0101, po = (u32)fo * 0x0101;
    u32 pp = (x & 1) ? po | (pe << 16) : pe | (po << 16);
    gen_run(row + x, row + x1, (u32)(gu << 3) + GRID_U0, (u32)(gv << 3) + GRID_U0, (u32)su << 3, (u32)sv << 3, pp);
}
/* off-grid ground, n column pairs into rows y and y+1 (one sample per column, both rows) */
static void gen_span2(u32 *dst, int n, int gu, int gv, int su, int sv, u32 pp0, u32 pp1, int lanes)
{
    gc.lanes = lanes;
    gen_run2((u16 *)dst, (u16 *)dst + 2 * n, (u32)(gu << 3) + GRID_U0, (u32)(gv << 3) + GRID_U0, (u32)su << 3, (u32)sv << 3, pp0, pp1);
}
/* any ground texel (16.16 texels), for the odd single pixels */
static __attribute__((noinline)) int ground_base(int gu, int gv, int lanes)
{
    if ((unsigned)(gu >> 16) < GRID_TX && (unsigned)(gv >> 16) < GRID_TX) return grid_px(gu, gv, tile_pal, lot_base);
    return gen_slow((u32)(gu << 3) + GRID_U0, (u32)(gv << 3) + GRID_U0, lanes);
}
/* Tight span loops, kept out of draw_ground so every variable stays in a register:
   n column pairs of on-grid ground, one texture sample per column. */
static __attribute__((noinline)) void grid_span(u32 *dst, int n, int gu, int gv, int su, int sv, u32 pp)
{
    const u8 *tp = tile_pal; const u8 *lb = lot_base;
    u32 *end = dst + n;
    do {
        int b0 = grid_px(gu, gv, tp, lb); gu += su; gv += sv;
        int b1 = grid_px(gu, gv, tp, lb); gu += su; gv += sv;
        *dst++ = ((u32)b0 | ((u32)b1 << 16)) * 0x0101u + pp;
    } while (dst < end);
}
/* the same into two rows at once (near ground: one sample shared by rows y and y+1) */
static __attribute__((noinline)) void grid_span2(u32 *dst, int n, int gu, int gv, int su, int sv, u32 pp0, u32 pp1)
{
    const u8 *tp = tile_pal; const u8 *lb = lot_base;
    u32 *end = dst + n;
    do {
        int b0 = grid_px(gu, gv, tp, lb); gu += su; gv += sv;
        int b1 = grid_px(gu, gv, tp, lb); gu += su; gv += sv;
        u32 bb = ((u32)b0 | ((u32)b1 << 16)) * 0x0101u;
        dst[SCR_W / 2] = bb + pp1;
        *dst++ = bb + pp0;
    } while (dst < end);
}

/* flat ground, floor-cast row by row into every pixel that is not covered by a wall */
static void draw_ground(void)
{
    const int c = C->c, s = C->s, camh = C->h;
    if (!lot_base[0]) for (int i = 0; i < LOTW * LOTW; i++) lot_base[i] = (u8)(8 + g_world.lotmat[i] * 8);   /* the lots never change: build once */
    const int gu0 = C->x << 13, gv0 = C->y << 13;            /* texel 16.16 */
    const int du0 = (c << 2) + ((s * col_k[0]) >> 14), dv0 = (s << 2) - ((c * col_k[0]) >> 14);   /* units 16.16 per z */
    const int kst = col_k[1] - col_k[0];
    const int ddu = (s * kst) >> 14, ddv = -((c * kst) >> 14);
    /* first ground row per column; columns with ground above a short wall get patched below */
    static u8 gs[SCR_W];
    int npatch = 0; static u8 patch[SCR_W];
    for (int x = 0; x < SCR_W; x++) {
        int ym = col_ymax[x], gf = col_gfl[x];
        if (ym <= HORIZON) gs[x] = (u8)(gf > SCR_H ? SCR_H : gf);
        else if (ym >= gf - 1) gs[x] = HORIZON + 1;
        else { gs[x] = (u8)gf; patch[npatch++] = (u8)x; }
    }
    int gsmax = 0;
    static u8 gmx[SCR_W / 2], gmn[SCR_W / 2];      /* per column pair: rows where ground starts in both / either */
    for (int i = 0; i < SCR_W / 2; i++) {
        int a = gs[2 * i], b = gs[2 * i + 1];
        gmx[i] = (u8)(a > b ? a : b); gmn[i] = (u8)(a < b ? a : b);
        if (gmx[i] > gsmax) gsmax = gmx[i];
    }
    for (int y = HORIZON + 1; y < SCR_H; y++) {
        int z = (camh * row_k[y]) >> 16;
        if (z > 4095) z = 4095;
        int gu = gu0 + ((z * du0) >> 3), gv = gv0 + ((z * dv0) >> 3);
        const int su = (z * ddu) >> 3, sv = (z * ddv) >> 3;
        const int fe = fogpair[z >> 5][0][y & 1], fo = fogpair[z >> 5][1][y & 1];
        const int lanes = z < 640;
        u16 *row = FB + y * SCR_W;
        /* is this whole row on the street grid? (it is a straight line in texel space) */
        const unsigned lim = GRID_TX;
        const int gue = gu + su * (SCR_W - 1), gve = gv + sv * (SCR_W - 1);
        const int ingrid = (unsigned)(gu >> 16) < lim && (unsigned)(gv >> 16) < lim && (unsigned)(gue >> 16) < lim && (unsigned)(gve >> 16) < lim;
        #define PAIR_BASE(b) do { if (ingrid) { int tx_ = gu >> 16, ty_ = gv >> 16; b = tile_pal[((ty_ & 127) << 7) | (tx_ & 127)]; \
            if (b >= 0xFE) b = lot_base[(ty_ >> 7) * LOTW + (tx_ >> 7)] + (b & 1); } else b = ground_base(gu, gv, lanes); } while (0)
        if (z < 256 && y + 1 < SCR_H) {        /* near rows: one sample per column, shared by two rows */
            const int y1 = y + 1;
            const int fe1 = fogpair[z >> 5][0][y1 & 1], fo1 = fogpair[z >> 5][1][y1 & 1];
            const u32 pp0 = (u32)(fe * 0x0101) | ((u32)(fo * 0x0101) << 16), pp1 = (u32)(fe1 * 0x0101) | ((u32)(fo1 * 0x0101) << 16);
            u32 *r0 = (u32 *)row, *r1 = (u32 *)(row + SCR_W);
            const int gur = gu, gvr = gv;
            for (int i = 0; i < SCR_W / 2; ) {
                if (y >= gmx[i]) {                     /* a run of column pairs that are ground in both rows */
                    int j = i + 1;
                    while (j < SCR_W / 2 && y >= gmx[j]) j++;
                    if (ingrid) grid_span2(r0 + i, j - i, gur + su * 2 * i, gvr + sv * 2 * i, su, sv, pp0, pp1);
                    else gen_span2(r0 + i, j - i, gur + su * 2 * i, gvr + sv * 2 * i, su, sv, pp0, pp1, lanes);
                    i = j; continue;
                }
                if (y1 < gmn[i]) { i++; continue; }
                gu = gur + su * 2 * i; gv = gvr + sv * 2 * i;
                int b0, b1;
                PAIR_BASE(b0); gu += su; gv += sv;
                PAIR_BASE(b1);
                u32 bb = (u32)b0 * 0x0101u | ((u32)b1 * 0x0101u << 16), p0 = bb + pp0, p1 = bb + pp1;
                if (y >= gmx[i]) { r0[i] = p0; r1[i] = p1; i++; continue; }
                int x = i * 2, ga = gs[x], gb = gs[x + 1];
                u16 *q0 = row + x, *q1 = q0 + SCR_W;
                if (y >= ga) q0[0] = (u16)p0;
                if (y >= gb) q0[1] = (u16)(p0 >> 16);
                if (y1 >= ga) q1[0] = (u16)p1;
                if (y1 >= gb) q1[1] = (u16)(p1 >> 16);
                i++;
            }
            y++;
            continue;
        }
        if (z < 256) {                         /* near rows: one sample per two columns */
            const int su2 = su * 2, sv2 = sv * 2;
            const u32 pp = (u32)(fe * 0x0101) | ((u32)(fo * 0x0101) << 16);
            u32 *rw = (u32 *)row;
            for (int i = 0; i < SCR_W / 2; i++, gu += su2, gv += sv2) {
                if (y < gmn[i]) continue;
                int base; PAIR_BASE(base);
                u32 p = (u32)base * 0x01010101u + pp;
                if (y >= gmx[i]) { rw[i] = p; continue; }
                if (y >= gs[2 * i]) row[2 * i] = (u16)p; else row[2 * i + 1] = (u16)(p >> 16);
            }
        } else {
            if (ingrid) {
                const u16 pe = (u16)(fe * 0x0101), po = (u16)(fo * 0x0101);
                const u32 pp = (u32)pe | ((u32)po << 16);
                u32 *rw = (u32 *)row;
                for (int i = 0; i < SCR_W / 2; ) {
                    if (y >= gmx[i]) {
                        int j = i + 1;
                        while (j < SCR_W / 2 && y >= gmx[j]) j++;
                        grid_span(rw + i, j - i, gu + su * 2 * i, gv + sv * 2 * i, su, sv, pp);
                        i = j; continue;
                    }
                    if (y >= gmn[i]) {                 /* only one column of the pair shows ground */
                        int x = y >= gs[2 * i] ? 2 * i : 2 * i + 1;
                        int b = grid_px(gu + su * x, gv + sv * x, tile_pal, lot_base);
                        row[x] = (u16)(b * 0x0101 + ((x & 1) ? po : pe));
                    }
                    i++;
                }
            } else {
                for (int x = 0; x < SCR_W; ) {
                    if (y < gs[x]) { x++; continue; }
                    int x1 = x + 1;
                    while (x1 < SCR_W && y >= gs[x1]) x1++;
                    gen_span(row, x, x1, gu + su * x, gv + sv * x, su, sv, fe, fo, lanes);
                    x = x1;
                }
            }
        }
    }
    for (int i = 0; i < npatch; i++) {           /* ground seen above a wall lower than the eye */
        int x = patch[i];
        for (int y = HORIZON + 1; y <= col_ymax[x]; y++) {
            int z = (camh * row_k[y]) >> 16; if (z > 4095) z = 4095;
            int gu = gu0 + ((z * (du0 + ddu * x)) >> 3), gv = gv0 + ((z * (dv0 + ddv * x)) >> 3);
            int base = ground_base(gu, gv, z < 640);
            FB[y * SCR_W + x] = (u16)((base + fogpair[z >> 5][x & 1][y & 1]) * 0x0101);
        }
    }
}

static void draw_sky(void)
{
    /* sky: 6-level gradient into the haze, with clouds (palette 6 = white, 7 = shade) that
       scroll with the camera heading. Filled column-pair by column-pair, only down to each
       column's first covered row, so hidden sky costs nothing. */
    static u32 grad[HORIZON + 1];             /* gradient colour pair per row (IWRAM: read for every sky pixel) */
    for (int y = 0; y <= HORIZON; y++) {
        int g = (y * 5 * 16) / HORIZON;
        int l = g >> 4, fr = g & 15;
        int le = l + (fr > bayer[(y & 1) << 1] && l < 5);
        int lo = l + (fr > bayer[1 | ((y & 1) << 1)] && l < 5);
        grad[y] = PIX2(le) | ((u32)PIX2(lo) << 16);
    }
    int hang = iatan2(C->s, C->c) >> 4;                    /* 0..255 */
    for (int x = 0; x < SCR_W; x += 2) {
        int ya = col_ymax[x], yb = col_ymax[x + 1];
        if (ya > HORIZON) ya = HORIZON;
        if (yb > HORIZON) yb = HORIZON;
        int ymin = ya < yb ? ya : yb, ymaxx = ya > yb ? ya : yb;
        if (ymaxx < 0) continue;
        const u8 *cl = clouds + ((hang + col_ang[x]) & 255);
        u16 *p = FB + x;
        int y = 0;
        for (; y <= ymin; y++, p += SCR_W) {
            u32 v = grad[y];
            if (y < 64) {
                int k = cl[y << 8];
                if (k == 2) v = PIX2(6) * 0x10001u;
                else if (k == 1) v = (y & 1) ? ((v & 0xFFFF0000u) | PIX2(7)) : ((v & 0xFFFFu) | ((u32)PIX2(7) << 16));
            }
            *(u32 *)p = v;
        }
        for (; y <= ymaxx; y++, p += SCR_W) {             /* the taller of the pair */
            u32 v = grad[y];
            if (y < 64) { int k = cl[y << 8]; if (k == 2) v = PIX2(6) * 0x10001u; }
            if (ya >= y) p[0] = (u16)v; else p[1] = (u16)(v >> 16);
        }
    }
}

/* -------------------------------------------------------------- sprites */
typedef struct { s16 idx; s16 pad; s32 z; s32 x; } SprEnt;
static u32 skey[MAX_SPRITES]; EWRAM_BSS static s16 sprx[MAX_SPRITES];   /* depth sort keys (IWRAM: hot) */

/* Sprites are stored at full horizontal resolution (240 px). Each logical column
   covers two physical pixels, which are merged into one halfword write. */
static void blit(const SprFrame *f, int sx8, int sb8, int z, const u8 *occ, int flip)
{
    int izs = recip_z[z > 4095 ? 4095 : z];
    int inv = ((z << 16) / f->upx) * 64 / 43;          /* source px per screen px, 16.16 */
    if (inv < 4096) inv = 4096;
    int i4 = inv >> 4;
    int ox = flip ? -(f->ox + f->w) : f->ox;
    const int ri = (1 << 30) / i4;                     /* one division (i4 >= 256), then (a << 20) / i4 ~= a * ri >> 10 */
    #define DIV_I4(a) ((int)(((s64)(a) * ri) >> 10))
    int xl = (sx8 << 1) + DIV_I4(ox);                  /* physical px, 8.8 */
    int xr = xl + DIV_I4(f->w);
    int yt = sb8 + DIV_I4(f->oy);
    int yb = yt + DIV_I4(f->h);
    #undef DIV_I4
    int p0 = (xl + 127) >> 8, p1 = (xr + 127) >> 8;     /* physical columns [p0,p1) */
    int r0 = (yt + 127) >> 8, r1 = (yb + 127) >> 8;
    int cs = p0 >> 1, ce = (p1 + 1) >> 1;
    if (cs < 0) cs = 0;
    if (ce > SCR_W) ce = SCR_W;
    int rs = r0 < 0 ? 0 : r0, re = r1 > SCR_H ? SCR_H : r1;
    if (cs >= ce || rs >= re) return;
    const u8 *pix = spr_pixels + f->off;
    int h = f->h, w = f->w;
    int v0 = (((rs << 8) + 128 - yt) * i4) >> 4;
    while (v0 < 0 && rs < re) { v0 += inv; rs++; }              /* rows above the image */
    {   /* last row still inside the image, worked out once instead of testing every pixel */
        int left = (h << 16) - v0;
        if (left <= 0) return;
        int rows = (left + inv - 1) / inv;
        if (rs + rows < re) re = rs + rows;
    }
    for (int c = cs; c < ce; c++) {
        int pa = c << 1, pb = pa + 1;
        int ua = ((((pa << 8) + 128 - xl) * i4) >> 4) >> 16;
        int ub = ((((pb << 8) + 128 - xl) * i4) >> 4) >> 16;
        int va = (pa >= p0 && pa < p1 && (unsigned)ua < (unsigned)w);
        int vb = (pb >= p0 && pb < p1 && (unsigned)ub < (unsigned)w);
        if (!va && !vb) continue;
        if (flip) { ua = w - 1 - ua; ub = w - 1 - ub; }
        int rend = re;
        if (izs < col_wiz[c] && col_wtop[c] < rend) rend = col_wtop[c];
        if (occ && occ[c] < rend) rend = occ[c];
        if (rs >= rend) continue;
        const u8 *sa = pix + (va ? ua : ub) * h;
        const u8 *sb = pix + (vb ? ub : ua) * h;
        u16 *p = FB + rs * SCR_W + c;
        int v = v0;
        if (va && vb && sa == sb) {                    /* magnified: both pixels of the pair share a source column */
            for (int r = rs; r < rend; r++) {
                int a = sa[v >> 16];
                if (a) *p = (u16)((a + SPR_PAL_BASE) * 0x0101);
                p += SCR_W; v += inv;
            }
        } else if (va && vb) {
            for (int r = rs; r < rend; r++) {
                int vi = v >> 16;
                int a = sa[vi], b = sb[vi];
                if (a && b) *p = (u16)((a + SPR_PAL_BASE) | ((b + SPR_PAL_BASE) << 8));
                else if (a) { volatile u16 *q = p; *q = (u16)((*q & 0xFF00) | (a + SPR_PAL_BASE)); }   /* halfword stores only (VRAM) */
                else if (b) { volatile u16 *q = p; *q = (u16)((*q & 0x00FF) | ((b + SPR_PAL_BASE) << 8)); }
                p += SCR_W; v += inv;
            }
        } else if (va) {
            for (int r = rs; r < rend; r++) {
                int vi = v >> 16;
                int a = sa[vi]; if (a) { volatile u16 *q = p; *q = (u16)((*q & 0xFF00) | (a + SPR_PAL_BASE)); }
                p += SCR_W; v += inv;
            }
        } else {
            for (int r = rs; r < rend; r++) {
                int vi = v >> 16;
                int b = sb[vi]; if (b) { volatile u16 *q = p; *q = (u16)((*q & 0x00FF) | ((b + SPR_PAL_BASE) << 8)); }
                p += SCR_W; v += inv;
            }
        }
    }
}

/* a lamp after dark: a small glowing dot, sized by distance, hidden behind nearer walls */
static void lamp_dot(int sx8, int sb8, int z, int tail)
{
    int izs = recip_z[z];
    int d = ((tail ? M(0.20) : M(0.28)) * izs) >> 16;
    if (d < 1) d = 1; if (d > (tail ? 3 : 5)) d = tail ? 3 : 5;
    int px0 = (sx8 >> 7) - (d >> 1), py0 = (sb8 >> 8) - (d >> 1);
    for (int yy = 0; yy < d; yy++) {
        int y = py0 + yy;
        if ((unsigned)y >= SCR_H) continue;
        int ey = yy == 0 || yy == d - 1;
        for (int xx = 0; xx < d; xx++) {
            int x = px0 + xx;
            if ((unsigned)x >= SCR_W * 2) continue;
            int ex = xx == 0 || xx == d - 1;
            if (d >= 3 && ex && ey) continue;                /* round the corners */
            int c = x >> 1;
            if (izs < col_wiz[c] && y >= col_wtop[c]) continue;
            int col = tail ? 252 : (d >= 4 && (ex || ey)) ? 253 : 254;   /* HUD red / yellow / white: never night-tinted */
            u16 *p = FB + y * SCR_W + c;
            { volatile u16 *q = p; *q = (x & 1) ? (u16)((*q & 0x00FF) | (col << 8)) : (u16)((*q & 0xFF00) | col); }
        }
    }
}

#define IS_DECAL(f) ((unsigned)(((f) & 0x7FFF) - BLOOD_POOL) < 12u)
static void draw_sprites(const Sprite *spr, int n)
{
    int nv = 0;
    for (int i = 0; i < n; i++) {
        int x, z;
        to_cam(spr[i].x, spr[i].y, &x, &z);
        if (z < M(2.0) || z > 4000) continue;
        if (x * FX > z * 90 || -x * FX > z * 90) continue;
        int key = z + (IS_DECAL(spr[i].frame) ? M(3) : 0);   /* decals sort behind things standing on them */
        /* sort compact 32-bit keys (depth, then sprite index) instead of whole records */
        u32 kk = ((u32)key << 8) | (u32)i;
        sprx[i] = (s16)x;
        int j = nv++;
        while (j > 0 && skey[j - 1] < kk) { skey[j] = skey[j - 1]; j--; }
        skey[j] = kk;
    }
    int first = nv > 64 ? nv - 64 : 0;          /* draw at most the 64 nearest */
    for (int k = first; k < nv; k++) {
        int i = skey[k] & 255;
        const Sprite *s = &spr[i];
        int x = sprx[i];
        int z = (int)(skey[k] >> 8) - (IS_DECAL(s->frame) ? M(3) : 0);
        /* x*FX*256/z == x*recip_z[z]/512 and dh*FY*256/z == dh*recip_z[z]/256: multiplies, no divisions */
        int sx8 = (60 << 8) + (int)(((s64)x * recip_z[z]) >> 9);
        int sb8 = (HORIZON << 8) + (int)(((s64)(C->h - s->h) * recip_z[z]) >> 8);
        if ((s->frame & 0x7FFF) >= SPR_LIGHT) { lamp_dot(sx8, sb8, z, (s->frame & 0x7FFF) - SPR_LIGHT); continue; }
        blit(&spr_frames[s->frame & 0x7FFF], sx8, sb8, z, 0, s->frame >> 15);
    }
}

__attribute__((section(".romcode"))) void render_init(void)   /* runs once: lives in ROM, not IWRAM */
{
    for (int t = 0; t < 128 * 8; t++) zone_of_t[t] = (u8)lane_zone(t);
    for (int i = 0; i < 128 * 128; i++) tile_pal[i] = tile_base[ground_tile[i]];
    for (int x = 0; x < SCR_W; x++) {
        col_ang[x] = (u8)(-(iatan2(2 * x + 1 - SCR_W, 2 * FX) >> 4));
        col_k[x] = ((2 * x + 1 - SCR_W) * 65536) / (2 * FX);
    }
    for (int i = 0; i < 128; i++) {
        int iz = (i << 8) + 128;
        int z = ZK / iz;
        fog_iz[i] = (u8)fog16(z);
        for (int p = 0; p < 2; p++) {
            int f = fog16(i * 32 + 16);
            fogpair[i][p][0] = (u8)(fog_pick(f, bayer[p]) * 2);
            fogpair[i][p][1] = (u8)(fog_pick(f, bayer[p | 2]) * 2);
        }
    }
}

void render_frame(u16 *fb, const Camera *cam, const Sprite *spr, int nspr)
{
    C = cam; FB = fb;
    for (int i = 0; i < SCR_W; i++) {
        col_ymax[i] = SCR_H - 1;
        col_wiz[i] = 0; col_wtop[i] = SCR_H;
    }
    cols_left = SCR_W;
    for (int i = 0; i < SCR_W; i++) col_gfl[i] = SCR_H;
    draw_buildings();
    draw_ground();
    draw_sky();
    draw_sprites(spr, nspr);
}
