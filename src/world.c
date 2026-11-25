/* City generation, collision and small math helpers. */
#include "engine.h"
#include "world.h"
#include "ground.h"

EWRAM_BSS World g_world;  /* EWRAM: frees 3.6 KB of IWRAM for stack; costs well under 1% of frame time */
EWRAM_BSS u16 blk_start[NBLK + 1], tree_start[NBLK + 1];
EWRAM_BSS Tree g_trees[MAX_TREES]; int g_ntrees;

static u32 rng_state = 0x1234567;
u32 rnd(void) { rng_state = rng_state * 1103515245u + 12345u; return rng_state >> 8; }
int rndr(int lo, int hi) { return lo + (int)(rnd() % (u32)(hi - lo + 1)); }

static void add_building(int x0, int y0, int x1, int y1, int h, int mat, int wmat)
{
    World *w = &g_world;
    if (w->nb >= MAX_BUILDINGS || x1 - x0 < M(6) || y1 - y0 < M(6)) return;
    Building *b = &w->b[w->nb++];
    b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1;
    b->h = h; b->mat = mat; b->wmat = wmat;
    /* sit on the lowest point of the footprint so the walls always reach the ground */
    int g = ground_h((x0 + x1) >> 1, (y0 + y1) >> 1), t;
    if ((t = ground_h(x0, y0)) < g) g = t;
    if ((t = ground_h(x1, y0)) < g) g = t;
    if ((t = ground_h(x0, y1)) < g) g = t;
    if ((t = ground_h(x1, y1)) < g) g = t;
    b->base = (s16)(g - 4);
}

/* building height in metres by district (see city_district in terrain.c) */
static int pick_height(int bx, int by)
{
    int d = city_district[by * NB + bx], h;
    switch (d) {
    case DI_DOWNTOWN: {                      /* towers, tallest around blocks 14,4 */
        int dx = bx - 14, dy = by - 4, r = dx * dx + dy * dy;
        h = rndr(22, r < 6 ? 64 : r < 20 ? 52 : 40);
        break; }
    case DI_INDUSTRIAL: h = rndr(8, 16); if (rnd() % 7 == 0) h = rndr(18, 26); break;   /* warehouses, the odd silo block */
    case DI_HEIGHTS:    h = rndr(9, 15); if (rnd() % 8 == 0) h = rndr(18, 24); break;   /* rowhouses and walk-ups */
    case DI_ISLAND:     h = rndr(14, 30); if (rnd() % 3 == 0) h = rndr(44, 62); break;  /* the half-built casino strip */
    default: {                               /* the Docks and Old Town: brick, taller toward block 4,4 */
        int dx = bx * 2 - 7, dy = by * 2 - 7, r = dx * dx + dy * dy;
        h = rndr(12, r < 10 ? 46 : r < 30 ? 36 : 28);
        if (rnd() % 6 == 0) h = rndr(12, 18);
        break; }
    }
    return M(h);
}

static void pick_mats(int h, int d, u8 *mat, u8 *wmat)
{
    static const u8 low[] = { MAT_BRICK, MAT_BROWN, MAT_BRICK, MAT_SAND, MAT_CONCRETE, MAT_BROWN };
    static const u8 ind[] = { MAT_CONCRETE, MAT_BRICK, MAT_CONCRETE, MAT_BROWN, MAT_TRIM, MAT_CONCRETE };
    static const u8 res[] = { MAT_BRICK, MAT_BROWN, MAT_SAND, MAT_BROWN, MAT_BRICK, MAT_SIDEWALK };
    if (d == DI_INDUSTRIAL) { *mat = ind[rnd() % 6]; *wmat = (rnd() & 1) ? MAT_TRIM : MAT_GLASS; return; }
    if (d == DI_HEIGHTS) { *mat = res[rnd() % 6]; *wmat = MAT_GLASS; return; }
    if (h > M(40) && rnd() % 2) { *mat = MAT_GLASS; *wmat = MAT_SILVER; return; }
    if (h > M(30) && rnd() % 2) { *mat = MAT_CONCRETE; *wmat = MAT_GLASS; return; }
    *mat = low[rnd() % 6]; *wmat = MAT_GLASS;
}

void world_generate(void)
{
    World *w = &g_world;
    w->nb = 0; g_ntrees = 0;
    for (int i = 0; i < (NB + 1) * (NB + 1); i++) w->lotmat[i] = MAT_WATER;
    for (int by = 0; by < NB; by++)
    for (int bx = 0; bx < NB; bx++) {
        int bi = by * NB + bx, kind = BLK_KIND(bx, by), d = city_district[bi];
        blk_start[bi] = (u16)w->nb; tree_start[bi] = (u16)g_ntrees;
        int ox = bx * 1024, oy = by * 1024;
        int lx0 = ox + M(15), ly0 = oy + M(15), lx1 = ox + M(61), ly1 = oy + M(61);
        if (kind == BK_WATER) continue;
        if (kind == BK_PARK) {
            w->lotmat[by * (NB + 1) + bx] = MAT_GRASS;
            for (int t = 0; t < 6 && g_ntrees < MAX_TREES; t++) {
                g_trees[g_ntrees].x = (s16)rndr(lx0 + M(4), lx1 - M(4));
                g_trees[g_ntrees].y = (s16)rndr(ly0 + M(4), ly1 - M(4));
                g_ntrees++;
            }
        } else {
            w->lotmat[by * (NB + 1) + bx] = MAT_CONCRETE;
            int split = rnd() % 10;
            if (d == DI_INDUSTRIAL) split = split < 5 ? 0 : 3;     /* big sheds */
            else if (d == DI_HEIGHTS) split = 9;                  /* four buildings per block */
            int mx = (lx0 + lx1) / 2 + rndr(-M(6), M(6));
            int my = (ly0 + ly1) / 2 + rndr(-M(6), M(6));
            int g = M(1);          /* half-alley between buildings */
            int x0s[4], y0s[4], x1s[4], y1s[4], n = 0;
            if (split < 2) {
                x0s[0] = lx0; y0s[0] = ly0; x1s[0] = lx1; y1s[0] = ly1; n = 1;
            } else if (split < 5) {
                if (rnd() & 1) {
                    x0s[0] = lx0; x1s[0] = mx - g; y0s[0] = ly0; y1s[0] = ly1;
                    x0s[1] = mx + g; x1s[1] = lx1; y0s[1] = ly0; y1s[1] = ly1;
                } else {
                    x0s[0] = lx0; x1s[0] = lx1; y0s[0] = ly0; y1s[0] = my - g;
                    x0s[1] = lx0; x1s[1] = lx1; y0s[1] = my + g; y1s[1] = ly1;
                }
                n = 2;
            } else {
                x0s[0] = lx0;    x1s[0] = mx - g; y0s[0] = ly0;    y1s[0] = my - g;
                x0s[1] = mx + g; x1s[1] = lx1;    y0s[1] = ly0;    y1s[1] = my - g;
                x0s[2] = lx0;    x1s[2] = mx - g; y0s[2] = my + g; y1s[2] = ly1;
                x0s[3] = mx + g; x1s[3] = lx1;    y0s[3] = my + g; y1s[3] = ly1;
                n = 4;
            }
            for (int k = 0; k < n; k++) {
                int h = pick_height(bx, by);
                u8 mat, wmat; pick_mats(h, d, &mat, &wmat);
                /* small random setbacks so blocks are not perfectly flat */
                int sx0 = 0, sy1 = 0;
                if (rnd() % 3 == 0) sx0 = M(rndr(0, 3));
                if (rnd() % 3 == 0) sy1 = M(rndr(0, 3));
                add_building(x0s[k] + sx0, y0s[k], x1s[k], y1s[k] - sy1, h, mat, wmat);
            }
        }
    }
    blk_start[NBLK] = (u16)w->nb; tree_start[NBLK] = (u16)g_ntrees;
}

/* ------------------------------------------------------------ palette */
#include "assets.h"
static const u8 base_rgb[NUM_MATS][3] = {
    {74,74,77},    /* asphalt  */ {150,147,140}, /* sidewalk */ {200,168,62},  /* yellow */
    {70,96,52},    /* grass    */ {124,62,48},   /* brick    */ {98,72,58},    /* brownstone */
    {136,135,132}, /* concrete */ {176,156,122}, /* sandstone*/ {82,100,116},  /* glass */
    {52,72,86},    /* water    */ {196,198,200}, /* white paint / frames */ {56,52,50}, /* trim */
};
static const u8 dark_pct[NUM_MATS] = { 92, 88, 80, 88, 74, 74, 78, 76, 66, 86, 82, 74 };
static const u8 fog_pct[4] = { 0, 20, 46, 80 };
static const u8 haze[3] = { 170, 174, 178 };
static const u8 sky_top[3] = { 92, 124, 168 };

static u16 rgb15(int r, int g, int b)
{
    r >>= 3; g >>= 3; b >>= 3;
    if (r > 31) r = 31; if (g > 31) g = 31; if (b > 31) b = 31;
    return (u16)(r | (g << 5) | (b << 10));
}

void palette_build(u16 *pal)
{
    for (int i = 0; i < 6; i++) {
        int c[3];
        for (int k = 0; k < 3; k++) c[k] = sky_top[k] + (haze[k] - sky_top[k]) * i / 5;
        pal[i] = rgb15(c[0], c[1], c[2]);
    }
    pal[6] = rgb15(222, 224, 228);        /* cloud */
    pal[7] = rgb15(178, 184, 194);        /* cloud shade */
    for (int m = 0; m < NUM_MATS; m++)
    for (int f = 0; f < 4; f++)
    for (int l = 0; l < 2; l++) {
        int c[3];
        for (int k = 0; k < 3; k++) {
            int v = base_rgb[m][k] * (l ? 100 : dark_pct[m]) / 100;
            c[k] = v + (haze[k] - v) * fog_pct[f] / 100;
        }
        pal[WPAL(m, f, l)] = rgb15(c[0], c[1], c[2]);
    }
    for (int i = 0; i < SPR_NCOL; i++) pal[SPR_PAL_BASE + 1 + i] = spr_palette[i];
    pal[252] = 0x0C3F;          /* HUD red   */
    pal[253] = 0x1BDF;          /* HUD yellow */
    pal[254] = 0x7FFF;          /* HUD white */
    pal[255] = 0x0000;          /* HUD black */
}

/* ------------------------------------------------------------ day and night */
/* The whole screen is one 256-colour palette, so time of day is a palette re-tint: darker and bluer
   at night, warm at dusk and dawn, a navy night sky, and building windows lit up warm. minute: 0..1439.
   Entries 252..255 (HUD colours) are left alone. Runs in ROM: a few hundred entries every few frames. */
static int clamp255(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }
static int smooth(int a, int b, int x)                  /* 0..256 ramp from a to b (minutes) */
{
    if (x <= a) return 0; if (x >= b) return 256;
    return (x - a) * 256 / (b - a);
}
int tod_daylight(int m)                                  /* 256 = full day, 0 = night */
{
    if (m < 4 * 60 + 30 || m >= 21 * 60) return 0;
    if (m < 7 * 60 + 30) return smooth(4 * 60 + 30, 7 * 60 + 30, m);
    if (m < 17 * 60 + 30) return 256;
    return 256 - smooth(17 * 60 + 30, 21 * 60, m);
}
static int warmth(int m)                                 /* 0..256, peaks at sunset (19:00) and sunrise (6:00) */
{
    int a = m - 19 * 60, b = m - 6 * 60;
    if (a < 0) a = -a; if (b < 0) b = -b;
    int d = a < b ? a : b;
    return d >= 90 ? 0 : (90 - d) * 256 / 90;
}
__attribute__((section(".romcode"))) void palette_tod(volatile u16 *out, const u16 *day, int minute)
{
    int dl = tod_daylight(minute), w = warmth(minute), n = 256 - dl;
    /* world and sprites: scale each channel; night is moonlit blue, dusk adds orange */
    int mr = 256 - (n * 150 >> 8) + (w * 40 >> 8), mg = 256 - (n * 140 >> 8) + (w * 6 >> 8), mb = 256 - (n * 90 >> 8) - (w * 40 >> 8);
    for (int i = 8; i < 252; i++) {
        u16 c = day[i];
        int r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        out[i] = rgb15(clamp255(r * mr >> 8), clamp255(g * mg >> 8), clamp255(b * mb >> 8));
    }
    /* lit windows: the glass in the light variant glows a warm yellow after dark */
    for (int f = 0; f < 4; f++) {
        int k = WPAL(MAT_GLASS, f, 1), u = n * (4 - f) * 5 / 32;    /* about 60% of the way to lamp-light, fading with distance */
        u16 c = out[k];
        int r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        out[k] = rgb15(r + ((214 - r) * u >> 8), g + ((168 - g) * u >> 8), b + ((96 - b) * u >> 8));
    }
    /* sky: day gradient, then sunset orange at the horizon, then navy night */
    static const u8 night_top[3] = { 10, 16, 40 }, night_hor[3] = { 34, 42, 72 }, dusk_top[3] = { 90, 80, 140 }, dusk_hor[3] = { 250, 150, 80 };
    for (int i = 0; i < 8; i++) {
        u16 c = day[i];
        int r = (c & 31) << 3, g = ((c >> 5) & 31) << 3, b = ((c >> 10) & 31) << 3;
        int t = i < 6 ? i * 256 / 5 : 200;                 /* 0 top .. 256 horizon; clouds near the horizon colour */
        int nr = night_top[0] + ((night_hor[0] - night_top[0]) * t >> 8), ng = night_top[1] + ((night_hor[1] - night_top[1]) * t >> 8), nb = night_top[2] + ((night_hor[2] - night_top[2]) * t >> 8);
        if (i >= 6) { nr += 14; ng += 14; nb += 16; }      /* clouds catch a little city glow */
        int dr = dusk_top[0] + ((dusk_hor[0] - dusk_top[0]) * t >> 8), dg = dusk_top[1] + ((dusk_hor[1] - dusk_top[1]) * t >> 8), db = dusk_top[2] + ((dusk_hor[2] - dusk_top[2]) * t >> 8);
        r = r + ((dr - r) * w >> 9); g = g + ((dg - g) * w >> 9); b = b + ((db - b) * w >> 9);   /* half-strength dusk tint */
        r = nr + ((r - nr) * dl >> 8); g = ng + ((g - ng) * dl >> 8); b = nb + ((b - nb) * dl >> 8);
        out[i] = rgb15(clamp255(r), clamp255(g), clamp255(b));
    }
}
