/* Ground height and surface lookups on the 1 m terrain map (bilinear between cell centres). */
#ifndef GROUND_H
#define GROUND_H
#include "engine.h"
#include "terrain.h"

static inline int ground_h(int x, int y) { (void)x; (void)y; return 0; }   /* flat ground */
static inline int ground_mat(int x, int y)        /* TM_* code (no shade bit) */
{
    int u = (x - TMAP_ORIGIN) >> 4, v = (y - TMAP_ORIGIN) >> 4;
    if ((unsigned)u >= TMAP_W || (unsigned)v >= TMAP_W) return TM_WATER;
    return terrain_m[v * TMAP_W + u] & 127;
}
#endif
