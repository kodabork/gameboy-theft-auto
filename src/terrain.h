#ifndef TERRAIN_H
#define TERRAIN_H
#define TMAP_W 656
#define TMAP_ORIGIN -1024   /* units: map cell 0 starts at -64 m */
enum { TM_GRID, TM_WATER, TM_SAND, TM_GRASS, TM_DIRT, TM_ROAD, TM_ROADC, TM_ROADE, TM_PATH, TM_GRAVEL };
#define TMAP_HMAX_U 314   /* highest terrain point, units */
#ifdef HOST
extern unsigned char terrain_h[], terrain_m[], terrain_r[], radar_map[];
#else
extern const unsigned char terrain_h[];     /* TMAP_W*TMAP_W, eighth metres, +1 m offset: units = v*2-16 */
extern const unsigned char terrain_m[];     /* TMAP_W*TMAP_W, material code | lit<<7 */
extern const unsigned char terrain_r[];     /* road lateral, 1/8 m + 64 (0 none) | dash<<7 */
extern const unsigned char radar_map[];     /* 328x328, 2 m texels, radar colours */
#endif
#define RADAR_MAP_W 328
#define TMAX_W 41
extern const unsigned char terrain_max16[];   /* max of terrain_h per 16 m cell */
#define NCOAST_WP 282
extern const short coast_wp[282][2];
#define NRING_TREES 160
extern const short ring_trees[160][2];
#define NCOAST_LAMPS 67
extern const short coast_lamps[67][3];
#endif
