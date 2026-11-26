#ifndef GAME_H
#define GAME_H
#include "engine.h"

#define KEY_A      0x0001
#define KEY_B      0x0002
#define KEY_SELECT 0x0004
#define KEY_START  0x0008
#define KEY_RIGHT  0x0010
#define KEY_LEFT   0x0020
#define KEY_UP     0x0040
#define KEY_DOWN   0x0080
#define KEY_R      0x0100
#define KEY_L      0x0200

typedef struct {
    int speed_mph, in_car, near_car, nitro;
    int hp, money, wanted, wanted_flash, weapon, ammo, hurt_t;
    const char *weapon_name;
    const char *msg1, *msg2; int msg_t;          /* mission / info text */
    const char *big; int big_color, big_t;       /* centre-screen banner */
    int countdown, race_cp, race_total;
    int px, py, pang;                            /* radar centre + heading */
    int bx[12], by[12], bc[12], nblips;          /* radar blips: 1 job, 2 goal, 3 target, 4 police */
    int timer_s, pressure, car_hp;              /* car_hp: 0..100, -1 on foot */                       /* mission clock (s, -1 none), chase pressure % (-1 none) */
    int title;                                   /* title screen */
    const char *menu_title, *menu_item[8], *menu_note; int menu_n, menu_sel;
    int shop, shop_pose, shop_cash, mini;
    int club_fr, club_booth, club_dancer;        /* the club: dance frame, private booth on, which dancer */        /* mini: a minigame is on screen */
                 /* shop interior: 1 + type (0 = outside), clerk pose, hold-up cash */
    const char *shop_name, *shop_note, *shop_line, *shop_big;
    const char *stats_line[16]; int stats_n;
    int clock_min;                               /* time of day for the HUD clock */     /* pause menu > STATS page */
    const char *dlg_who, *dlg_l1, *dlg_l2;       /* story dialogue page */
    int cutscene;                                /* letterbox the dialogue */
    const char *radio_name; int radio_t;         /* station name, shown briefly after a change */
#define MAX_PK_BLIPS 16
    s16 pk_x[MAX_PK_BLIPS], pk_y[MAX_PK_BLIPS]; u8 pk_t[MAX_PK_BLIPS]; int npk;   /* pickups on the radar (type: 2 = health) */
#define MAX_MAP_ICONS 64
    int map;                                     /* pause menu > MAP: the whole city on screen */
    s16 map_x[MAX_MAP_ICONS], map_y[MAX_MAP_ICONS]; u8 map_t[MAX_MAP_ICONS]; int map_n;   /* 10 + shop type, 1 job */
} GameHud;
void hud_draw(u16 *fb);
typedef struct { s16 x, y; u8 g, bank, scale, pad; } TxtObj;
#define MAX_TXT 96
extern TxtObj g_txt[MAX_TXT]; extern int g_ntxt;
void hud_glyph_tile(int g, u32 *tile); int hud_nglyphs(void); void radar_init(void);
void radar_build(u32 *tiles4bpp, int part);      /* 64x64, 4bpp, 1D tile order; part 0..3 per frame */
extern GameHud g_hud;
extern const u16 map_marker_rgb[7];            /* palette entries 1..6 while the city map is up */

void game_init(void);
void game_update(u16 keys, int dt);           /* keys: 1 = pressed; dt in 1/16384 s */
int game_title_active(void);
int game_tod(void);                           /* time of day, minutes 0..1439 */
int game_daylight_screen(void);               /* title, shop and minigame screens keep daylight colours */
void game_build_scene(Camera *cam, Sprite *out, int *nout);
int game_beam(u32 *tab, u16 *lv, const Camera *cam);   /* headlight window span and brightness per scanline; 1 = on */

#endif
