#ifndef WORLD_H
#define WORLD_H
#include "engine.h"

#include "terrain.h"

#define NB CITY_NB                    /* city blocks per side; roads 0..NB */
#define NBLK (NB * NB)
#define GRID_U (NB * 1024 + M(15))    /* far edge of the street grid, units */
#define BLK_KIND(bx, by) (city_kind[(by) * NB + (bx)])

#define MAX_TREES 640
typedef struct { s16 x, y; } Tree;
extern Tree g_trees[MAX_TREES];
extern int g_ntrees;
extern u16 blk_start[NBLK + 1], tree_start[NBLK + 1];   /* buildings / park trees of block b: [start[b], start[b+1]) */

u32 rnd(void);
int rndr(int lo, int hi);
void world_generate(void);
int world_collide(int *px, int *py, int rad);
int world_point_blocked(int x, int y, int margin);
int car_vs_world(int cx, int cy, int c, int s, int hl, int hw, int *nx, int *ny, int *qx, int *qy);

/* road centre line for road index i (0..NB), in units */
#define ROAD_C(i) ((i) * 1024 + M(6))
#define LANE_OFS  M(3)

#endif
