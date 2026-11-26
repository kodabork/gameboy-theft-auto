/* Collision queries. ARM code in IWRAM (called many times per frame). */
#include "engine.h"
#include "world.h"
#include "ground.h"


/* atan2 without division: normalise to 9 bits, then multiply by a reciprocal table */
int iatan2(int y, int x)
{
    if (x == 0 && y == 0) return 0;
    int ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
    int a, hi, lo;
    if (ax >= ay) { hi = ax; lo = ay; } else { hi = ay; lo = ax; }
    while (hi >= 512) { hi >>= 1; lo >>= 1; }
    if (hi == 0) hi = 1;
    int r = (lo * recip_atan[hi]) >> 16;          /* lo/hi in Q8 */
    if (r > 256) r = 256;
    a = atan_tab[r];
    if (ax < ay) a = 1024 - a;
    if (x < 0) a = 2048 - a;
    if (y < 0) a = -a;
    return a & 4095;
}

int isqrt(u32 v)
{
    u32 r = 0, b = 1u << 30;
    while (b > v) b >>= 2;
    while (b) {
        if (v >= r + b) { v -= r + b; r = (r >> 1) + b; } else r >>= 1;
        b >>= 2;
    }
    return (int)r;
}

/* Sea: water cells are solid. Returns a shallow penetration depth with a push-out normal
   (q10) pointing uphill, toward the shore. */
#define MAP_LO (TMAP_ORIGIN + M(6))
#define MAP_HI (TMAP_ORIGIN + TMAP_W * 16 - M(6))
#ifdef HOST
#define ROM_THUMB
#else
#define ROM_THUMB __attribute__((section(".romcode"), target("thumb")))   /* compact Thumb code in ROM: frees IWRAM */
#endif
static int water_push(int x, int y, int *nx, int *ny)
{
    if (x < MAP_LO || x > MAP_HI || y < MAP_LO || y > MAP_HI || ground_mat(x, y) == TM_WATER) {
        int gx = ground_h(x + M(2), y) - ground_h(x - M(2), y);
        int gy = ground_h(x, y + M(2)) - ground_h(x, y - M(2));
        if (x < MAP_LO) gx += 64; else if (x > MAP_HI) gx -= 64;
        if (y < MAP_LO) gy += 64; else if (y > MAP_HI) gy -= 64;
        if (gx == 0 && gy == 0) {
            if ((unsigned)x < NB * 1024 && (unsigned)y < NB * 1024) {   /* river / channel block: nearest quay */
                int lx = x & 1023, ly = y & 1023;
                int dl = lx - M(15), dr = M(61) - lx, dd = ly - M(15), du = M(61) - ly, m = dl;
                gx = -1; gy = 0;
                if (dr < m) { m = dr; gx = 1; gy = 0; }
                if (dd < m) { m = dd; gx = 0; gy = -1; }
                if (du < m) { gx = 0; gy = 1; }
                *nx = gx * 1024; *ny = gy * 1024;
                return M(0.5);
            }
            gx = (GRID_U >> 1) - x; gy = (GRID_U >> 1) - y;     /* flat: head for the city */
        }
        int l = isqrt((u32)(gx * gx + gy * gy)); if (l == 0) l = 1;
        *nx = gx * 1024 / l; *ny = gy * 1024 / l;
        return M(0.5);
    }
    return 0;
}

/* Push a circle (x,y,radius; units) out of buildings and the sea.
   Returns 1 if a collision happened. */
ROM_THUMB int world_collide(int *px, int *py, int rad)
{
    int x = *px, y = *py, hit = 0;
    int bx = x >> 10, by = y >> 10;
    for (int j = by - 1; j <= by + 1; j++)
    for (int i = bx - 1; i <= bx + 1; i++) {
        if ((unsigned)i >= NB || (unsigned)j >= NB) continue;
        int bi = j * NB + i;
        for (int k = blk_start[bi], e = blk_start[bi + 1]; k < e; k++) {
            const Building *b = &g_world.b[k];
            int qx = x < b->x0 ? b->x0 : (x > b->x1 ? b->x1 : x);
            int qy = y < b->y0 ? b->y0 : (y > b->y1 ? b->y1 : y);
            int dx = x - qx, dy = y - qy;
            if (dx == 0 && dy == 0) {
                /* centre inside: push out along the smallest axis */
                int l = x - b->x0, r = b->x1 - x, d = y - b->y0, u = b->y1 - y;
                int m = l; int ax = 0;
                if (r < m) { m = r; ax = 1; }
                if (d < m) { m = d; ax = 2; }
                if (u < m) { m = u; ax = 3; }
                if (ax == 0) x = b->x0 - rad; else if (ax == 1) x = b->x1 + rad;
                else if (ax == 2) y = b->y0 - rad; else y = b->y1 + rad;
                hit = 1; continue;
            }
            int d2 = dx * dx + dy * dy;
            if (d2 < rad * rad) {
                int d = isqrt((u32)d2);
                if (d == 0) d = 1;
                x = qx + dx * rad / d; y = qy + dy * rad / d;
                hit = 1;
            }
        }
    }
    /* the sea: step back toward the shore */
    for (int it = 0; it < 4; it++) {
        int nx, ny;
        if (!water_push(x, y, &nx, &ny)) break;
        x += (nx * M(0.5)) >> 10; y += (ny * M(0.5)) >> 10; hit = 1;
    }
    *px = x; *py = y;
    return hit;
}

int world_point_blocked(int x, int y, int margin)
{
    if (ground_mat(x, y) == TM_WATER) return 1;
    int bx = x >> 10, by = y >> 10;
    if ((unsigned)bx >= NB || (unsigned)by >= NB) return 0;
    int bi = by * NB + bx;
    for (int k = blk_start[bi], e = blk_start[bi + 1]; k < e; k++) {
        const Building *b = &g_world.b[k];
        if (x > b->x0 - margin && x < b->x1 + margin && y > b->y0 - margin && y < b->y1 + margin)
            return 1;
    }
    return 0;
}


/* ---------------------------------------------------------------------------
   Oriented-box car vs. buildings and island edge.
   Car centre (cx,cy), heading cos/sin Q14 (c,s), half length hl, half width hw (units).
   Returns penetration depth (0 = no contact) and the push-out vector (px,py, q10 unit
   normal times depth via *nx,*ny,*depth) plus the contact point. */
static int point_in_buildings(int x, int y, int *nx, int *ny)
{
    int best = 0;
    best = water_push(x, y, nx, ny);
    int bx = x >> 10, by = y >> 10;
    if ((unsigned)bx >= NB || (unsigned)by >= NB) return best;
    int bi = by * NB + bx;
    for (int k = blk_start[bi], e = blk_start[bi + 1]; k < e; k++) {
        const Building *b = &g_world.b[k];
        if (x <= b->x0 || x >= b->x1 || y <= b->y0 || y >= b->y1) continue;
        int dl = x - b->x0, dr = b->x1 - x, dd = y - b->y0, du = b->y1 - y;
        int m = dl, ax = 0;
        if (dr < m) { m = dr; ax = 1; }
        if (dd < m) { m = dd; ax = 2; }
        if (du < m) { m = du; ax = 3; }
        if (m > best) {
            best = m;
            *nx = ax == 0 ? -1024 : ax == 1 ? 1024 : 0;
            *ny = ax == 2 ? -1024 : ax == 3 ? 1024 : 0;
        }
    }
    return best;
}

int car_vs_world(int cx, int cy, int c, int s, int hl, int hw,
                 int *nx, int *ny, int *qx, int *qy)
{
    int best = 0;
    /* 1. car corners / edge points inside buildings */
    static const s8 pts[8][2] = { {1,1},{1,-1},{-1,1},{-1,-1},{0,1},{0,-1},{1,0},{-1,0} };
    for (int i = 0; i < 8; i++) {
        int f = pts[i][0] * hl, r = pts[i][1] * hw;
        int px = cx + ((f * c + r * s) >> 14);
        int py = cy + ((f * s - r * c) >> 14);
        int tnx, tny;
        int d = point_in_buildings(px, py, &tnx, &tny);
        if (d > best) { best = d; *nx = tnx; *ny = tny; *qx = px; *qy = py; }
    }
    /* 2. building corners inside the car (side-swiping a corner) */
    int bx = cx >> 10, by = cy >> 10;
    for (int j = by - 1; j <= by + 1; j++)
    for (int i = bx - 1; i <= bx + 1; i++) {
        if ((unsigned)i >= NB || (unsigned)j >= NB) continue;
        int bi = j * NB + i;
        for (int k = blk_start[bi], e = blk_start[bi + 1]; k < e; k++) {
            const Building *b = &g_world.b[k];
            if (b->x1 < cx - hl - hw || b->x0 > cx + hl + hw || b->y1 < cy - hl - hw || b->y0 > cy + hl + hw) continue;
            for (int q = 0; q < 4; q++) {
                int wx = (q & 1) ? b->x1 : b->x0, wy = (q & 2) ? b->y1 : b->y0;
                int dx = wx - cx, dy = wy - cy;
                int lf = (dx * c + dy * s) >> 14, lr = (dx * s - dy * c) >> 14;
                int pf = hl - (lf < 0 ? -lf : lf), pr = hw - (lr < 0 ? -lr : lr);
                if (pf <= 0 || pr <= 0) continue;
                int d, fx, fy;
                if (pf < pr) { d = pf; int sg = lf > 0 ? -1 : 1; fx = sg * c >> 4; fy = sg * s >> 4; }
                else { d = pr; int sg = lr > 0 ? -1 : 1; fx = sg * s >> 4; fy = -sg * c >> 4; }
                if (d > best) { best = d; *nx = fx; *ny = fy; *qx = wx; *qy = wy; }
            }
        }
    }
    return best;
}
