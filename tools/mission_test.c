#include "game.c"
const unsigned char spr_pixels[1 << 25];
unsigned char terrain_m[TMAP_W * TMAP_W], terrain_r[TMAP_W * TMAP_W], radar_map[RADAR_MAP_W * RADAR_MAP_W];
static void tick(int n, u16 keys) { for (int i = 0; i < n; i++) game_update(keys, TICKS / 20); }
static void go(int x, int y) { if (pl.in_car >= 0) { Car *c = &cars[pl.in_car]; c->x = Q8(x); c->y = Q8(y); c->vx = c->vy = 0; c->speed = 0; } pl.x = Q8(x); pl.y = Q8(y); }
static void skip_dialog(void) { for (int k = 0; k < 12 && gstate == GS_DIALOG; k++) { tick(1, KEY_A); tick(1, 0); } }
static void kill_all(int k0, int k1) { for (int k = k0; k < k1; k++) { Ped *p = &peds[PED_MISSION0 + k]; if (p->active) { p->down = 1; p->timer = 30 * 256; } } }
int main(void)
{
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    game_init(); new_game_state(); gstate = GS_PLAY;
    int fails = 0;
    for (int m = 0; m < NACT1; m++) {
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
    /* Acts Two to Four: a generic solver that cheats its way through each step */
    for (int m = NACT1; m < NMISSIONS; m++) {
        if (mission != m) { printf("expected mission %d, at %d\n", m, mission); return 1; }
        const MissionDef *d = story_def();
        wanted = 0; pl.hp = 100; pl.dead_t = 0;
        if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
        go(M(1) * d->sx - M(2), M(1) * d->sy);
        tick(2, 0);
        if (gstate != GS_DIALOG || !cut_on) { printf("m%d: no cutscene\n", m); fails++; }
        skip_dialog();
        if (mstate != 1) { printf("m%d: did not start (gstate %d dlg %d/%d who %s)\n", m, gstate, dlg_i, dlg_n, dlg ? dlg[0].who : "-"); fails++; return 1; }
        int money0 = pl.money, guard = 0, failed = 0;
        while (mission == m && guard++ < 120) {
            if (gstate == GS_DIALOG) { skip_dialog(); continue; }
            if (mstate != 1) { printf("m%d %s: failed at step %d (%s)\n", m, d->title, phase, g_hud.big ? g_hud.big : "?"); failed = 1; break; }
            const Step *st = cur_step();
            int before = phase;
            if (ssub == 0) tick(1, 0);                        /* let the step set itself up */
            if (mstate != 1 || phase != before) continue;
            int X = st->x == NOWHERE ? sx_dyn : M(1) * st->x, Y = st->x == NOWHERE ? sy_dyn : M(1) * st->y;
            Car *mc = &cars[SLOT_MISSION];
            wanted = 0; pl.hp = 100;
            switch (st->op) {
            case OP_GO: case OP_PICKUP:
                if (st->flags & F_FOOT) { if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; } }
                else if (st->flags & F_MCAR) pl.in_car = SLOT_MISSION;
                else if (st->op == OP_PICKUP || (st->flags & F_CAR)) {
                    if (pl.in_car < 0) { park_car(SLOT_PARKED0, CAR_SEDAN_BLUE, X, Y, 0, 0); pl.in_car = SLOT_PARKED0; }
                }
                go(X, Y); tick(3, 0); break;
            case OP_CAR: pl.in_car = SLOT_MISSION; tick(2, 0); break;
            case OP_DROP: pl.in_car = SLOT_MISSION; go(X, Y); tick(3, 0); break;
            case OP_KILL: case OP_BOSS: kill_all(0, 9); tick(2, 0); break;
            case OP_WRECK: for (int k = 0; k < 3; k++) car_damage(&cars[gang_slot[k]], 1000, 1); tick(20 * 7, 0); break;
            case OP_CHASE: if (st->flags & F_BAIL) { go((mc->x >> 8) - M(8), mc->y >> 8); spress = 5 * TICKS; tick(2, 0); } else { car_damage(mc, 1000, 1); tick(20 * 7, 0); } break;
            case OP_TAIL: stimer = st->t * TICKS; go((mc->x >> 8) - M(30), mc->y >> 8); tick(1, 0); break;
            case OP_DEFEND: kill_all(0, 3); tick(2, 0); break;
            case OP_HUNT: for (int k = 0; k < 2; k++) car_damage(&cars[gang_slot[k]], 1000, 1); tick(20 * 7, 0); break;
            case OP_HEAT: tick(2, 0); break;
            case OP_SURVIVE: for (int k = 0; k < st->t * 20 + 10 && phase == before; k++) { pl.hp = 100; tick(1, 0); } break;
            case OP_SPOT: if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; } go(X, Y); tick(50, 0); break;
            case OP_CHOICE: peds[PED_MISSION0].down = 1; tick(2, 0); break;
            default: tick(1, 0); break;
            }
            if (pl.dead_t > 0) { pl.dead_t = 0; pl.hp = 100; }
        }
        if (gstate == GS_DIALOG) skip_dialog();
        int ok = mission == m + 1 && !failed;
        printf("m%-2d %-16s -> next %d  money +%d  %s\n", m, d->title, mission, pl.money - money0, ok ? "PASS" : "FAIL");
        if (!ok) { fails++; if (mission == m) { mission++; mstate = 0; phase = 0; } }
    }
    printf("%s\n", fails ? "SOME FAILED" : "ALL JOBS COMPLETE");
    return fails != 0;
}
