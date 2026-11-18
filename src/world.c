/* City generation, collision and small math helpers. */
#include "engine.h"
#include "world.h"
#include "ground.h"

EWRAM_BSS World g_world;  /* EWRAM: frees 3.6 KB of IWRAM for stack; costs well under 1% of frame time */
u16 blk_start[64], blk_count[64];
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

static int pick_height(int bx, int by)
{
    /* taller towards the middle of the island ("downtown" around block 4,4) */
    int dx = bx * 2 - 7, dy = by * 2 - 7;
    int d = dx * dx + dy * dy;                 /* 2..98 */
    int maxh = d < 10 ? 58 : d < 30 ? 42 : 28;
    int h = rndr(12, maxh);
    if (rnd() % 6 == 0) h = rndr(12, 18);
    return M(h);
}

static void pick_mats(int h, u8 *mat, u8 *wmat)
{
    static const u8 low[] = { MAT_BRICK, MAT_BROWN, MAT_BRICK, MAT_SAND, MAT_CONCRETE, MAT_BROWN };
    if (h > M(40) && rnd() % 2) { *mat = MAT_GLASS; *wmat = MAT_SILVER; return; }
    if (h > M(30) && rnd() % 2) { *mat = MAT_CONCRETE; *wmat = MAT_GLASS; return; }
    *mat = low[rnd() % 6]; *wmat = MAT_GLASS;
}

void world_generate(void)
{
    World *w = &g_world;
    w->nb = 0; g_ntrees = 0;
    for (int i = 0; i < 81; i++) w->lotmat[i] = MAT_WATER;
    for (int by = 0; by < 8; by++)
    for (int bx = 0; bx < 8; bx++) {
        int bi = by * 8 + bx;
        blk_start[bi] = (u16)w->nb;
        int ox = bx * 1024, oy = by * 1024;
        int lx0 = ox + M(15), ly0 = oy + M(15), lx1 = ox + M(61), ly1 = oy + M(61);
        int park = (bx == 2 && by == 5) || (bx == 6 && by == 2) || (rnd() % 14 == 0);
        if (park) {
            w->lotmat[by * 9 + bx] = MAT_GRASS;
            for (int t = 0; t < 6 && g_ntrees < MAX_TREES; t++) {
                g_trees[g_ntrees].x = rndr(lx0 + M(4), lx1 - M(4));
                g_trees[g_ntrees].y = rndr(ly0 + M(4), ly1 - M(4));
                g_ntrees++;
            }
        } else {
            w->lotmat[by * 9 + bx] = MAT_CONCRETE;
            int split = rnd() % 10;
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
                u8 mat, wmat; pick_mats(h, &mat, &wmat);
                /* small random setbacks so blocks are not perfectly flat */
                int sx0 = (x0s[k] == lx0) ? 0 : 0, sy0 = 0, sx1 = 0, sy1 = 0;
                if (rnd() % 3 == 0) sx0 = M(rndr(0, 3));
                if (rnd() % 3 == 0) sy1 = M(rndr(0, 3));
                add_building(x0s[k] + sx0, y0s[k] + sy0, x1s[k] - sx1, y1s[k] - sy1, h, mat, wmat);
            }
        }
        blk_count[bi] = (u16)(w->nb - blk_start[bi]);
    }
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
