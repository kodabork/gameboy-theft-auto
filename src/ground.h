/* Ground height and surface lookups on the 1 m terrain map (bilinear between cell centres). */
#ifndef GROUND_H
#define GROUND_H
#include "engine.h"
#include "terrain.h"

static inline int ground_h_map(int x, int y)      /* units (terrain heights: unused while the ground is flat) */
{
    int u = x - TMAP_ORIGIN - 8, v = y - TMAP_ORIGIN - 8;
    const int hi = (TMAP_W - 2) * 16 + 15;
    if (u < 0) u = 0; else if (u > hi) u = hi;
    if (v < 0) v = 0; else if (v > hi) v = hi;
    int fx = u & 15, fy = v & 15;
    const u8 *p = terrain_h + (v >> 4) * TMAP_W + (u >> 4);
    int a = p[0] * (16 - fx) + p[1] * fx;
    int b = p[TMAP_W] * (16 - fx) + p[TMAP_W + 1] * fx;
    return ((a * (16 - fy) + b * fy) >> 7) - 16;
}
static inline int ground_h(int x, int y) { (void)x; (void)y; return 0; }   /* flat ground */
static inline int ground_mat(int x, int y)        /* TM_* code (no shade bit) */
{
    int u = (x - TMAP_ORIGIN) >> 4, v = (y - TMAP_ORIGIN) >> 4;
    if ((unsigned)u >= TMAP_W || (unsigned)v >= TMAP_W) return TM_WATER;
    return terrain_m[v * TMAP_W + u] & 127;
}
#endif
