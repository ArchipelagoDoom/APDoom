
#ifndef __LV_VIDEO_H__
#define __LV_VIDEO_H__

#include "SDL.h"

#include "doomtype.h"
#include "v_patch.h"

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 360

typedef struct layer_s {
    SDL_Surface *surf;
    SDL_Texture *tex;

    byte brightness;
    byte _old_brightness;
    char _bspeed;

    struct layer_s *_next; // Linked list.
} layer_t;

void LV_InitVideo(void);
void LV_RenderFrame(void);
layer_t *LV_MakeLayer(void);

void LV_EnterMinimalMode(int (*unminimize_callback)(void));

// All palette colorations, for reference:
// 0 - Normal coloration
// 1 - Medium/Dark Red
// 2 - Gold
// 3 - Green
// 4 - Cyan
// 5 - Magenta
// 6 - (unused)
// 7 - (unused)
// 8 - (unused)
// 9 - Grayscale Dark
void LV_SetPalette(int palnum);
int LV_GetPalette(void);

void LV_OutlineRect(layer_t *layer, int x, int y, int w, int h, int size, unsigned int c);
void LV_FillRect(layer_t *layer, int x, int y, int w, int h, unsigned int c);

void LV_SetBrightness(layer_t *layer, byte brightness, char fade_speed);
void LV_ClearLayer(layer_t *layer);
void LV_DrawPatch(layer_t *layer, int x, int y, patch_t *patch);

#endif // __LV_VIDEO_H__
