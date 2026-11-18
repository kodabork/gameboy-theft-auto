/* Steel Harbor HUD: text in an original 5x7 pixel font drawn into the frame buffer
   at full 240-pixel resolution, health bar, wanted stars, and a rotating radar. */
#include "engine.h"
#include "game.h"
#include "logo.h"
#include "ground.h"

#define COL_RED 252
#define COL_YELLOW 253
#define COL_WHITE 254
#define COL_BLACK 255

/* 5x7 glyphs, one byte per row (low 5 bits, MSB = left) */
static const u8 font[][7] = {
    /* ' ' */ {0,0,0,0,0,0,0},
    /* '!' */ {4,4,4,4,4,0,4},
    /* '$' */ {4,15,20,14,5,30,4},
    /* '*' */ {4,21,14,31,14,21,4},
    /* '+' */ {0,4,4,31,4,4,0},
    /* '-' */ {0,0,0,31,0,0,0},
    /* '.' */ {0,0,0,0,0,12,12},
    /* '/' */ {1,1,2,4,8,16,16},
    /* '0' */ {14,17,19,21,25,17,14}, {4,12,4,4,4,4,14}, {14,17,1,2,4,8,31}, {31,2,4,2,1,17,14},
    {2,6,10,18,31,2,2}, {31,16,30,1,1,17,14}, {6,8,16,30,17,17,14}, {31,1,2,4,8,8,8},
    {14,17,17,14,17,17,14}, {14,17,17,15,1,2,12},
    /* ':' */ {0,12,12,0,12,12,0},
    /* '?' */ {14,17,1,2,4,0,4},
    /* 'A' */ {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14}, {28,18,17,17,17,18,28},
    {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16}, {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17},
    {14,4,4,4,4,4,14}, {7,2,2,2,2,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
    {17,27,21,21,17,17,17}, {17,17,25,21,19,17,17}, {14,17,17,17,17,17,14}, {30,17,17,30,16,16,16},
    {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17}, {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4},
    {17,17,17,17,17,17,14}, {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
    {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31},
    /* '(' */ {2,4,8,8,8,4,2}, /* ')' */ {8,4,2,2,2,4,8}, /* ',' */ {0,0,0,0,12,4,8},
    /* ''' */ {4,4,8,0,0,0,0}, /* '>' */ {8,4,2,1,2,4,8},
};

static int glyph_index(char ch)
{
    if (ch >= 'a' && ch <= 'z') ch -= 32;
    switch (ch) {
    case '!': return 1; case '$': return 2; case '*': return 3; case '+': return 4;
    case '-': return 5; case '.': return 6; case '/': return 7; case ':': return 18; case '?': return 19;
    case '(': return 46; case ')': return 47; case ',': return 48; case '\'': return 49; case '>': return 50;
    }
    if (ch >= '0' && ch <= '9') return 8 + ch - '0';
    if (ch >= 'A' && ch <= 'Z') return 20 + ch - 'A';
    return 0;
}

static u16 *FBH;

static inline void put_px(int x, int y, int c)
{
    if ((unsigned)x >= 240 || (unsigned)y >= 160) return;
    u16 *p = FBH + y * SCR_W + (x >> 1);
    if (x & 1) *p = (u16)((*p & 0x00FF) | (c << 8));
    else *p = (u16)((*p & 0xFF00) | c);
}

static void rect(int x, int y, int w, int h, int c)
{
    /* clip, then fill whole 2-pixel words; only the odd edge pixels need read-modify-write */
    int x1 = x + w, y1 = y + h;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x1 > 240) x1 = 240;
    if (y1 > 160) y1 = 160;
    if (x >= x1 || y >= y1) return;
    const u16 cc = (u16)(c | (c << 8));
    for (int j = y; j < y1; j++) {
        int a = x, b = x1;
        if (a & 1) { put_px(a, j, c); a++; }
        if (b & 1) { b--; put_px(b, j, c); }
        u16 *p = FBH + j * SCR_W + (a >> 1), *e = FBH + j * SCR_W + (b >> 1);
        while (p < e) *p++ = cc;
    }
}

static int text_width(const char *s, int scale) { int n = 0; while (*s++) n++; return n * 6 * scale; }

#ifndef HOST
/* On hardware, small text (scale 1-2) goes out as 8x8 OBJ glyphs: free for the CPU. */
EWRAM_BSS TxtObj g_txt[MAX_TXT]; int g_ntxt;
static void text(int x, int y, const char *s, int col, int scale);
static void text_fb(int x, int y, const char *s, int col, int scale);
static void text(int x, int y, const char *s, int col, int scale)
{
    if (scale > 2) { text_fb(x, y, s, col, scale); return; }
    int bank = col == COL_WHITE ? 2 : col == COL_YELLOW ? 3 : col == COL_RED ? 4 : 5;
    for (; *s; s++, x += 6 * scale) {
        int g = glyph_index(*s);
        if (g == 0 || g_ntxt >= MAX_TXT) continue;
        TxtObj *o = &g_txt[g_ntxt++];
        o->x = (s16)x; o->y = (s16)y; o->g = (u8)g; o->bank = (u8)bank; o->scale = (u8)scale;
    }
}
void hud_glyph_tile(int g, u32 *tile)       /* 4bpp: 1 = colour, 2 = shadow */
{
    u8 px[8][8] = {{0}};
    for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++)
        if (font[g][r] & (16 >> b)) px[r + 1][b + 1] = 2;
    for (int r = 0; r < 7; r++) for (int b = 0; b < 5; b++)
        if (font[g][r] & (16 >> b)) px[r][b] = 1;
    for (int y = 0; y < 8; y++) { u32 row = 0; for (int x = 0; x < 8; x++) row |= (u32)px[y][x] << (x * 4); tile[y] = row; }
}
int hud_nglyphs(void) { return (int)(sizeof font / sizeof font[0]); }
#define text text_fb
#endif
static void text(int x, int y, const char *s, int col, int scale)
{
    for (; *s; s++, x += 6 * scale) {
        const u8 *g = font[glyph_index(*s)];
        for (int pass = 0; pass < 2; pass++)           /* shadow, then colour */
        for (int r = 0; r < 7; r++)
        for (int b = 0; b < 5; b++)
            if (g[r] & (16 >> b))
                for (int sy = 0; sy < scale; sy++)
                for (int sx = 0; sx < scale; sx++)
                    put_px(x + b * scale + sx + (pass ? 0 : 1), y + r * scale + sy + (pass ? 0 : 1), pass ? col : COL_BLACK);
    }
}

#ifndef HOST
#undef text
#endif
static void text_center(int y, const char *s, int col, int scale) { text(120 - text_width(s, scale) / 2, y, s, col, scale); }

static char *fmt_int(char *p, int v)
{
    char tmp[12]; int n = 0;
    if (v < 0) { *p++ = '-'; v = -v; }
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *p++ = tmp[--n];
    *p = 0;
    return p;
}

static void blit_img(const Img *im, int x0, int y0)     /* palette-index image, 0 = transparent */
{
    int xs = x0 < 0 ? 0 : x0, xe = x0 + im->w > 240 ? 240 : x0 + im->w;
    if (xs >= xe) return;
    for (int y = 0; y < im->h; y++) {
        int sy = y0 + y; if (sy < 0 || sy >= 160) continue;
        const u8 *src = im->px + y * im->w - x0;     /* indexed by screen x */
        u16 *row = FBH + sy * 120;
        int sx = xs;
        if (sx & 1) { int v = src[sx]; if (v) row[sx >> 1] = (u16)((row[sx >> 1] & 0x00FF) | (v << 8)); sx++; }
        for (; sx + 1 < xe; sx += 2) {               /* whole 2-pixel words */
            int a = src[sx], b = src[sx + 1];
            if (!(a | b)) continue;
            u16 *w = &row[sx >> 1];
            if (a && b) *w = (u16)(a | (b << 8));
            else if (a) *w = (u16)((*w & 0xFF00) | a);
            else *w = (u16)((*w & 0x00FF) | (b << 8));
        }
        if (sx < xe) { int v = src[sx]; if (v) row[sx >> 1] = (u16)((row[sx >> 1] & 0xFF00) | v); }
    }
}
static void shade_band(int y0, int y1)          /* darken a strip of the frame (checker of black) */
{
    for (int y = y0; y < y1; y++) {
        u16 *r = FBH + y * 120;
        for (int x = (y & 1); x < 120; x += 2) r[x] = (u16)(COL_BLACK | (COL_BLACK << 8));
    }
}
static void draw_menu(int y)
{
    for (int i = 0; i < g_hud.menu_n; i++) {
        int sel = i == g_hud.menu_sel;
        int w = text_width(g_hud.menu_item[i], 1);
        int x = (240 - w) / 2;
        if (sel) text(x - 10, y + i * 11, ">", COL_YELLOW, 1);
        text(x, y + i * 11, g_hud.menu_item[i], sel ? COL_YELLOW : COL_WHITE, 1);
    }
}

void hud_draw(u16 *fb)
{
    FBH = fb;
#ifndef HOST
    g_ntxt = 0;
#endif
    char buf[32], *p;
    if (g_hud.title) {                           /* title screen: pre-drawn artwork */
        blit_img(&img_logo, (240 - img_logo.w) / 2, 8);
        blit_img(&img_city, (240 - img_city.w) / 2, 76);
        for (int i = 0; i < g_hud.menu_n; i++) {
            int sel = i == g_hud.menu_sel;
            const Img *im = i == 0 ? (sel ? &img_new_s : &img_new) : (sel ? &img_cont_s : &img_cont);
            blit_img(im, (240 - im->w) / 2, 100 + i * 19);
        }
        blit_img(&img_press, (240 - img_press.w) / 2, 142);
        return;
    }
    if (g_hud.dlg_l1) {                          /* story dialogue */
        if (g_hud.cutscene) { rect(0, 0, 240, 12, COL_BLACK); rect(0, 113, 240, 47, COL_BLACK); }
        else shade_band(112, 156);
        if (g_hud.dlg_who && g_hud.dlg_who[0]) text(8, 116, g_hud.dlg_who, COL_YELLOW, 1);
        text(8, 128, g_hud.dlg_l1, COL_WHITE, 1);
        if (g_hud.dlg_l2) text(8, 138, g_hud.dlg_l2, COL_WHITE, 1);
        text(214, 148, "A >", COL_YELLOW, 1);
        return;
    }
    if (g_hud.menu_n) {                          /* pause menu */
        shade_band(40, 112);
        text_center(46, g_hud.menu_title, COL_YELLOW, 2);
        draw_menu(68);
        if (g_hud.menu_note) text_center(116, g_hud.menu_note, COL_WHITE, 1);
        return;
    }
    /* money, top right */
    buf[0] = '$'; fmt_int(buf + 1, g_hud.money);
    text(236 - text_width(buf, 1), 4, buf, COL_WHITE, 1);
    /* wanted stars */
    if (g_hud.wanted > 0)
        for (int i = 0; i < 5; i++)
            text(206 + i * 6, 14, "*", i < g_hud.wanted ? COL_YELLOW : COL_BLACK, 1);
    /* weapon / ammo, top left under the fps counter */
    if (g_hud.weapon) {
        p = buf; const char *w = g_hud.weapon_name; while (*w) *p++ = *w++;
        *p++ = ' '; fmt_int(p, g_hud.ammo);
        text(4, 37, buf, COL_WHITE, 1);
    }
    /* health bar, and the car's condition under it while driving */
    rect(3, 24, 52, 5, COL_BLACK);
    rect(4, 25, g_hud.hp / 2, 3, g_hud.hurt_t > 0 ? COL_WHITE : COL_RED);
    if (g_hud.car_hp >= 0) {
        rect(3, 30, 52, 4, COL_BLACK);
        int ch = g_hud.car_hp < 0 ? 0 : g_hud.car_hp;
        rect(4, 31, ch / 2, 2, ch < 12 ? COL_RED : ch < 35 ? COL_YELLOW : COL_WHITE);
    }
    /* race progress / countdown */
    if (g_hud.countdown > 0) { buf[0] = (char)('0' + g_hud.countdown); buf[1] = 0; text_center(50, buf, COL_YELLOW, 4); }
    if (g_hud.race_total) {
        p = buf; const char *w = "CHECKPOINT "; while (*w) *p++ = *w++;
        p = fmt_int(p, g_hud.race_cp + 1 > g_hud.race_total ? g_hud.race_total : g_hud.race_cp + 1); *p++ = '/'; fmt_int(p, g_hud.race_total);
        text_center(84, buf, COL_WHITE, 1);
    }
    /* mission clock and chase pressure */
    if (g_hud.timer_s >= 0) {
        p = buf; const char *w = "TIME "; while (*w) *p++ = *w++;
        p = fmt_int(p, g_hud.timer_s / 60); *p++ = ':'; *p++ = (char)('0' + (g_hud.timer_s % 60) / 10); *p++ = (char)('0' + g_hud.timer_s % 10); *p = 0;
        text_center(4, buf, g_hud.timer_s <= 10 ? COL_RED : COL_WHITE, 1);
    }
    if (g_hud.pressure >= 0) {
        rect(90, 14, 62, 5, COL_BLACK);
        rect(91, 15, g_hud.pressure * 60 / 100, 3, COL_YELLOW);
        text_center(21, "PRESSURE", COL_WHITE, 1);
    }
    /* messages */
    if (g_hud.msg_t > 0) {
        if (g_hud.msg1) text_center(42, g_hud.msg1, COL_WHITE, 1);       /* up in the sky, off the road */
        if (g_hud.msg2) text_center(51, g_hud.msg2, COL_WHITE, 1);
    }
    if (g_hud.big_t > 0 && g_hud.big) text_center(64, g_hud.big, g_hud.big_color ? COL_YELLOW : COL_RED, 2);
}

/* ------------------------------------------------------------- radar */
/* palette bank 1: 1 block, 2 road, 3 water, 4 rim, 5 player, 6 job, 7 goal, 8 target, 9 police, 10 sidewalk */
#define rpx(t, x, y, c) do { int x_ = (x), y_ = (y); u32 *w_ = &(t)[(((y_) >> 3) * 8 + ((x_) >> 3)) * 8 + ((y_) & 7)]; \
    int sh_ = ((x_) & 7) * 4; *w_ = (*w_ & ~(15u << sh_)) | ((u32)(c) << sh_); } while (0)

#define MM_W RADAR_MAP_W
#define minimap radar_map
void radar_init(void) {}

#ifdef HOST
#define IWRAM_ARM
#else
#define IWRAM_ARM __attribute__((section(".iwram"), target("arm"), noinline))
#endif
/* Built a quarter of the disc per frame (part 0..3) from values latched at part 0, so the cost is
   spread evenly instead of landing on one frame; the finished picture is shown after part 3. */
IWRAM_ARM void radar_build(u32 *t, int part)
{
    static int c, s, ox, oy, px0, py0;
    const int R = 28, SCALE = M(2.5);              /* 2.5 m per radar pixel */
    if (part == 0) {
        for (int i = 0; i < 64 * 8; i++) t[i] = 0;
        c = icos(g_hud.pang); s = isin(g_hud.pang);
        px0 = g_hud.px; py0 = g_hud.py;
        /* minimap texel coordinates in 16.16, stepped incrementally */
        ox = (px0 - TMAP_ORIGIN) << 11; oy = (py0 - TMAP_ORIGIN) << 11;   /* units/32 << 16 */
    }
    const int sxx = (SCALE * s) >> 3, sxy = -(SCALE * c) >> 3;     /* per +x radar px (>>14, <<11) */
    const int syx = -(SCALE * c) >> 3, syy = -(SCALE * s) >> 3;    /* per +y radar px */
    /* disc spans per row: outer half-width and inner (non-rim) half-width; 8 pixels packed per word write */
    static const u8 span_o[29] = { 28,27,27,27,27,27,27,27,26,26,26,25,25,24,24,23,22,22,21,20,19,18,17,15,14,12,10,7,0 };
    static const s8 span_i[29] = { 27,26,26,26,26,26,26,26,25,25,25,24,24,23,23,22,21,20,20,19,18,16,15,14,12,10,7,0,-1 };
    static const s8 ystart[5] = { -28, -14, 0, 14, 29 };
    for (int y = ystart[part]; y < ystart[part + 1]; y++) {
        int ay = y < 0 ? -y : y;
        int xo = span_o[ay], xi = span_i[ay];
        int u = ox + y * syx - xo * sxx, v = oy + y * syy - xo * sxy;
        int py = 32 + y;
        u32 *trow = t + (py >> 3) * 64 + (py & 7);
        u32 acc = 0;
        for (int x = -xo; x <= xo; x++, u += sxx, v += sxy) {
            int px = 32 + x, col;
            if (x < -xi || x > xi) col = 4;
            else {
                unsigned mx = (unsigned)u >> 16, my = (unsigned)v >> 16;
                col = (mx < MM_W && my < MM_W) ? minimap[my * MM_W + mx] : 3;
            }
            acc |= (u32)col << ((px & 7) * 4);
            if ((px & 7) == 7 || x == xo) { trow[(px >> 3) * 8] = acc; acc = 0; }
        }
    }
    if (part != 3) return;
    /* blips (clamped to the rim) */
    for (int i = 0; i < g_hud.nblips; i++) {
        int dx = g_hud.bx[i] - px0, dy = g_hud.by[i] - py0;
        int f = (dx * c + dy * s) >> 14, rr = (dx * s - dy * c) >> 14;
        int x = rr / SCALE, y = -f / SCALE;
        int d2 = x * x + y * y, lim = R - 3;
        if (d2 > lim * lim) { int d = isqrt((u32)d2); x = x * lim / d; y = y * lim / d; }
        int col = 5 + g_hud.bc[i];
        for (int j = -1; j <= 1; j++) for (int k = -1; k <= 1; k++) rpx(t, 32 + x + k, 32 + y + j, col);
    }
    /* player arrow */
    static const s8 arrow[][2] = { {0,-3},{0,-2},{-1,-1},{0,-1},{1,-1},{-1,0},{0,0},{1,0},{-2,1},{-1,1},{0,1},{1,1},{2,1} };
    for (unsigned i = 0; i < sizeof arrow / 2; i++) rpx(t, 32 + arrow[i][0], 32 + arrow[i][1], 5);
}
