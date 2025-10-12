
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "li_event.h"
#include "ln_util.h"
#include "lv_video.h"
#include "lv_text.h"
#include "m_misc.h"

// ----------------------------------------------------------------------------

extern layer_t *l_primary;
extern layer_t *l_background;
extern layer_t *l_dialog;
extern font_t large_font;
extern font_t small_font;

int dialog_open = false;
static int dialog_type = DIALOG_OK;
static void (*dialog_responder)(int) = NULL;

void LN_DialogResponder(void (*responder)(int))
{
    dialog_responder = responder;
}

void LN_OpenDialog(int type, const char *header, const char *msg)
{
    dialog_type = type;

    LV_ClearLayer(l_dialog);
    LV_SetBrightness(l_primary, 100, 12);
    LV_SetBrightness(l_background, 100, 12);

    int center_header = LV_TextWidth(&large_font, header) / 2;
    LV_FillRect(l_dialog, (SCREEN_WIDTH/2) - 180, 50, 360, 20, 0xC0300000);
    LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 50, 360, 20, 2, 0xFF500000);
    LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 50, 360, 20, 1, 0xFF700000);
    LV_PrintText(l_dialog, (SCREEN_WIDTH/2) - center_header, 55, &large_font, header);

    char *wrap_msg = LV_WrapText(&small_font, 320, msg);
    int wrap_height = LV_TextHeight(&small_font, wrap_msg);
    LV_FillRect(l_dialog, (SCREEN_WIDTH/2) - 180, 70, 360, 20 + wrap_height, 0xC0300000);
    LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 70, 360, 20 + wrap_height, 2, 0xFF500000);
    LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 70, 360, 20 + wrap_height, 1, 0xFF700000);
    LV_PrintText(l_dialog, (SCREEN_WIDTH/2) - 160, 80, &small_font, wrap_msg);
    free(wrap_msg);

    if (dialog_type != DIALOG_EMPTY)
    {
        LV_FillRect(l_dialog, (SCREEN_WIDTH/2) - 180, 90 + wrap_height, 360, 20, 0xC0300000);
        LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 90 + wrap_height, 360, 20, 2, 0xFF500000);
        LV_OutlineRect(l_dialog, (SCREEN_WIDTH/2) - 180, 90 + wrap_height, 360, 20, 1, 0xFF700000);
    }

    int text_x = ((SCREEN_WIDTH/2) + 180);
    switch (dialog_type)
    {
    default:
        break;
    case DIALOG_OK:
        text_x -= LV_TextWidth(&large_font, "OK");
        text_x -= 10;
        LV_PrintText(l_dialog, text_x, 95 + wrap_height, &large_font, "OK");
        text_x -= LV_TextWidth(&small_font, "\xF9(return)");
        text_x -= 8;
        LV_PrintText(l_dialog, text_x, 100 + wrap_height, &small_font, "\xF9(return)");
        break;
    case DIALOG_YES_NO:
        text_x -= LV_TextWidth(&large_font, "No");
        text_x -= 10;
        LV_PrintText(l_dialog, text_x, 95 + wrap_height, &large_font, "No");
        text_x -= LV_TextWidth(&small_font, "\xF9(esc)");
        text_x -= 8;
        LV_PrintText(l_dialog, text_x, 100 + wrap_height, &small_font, "\xF9(esc)");
        text_x -= LV_TextWidth(&large_font, "Yes");
        text_x -= 20;
        LV_PrintText(l_dialog, text_x, 95 + wrap_height, &large_font, "Yes");
        text_x -= LV_TextWidth(&small_font, "\xF9(return)");
        text_x -= 8;
        LV_PrintText(l_dialog, text_x, 100 + wrap_height, &small_font, "\xF9(return)");
        break;
    }

    dialog_open = true;
}

void LN_CloseDialog(void)
{
    LV_ClearLayer(l_dialog);
    LV_SetBrightness(l_primary, 255, 8);
    LV_SetBrightness(l_background, 255, 8);
    dialog_open = false;
    dialog_responder = NULL;
}

void LN_HandleDialog(void)
{
    int result;

    switch (dialog_type)
    {
    default: // DIALOG_EMPTY
        break;
    case DIALOG_OK:
        if (nav[NAV_BACK] || nav[NAV_PRIMARY] || mouse.primary || mouse.secondary)
            LN_CloseDialog();
        break;
    case DIALOG_YES_NO:
        if (nav[NAV_BACK] || nav[NAV_SECONDARY] || mouse.secondary)
            result = 0;
        else if (nav[NAV_PRIMARY] || mouse.primary)
            result = 1;
        else
            break;

        if (dialog_responder)
            dialog_responder(result);
        LN_CloseDialog();
        break;
    }
}

// ----------------------------------------------------------------------------

char *LN_allocvsprintf(const char *fmt, va_list args)
{
    static char buf[2048];

    M_vsnprintf(buf, sizeof(buf), fmt, args);

    int mem_len = strlen(buf) + 1;
    char *new_str = malloc(mem_len);
    memcpy(new_str, buf, mem_len);
    return new_str;
}

char *LN_allocsprintf(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    char *ret = LN_allocvsprintf(fmt, args);
    va_end(args);

    return ret;
}
