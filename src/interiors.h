#ifndef INTERIORS_H
#define INTERIORS_H
#include "logo.h"
/* per shop type (0 gun, 1 store, 2 pawn, 3 cars) */
extern const Img *const room_img[4], *const front_img[4], *const clerk_img[4][2];
extern const s16 clerk_pos[4][2][2];
extern const s16 front_y[4];
#endif
