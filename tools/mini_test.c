/* plays complete bowling and pool games with simple scripted input and checks that every game ends */
#include "game.c"
#include "minigames.c"
const unsigned char spr_pixels[1 << 25];
unsigned char terrain_m[TMAP_W * TMAP_W], terrain_r[TMAP_W * TMAP_W], radar_map[RADAR_MAP_W * RADAR_MAP_W];
int throw_once(int sx, int spin, int pw, int aimv);
int main(int argc, char **argv)
{
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    game_init();
    if (argc > 1 && argv[1][0] == 'g') {   /* gutter check: how often throws find the gutter */
        int tot = 0, gut = 0;
        for (int sx = -480; sx <= 480; sx += 80) for (int aimv = -60; aimv <= 60; aimv += 15) for (int sp = -2; sp <= 2; sp++) {
            int n = throw_once(sx, sp, 70, aimv); tot++; if (bgutter) gut++; (void)n; }
        printf("gutter balls: %d of %d throws across every stand spot, aim and hook\n", gut, tot);
        printf("straight from the middle, no hook: %d pins; from the edge aiming out: %d pins (gutter %d)\n", throw_once(0, 0, 70, 0), throw_once(480, 0, 70, 60), bgutter);
        return 0;
    }
    if (argc > 1) {      /* throw table: start x vs hook */
        for (int pw = 40; pw <= 100; pw += 30) {
            printf("power %d\n      ", pw); for (int sp = -2; sp <= 2; sp++) printf(" hook%+d", sp); printf("\n");
            for (int sx = -350; sx <= 350; sx += 50) {
                printf("x%+5d", sx);
                for (int sp = -2; sp <= 2; sp++) { int tot = 0; for (int k = 0; k < 5; k++) tot += throw_once(sx, sp, pw, 0); printf("  %4.1f", tot / 5.0); }
                printf("\n");
            }
        }
        return 0;
    }
    int fails = 0;
    for (int kind = MINI_BOWL; kind <= MINI_POOL; kind++) {
        int wins = 0, losses = 0, longest = 0;
        for (int g = 0; g < 20; g++) {
            mini_start(kind);
            int r = 0, f = 0; u16 prev = 0;
            for (; f < 60 * 20 * 30 && !r; f++) {          /* up to 30 minutes of play at 20 fps */
                u16 k = 0;
                if (kind == MINI_BOWL) k = (f % 9 == 0) ? KEY_A : 0;
                else { int ph = f % 50; k = ph < 18 ? KEY_RIGHT * ((g + f / 50) % 3 == 0) : ph < 18 + (f / 50) % 25 ? KEY_A : 0; }
                r = mini_update(k, (u16)(k & ~prev), TICKS / 20); prev = k;
            }
            if (!r) { printf("%s game %d never ended\n", kind == MINI_BOWL ? "bowling" : "pool", g); fails++; }
            else if (r == 1) wins++; else losses++;
            if (f > longest) longest = f;
            if (kind == MINI_BOWL) printf("  bowl %d: you %d  artie %d\n", g, bowl_score(rolls[0], nrolls[0], 0), bowl_score(rolls[1], nrolls[1], 0));
        }
        printf("%-8s 20 games: won %d lost %d, longest %d s\n", kind == MINI_BOWL ? "bowling" : "pool", wins, losses, longest / 20);
    }
    printf("%s\n", fails ? "SOME FAILED" : "ALL GAMES FINISHED");
    return fails;
}
/* pins knocked down by one throw from a full rack */
int throw_once(int sx, int spin, int pw, int aimv)
{
    pins_reset(); bx = sx; by = START_Y; bgutter = 0; bspin = spin;
    int sp = 5800 + pw * 38, a = aimv * 26 / 60;
    bvx = sp * isin(a) >> 14; bvy = sp * icos(a) >> 14;
    for (int t = 0; t < 2000 && !(by > PIT_Y && !pins_moving()); t++) bowl_physics(4);
    int n = 0; for (int i = 0; i < 10; i++) n += pins[i].down || pins[i].gone;
    return n;
}
