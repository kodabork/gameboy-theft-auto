#ifndef WORLD_H
#define WORLD_H
#include "engine.h"

#define MAX_TREES 48
typedef struct { int x, y; } Tree;
extern Tree g_trees[MAX_TREES];
extern int g_ntrees;

u32 rnd(void);
int rndr(int lo, int hi);
void world_generate(void);
int world_collide(int *px, int *py, int rad);
int world_point_blocked(int x, int y, int margin);
int car_vs_world(int cx, int cy, int c, int s, int hl, int hw, int *nx, int *ny, int *qx, int *qy);

/* road centre line for road index i (0..8), in units */
#define ROAD_C(i) ((i) * 1024 + M(6))
#define LANE_OFS  M(3)

#endif
