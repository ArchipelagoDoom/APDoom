
#ifndef __LV_TEXT_H__
#define __LV_TEXT_H__

#include "doomtype.h"
#include "lv_video.h"

#define LV_FONTSTART   32 // First character that can have a patch
#define LV_FONTEND    127 // Last character that can have a patch
#define LV_COLORSTART 160 // First character that represents a color control code

typedef struct {
    int space_x;
    int line_y;
    patch_t *patches[1 + (LV_FONTEND - LV_FONTSTART)];
} font_t;

void LV_LoadFont(font_t *dst_font, const char *prefix, int space_x, int line_y);

void LV_PrintText(layer_t *layer, int x, int y, font_t *f, const char *c);
void LV_FormatText(layer_t *layer, int x, int y, font_t *f, const char *fmtc, ...) PRINTF_ATTR(5, 6);

int LV_TextWidth(font_t *f, const char *c);
int LV_TextHeight(font_t *f, const char *c);

char *LV_WrapText(font_t *f, int w, const char *c);

#endif
