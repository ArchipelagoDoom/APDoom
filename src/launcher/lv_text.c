
#include "ln_util.h"
#include "lv_text.h"
#include "lv_video.h"

#include "m_misc.h"
#include "v_patch.h"
#include "w_wad.h"
#include "z_zone.h"

void LV_LoadFont(font_t *dst_font, const char *prefix, int space_x, int line_y)
{
    printf("LV_LoadFont: Loading font %s...\n", prefix);
    dst_font->space_x = space_x;
    dst_font->line_y = line_y;

    char buffer[9];
    for (int i = 33; i < 127; ++i)
    {
        snprintf(buffer, sizeof(buffer), "%s%03d", prefix, i);

        // Don't use CacheLumpName, because we don't want to bomb out if not found.
        const lumpindex_t idx = W_CheckNumForName(buffer);
        dst_font->patches[i - 33] = (idx >= 0) ? (patch_t *)W_CacheLumpNum(idx, PU_STATIC) : NULL;
    }
}

// ----------------------------------------------------------------------------

void LV_PrintTextRange(layer_t *layer, int orig_x, int y, font_t *f, const char *c, const char *c_end)
{
    int x = orig_x;
    int orig_pal = LV_GetPalette();
    for (;*c && c < c_end;++c)
    {
        const byte b = (byte)*c;
        if (b >= 0xF0)
        {
            LV_SetPalette(b - 0xF0);
            continue;
        }
        else if (b == '\n')
        {
            x = orig_x;
            y += f->line_y;
            continue;
        }
        else if (b < 33 || b > 126 || !f->patches[b - 33])
        {
            x += f->space_x;
            continue;
        }

        patch_t *p = f->patches[b - 33];
        LV_DrawPatch(layer, x, y, p);
        x += p->width;
    }
    LV_SetPalette(orig_pal);
}

int LV_TextWidthRange(font_t *f, const char *c, const char *c_end)
{
    int w = 0;
    for (;*c && c < c_end;++c)
    {
        const byte b = (byte)*c;
        if (b >= 0xF0)
            continue;
        else if (b < 33 || b > 126 || !f->patches[b - 33])
            w += f->space_x;
        else
            w += f->patches[b - 33]->width;
    }
    return w;
}

char *LV_WrapText(font_t *f, int w, const char *str)
{
    int running_width = 0;

    int max_len = strlen(str) + 20;
    char *wrapped_str = malloc(max_len + 1);

    const char *in_p = str;
    char *out_p = wrapped_str;
    const char *last_space = str;
    const char *last_line = str;

    while (true)
    {
        const byte b = (byte)*str;

        if (b >= 0xF0 || b == '\n')
            {} // no operation
        else if (b < 33 || b > 126 || !f->patches[b - 33])
            running_width += f->space_x;
        else
            running_width += f->patches[b - 33]->width;

        if (b == ' ')
            last_space = str;

        if (!b || b == '\n')
        {
            last_line = str;
            last_space = str;
        }
        else if (running_width > w)
        {
            if (str - 1 == last_line) // no movement happened
                break;
            else if (last_space == in_p) // no space found
                last_line = str - 1;
            else
                last_line = last_space;
        }

        if (last_line > in_p)
        {
            while (last_line > in_p)
            {
                if (out_p - wrapped_str >= max_len)
                    goto end_str;
                *out_p++ = *in_p++;
            }

            if (out_p - wrapped_str >= max_len || !*in_p)
                break;
            running_width = 0;
            str = in_p;

            *out_p++ = '\n';
            if (*in_p == ' ' || *in_p == '\n')
                ++in_p;
        }
        else
            ++str;
    }

end_str:
    *out_p = 0;
    return wrapped_str;
}

// ----------------------------------------------------------------------------

void LV_PrintText(layer_t *layer, int x, int y, font_t *f, const char *c)
{
    LV_PrintTextRange(layer, x, y, f, c, c + strlen(c));
}

int LV_TextWidth(font_t *f, const char *c)
{
    return LV_TextWidthRange(f, c, c + strlen(c));
}

int LV_TextHeight(font_t *f, const char *c)
{
    int i = 1;
    for (;*c;++c)
    {
        if (*c == '\n')
            ++i;
    }
    return i * f->line_y;
}

void LV_FormatText(layer_t *layer, int x, int y, font_t *f, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    LV_PrintTextRange(layer, x, y, f, str, str+strlen(str));

    free(str);
}
