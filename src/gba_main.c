/* New York - GBA hardware layer: video setup, input, timing, HUD sprites. */
#include "engine.h"
#include "game.h"
#include "sound.h"
#include "portraits.h"
extern int g_face, g_face_x, g_face_y;
extern void irq_handler(void);

#define REG16(a) (*(volatile u16 *)(a))
#define REG_DISPCNT  REG16(0x04000000)
#define REG_VCOUNT   REG16(0x04000006)
#define REG_BG2PA    REG16(0x04000020)
#define REG_BG2PD    REG16(0x04000026)
#define REG_TM2D     REG16(0x04000108)
#define REG_TM2CNT   REG16(0x0400010A)
#define REG_KEYINPUT REG16(0x04000130)
#define REG_WAITCNT  REG16(0x04000204)
#define PAL_BG   ((volatile u16 *)0x05000000)
#define PAL_OBJ  ((volatile u16 *)0x05000200)
#define OBJ_TILES ((volatile u32 *)0x06014000)
#define OAM      ((volatile u16 *)0x07000000)
#define PAGE0    ((u16 *)0x06000000)
#define PAGE1    ((u16 *)0x0600A000)

EWRAM_BSS static Sprite sprs[MAX_SPRITES];
volatile u8 flip_req;
/* buttons are latched at every VBlank, so a quick tap is never lost between game frames */
volatile u16 key_seen;
void key_latch(void) { key_seen |= (u16)(~REG_KEYINPUT & 0x03FF); }
/* tells emulators and flash carts that this cartridge has 32 KB of battery SRAM */
__attribute__((used, aligned(4))) const char sram_tag[] = "SRAM_V113";

/* 3x5 glyphs: digits 0-9, then 'R', 'F' */
static const u16 glyph[14] = {
    075557, 022232, 071747, 071717, 055711, 074717, 074757, 071111, 075757, 075717,
    075655, 074644, 077777, 070007,
};

static void make_tiles(void)
{
    for (int g = 0; g < 14; g++) {
        u8 px[8][8] = {{0}};
        for (int y = 0; y < 5; y++)
        for (int x = 0; x < 3; x++)
            if ((glyph[g] >> ((4 - y) * 3 + (2 - x))) & 1) {
                for (int oy = -1; oy <= 1; oy++)          /* dark outline */
                for (int ox = -1; ox <= 1; ox++)
                    if (!px[y + 2 + oy][x * 2 + 1 + ox]) px[y + 2 + oy][x * 2 + 1 + ox] = 2;
            }
        for (int y = 0; y < 5; y++)
        for (int x = 0; x < 3; x++)
            if ((glyph[g] >> ((4 - y) * 3 + (2 - x))) & 1) { px[y + 2][x * 2 + 1] = 1; px[y + 2][x * 2 + 2] = 1; }
        for (int y = 0; y < 8; y++) {
            u32 row = 0;
            for (int x = 0; x < 8; x++) row |= (u32)px[y][x] << (x * 4);
            OBJ_TILES[g * 8 + y] = row;
        }
    }
    PAL_OBJ[1] = 0x7FFF; PAL_OBJ[2] = 0x0842;
    /* nitro bar tiles: 12 = full (cyan), 13 = empty frame */
    for (int y = 0; y < 8; y++) { OBJ_TILES[12 * 8 + y] = (y >= 2 && y <= 5) ? 0x33333333 : 0x22222222; OBJ_TILES[13 * 8 + y] = (y >= 2 && y <= 5) ? 0x20000002 : 0x22222222; }
    PAL_OBJ[3] = 0x7FE0 | 0x0000; PAL_OBJ[3] = (31 << 10) | (28 << 5) | 6;
    /* HUD text glyphs (tiles 592..), palette banks 2 white, 3 yellow, 4 red, 5 black */
    for (int g = 0; g < hud_nglyphs(); g++) { u32 t[8]; hud_glyph_tile(g, t); for (int y = 0; y < 8; y++) OBJ_TILES[(80 + g) * 8 + y] = t[y]; }
    static const u16 tcol[4] = { 0x7FFF, 0x03FF, 0x10DF, 0x0000 };
    for (int b = 0; b < 4; b++) { PAL_OBJ[(2 + b) * 16 + 1] = tcol[b]; PAL_OBJ[(2 + b) * 16 + 2] = 0x0421; }
    OAM[3] = 128; OAM[7] = 0; OAM[11] = 0; OAM[15] = 128;      /* affine 0: 2x */
}

static void text_objs(void)
{
    int n = g_ntxt;
    for (int i = 0; i < MAX_TXT; i++) {
        volatile u16 *o = OAM + (24 + i) * 4;
        if (i >= n) { o[0] = 0x0200; continue; }
        const TxtObj *t = &g_txt[i];
        o[0] = (u16)((t->y & 255) | (t->scale > 1 ? 0x0300 : 0));
        o[1] = (u16)(t->x & 511);
        o[2] = (u16)((592 + t->g) | (t->bank << 12));
    }
}

static void set_sprite(int i, int x, int y, int glyph_id)
{
    volatile u16 *o = OAM + i * 4;
    if (glyph_id < 0) { o[0] = 0x0200; return; }      /* hidden */
    o[0] = (u16)(y & 255);
    o[1] = (u16)(x & 511);
    o[2] = (u16)(512 + glyph_id);
}

static void draw_number(int first, int x, int y, int v, int digits)
{
    for (int i = digits - 1; i >= 0; i--) {
        set_sprite(first + i, x + i * 7, y, (v > 0 || i == digits - 1) ? v % 10 : -1);
        v /= 10;
    }
}

int main(void)
{
    REG_WAITCNT = 0x4317;                     /* faster ROM access + prefetch */
    REG_DISPCNT = 0x0080;                     /* forced blank during setup */
    u16 pal[256];
    palette_build(pal);
    render_init();
    for (int i = 0; i < 256; i++) PAL_BG[i] = pal[i];
    for (int i = 0; i < 128; i++) set_sprite(i, 0, 0, -1);
    make_tiles();
    game_init();
    radar_init();
    sound_init();
    *(volatile u32 *)0x03007FFC = (u32)irq_handler;
    REG16(0x04000004) |= 0x0008;          /* VBlank IRQ */
    REG16(0x04000200) = 0x0001;           /* IE = VBlank */
    REG16(0x04000208) = 1;                /* IME */
    REG_TM2D = 0; REG_TM2CNT = 0x0083;          /* 16384 Hz free-running timer */
    u16 last = REG_TM2D;
    int fps_acc = 0, fps_frames = 0, fps = 0, show_fps = 1;
    u16 prev = 0;
    EWRAM_BSS static u32 radar_buf[64 * 8];
    u32 frame_n = 0;
    /* 13 gun pickup (cyan), 14 health cross (red), 15 gun outline (white) */
    static const u16 radar_pal[16] = { 0, 0x1484, 0x318C, 0x5145, 0x7FFF, 0x7FFF, 0x13FF, 0x13E4, 0x033F, 0x7D08, 0x2529, 0x1A46, 0x3ED8, 0x7FE4, 0x0C7F, 0x7FFF };
    for (int i = 0; i < 16; i++) PAL_OBJ[16 + i] = radar_pal[i];
    REG_DISPCNT = 0x0004 | 0x0400 | 0x1000 | 0x0040;   /* mode 4, BG2, OBJ, 1D */
    for (;;) {
        u16 keys = (u16)((~REG_KEYINPUT & 0x03FF) | key_seen);
        key_seen = 0;
        if (((keys & ~prev) & KEY_START) && (keys & KEY_SELECT)) show_fps ^= 1;
        prev = keys;
        u16 now = REG_TM2D;
        int dt = (u16)(now - last);
        last = now;
        fps_acc += dt; fps_frames++;
        if (fps_acc >= 16384) { fps = fps_frames * 16384 / fps_acc; fps_acc = 0; fps_frames = 0; }

        /* game logic runs while the previous frame waits for its VBlank flip */
        game_update(keys, dt);
        Camera cam; int nobj;
        game_build_scene(&cam, sprs, &nobj);
        while (flip_req) ;
        u16 *back = (REG_DISPCNT & 0x0010) ? PAGE0 : PAGE1;
        if (!g_hud.shop && !g_hud.mini) render_frame(back, &cam, sprs, nobj);   /* full-screen 2D screens skip the 3D view */
        hud_draw(back);
        int part = frame_n++ & 3, radar_new = part == 3;
        radar_build(radar_buf, part);

        flip_req = 1;                             /* the VBlank interrupt flips the page */
        /* HUD */
        if (show_fps && !g_hud.title) { set_sprite(0, 4, 4, 11); draw_number(1, 12, 4, fps, 2); }
        else for (int i = 0; i < 3; i++) set_sprite(i, 0, 0, -1);
        int hide = g_hud.title || g_hud.dlg_l1 || g_hud.menu_n || g_hud.shop;
        if (g_hud.in_car && !hide) draw_number(4, 212, 148, g_hud.speed_mph, 3);
        else for (int i = 4; i < 7; i++) set_sprite(i, 0, 0, -1);
        set_sprite(7, 116, 120, g_hud.near_car && !hide ? 10 : -1);
        if (radar_new) for (int i = 0; i < 64 * 8; i++) OBJ_TILES[16 * 8 + i] = radar_buf[i];    /* radar tiles 528.. */
        text_objs();
        {   /* cutscene portrait: a 64x64 8bpp sprite at tiles 896.., colours in OBJ palette 96..255 */
            static int face_loaded = -1;
            volatile u16 *o = OAM + 127 * 4;
            if (g_face >= 0) {
                if (g_face != face_loaded) {
                    for (int i = 0; i < 64 * 16; i++) OBJ_TILES[384 * 8 + i] = face_tiles[g_face][i];
                    for (int i = 0; i < 160; i++) PAL_OBJ[96 + i] = face_pal[g_face][i];
                    face_loaded = g_face;
                }
                o[0] = (u16)((g_face_y & 255) | 0x2000); o[1] = (u16)((g_face_x & 511) | 0xC000); o[2] = (u16)896;
            } else o[0] = 0x0200;
        }
        { volatile u16 *o = OAM + 20 * 4; o[0] = hide ? 0x0200 : (u16)(94 & 255); o[1] = (u16)(2 | 0xC000); o[2] = (u16)((512 + 16) | (1 << 12)); }
        for (int i = 0; i < 8; i++)            /* nitro bar, bottom left */
            set_sprite(8 + i, 88 + i * 8, 150, g_hud.in_car && !hide ? (g_hud.nitro > i * 125 + 62 ? 12 : 13) : -1);
    }
}
