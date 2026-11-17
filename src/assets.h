#ifndef ASSETS_H
#define ASSETS_H
typedef struct { unsigned char w, h; signed char ox; signed short oy; unsigned short upx; unsigned int off; } SprFrame;
extern const SprFrame spr_frames[];
extern const unsigned short spr_palette[];
extern const unsigned char spr_pixels[];
#define SPR_NCOL 148
#define SPR_PAL_BASE 103
#define CAR_COUPE_BLUE 0
#define CAR_COUPE_BLACK 128
#define CAR_MUSCLE_ORANGE 256
#define CAR_MUSCLE_SILVER 384
#define CAR_HATCH_LIME 512
#define CAR_HATCH_WHITE 640
#define CAR_SUPER_RED 768
#define CAR_SUPER_YELLOW 896
#define CAR_SEDAN_SILVER 1024
#define CAR_SEDAN_BLUE 1152
#define CAR_TAXI 1280
#define CAR_SUV_BLACK 1408
#define CAR_SUV_WHITE 1536
#define CAR_POLICE 1664
#define CAR_HATCH5_RED 1792
#define CAR_HATCH5_GREEN 1920
#define CAR_PICKUP_WHITE 2048
#define CAR_PICKUP_BLUE 2176
#define CAR_VAN_WHITE 2304
#define PED_PLAYER 2432
#define PED_GREY 2584
#define PED_RED 2888
#define PED_NAVY 3192
#define PED_SUIT 3496
#define PED_GREEN 3800
#define PED_WHITE 4104
#define PED_COP 4408
#define BLOOD_POOL 4712
#define BLOOD_DROP 4724
#define FX_SIREN 4725
#define FX_FLASH 4727
#define FX_MARKER 4728
#define FX_PICKUP 4730
#define TREE 4735
#define LAMP 4736
#define SPR_NFRAMES 4902
#define PROP_TLIGHT 4744
#define PROP_HYDRANT 4752
#define PROP_TRASH 4753
#define PROP_BENCH 4754
#define PROP_STREETTREE 4758
#define PROP_WATERTOWER 4759
#define CAR_WRECK 4760
#define FX_SMOKE 4888
#define FX_FIRE 4892
#define FX_EXPL 4896
#endif
