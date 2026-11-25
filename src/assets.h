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
#define PED_DIR_FRAMES 37
#define PED_CYCLE 16
#define PED_IDLE 4
#define PED_DOWN 333
#define PED_TUMBLE 341
#define PED_BLOCK 365
#define PED_STANCE 365
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
#define PED_GREY 3001
#define PED_RED 3731
#define PED_NAVY 4461
#define PED_SUIT 5191
#define PED_GREEN 5921
#define PED_WHITE 6651
#define PED_COP 7381
#define PED_BOHDAN 8111
#define PED_DEE 8841
#define PED_MARTA 9571
#define PED_WOMAN_A 10301
#define PED_WOMAN_B 11031
#define BLOOD_POOL 11761
#define BLOOD_DROP 11773
#define FX_SIREN 11774
#define FX_FLASH 11776
#define FX_MARKER 11777
#define FX_PICKUP 11779
#define FX_ROCKET 11785
#define TREE 11786
#define LAMP 11787
#define SPR_NFRAMES 12045
#define PROP_TLIGHT 11795
#define PROP_HYDRANT 11803
#define PROP_TRASH 11804
#define PROP_BENCH 11805
#define PROP_STREETTREE 11809
#define PROP_WATERTOWER 11810
#define CAT_TABBY 11811
#define CAT_BLACK 11851
#define CAT_GREY 11891
#define CAR_WRECK 11931
#define FX_SMOKE 12031
#define FX_FIRE 12035
#define FX_EXPL 12039
#endif
