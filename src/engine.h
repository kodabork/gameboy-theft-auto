/* Steel Harbor - original 3D open-city engine for Game Boy Advance.
   World units: 1 unit = 1/16 metre. Angles: 4096 per full turn. */
#ifndef ENGINE_H
#define ENGINE_H
#include "tables.h"

#ifdef HOST
#define EWRAM_BSS
#else
#define EWRAM_BSS __attribute__((section(".ewram")))
#endif

typedef unsigned char  u8;  typedef signed char  s8;
typedef unsigned short u16; typedef signed short s16;
typedef unsigned int   u32; typedef signed int   s32;

#define SCR_W   120      /* logical columns (each is 2 physical pixels) */
#define SCR_H   160
#define HORIZON 80
#define FX      86
#define FY      172
#define NEARZ   8
#define FARZ    3200
#define M(x)    ((int)((x)*16))   /* metres -> units */

static inline int isin(int a) { return sin_tab[a & 4095]; }
static inline int icos(int a) { return sin_tab[(a + 1024) & 4095]; }
int iatan2(int y, int x);
int isqrt(u32 v);

/* Palette layout:
     0..7    sky gradient
     8..103  world materials: 8 + mat*8 + fog*2 + light   (fog 0..3, light 0..1)
     104..254 pre-rendered sprite colours (sprite index i -> 103 + i) */
enum { MAT_ASPHALT, MAT_SIDEWALK, MAT_YELLOW, MAT_GRASS, MAT_BRICK, MAT_BROWN,
       MAT_CONCRETE, MAT_SAND, MAT_GLASS, MAT_WATER, MAT_SILVER, MAT_TRIM, NUM_MATS };
#define WPAL(m, fog, light) (8 + (m) * 8 + (fog) * 2 + (light))

typedef struct { s16 x0, y0, x1, y1; u16 h; s16 base; u8 mat, wmat; } Building;   /* base: ground level of the pad */

typedef struct { int x, y, h, c, s, gh; } Camera;   /* h: eye height (absolute), gh: ground under it; c,s Q14 */

/* A sprite to draw: world ground position + frame index into spr_frames */
typedef struct { int x, y; u16 frame; s16 h; } Sprite;   /* h: absolute height of the sprite's base (units) */
#define SPR_DECAL (-32768)   /* game side: push_h(..., SPR_DECAL) = flat on the ground */

#define MAX_BUILDINGS 256
#define MAX_SPRITES 160

typedef struct {
    Building b[MAX_BUILDINGS]; int nb;
    u8 lotmat[9*9];
} World;

extern World g_world;

void render_init(void);
void render_frame(u16 *fb, const Camera *cam, const Sprite *spr, int nspr);

/* People drawn as hardware sprites (full horizontal resolution) */
#define MAX_PEOPLE 13
typedef struct { int x, y; u16 frame; u8 player, pad; } PersonReq;
typedef struct { s16 x, y; u16 pa; u16 frame; u8 half, dbl, hidden, player; } ObjPlace;
int render_people(const PersonReq *req, int n, ObjPlace *out);
void palette_build(u16 *pal);

#endif
