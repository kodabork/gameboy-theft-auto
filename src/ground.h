/* Ground height and surface lookups on the 1 m terrain map. */
#ifndef GROUND_H
#define GROUND_H
#include "engine.h"
#include "terrain.h"

static inline int ground_h(int x, int y) { (void)x; (void)y; return 0; }   /* flat ground */

/* terrain_m / terrain_r of map cell (u, v), 0 <= u, v < TMAP_W. In the ROM they are packed
   (tools/pack_terrain.py): the coast ring in full, and inside the street grid (cells 64..1357) only a
   water bit, since only "is it water?" is ever asked there (tm_cell gives TM_WATER or TM_GRID) and no
   road samples lie there (tr_cell gives 0). */
#define TRING_A 64
#define TRING_B 1358
#ifdef HOST
static inline int tm_cell(int u, int v) { return terrain_m[v * TMAP_W + u]; }
static inline int tr_cell(int u, int v) { return terrain_r[v * TMAP_W + u]; }
#else
static inline int tring_index(int u, int v)      /* -1 inside the grid */
{
    if (v < TRING_A) return v * TMAP_W + u;
    if (v >= TRING_B) return TRING_A * TMAP_W + (TRING_B - TRING_A) * (TRING_A + TMAP_W - TRING_B) + (v - TRING_B) * TMAP_W + u;
    int row = TRING_A * TMAP_W + (v - TRING_A) * (TRING_A + TMAP_W - TRING_B);
    if (u < TRING_A) return row + u;
    if (u >= TRING_B) return row + TRING_A + (u - TRING_B);
    return -1;
}
int tm_cell(int u, int v);                        /* collide.c */
int tr_cell(int u, int v);
#endif
static inline int ground_mat(int x, int y)        /* TM_* code (no shade bit); inside the grid only water is told apart */
{
    int u = (x - TMAP_ORIGIN) >> 4, v = (y - TMAP_ORIGIN) >> 4;
    if ((unsigned)u >= TMAP_W || (unsigned)v >= TMAP_W) return TM_WATER;
    return tm_cell(u, v) & 127;
}
#endif
