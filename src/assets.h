#ifndef ASSETS_H
#define ASSETS_H
typedef struct { unsigned char w, h; signed char ox; signed short oy; unsigned short upx; unsigned int off; } SprFrame;
extern const SprFrame spr_frames[];
extern const unsigned short spr_palette[];
extern const unsigned char spr_pixels[];
#define SPR_NCOL 148
#define SPR_PAL_BASE 103
extern const unsigned char car_dims[][2];   /* half length, half width per car set (1/16 m) */
#define CAR_SET_FRAMES 100
#define CAR_NSETS 26
#define PED_DIR_FRAMES 26
#define PED_CYCLE 12
#define PED_DOWN 234
#define PED_TUMBLE 242
#define PED_BLOCK 266
#define PED_STANCE 266
#define PED_NSTANCE 4
#define CAR_COUPE_BLUE 0
#define CAR_COUPE_BLACK 100
#define CAR_MUSCLE_ORANGE 200
#define CAR_MUSCLE_SILVER 300
#define CAR_HATCH_LIME 400
#define CAR_HATCH_WHITE 500
#define CAR_SUPER_RED 600
#define CAR_SUPER_YELLOW 700
#define CAR_SEDAN_SILVER 800
#define CAR_SEDAN_BLUE 900
#define CAR_TAXI 1000
#define CAR_SUV_BLACK 1100
#define CAR_SUV_WHITE 1200
#define CAR_POLICE 1300
#define CAR_HATCH5_RED 1400
#define CAR_HATCH5_GREEN 1500
#define CAR_PICKUP_WHITE 1600
#define CAR_PICKUP_BLUE 1700
#define CAR_VAN_WHITE 1800
#define CAR_WAGON_BROWN 1900
#define CAR_TUNER_GREEN 2000
#define CAR_LUXURY_BLACK 2100
#define CAR_MINIVAN_SILVER 2200
#define CAR_BOXTRUCK_WHITE 2300
#define CAR_YACHT_MAROON 2400
#define CAR_AMBULANCE 2500
#define PED_PLAYER 2600
#define PED_GREY 2902
#define PED_RED 3434
#define PED_NAVY 3966
#define PED_SUIT 4498
#define PED_GREEN 5030
#define PED_WHITE 5562
#define PED_COP 6094
#define PED_DEE 6626
#define PED_MARTA 7158
#define PED_WOMAN_A 7690
#define PED_WOMAN_B 8222
#define BLOOD_POOL 8754
#define BLOOD_DROP 8766
#define FX_SIREN 8767
#define FX_FLASH 8769
#define FX_MARKER 8770
#define FX_PICKUP 8772
#define FX_ROCKET 8778
#define TREE 8779
#define LAMP 8780
#define SPR_NFRAMES 9038
#define PROP_TLIGHT 8788
#define PROP_HYDRANT 8796
#define PROP_TRASH 8797
#define PROP_BENCH 8798
#define PROP_STREETTREE 8802
#define PROP_WATERTOWER 8803
#define CAT_TABBY 8804
#define CAT_BLACK 8844
#define CAT_GREY 8884
#define CAR_WRECK 8924
#define FX_SMOKE 9024
#define FX_FIRE 9028
#define FX_EXPL 9032
#endif
