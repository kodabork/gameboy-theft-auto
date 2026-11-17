#ifndef ASSETS_H
#define ASSETS_H
typedef struct { unsigned char w, h; signed char ox; signed short oy; unsigned short upx; unsigned int off; } SprFrame;
extern const SprFrame spr_frames[];
extern const unsigned short spr_palette[];
extern const unsigned char spr_pixels[];
#define SPR_NCOL 148
#define SPR_PAL_BASE 103
extern const unsigned char car_dims[][2];   /* half length, half width per car set (1/16 m) */
#define CAR_SET_FRAMES 192
#define CAR_NSETS 26
#define CAR_COUPE_BLUE 0
#define CAR_COUPE_BLACK 192
#define CAR_MUSCLE_ORANGE 384
#define CAR_MUSCLE_SILVER 576
#define CAR_HATCH_LIME 768
#define CAR_HATCH_WHITE 960
#define CAR_SUPER_RED 1152
#define CAR_SUPER_YELLOW 1344
#define CAR_SEDAN_SILVER 1536
#define CAR_SEDAN_BLUE 1728
#define CAR_TAXI 1920
#define CAR_SUV_BLACK 2112
#define CAR_SUV_WHITE 2304
#define CAR_POLICE 2496
#define CAR_HATCH5_RED 2688
#define CAR_HATCH5_GREEN 2880
#define CAR_PICKUP_WHITE 3072
#define CAR_PICKUP_BLUE 3264
#define CAR_VAN_WHITE 3456
#define CAR_WAGON_BROWN 3648
#define CAR_TUNER_GREEN 3840
#define CAR_LUXURY_BLACK 4032
#define CAR_MINIVAN_SILVER 4224
#define CAR_BOXTRUCK_WHITE 4416
#define CAR_YACHT_MAROON 4608
#define CAR_AMBULANCE 4800
#define PED_PLAYER 4992
#define PED_GREY 5144
#define PED_RED 5448
#define PED_NAVY 5752
#define PED_SUIT 6056
#define PED_GREEN 6360
#define PED_WHITE 6664
#define PED_COP 6968
#define BLOOD_POOL 7272
#define BLOOD_DROP 7284
#define FX_SIREN 7285
#define FX_FLASH 7287
#define FX_MARKER 7288
#define FX_PICKUP 7290
#define TREE 7295
#define LAMP 7296
#define SPR_NFRAMES 7526
#define PROP_TLIGHT 7304
#define PROP_HYDRANT 7312
#define PROP_TRASH 7313
#define PROP_BENCH 7314
#define PROP_STREETTREE 7318
#define PROP_WATERTOWER 7319
#define CAR_WRECK 7320
#define FX_SMOKE 7512
#define FX_FIRE 7516
#define FX_EXPL 7520
#endif
