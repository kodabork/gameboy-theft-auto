/* high-speed car-vs-car test: player car at speed S straight at a stopped car 30 m ahead */
#include "game.c"
const unsigned char spr_pixels[1 << 25];
unsigned char terrain_h[656 * 656], terrain_m[656 * 656], terrain_r[656 * 656], radar_map[328 * 328];
int main(int argc, char **argv)
{
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_h, 1, sizeof terrain_h, tf); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    game_init();
    int hits = 0, misses = 0;
    int through = 0;
    for (int trial = 0; trial < 120; trial++) {
        new_game_state(); gstate = GS_PLAY;
        int fps = 12 + (trial % 4) * 3;                /* 12..21 fps */
        int mps = 30 + ((trial / 4) % 10) * 5;         /* 30..75 m/s */
        int side = trial >= 40 && trial < 80, oncoming = trial >= 80;
        if (pl.in_car < 0) { printf("no car\n"); return 1; }
        Car *P = &cars[pl.in_car];
        /* clear everything else out of the way */
        for (int i = 0; i < ncars; i++) if (i != pl.in_car) cars[i].active = 0;
        for (int i = 0; i < MAX_PEDS; i++) peds[i].active = 0;
        int a = P->head >> 8, ca = icos(a), sa = isin(a);
        int T = -1;
        for (int i = 0; i < ncars; i++) if (i != pl.in_car && i >= SLOT_TRAFFIC0) { T = i; break; }
        Car *C = &cars[T]; *C = *P;
        C->active = 1; C->ai = AI_NONE; C->speed = 0; C->vx = C->vy = 0; C->kx = C->ky = 0; C->spin = 0; C->dmg = 0; C->police = 0;
        C->model = CAR_SEDAN_SILVER; if (side) C->head = (P->head + Q8(1024)) & (Q8(4096) - 1);
        if (oncoming) { C->head = (P->head + Q8(2048)) & (Q8(4096) - 1); C->speed = Q8(M(25)); }
        C->x = P->x + Q8((M(30) * ca) >> 14); C->y = P->y + Q8((M(30) * sa) >> 14);
        P->vx = Q8(M(mps)) / 16384 * ca; P->vy = Q8(M(mps)) / 16384 * sa; P->speed = Q8(M(mps));
        int cx0 = C->x, cy0 = C->y, hit = 0, thru = 0, prev = -1;
        for (int f = 0; f < fps * 3; f++) {
            game_update(KEY_A, TICKS / fps);
            if (getenv("TRACE") && trial == 1) printf("f%d P %d,%d v %d,%d  C %d,%d k %d,%d\n", f, P->x>>8, P->y>>8, P->vx>>8, P->vy>>8, C->x>>8, C->y>>8, C->kx>>8, C->ky>>8);
            if (C->kx || C->ky || C->dmg) hit = 1;
            { int dx = (P->x - C->x) >> 8, dy = (P->y - C->y) >> 8;
              int al = (dx * ca + dy * sa) >> 14, lat = (dx * sa - dy * ca) >> 14;
              if (lat < 0) lat = -lat;
              if (prev < 0 && al > 0 && lat < M(1.6)) thru = 1;     /* went from behind to in front while overlapping sideways */
              prev = al; }
        }
        int along = (((P->x - C->x) >> 8) * ca + ((P->y - C->y) >> 8) * sa) >> 14;
        if (hit) hits++; else misses++;
        (void)along; if (thru) { through++; printf("THROUGH %s fps %d speed %d m/s\n", oncoming ? "oncoming" : side ? "side" : "head", fps, mps); }
    }
    printf("hits %d misses %d, drove through %d of 120\n", hits, misses, through);
    return 0;
}
