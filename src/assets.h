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
#define PED_GREY 2752
#define PED_RED 3056
#define PED_NAVY 3360
#define PED_SUIT 3664
#define PED_GREEN 3968
#define PED_WHITE 4272
#define PED_COP 4576
#define BLOOD_POOL 4880
#define BLOOD_DROP 4892
#define FX_SIREN 4893
#define FX_FLASH 4895
#define FX_MARKER 4896
#define FX_PICKUP 4898
#define FX_ROCKET 4904
#define TREE 4905
#define LAMP 4906
#define SPR_NFRAMES 5164
#define PROP_TLIGHT 4914
#define PROP_HYDRANT 4922
#define PROP_TRASH 4923
#define PROP_BENCH 4924
#define PROP_STREETTREE 4928
#define PROP_WATERTOWER 4929
#define CAT_TABBY 4930
#define CAT_BLACK 4970
#define CAT_GREY 5010
#define CAR_WRECK 5050
#define FX_SMOKE 5150
#define FX_FIRE 5154
#define FX_EXPL 5158
#endif
