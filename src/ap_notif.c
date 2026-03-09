//
// Copyright(C) 2023 David St-Louis
// Copyright(C) 2026 Kay "Kaito" Sinclaire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
// Common functions for handling notification boxes
//

#include <stdlib.h>
#include <string.h>

#include "crispy.h"
#include "doomtype.h"
#include "i_video.h"
#include "i_swap.h"
#include "v_video.h"
#include "v_trans.h"
#include "w_wad.h"
#include "z_zone.h"

#include "apdoom.h"
#include "ap_basic.h"
#include "ap_notif.h"

typedef struct icon_cache_s
{
    lumpindex_t lumpnum;
    pixel_t* pixels;
    struct icon_cache_s* next;
} icon_cache_t;

static icon_cache_t cache_head = {0, NULL, NULL};
static icon_cache_t *cache_tail = &cache_head;

// Takes a lump name (patch) and converts it to a pixel block.
// Result is stored in the cache for later use.
static pixel_t* convert_cache_icon(const char* name)
{
    lumpindex_t lumpnum = W_GetNumForName(name);

    icon_cache_t *icon;
    for (icon = cache_head.next; icon; icon = icon->next)
    {
        if (lumpnum == icon->lumpnum)
            return icon->pixels;
    }

    // Not cached yet, so add a new one
    icon = (icon_cache_t*)malloc(sizeof(icon_cache_t));
    icon->lumpnum = lumpnum;
    icon->pixels = malloc(AP_NOTIF_ICONSIZE * AP_NOTIF_ICONSIZE);
    icon->next = NULL;
    cache_tail = (cache_tail->next = icon);

    patch_t *patch = W_CacheLumpNum(lumpnum, PU_CACHE);
    pixel_t *raw = malloc(patch->width * patch->height);
    memset(raw, 0, patch->width * patch->height);

    for (int x = 0; x < patch->width; ++x)
    {
        column_t* column = (column_t *)((byte *)patch + LONG(patch->columnofs[x]));

        // step through the posts in a column
        while (column->topdelta != 0xff)
        {
            byte* source = (byte *)column + 3;

            for (int y = 0; y < column->length; ++y)
            {
                int k = (y + column->topdelta) * patch->width + x;
                raw[k] = *source++;
            }

            column = (column_t *)((byte *)column + column->length + 4);
        }
    }

    // Scale down raw into cached icon (I inverted src and dst, too lazy to change)
    const int max_size = MAX(patch->width, patch->height);
    const float scale = (max_size > AP_NOTIF_ICONSIZE) ? (float)max_size / (float)AP_NOTIF_ICONSIZE : 1.0f ;
    const int offsetx = (patch->width  - (int)((float)AP_NOTIF_ICONSIZE * scale)) / 2;
    const int offsety = (patch->height - (int)((float)AP_NOTIF_ICONSIZE * scale)) / 2;
    for (int srcy = 0; srcy < AP_NOTIF_ICONSIZE; ++srcy)
    {
        for (int srcx = 0; srcx < AP_NOTIF_ICONSIZE; ++srcx)
        {
            int srck = srcy * AP_NOTIF_ICONSIZE + srcx;
            int dstx = (int)((float)srcx * scale) + offsetx;
            int dsty = (int)((float)srcy * scale) + offsety;
            if (dstx < 0 || dstx >= patch->width ||
                dsty < 0 || dsty >= patch->height)
            {
                icon->pixels[srck] = 0;
                continue; // Skip, outside source patch
            }
            int dstk = dsty * patch->width + dstx;
            icon->pixels[srck] = raw[dstk];
        }
    }

    free(raw);
    return icon->pixels;
}

void APC_DrawNotifBox(int x, int y, const char *sprite, boolean disabled)
{
    pixel_t *pixels = convert_cache_icon(sprite);

    dp_translation = disabled ? cr[CR_DARK] : NULL;

    V_DrawPatch(x - AP_NOTIF_SIZE/2, 
                y - AP_NOTIF_SIZE/2, 
                W_CacheLumpName("NOTIFBG", PU_CACHE));

    V_DrawScaledBlockTransparency(
        x - AP_NOTIF_ICONSIZE/2,
        y - AP_NOTIF_ICONSIZE/2,
        AP_NOTIF_ICONSIZE, AP_NOTIF_ICONSIZE,
        pixels);

    dp_translation = NULL;
}

void APC_DrawNotifs(void (textfunc)(const char *, int, int))
{
    int notif_count;
    const ap_notification_icon_t* notifs = ap_get_notification_icons(&notif_count);

    for (int i = 0; i < notif_count; ++i)
    {
        const ap_notification_icon_t* notif = notifs + i;
        if (notif->state == AP_NOTIF_STATE_PENDING)
            continue;

        const int center_y = 172 + notif->y;

        APC_DrawNotifBox(notif->x - WIDESCREENDELTA, center_y, notif->sprite, notif->disabled);
        if (notif->text[0])
            textfunc(notif->text, notif->x + AP_NOTIF_SIZE / 2 + 3 - WIDESCREENDELTA, center_y - 5);
    }
}
