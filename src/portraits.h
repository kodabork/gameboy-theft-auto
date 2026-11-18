#ifndef PORTRAITS_H
#define PORTRAITS_H
/* cutscene portraits (tools/make_portraits.py): 64x64 anime-style pixel art, one per speaking
   character. Hardware: an 8bpp 64x64 sprite; its colours live in OBJ palette entries 96..255. */
#define NFACES 6
extern const u16 face_pal[NFACES][160];
extern const u32 face_tiles[NFACES][64 * 16];
extern const char *const face_names[NFACES];
#ifdef HOST
#include "logo.h"
extern const Img img_faces[NFACES];
#endif
#endif
