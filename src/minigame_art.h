#ifndef MINIGAME_ART_H
#define MINIGAME_ART_H
#include "logo.h"
extern const Img lane_img;
extern const Img pincam_img;
extern const Img pinbig_up;
extern const Img pinbig_l;
extern const Img pinbig_r;
extern const Img pin_up;
extern const Img pin_l;
extern const Img pin_r;
extern const Img pool_img;
#define NBBALL 12
extern const Img *const bball_img[NBBALL], *const pball_img[16];
static const unsigned char bball_d[NBBALL] = { 4, 5, 6, 7, 8, 10, 12, 14, 17, 20, 24, 28 };
#define LANE_CAMY_MM -2500
#define LANE_CAMZ_MM 1600
#define LANE_TILT_COS 16294
#define LANE_TILT_SIN -1712
#define LANE_FOCAL 300
#define LANE_HZ_Y 48
#define PCAM_Y_MM 14890
#define PCAM_Z_MM 1000
#define PCAM_COS 15582
#define PCAM_SIN -5062
#define PCAM_FOCAL 250
#define PCAM_HZ_Y 48
#define POOL_X0 28
#define POOL_Y0 34
#define POOL_X1 212
#define POOL_Y1 126
#endif
