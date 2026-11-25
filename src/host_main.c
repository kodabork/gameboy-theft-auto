/* PC preview: runs the game with scripted input and writes frames as PPM. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "engine.h"
#include "game.h"

static u16 fb[SCR_W * SCR_H], pal[256];
static Sprite objs[MAX_SPRITES];
unsigned char spr_pixels[1 << 25];
unsigned char terrain_m[TMAP_W * TMAP_W], terrain_r[TMAP_W * TMAP_W], radar_map[RADAR_MAP_W * RADAR_MAP_W];

static void dump(const char *name)
{
    FILE *f = fopen(name, "wb");
    fprintf(f, "P6\n240 160\n255\n");
    for (int i = 0; i < SCR_W * SCR_H; i++) {
        u16 c = pal[fb[i] & 255];
        unsigned char px[3] = { (c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3 };
        fwrite(px, 3, 1, f); fwrite(px, 3, 1, f);
    }
    fclose(f);
}

void game_debug_knock(int ahead_m, int age);
void game_debug_set(int w, int guns);
int main(int argc, char **argv)
{
    FILE *af = fopen("../build/assets.bin", "rb"); fread(spr_pixels, 1, sizeof spr_pixels, af); fclose(af);
    FILE *tf = fopen("../build/terrain.bin", "rb"); fread(terrain_m, 1, sizeof terrain_m, tf); fread(terrain_r, 1, sizeof terrain_r, tf); fread(radar_map, 1, sizeof radar_map, tf); fclose(tf);
    palette_build(pal);
    static u16 day_pal[256]; for (int i = 0; i < 256; i++) day_pal[i] = pal[i];
    render_init();
    game_init();
    /* script: "frames:keys,frames:keys,...@dumpframe,..." keys hex */
    const char *script = argc > 1 ? argv[1] : "1:0";
    const char *dumps = argc > 2 ? argv[2] : "0";
    int frame = 0;
    static char buf[16384]; strncpy(buf, script, 16383);
    int dumpat[64], nd = 0;
    char db[512]; strncpy(db, dumps, 511);
    for (char *t = strtok(db, ","); t; t = strtok(NULL, ",")) dumpat[nd++] = atoi(t);
    char *save;
    for (char *t = strtok_r(buf, ",", &save); t; t = strtok_r(NULL, ",", &save)) {
        int n = atoi(t); int keys = (int)strtol(strchr(t, ':') + 1, NULL, 16);
        for (int i = 0; i < n; i++, frame++) {
            if (keys == 0xFFF) { game_debug_knock(9, 1200); keys = 0; }
            if (keys >= 0xB00 && keys < 0xC00) { void game_debug_street(int); game_debug_street(keys - 0xB00); keys = 0; }
            if (keys >= 0xC00 && keys < 0xD00) { void game_debug_coast(int); game_debug_coast(keys - 0xC00); keys = 0; }
            if (keys >= 0x10000) { void game_debug_block(int, int, int); game_debug_block((keys >> 8) & 255, keys & 255, (keys >> 16) - 1); keys = 0; }
            if (keys >= 0xF10 && keys < 0xF30) { void game_debug_shop(int); game_debug_shop(keys - 0xF10); keys = 0; }
            if (keys >= 0xE00 && keys < 0xE30) { void game_debug_mission(int); game_debug_mission(keys - 0xE00); keys = 0; }
            if (keys >= 0xD00 && keys < 0xD18) { void game_debug_time(int); game_debug_time(keys - 0xD00); keys = 0; }
            if (keys == 0xFFE) { game_debug_set(3, 1); keys = 0; }
            if (keys == 0xF00) { void game_debug_burn_self(void); game_debug_burn_self(); keys = 0; }
            if (keys == 0xF01) { void game_debug_near_cat(void); game_debug_near_cat(); keys = 0; }
            if (keys == 0xF02) { void game_debug_ragdoll(void); game_debug_ragdoll(); keys = 0; }
            if (keys >= 0xFC0 && keys < 0xFD0) { void game_debug_burn(int); game_debug_burn((keys - 0xFC0) * 100); keys = 0; }
            if (keys == 0xFD0) { void game_debug_play(void); game_debug_play(); keys = 0; }
            if (keys >= 0xFE0 && keys <= 0xFE8) { void game_debug_tp_mission(int); game_debug_tp_mission(keys - 0xFE0); keys = 0; }
            if (keys >= 0xFF0 && keys <= 0xFF3) { void game_debug_tp_mission(int); game_debug_tp_mission(keys - 0xFF0); keys = 0; }
            game_update((u16)keys, 16384 / 20);
            { int game_tod(void), game_daylight_screen(void); palette_tod(pal, day_pal, game_daylight_screen() ? 720 : game_tod()); }
            for (int k = 0; k < nd; k++) if (dumpat[k] == frame) {
                Camera cam; int no;
                game_build_scene(&cam, objs, &no);
                memset(fb, 0xEE, sizeof fb);   /* garbage colour to catch unfilled pixels */
                render_frame(fb, &cam, objs, no);
                hud_draw(fb);
                char name[64]; sprintf(name, "../build/f%04d.ppm", frame); dump(name);
                printf("frame %d cam %d,%d objs %d\n", frame, cam.x, cam.y, no);
                { void game_debug(void); game_debug(); }
            }
        }
    }
    return 0;
}
