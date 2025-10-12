
#ifndef __LV_TEXT_H__
#define __LV_TEXT_H__

#include "doomtype.h"
#include "lv_video.h"

typedef struct {
    int space_x;
    int line_y;
    patch_t *patches[96];
} font_t;

void LV_LoadFont(font_t *dst_font, const char *prefix, int space_x, int line_y);

void LV_PrintText(layer_t *layer, int x, int y, font_t *f, const char *c);
void LV_FormatText(layer_t *layer, int x, int y, font_t *f, const char *fmtc, ...) PRINTF_ATTR(5, 6);

int LV_TextWidth(font_t *f, const char *c);
int LV_TextHeight(font_t *f, const char *c);

char *LV_WrapText(font_t *f, int w, const char *c);

#endif
