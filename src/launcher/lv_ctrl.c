
#include "li_event.h"
#include "lv_video.h"
#include "lv_text.h"
#include "lv_ctrl.h"

#include "w_wad.h"
#include "z_zone.h"

extern font_t large_font;

// In order: Primary, Secondary, Options, Back
static const char *buttons_kb[4]    = { "KB_ENTER", "KB_SPACE", "KB_TAB",   "KB_ESC"   };
static const char *buttons_steam[4] = { "DECKBN_S", "DECKBN_W", "DECKBN_N", "DECKBN_E" };
static const char *buttons_ninty[4] = { "DECKBN_E", "DECKBN_N", "DECKBN_W", "DECKBN_S" };
static const char *buttons_ps[4]    = { "PSBTN_S",  "PSBTN_W",  "PSBTN_N",  "PSBTN_E"  };
static const char *buttons_xbox[4]  = { "XBOXBN_S", "XBOXBN_W", "XBOXBN_N", "XBOXBN_E" };
static const char **buttons_map = buttons_kb;

// Set to something to have set true when style changes happen
int *style_change_var = NULL;

void LV_SetStyleChangeVar(int *var)
{
    style_change_var = var;
}

void LV_SetButtonStyle(int style)
{
    const char **old_map = buttons_map;
    switch (style)
    {
    case STYLE_KEYBOARD: buttons_map = buttons_kb;    break;
    case STYLE_STEAM:    buttons_map = buttons_steam; break;
    case STYLE_NINTENDO: buttons_map = buttons_ninty; break;
    case STYLE_PS:       buttons_map = buttons_ps;    break;
    case STYLE_XBOX:     buttons_map = buttons_xbox;  break;
    default: return;
    }
    if (buttons_map != old_map && style_change_var)
        *style_change_var = true;
}

patch_t *LV_ButtonPatch(int button)
{
    if (button >= NAV_ISBUTTON && button < NAV_BACKSPACE)
        return W_CacheLumpName(buttons_map[button - NAV_ISBUTTON], PU_CACHE);
    return NULL;
}

// ----------------------------------------------------------------------------

void LV_PrintControls(layer_t *layer, int x, int y, const char *ctrl_list[4])
{
    for (int i = 3; i >= 0; --i)
    {
        if (!ctrl_list[i])
            continue;
        x -= LV_TextWidth(&large_font, ctrl_list[i]);
        LV_PrintText(layer, x, y, &large_font, ctrl_list[i]);
        x -= 20;
        LV_DrawPatch(layer, x, y - 1, LV_ButtonPatch(NAV_ISBUTTON + i));
        x -= 10;
    }
}
