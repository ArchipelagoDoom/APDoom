
#ifndef __LV_CTRL_H__
#define __LV_CTRL_H__

#include "doomtype.h"
#include "lv_video.h"
#include "v_patch.h"

#define STYLE_KEYBOARD 0
#define STYLE_STEAM    1
#define STYLE_NINTENDO 2
#define STYLE_PS       3
#define STYLE_XBOX     4

void LV_SetStyleChangeVar(int *var);
void LV_SetButtonStyle(int style);
patch_t *LV_ButtonPatch(int button);
void LV_PrintControls(layer_t *layer, int x, int y, const char *ctrl_list[4]);

#endif
