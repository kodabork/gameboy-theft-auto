#include "game.c"
const unsigned char spr_pixels[1 << 25];
unsigned char terrain_h[656 * 656], terrain_m[656 * 656], terrain_r[656 * 656], radar_map[328 * 328];
static void tick(int n, u16 keys) { for (int i = 0; i < n; i++) game_update(keys, TICKS / 20); }
static void go(int x, int y) { if (pl.in_car >= 0) { Car *c = &cars[pl.in_car]; c->x = Q8(x); c->y = Q8(y); c->vx = c->vy = 0; c->speed = 0; } pl.x = Q8(x); pl.y = Q8(y); }
static void skip_dialog(void) { for (int k = 0; k < 12 && gstate == GS_DIALOG; k++) { tick(1, KEY_A); tick(1, 0); } }
static void kill_all(int k0, int k1) { for (int k = k0; k < k1; k++) { Ped *p = &peds[PED_MISSION0 + k]; if (p->active) { p->down = 1; p->timer = 30 * 256; } } }
int main(void)
{
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_h, 1, sizeof terrain_h, tf); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    game_init(); new_game_state(); gstate = GS_PLAY;
    int fails = 0;
    for (int m = 0; m < NMISSIONS; m++) {
        if (mission != m) { printf("expected mission %d, at %d\n", m, mission); return 1; }
        wanted = 0;
        if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
        go(mission_start_x[m] - M(2), mission_start_y[m]);
        tick(2, 0);
        if (gstate != GS_DIALOG || !cut_on) { printf("m%d: no cutscene\n", m); fails++; }
        skip_dialog();
        if (mstate != 1) { printf("m%d: did not start\n", m); fails++; continue; }
        int money0 = pl.money;
        Car *mc = &cars[SLOT_MISSION];
        switch (m) {
        case 0: pl.in_car = SLOT_MISSION; go(garage_x, garage_y); tick(3, 0); break;
        case 1: go(dock_x, dock_y); tick(2, 0); if (phase != 1 || wanted < 1) printf("m1: pickup failed\n"); wanted = 0; go(club_x, club_y); tick(2, 0); break;
        case 2: kill_all(0, 1); tick(2, 0); break;
        case 3: for (int k = 0; k < 3; k++) { car_damage(&cars[gang_slot[k]], 1000, 1); } tick(20 * 7, 0); break;
        case 4: kill_all(0, 2); tick(2, 0); if (phase != 1 || wanted != 3) printf("m4: heat not on\n"); wanted = 0; tick(2, 0); break;
        case 5: pl.in_car = SLOT_MISSION; go(drop_x, drop_y); tick(3, 0); break;
        case 6: kill_all(0, 3); tick(2, 0); if (phase != 1) printf("m6: phase stuck\n"); wanted = 0; go(safe_x, safe_y); tick(2, 0); break;
        case 7: car_damage(mc, 1000, 1); tick(20 * 7, 0); break;
        case 8: kill_all(0, 1); tick(2, 0); break;
        }
        if (gstate == GS_DIALOG) skip_dialog();
        printf("m%d %-16s -> next %d  money +%d  %s\n", m, mission_title[m], mission, pl.money - money0, mission == m + 1 ? "PASS" : "FAIL");
        if (mission != m + 1) fails++;
        if (pl.dead_t > 0) { pl.dead_t = 0; pl.hp = 100; }
    }
    printf("%s\n", fails ? "SOME FAILED" : "ALL JOBS COMPLETE");
    return fails != 0;
}
