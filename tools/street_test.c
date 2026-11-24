/* stands the player on a street corner for ten minutes of game time and counts street crimes */
#include "game.c"
const unsigned char spr_pixels[1 << 25];
unsigned char terrain_m[TMAP_W * TMAP_W], terrain_r[TMAP_W * TMAP_W], radar_map[RADAR_MAP_W * RADAR_MAP_W];
int main(void)
{
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    game_init(); new_game_state(); gstate = GS_PLAY; mission = NMISSIONS;      /* free roam */
    if (pl.in_car >= 0) { cars[pl.in_car].ai = AI_NONE; pl.in_car = -1; }
    int wanted_seen = 0;
    for (int f = 0; f < 20 * 600; f++) {
        game_update(0, TICKS / 20);
        if (gstate == GS_DIALOG) game_update(KEY_A, TICKS / 20);
        if (wanted) wanted_seen = 1;
        if (f % (20 * 60) == 0) { int a = (int)(pl.x >> 8), b = (int)(pl.y >> 8); pl.x = Q8(a + M(64)); pl.y = Q8(b); }   /* stroll a block each minute */
    }
    printf("10 minutes: %d muggings/fights, %d carjackings, %d arrests on foot, %d armed suspects shot, %d carjackers stopped; player wanted: %s\n",
           st_crimes, st_carjacks, st_arrests, st_shot, st_carstops, wanted_seen ? "YES (bug)" : "no");
    return wanted_seen;
}
