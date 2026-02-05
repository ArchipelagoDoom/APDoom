
#include "SDL.h"
#include "doomtype.h"

#include "li_event.h"
#include "ln_exec.h"
#include "ln_util.h"
#include "lv_video.h"
#include "lv_text.h"

#include "tables.h"
#include "d_iwad.h"
#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"
#include "w_wad.h"
#include "z_zone.h"

#include "ap_basic.h" // C code
#include "apdoom.h" // C++ code

#include <time.h> // strftime

layer_t *l_bg_primary;
layer_t *l_primary;
layer_t *l_bg_secondary;
layer_t *l_secondary;
layer_t *l_dialog;

font_t large_font;
font_t small_font;

// Cursor visual effect variables
static const int anim_text_move[15] =  // Text movement of selected item
    { 5, 10, 12, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};
static const int anim_bg_fade[15] =   // Background glow of selected item
    { 2,  4,  6,  8,  9, 10, 11, 12, 13, 14, 14, 15, 15, 16, 16};
static byte anim_step = 0;
static int pulsating_color = 0;

// Save settings that we build for new connections
ap_savesettings_t game_settings = {
    "", "archipelago.gg:", "", false,
    -1, -1, -1, -1, -1, -1, -1,
    ""
};

// Pointer to the save settings to use when launching.
// Usually points to game_settings above, but may differ if, e.g., loading a save.
const ap_savesettings_t *settings_to_execute = NULL;

// Set to true to refresh save games.
// Can be done manually, or gets auto set when a game is executed.
bool invalidate_savegame_cache = false;

// We're editing connection info for an already present save, which limits what we can do.
static bool editing_savegame = false;

// ============================================================================
// Game / World Functionality Testing
// ============================================================================

typedef struct {
    bool is_functional;
    char *error_reason;
} extrainfo_t;

int world_count;
static const ap_worldinfo_t **all_worlds = NULL;
extrainfo_t *extra_world_info = NULL;

static bool TestIWAD(const char *iwad, char **error_str)
{
    const char *iwad_path = D_FindWADByName(iwad);
    if (!iwad_path)
    {
        const char *descriptive_text = "";
        if (!strcmp(iwad, "DOOM.WAD")
            || !strcmp(iwad, "DOOM2.WAD")
            || !strcmp(iwad, "TNT.WAD")
            || !strcmp(iwad, "PLUTONIA.WAD"))
        {
            descriptive_text = "\n\n"
                "The easiest way to obtain this file is to purchase\xF2 DOOM + DOOM II\xF0 on Steam; "
                "APDoom can usually load the game files from this version automatically."
                "\n\n"
                "If you already own this game, place the IWAD file into the same directory as APDoom. "
                "For newer rereleases, you want to use the IWAD file that is in the /base/ directory, "
                "and \xF1NOT\xF0 the one in the /rerelease/ directory.";
        }
        else if (!strcmp(iwad, "HERETIC.WAD"))
        {
            descriptive_text = "\n\n"
                "The easiest way to obtain this file is to purchase\xF2 Heretic + Hexen\xF0 on Steam; "
                "APDoom can usually load the game files from this version automatically."
                "\n\n"
                "If you already own this game, place the IWAD file into the same directory as APDoom. "
                "For newer rereleases, you want to use the IWAD file that is in the /dos/base/ directory.";
        }
        *error_str = LN_allocsprintf("The IWAD for this game, \xF2%s\xF0, could not be found.%s",
            iwad, descriptive_text);
        return false;
    }
    return true;
}

static bool TestPWAD(const char **wad_list, char **error_str)
{
    const char *not_found_list[8];
    int not_found = 0;

    for (int i = 0; wad_list[i]; ++i)
    {
        const char *pwad_path = D_FindWADByName(wad_list[i]);
        if (!pwad_path)
            not_found_list[not_found++] = wad_list[i];
        if (not_found == 8)
            break;
    }

    if (not_found > 0)
    {
        char not_found_buf[1024];
        not_found_buf[0] = 0;
        const char *extra_descriptive_text = "";

        for (int i = 0; i < not_found; ++i)
        {
            M_StringConcat(not_found_buf, "\n - ", 1024);
            M_StringConcat(not_found_buf, not_found_list[i], 1024);

            if (!strcmp(not_found_list[i], "nerve.wad"))
            {
                extra_descriptive_text = "\n\n"
                    "\xF2nerve.wad\xF0 contains the No Rest for the Living levels, and can be found in "
                    "the /rerelease/ directory for\xF2 DOOM + DOOM II\xF0.";
            }
        }

        *error_str = LN_allocsprintf("The following WADs are required for this game, but could not be found:\n%s%s",
            not_found_buf, extra_descriptive_text);
        return false;
    }
    return true;
}

void TestWorldFunctionality(void)
{
    all_worlds = ap_list_worlds();

    world_count = 0;
    for (; all_worlds[world_count]; ++world_count);

    if (!world_count)
        I_Error("No worlds available! Can't run!");

    extra_world_info = calloc(world_count, sizeof(extrainfo_t));
    for (size_t i = 0; all_worlds[i]; ++i)
    {
        if (
            !TestIWAD(all_worlds[i]->iwad, &extra_world_info[i].error_reason)
            || !TestPWAD(all_worlds[i]->required_wads, &extra_world_info[i].error_reason)
        )
        {
            continue;
        }

        extra_world_info[i].is_functional = true;
    }

}


// ============================================================================
// Menus
// ============================================================================

typedef enum {
    MENUSPEC_EXECUTE_SETUP = -4,
    MENUSPEC_EXECUTE_GAME = -3,
    MENUSPEC_REINIT = -2,
    MENUSPEC_BACK = -1,
    MENU_NONE = 0,
    MENU_MAIN,
    MENU_SELECT_GAME,
    MENU_CONNECT,
    MENU_PRACTICE,
    MENU_ADVANCED_OPTIONS,
    MENU_LOAD_SAVED_GAME,
    MENU_LOAD_OPTIONS,
    NUM_MENUS
} menulist_t;

enum {
    INTERACT_NONE,
    INTERACT_SELECT,
    INTERACT_LEFT,
    INTERACT_RIGHT,
};

struct menudata_s;

typedef struct {
    int x;
    int y;
    const char *text;

    // Return nonzero to suppress default menu drawing.
    int (*draw_handler)(int num, struct menudata_s *data, void *arg);

    // Additional data to pass to draw handler, if necessary.
    void *arg;
} menutarget_t;

typedef struct menudata_s {
    int cursor;

    int target_count;
    const menutarget_t *target_list;

    layer_t *layer; // The layer the menu is drawn on.

    struct {
        // If scrolling is enabled for this menu. Nothing here applies if false.
        bool enable;

        // Minimum and maximum allowed bounds of the menu, set by menu.
        int min, max;

        // cur: Actual offset of menu. Tries to match inter.
        // inter: Intermediate destination. Usually equals dest, but can overshoot min/max.
        // dest: The true position of the menu that we're aiming towards.
        int cur, inter, dest;
    } scroll;
} menudata_t;

static void Main_Init(menudata_t *data);
static void Main_Draw(menudata_t *data);
static void Main_Input(menudata_t *data);

static void SelectGame_Init(menudata_t *data);
static void SelectGame_Draw(menudata_t *data);
static void SelectGame_Input(menudata_t *data);

static void Connect_Init(menudata_t *data);
static void Connect_Draw(menudata_t *data);
static void Connect_Input(menudata_t *data);

static void Practice_Init(menudata_t *data);
static void Practice_Draw(menudata_t *data);
static void Practice_Input(menudata_t *data);

static void AdvancedOptions_Init(menudata_t *data);
static void AdvancedOptions_Draw(menudata_t *data);
static void AdvancedOptions_Input(menudata_t *data);

static void LoadSavedGame_Init(menudata_t *data);
static void LoadSavedGame_Draw(menudata_t *data);
static void LoadSavedGame_Input(menudata_t *data);

static void LoadOptions_Init(menudata_t *data);
static void LoadOptions_Draw(menudata_t *data);
static void LoadOptions_Input(menudata_t *data);

struct {
    void (*initfunc)(menudata_t *data);
    void (*drawfunc)(menudata_t *data);
    void (*inputfunc)(menudata_t *data);

    menudata_t data;
} menus[NUM_MENUS] = {
    {NULL, NULL, NULL}, // Corresponds to MENU_NONE, must be left empty.
    {Main_Init, Main_Draw, Main_Input},
    {SelectGame_Init, SelectGame_Draw, SelectGame_Input},
    {Connect_Init, Connect_Draw, Connect_Input},
    {Practice_Init, Practice_Draw, Practice_Input},
    {AdvancedOptions_Init, AdvancedOptions_Draw, AdvancedOptions_Input},
    {LoadSavedGame_Init, LoadSavedGame_Draw, LoadSavedGame_Input},
    {LoadOptions_Init, LoadOptions_Draw, LoadOptions_Input},
};

static int menu_stack_pos = 0;
static menulist_t menu_stack[6] = {MENU_MAIN};
static menulist_t next_menu = MENU_NONE;

static void RunMenuScroll(menudata_t *data)
{
    // Bring inter towards dest if it's not equal
    if (data->scroll.inter < data->scroll.dest)
        data->scroll.inter += ((data->scroll.dest - data->scroll.inter) >> 2) + 1;
    else if (data->scroll.inter > data->scroll.dest)
        data->scroll.inter -= ((data->scroll.inter - data->scroll.dest) >> 2) + 1;

    // Bring cur towards inter
    if (data->scroll.cur < data->scroll.inter)
        data->scroll.cur += ((data->scroll.inter - data->scroll.cur) >> 2) + 1;
    else if (data->scroll.cur > data->scroll.inter)
        data->scroll.cur -= ((data->scroll.cur - data->scroll.inter) >> 2) + 1;
}

// ----------------------------------------------------------------------------

static void DrawHeader(layer_t *layer, int y, const char *txt)
{
    const int center_header = LV_TextWidth(&large_font, txt) / 2;
    LV_SetPalette(2);
    LV_PrintText(layer, (SCREEN_WIDTH/2)-center_header, y, &large_font, txt);
    LV_SetPalette(0);
}

static void DrawMenuItem(layer_t *layer, int x, int y, int selected, const char *fmt, ...)
{
    if (selected)
        x += anim_text_move[anim_step];

    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    LV_PrintText(layer, x, y, &large_font, str);

    free(str);
}

static void DrawLabel(layer_t *layer, int x, int y, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    const int width = LV_TextWidth(&large_font, str);
    LV_PrintText(layer, (SCREEN_WIDTH-x)-width, y, &large_font, str);

    free(str);
}

void StandardMenuDraw(menudata_t *data)
{
    int pal = LV_GetPalette();
    for (int i = 0; i < data->target_count; ++i)
    {
        const menutarget_t *target = &data->target_list[i];
        int y = target->y;

        if (data->scroll.enable)
        {
            LV_SetAlpha(0xFF);
            y -= data->scroll.cur;
            if (y <= 112) continue;
            else if (y >= 328) break;
            else if (y < 120) LV_SetAlpha(255 - ((120 - y) << 5));
            else if (y > 320) LV_SetAlpha(255 - ((y - 320) << 5));
        }

        if (data->cursor == i)
        {
            const int h = anim_bg_fade[anim_step];
            uint32_t color = 0x000070FF + (pulsating_color << 8) + (pulsating_color << 17);

            LV_FillRect(data->layer, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 0x60000000 | color);
            LV_OutlineRect(data->layer, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 1, 0x40000000 | color);
        }

        if (target->draw_handler && target->draw_handler(i, data, target->arg))
            continue;
        DrawMenuItem(data->layer, target->x, y, data->cursor == i, target->text);
        LV_SetPalette(pal); // Restore palette, as draw handler may change it
    }
    if (data->scroll.enable)
        LV_SetAlpha(0xFF);
}

int StandardMenuInput(menudata_t *data, int *interaction_type)
{
    if (data->target_count == 0)
    {
        // Special behavior for empty menus.
        if (mouse.active && mouse.secondary)
            next_menu = MENUSPEC_BACK;
        else if (!mouse.active && nav[NAV_BACK])
            next_menu = MENUSPEC_BACK;
        return -1;
    }

    int oldcursor = data->cursor;

    if (data->scroll.enable)
        RunMenuScroll(data);

    if (interaction_type)
        *interaction_type = INTERACT_NONE;

    if (mouse.active)
    {
        // Mouse controls are allowed to freely scroll with the mousewheel.
        if (data->scroll.enable && mouse.wheel)
        {
            // Inter and dest both move, which allows the menu to overshoot a bit.
            // This adds a bit of response when using mousewheel at the end of a menu.
            data->scroll.dest += (mouse.wheel * -10);
            data->scroll.inter += (mouse.wheel * -10);

            // Instantly lock dest to min/max
            if (data->scroll.dest < data->scroll.min)      data->scroll.dest = data->scroll.min;
            else if (data->scroll.dest > data->scroll.max) data->scroll.dest = data->scroll.max;
        }

        for (int i = 0; i < data->target_count; ++i)
        {
            const menutarget_t *target = &data->target_list[i];
            const int x = target->x;
            int y = target->y;

            if (data->scroll.enable)
            {
                y -= data->scroll.cur;
                if (y < 120) continue;
                if (y > 320) break;
            }

            if (mouse.x >= x - 5 && mouse.y >= y - 4 && mouse.y < y + 12)
            {
                data->cursor = i;
                if (mouse.primary)
                {
                    if (interaction_type)
                        *interaction_type = INTERACT_SELECT;
                    return i;
                }
                break;
            }
        }

        if (mouse.secondary)
        {
            next_menu = MENUSPEC_BACK;
            return -1;
        }
    }
    else
    {
        if (nav[NAV_UP])        data->cursor += data->target_count - 1;
        else if (nav[NAV_DOWN]) ++data->cursor;
        data->cursor %= data->target_count;

        // On a scrollable page, if the new target is either off the edge or close to it,
        // scroll the contents of the page to put it a little closer to the center.
        // We want to show at least one extra option above or below if it's available.
        if (data->scroll.enable && oldcursor != data->cursor)
        {
            const menutarget_t *target = &data->target_list[data->cursor];
            int y = target->y - data->scroll.dest;
            if (y < 140)      data->scroll.dest = target->y - 140;
            else if (y > 300) data->scroll.dest = target->y - 300;

            // Instantly lock dest to min/max
            if (data->scroll.dest < data->scroll.min)      data->scroll.dest = data->scroll.min;
            else if (data->scroll.dest > data->scroll.max) data->scroll.dest = data->scroll.max;

            // Lock inter to dest, prevent overshooting
            data->scroll.inter = data->scroll.dest;
        }
    }

    if (data->cursor != oldcursor)
        anim_step = 0;

    if (nav[NAV_BACK])
    {
        next_menu = MENUSPEC_BACK;
        return -1;
    }

    if (interaction_type)
    {
        if (nav[NAV_PRIMARY])
            *interaction_type = INTERACT_SELECT;
        else if (nav[NAV_LEFT])
            *interaction_type = INTERACT_LEFT;
        else if (nav[NAV_RIGHT])
            *interaction_type = INTERACT_RIGHT;
        else
            return -1;
        return data->cursor;
    }
    else
    {
        return (nav[NAV_PRIMARY]) ? data->cursor : -1;
    }
}

static int TextInputDrawer(int num, menudata_t *data, void *arg)
{
    const char *text = (const char *)arg;
    uint32_t border_color = 0x00606060;
    if (data->cursor == num)
        border_color = 0x000070FF + (pulsating_color << 8) + (pulsating_color << 17);

    LV_FillRect(data->layer, SCREEN_WIDTH/2 - 2, data->target_list[num].y, (SCREEN_WIDTH/2) - 32, 10, 0xA0000000);
    LV_OutlineRect(data->layer, SCREEN_WIDTH/2 - 4, data->target_list[num].y - 1, (SCREEN_WIDTH/2) - 28, 12, 1, 0x80000000 | border_color);
    LV_OutlineRect(data->layer, SCREEN_WIDTH/2 - 3, data->target_list[num].y - 2, (SCREEN_WIDTH/2) - 30, 14, 1, 0x80000000 | border_color);
    LV_OutlineRect(data->layer, SCREEN_WIDTH/2 - 3, data->target_list[num].y - 1, (SCREEN_WIDTH/2) - 30, 12, 1, 0xA0000000 | border_color);

    LV_PrintText(data->layer, SCREEN_WIDTH/2, data->target_list[num].y + 3, &small_font, text);
    if (LI_HasTextInput(text) && SDL_GetTicks() % 500 > 250)
    {
        const int width = LV_TextWidth(&small_font, text);
        LV_PrintText(data->layer, SCREEN_WIDTH/2 + width, data->target_list[num].y + 3, &small_font, "_");
    }

    if (editing_savegame && text == game_settings.slot_name) // Kinda hacky...
        LV_SetPalette(9); // Slot name input isn't allowed when editing.
    return false;
}

// ----- Main Menu ------------------------------------------------------------

static const menutarget_t MainTargets[] = {
    {60, 120, "Connect to Game"},
    {60, 140, "Load Previous Game"},
    {60, 200, "Practice"},
    {60, 220, "Launch Setup"},
    {40, 320, "Quit"},
};

static void Main_Init(menudata_t *data)
{
    data->target_count = 5;
    data->target_list = MainTargets;
}

static void Main_Draw(menudata_t *data)
{
    DrawMenuItem(data->layer, 40, 100, false, "\xF2" "Archipelago");
    DrawMenuItem(data->layer, 40, 180, false, "\xF2" "Offline");
}

static void Main_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);
    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_CONNECT; break;
    case 1: next_menu = MENU_LOAD_SAVED_GAME; break;
    case 2: next_menu = MENU_PRACTICE; break;
    case 3: next_menu = MENUSPEC_EXECUTE_SETUP; break;
    case 4: next_menu = MENUSPEC_BACK; break;
    }
}

// ----- Load Saved Game ------------------------------------------------------

menutarget_t *lsg_targets = NULL;
const ap_savesettings_t *lsg_savegame_cache = NULL;
int lsg_num_saves = 0;
char lsg_lastpath[256+1] = ""; // Used to maintain cursor position after refresh/invalidation

static void CreateSaveCache(void)
{
    lsg_savegame_cache = APDOOM_FindSaves(&lsg_num_saves);

    if (lsg_targets)
        free(lsg_targets);
    if (lsg_num_saves <= 0)
        lsg_targets = NULL;
    else
    {
        lsg_targets = calloc(lsg_num_saves, sizeof(menutarget_t));

        for (int i = 0; i < lsg_num_saves; ++i)
        {
            lsg_targets[i].x = 40;
            lsg_targets[i].y = 120 + (i * 20);
            lsg_targets[i].text = lsg_savegame_cache[i].description;
        }
    }

    invalidate_savegame_cache = false;
}

int sidebar_id = -1;
char *sidebar_text = NULL;

static void LoadSavedGame_Init(menudata_t *data)
{
    data->cursor = 0;
    data->scroll.dest = 0;

    if (invalidate_savegame_cache || !lsg_targets)
        CreateSaveCache();

    data->target_list = lsg_targets;
    data->target_count = lsg_num_saves;
    data->scroll.min = 0;
    data->scroll.max = (lsg_num_saves * 20) - 220;
    data->scroll.enable = (data->scroll.min < data->scroll.max);

    if (lsg_lastpath[0])
    {
        for (int i = 0; i < lsg_num_saves; ++i)
        {
            if (!strncmp(lsg_lastpath, lsg_savegame_cache[i].path, 256))
            {
                data->scroll.dest = (i * 20) - 80; // Show current world fifth if possible.
                data->cursor = i;
                break;
            }
        }
        if (data->scroll.dest < data->scroll.min)
            data->scroll.dest = data->scroll.min;
        else if (data->scroll.dest > data->scroll.max)
            data->scroll.dest = data->scroll.max;

        lsg_lastpath[0] = 0;
    }

    sidebar_id = -1;
}

static void LoadSavedGame_Draw(menudata_t *data)
{
    DrawHeader(data->layer, 100, "Load Previous Game");

    const uint32_t border_color = 0x000070FF + (pulsating_color << 8) + (pulsating_color << 17);
    LV_FillRect(l_primary, (SCREEN_WIDTH/4)*3 - 2, 118, (SCREEN_WIDTH/4) + 5, 214, 0xA0000000);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 4, 118 - 1, (SCREEN_WIDTH/4) + 5, 214 + 2, 1, 0x80000000 | border_color);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 3, 118 - 2, (SCREEN_WIDTH/4) + 5, 214 + 4, 1, 0x80000000 | border_color);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 3, 118 - 1, (SCREEN_WIDTH/4) + 5, 214 + 2, 1, 0xA0000000 | border_color);

    if (!lsg_targets)
    {
        const char *info_txt = "No saved games found.";
        const int center_header = LV_TextWidth(&large_font, info_txt) / 2;
        LV_SetPalette(9);
        LV_PrintText(l_primary, (SCREEN_WIDTH/2)-center_header, 140, &large_font, info_txt);
        LV_SetPalette(0);
    }
    else
    {
        if (sidebar_id != data->cursor)
        {
            char initdt[48], lastdt[48];
            if (sidebar_text)
                free(sidebar_text);

            sidebar_id = data->cursor;
            time_t init_timestamp = (time_t)lsg_savegame_cache[sidebar_id].initial_timestamp;
            time_t last_timestamp = (time_t)lsg_savegame_cache[sidebar_id].last_timestamp;
            strftime(initdt, 48, "%B %d, %Y\n  %r", localtime(&init_timestamp));
            strftime(lastdt, 48, "%B %d, %Y\n  %r", localtime(&last_timestamp));

            sidebar_text = LN_allocsprintf(
                "Game:\n"        "  \xF4%s\xF0\n\n"
                "Server:\n"      "  \xF2%s\xF0\n\n"
                "Slot Name:\n"   "  \xF2%s\xF0\n\n"
                "Started:\n"     "  %s\n\n"
                "Last Played:\n" "  %s\n\n",
                lsg_savegame_cache[sidebar_id].world->fullname,
                lsg_savegame_cache[sidebar_id].address,
                lsg_savegame_cache[sidebar_id].slot_name,
                initdt,
                lastdt);
        }

        if (sidebar_text)
            LV_PrintText(l_primary, (SCREEN_WIDTH/4)*3, 120, &small_font, sidebar_text);
    }

    LV_FormatText(l_primary, (SCREEN_WIDTH/4)*3, 325, &small_font, "%d of %d", data->cursor + 1, data->target_count);
    LV_PrintText(l_primary, (SCREEN_WIDTH/4)*3, 325-32, &small_font, "\xF9(tab)\n(esc)");
    LV_PrintText(l_primary, (SCREEN_WIDTH/4)*3 + 32, 325-32, &small_font, "More Options\nBack");
}

static void LoadSavedGame_Input(menudata_t *data)
{
    if (!lsg_targets)
    {
        // No saves found, the only valid moves are refresh and back.
        if (nav[NAV_SECONDARY])
        {
            invalidate_savegame_cache = true;
            next_menu = MENUSPEC_REINIT;
        }
        else if (mouse.active && mouse.secondary)
            next_menu = MENUSPEC_BACK;
        else if (!mouse.active && nav[NAV_BACK])
            next_menu = MENUSPEC_BACK;
        return;
    }

    if (nav[NAV_OPTIONS])
    {
        next_menu = MENU_LOAD_OPTIONS;
        return;
    }
    if (nav[NAV_SECONDARY]) // Refresh
        invalidate_savegame_cache = true;

    if (invalidate_savegame_cache)
    {
        // Refresh can also be triggered by other events
        memcpy(lsg_lastpath, lsg_savegame_cache[data->cursor].path, sizeof(lsg_lastpath));
        next_menu = MENUSPEC_REINIT;
        return;
    }

    int result = StandardMenuInput(data, NULL);
    if (result < 0)
        return;

    next_menu = MENUSPEC_EXECUTE_GAME;
    if (next_menu == MENUSPEC_EXECUTE_GAME)
        settings_to_execute = &lsg_savegame_cache[data->cursor];
}

// ----- Load Saved Game - Options Submenu ------------------------------------

const char *current_memo;
char editable_memo_buffer[64 + 1];

static const menutarget_t LoadOptionsTargets[] = {
    {40, 220, "Change Connection Info"},
    {40, 240, "Slot Memo", TextInputDrawer, &editable_memo_buffer},
    {40, 260, "Delete"},
    {40, 300, "Back"},
};

static void DeleteGameResponder(int result)
{
    if (!result)
        return;

    const int save_cursor = menus[MENU_LOAD_SAVED_GAME].data.cursor;
    APDOOM_DeleteSave(&lsg_savegame_cache[save_cursor]);
    invalidate_savegame_cache = true;
}

static void LoadOptions_Init(menudata_t *data)
{
    const int save_cursor = menus[MENU_LOAD_SAVED_GAME].data.cursor;
    current_memo = APDOOM_GetSaveMemo(&lsg_savegame_cache[save_cursor]);
    strncpy(editable_memo_buffer, current_memo, 64);
    editable_memo_buffer[64] = 0;

    data->target_count = 4;
    data->target_list = LoadOptionsTargets;
    data->layer = l_secondary;

    LV_SetBrightness(l_primary, 100, 12);

    LV_SetLayerActive(l_secondary, true);
    LV_SetBrightness(l_secondary, 128, 0);
    LV_SetBrightness(l_secondary, 255, 16);

    LV_SetLayerActive(l_bg_secondary, true);
    LV_ClearLayer(l_bg_secondary);
    LV_FillRect(l_bg_secondary, 0, 210, SCREEN_WIDTH, 110, 0xA0000000);
    LV_OutlineRect(l_bg_secondary, 0 - 2, 210 - 2, SCREEN_WIDTH + 2, 110 + 4, 2, 0x80000000);
    LV_OutlineRect(l_bg_secondary, 0 - 2, 210 - 4, SCREEN_WIDTH + 2, 110 + 8, 2, 0x60000000);
    LV_OutlineRect(l_bg_secondary, 0 - 2, 210 - 6, SCREEN_WIDTH + 2, 110 + 12, 2, 0x40000000);
}

static void LoadOptions_Draw(menudata_t *data)
{
    if (next_menu != MENU_NONE)
    {
        LV_SetLayerActive(l_bg_secondary, false);
        LV_SetLayerActive(l_secondary, false);
        return;
    }

}

static void LoadOptions_Input(menudata_t *data)
{
    const int save_cursor = menus[MENU_LOAD_SAVED_GAME].data.cursor;
    int result = StandardMenuInput(data, NULL);

    LI_SetTextInput((data->cursor == 1 ? editable_memo_buffer : NULL), 64 + 1);

    if (nav[NAV_OPTIONS]) // Pressing options/TAB again goes back.
        next_menu = MENUSPEC_BACK;

    switch (result)
    {
    default: break;
    case 0:
    {
        memcpy(&game_settings, &lsg_savegame_cache[save_cursor], sizeof(ap_savesettings_t));
        next_menu = MENU_CONNECT;
        break;
    }
    case 2:
    {
        char *warn_msg = LN_allocsprintf(
            "Are you sure you want to delete the save game \xF2%s\xF0?\n\n"
            "\xF1This operation cannot be undone!",
            lsg_savegame_cache[save_cursor].path
        );
        LN_DialogResponder(DeleteGameResponder);
        LN_OpenDialog(DIALOG_YES_NO, "Warning", warn_msg);
        free(warn_msg);
        next_menu = MENUSPEC_BACK;
        break;
    }
    case 3:
        next_menu = MENUSPEC_BACK;
        break;
    }

    if (next_menu)
    {
        if (strncmp(editable_memo_buffer, current_memo, 64))
        {
            APDOOM_SetSaveMemo(&lsg_savegame_cache[save_cursor], editable_memo_buffer);
            invalidate_savegame_cache = true;
        }
        if (next_menu > MENU_NONE)
            --menu_stack_pos; // Don't back up to this sub menu.
    }
}

// ----- Select Game ----------------------------------------------------------

static int GameActionHandler(int num, menudata_t *data, void *arg)
{
    (void)arg;
    if (!extra_world_info[num].is_functional)
        LV_SetPalette(9);
    return false;
}

static void SelectGame_Init(menudata_t *data)
{
    data->scroll.min = 0;
    data->scroll.max = (world_count * 20) - 220;
    data->scroll.enable = (data->scroll.min < data->scroll.max);

    if (!data->target_list)
    {
        // First init
        menutarget_t *newtargets = calloc(world_count, sizeof(menutarget_t));

        for (int i = 0; i < world_count; ++i)
        {
            newtargets[i].x = 40;
            newtargets[i].y = 120 + (i * 20);
            newtargets[i].text = all_worlds[i]->fullname;
            newtargets[i].draw_handler = GameActionHandler;
        }
        data->target_list = newtargets;
        data->target_count = world_count;
    }

    data->scroll.dest = 0;
    for (int i = 0; i < world_count; ++i)
    {
        if (game_settings.world == all_worlds[i])
        {
            data->scroll.dest = (i * 20) - 80; // Show current world fifth if possible.
            data->cursor = i;
            break;
        }
    }
    if (data->scroll.dest < data->scroll.min)
        data->scroll.dest = data->scroll.min;
    else if (data->scroll.dest > data->scroll.max)
        data->scroll.dest = data->scroll.max;
}

static void SelectGame_Draw(menudata_t *data)
{
    DrawHeader(data->layer, 100, "Select a Game");

    const uint32_t border_color = 0x000070FF + (pulsating_color << 8) + (pulsating_color << 17);
    LV_FillRect(l_primary, (SCREEN_WIDTH/4)*3 - 2, 118, (SCREEN_WIDTH/4) + 5, 214, 0xA0000000);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 4, 118 - 1, (SCREEN_WIDTH/4) + 5, 214 + 2, 1, 0x80000000 | border_color);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 3, 118 - 2, (SCREEN_WIDTH/4) + 5, 214 + 4, 1, 0x80000000 | border_color);
    LV_OutlineRect(l_primary, (SCREEN_WIDTH/4)*3 - 3, 118 - 1, (SCREEN_WIDTH/4) + 5, 214 + 2, 1, 0xA0000000 | border_color);

    LV_FormatText(l_primary, (SCREEN_WIDTH/4)*3, 325, &small_font, "%d of %d", data->cursor + 1, data->target_count);
}

static void SelectGame_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);
    if (result < 0)
        return;

    if (extra_world_info[result].is_functional)
    {
        game_settings.world = all_worlds[result];
        next_menu = MENUSPEC_BACK;
    }
    else if (extra_world_info[result].error_reason)
        LN_OpenDialog(DIALOG_OK, "Can't Select Game", extra_world_info[result].error_reason);
}

// ----- Practice -------------------------------------------------------------

static int DrawGameName(int num, menudata_t *data, void *arg)
{
    (void)arg;
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, "%c%s",
        (game_settings.world ? 0xF4 : 0xF9),
        (game_settings.world ? game_settings.world->fullname : "<no game selected>"));

    // Selection disabled if previous menu is the save game menu. Can't change game.
    if (editing_savegame)
        LV_SetPalette(9);
    return false;
}

static int DisableStartIfNoWorld(int num, menudata_t *data, void *arg)
{
    (void)arg;
    if (game_settings.world == NULL)
        LV_SetPalette(9);
    return false;
}

static const menutarget_t PracticeTargets[] = {
    {40, 120, "Select Game...", DrawGameName},
    {40, 240, "Start", DisableStartIfNoWorld},
    {40, 280, "Advanced Options..."},
    {40, 320, "Back"},
};

static void Practice_Init(menudata_t *data)
{
    data->target_count = 4;
    data->target_list = PracticeTargets;

    game_settings.practice_mode = true;

    game_settings.skill = 3;
    game_settings.monster_rando = 0;
    game_settings.item_rando = 0;
    game_settings.music_rando = 0;
    game_settings.flip_levels = 0;
    game_settings.reset_level = 0;

    editing_savegame = false;
}

static void Practice_Draw(menudata_t *data)
{
    DrawHeader(data->layer, 100, "Setup Practice Game");
}

static void Practice_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);
    if (result < 0)
        return;

    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_SELECT_GAME; break;
    case 1: next_menu = MENUSPEC_EXECUTE_GAME; break;
    case 2: next_menu = MENU_ADVANCED_OPTIONS; break;
    case 3: next_menu = MENUSPEC_BACK; break;
    }

    if (next_menu == MENUSPEC_EXECUTE_GAME && !game_settings.world)
        next_menu = MENU_NONE;
    if (next_menu == MENUSPEC_EXECUTE_GAME)
        settings_to_execute = &game_settings;
}

// ----- Connect --------------------------------------------------------------

static int IsReadyToConnect(void)
{
    return (game_settings.world && game_settings.slot_name[0] && game_settings.address[0]);
}

static int DisableStartIfNotReady(int num, menudata_t *data, void *arg)
{
    (void)arg;
    if (!IsReadyToConnect())
        LV_SetPalette(9);
    return false;
}

static const menutarget_t ConnectTargets[] = {
    {40, 120, "Select Game...", DrawGameName},
    {40, 160, "Slot Name", TextInputDrawer, &game_settings.slot_name},
    {40, 180, "Server Address", TextInputDrawer, &game_settings.address},
    {40, 200, "Server Password", TextInputDrawer, &game_settings.password},
    {40, 240, "Connect to Server", DisableStartIfNotReady},
    {40, 280, "Advanced Options..."},
    {40, 320, "Back"},
};

static void Connect_Init(menudata_t *data)
{
    data->target_count = 7;
    data->target_list = ConnectTargets;

    game_settings.practice_mode = false;

    editing_savegame = (menu_stack[menu_stack_pos-1] == MENU_LOAD_SAVED_GAME);
    if (editing_savegame)
    {
        // Don't set defaults, we just had them copied in from the save.
        data->cursor = 4;
    }
    else
    {
        // Set overrides to unchanged.
        game_settings.skill = -1;
        game_settings.monster_rando = -1;
        game_settings.item_rando = -1;
        game_settings.music_rando = -1;
        game_settings.flip_levels = -1;
        game_settings.reset_level = -1;
        game_settings.no_deathlink = -1;
    }
}

static void Connect_Draw(menudata_t *data)
{
    DrawHeader(data->layer, 100, "Connect to Game");
}

static void Connect_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);

    switch (data->cursor)
    {
    case 1:
        if (!editing_savegame) 
            LI_SetTextInput(game_settings.slot_name, 16 + 1);
        else
            LI_SetTextInput(NULL, 0);
        break;
    case 2:  LI_SetTextInput(game_settings.address, 128 + 1); break;
    case 3:  LI_SetTextInput(game_settings.password, 128 + 1); break;
    default: LI_SetTextInput(NULL, 0); break;
    }

    if (result < 0)
        return;

    switch (result)
    {
    default: break;
    case 0: if (!editing_savegame) { next_menu = MENU_SELECT_GAME; } break;
    case 4: next_menu = MENUSPEC_EXECUTE_GAME; break;
    case 5: next_menu = MENU_ADVANCED_OPTIONS; break;
    case 6: next_menu = MENUSPEC_BACK; break;
    }

    if (next_menu == MENUSPEC_EXECUTE_GAME && !IsReadyToConnect())
        next_menu = MENU_NONE;
    if (next_menu == MENUSPEC_EXECUTE_GAME)
        settings_to_execute = &game_settings;
}

// ----- Advanced Options -----------------------------------------------------

static int AdvOptDrawSkill(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    switch (game_settings.skill)
    {
    case 1: text = "Baby"; break;
    case 2: text = "Easy"; break;
    case 3: text = "Medium"; break;
    case 4: text = "Hard"; break;
    case 5: text = "Nightmare"; break;
    default: break;
    }
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawMapThingRando(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    const int value = (num == 1 ? game_settings.monster_rando : game_settings.item_rando);

    switch (value)
    {
    case 0: text = "Off"; break;
    case 1: text = "Shuffle"; break;
    case 2: text = "Same Type"; break;
    case 3: text = "Balanced"; break;
    case 4: text = "Chaotic"; break;
    default: break;
    }
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawMusicRando(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    switch (game_settings.music_rando)
    {
    case 0: text = "Off"; break;
    case 1: text = "Shuffle Selected"; break;
    case 2: text = "Shuffle Game"; break;
    default: break;
    }
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawFlipLevels(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    if (game_settings.world && !strcmp(game_settings.world->iwad, "HERETIC.WAD"))
    {
        LV_SetPalette(9);
        text = "\xF9<not available>";
    }
    else switch (game_settings.flip_levels)
    {
    case 0: text = "Off"; break;
    case 1: text = "On"; break;
    case 2: text = "Random Mix"; break;
    default: break;
    }
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawResetLevel(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    switch (game_settings.reset_level)
    {
    case 0: text = "Off"; break;
    case 1: text = "On"; break;
    default: break;
    }
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawDeathLink(int num, menudata_t *data, void *arg)
{
    const char *text = "\xF9<unchanged>";
    if (game_settings.practice_mode)
    {
        LV_SetPalette(9);
        text = "\xF9<not available>";
    }
    else if (game_settings.no_deathlink > 0)
        text = "Force Off";
    DrawLabel(data->layer, data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static const menutarget_t AdvancedOptsTargets[] = {
    {40, 120, "Skill",                AdvOptDrawSkill},
    {40, 140, "Random Monsters",      AdvOptDrawMapThingRando},
    {40, 160, "Random Pickups",       AdvOptDrawMapThingRando},
    {40, 180, "Random Music",         AdvOptDrawMusicRando},
    {40, 200, "Flip Levels",          AdvOptDrawFlipLevels},
    {40, 220, "Reset Level on Death", AdvOptDrawResetLevel},
    {40, 240, "DeathLink",            AdvOptDrawDeathLink},
    {40, 280, "Command Line Args.",   TextInputDrawer, &game_settings.extra_cmdline},
    {40, 320, "Back"}
};

struct {
    int *value;

    int range_min;
    int range_max;
} AdvOptValues[] = {
    {&game_settings.skill,         1, 5},
    {&game_settings.monster_rando, 0, 4},
    {&game_settings.item_rando,    0, 4},
    {&game_settings.music_rando,   0, 2},
    {&game_settings.flip_levels,   0, 2},
    {&game_settings.reset_level,   0, 1},
    {&game_settings.no_deathlink,  1, 1},
};

static void AdvancedOptions_Init(menudata_t *data)
{
    data->target_count = 9;
    data->target_list = AdvancedOptsTargets;
}

static void AdvancedOptions_Draw(menudata_t *data)
{
    DrawHeader(data->layer, 100, "Option Overrides");
}

static void AdvancedOptions_Input(menudata_t *data)
{
    int interaction_type;
    int result = StandardMenuInput(data, &interaction_type);

    LI_SetTextInput((data->cursor == 7 ? game_settings.extra_cmdline : NULL), 256 + 1);

    if (result < 0)
        return;

    switch (result)
    {
    default:
        if (interaction_type == INTERACT_LEFT)
        {
            if (*AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_max;
            else if (--*AdvOptValues[result].value < AdvOptValues[result].range_min)
                *AdvOptValues[result].value = -1;
            if (game_settings.practice_mode && *AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_max;
        }
        else
        {
            if (*AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_min;
            else if (++*AdvOptValues[result].value > AdvOptValues[result].range_max)
                *AdvOptValues[result].value = -1;
            if (game_settings.practice_mode && *AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_min;
        }
        break;
    case 7:
        break;
    case 8:
        if (interaction_type == INTERACT_SELECT)
            next_menu = MENUSPEC_BACK;
        break;
    }
}


// ============================================================================
// Main Loop
// ============================================================================

void D_Cleanup(void)
{
    for (int i = 0; all_worlds[i]; ++i)
    {
        if (extra_world_info[i].error_reason)
            free(extra_world_info[i].error_reason);
    }
    free(extra_world_info);
}

void D_DoomMain(void)
{
    I_PrintBanner("Archipelago Doom Launcher " PACKAGE_VERSION);

    // If a game is specified, go directly to the game executable
    // and pass all arguments.
    if (M_CheckParm("-game"))
    {
        int p;
        if ((p = M_CheckParmWithArgs("-game", 1)))
        {
            const ap_worldinfo_t *world = ap_get_world(myargv[p + 1]);
            if (!world)
            {
                printf("No valid apworld for the game '%s' exists.\n    Currently available games are:\n", myargv[p + 1]);
                const ap_worldinfo_t **games_list = ap_list_worlds();
                for (int i = 0; games_list[i]; ++i)
                    printf("    - '%s' -> %s\n", games_list[i]->shortname, games_list[i]->fullname);
                I_Error("Please select a valid game.");
            }
            LN_ImmediateExecute(world);
        }
        else
            I_Error("No game specified.");
        // all code paths to here cannot return
    }

    //!
    // @category launcher
    //
    // Dumps all embedded files into the current working directory.
    //
    if (M_CheckParm("-dump_embedded_files"))
    {
        APC_DumpEmbeddedFiles();
        return;
    }

    Z_Init();

    I_AtExit(D_Cleanup, true);
    TestWorldFunctionality();

    printf("Initializing assets...\n");
    APC_InitAssets();

    wad_file_t *main_wad;
    if (M_CheckParm("-dev"))
        main_wad = W_AddFile("/home/ks/Projects/APDoom/embed/BaseAssets_WIP/Launcher.wad");
    else
        main_wad = W_AddFile(":assets:/Launcher.wad");
    if (!main_wad)
    {
        printf("Couldn't load main WAD file, can't start.\n");
        return;
    }

    LV_InitVideo();
    l_bg_primary = LV_MakeLayer(true);
    l_primary = LV_MakeLayer(true);
    l_bg_secondary = LV_MakeLayer(false);
    l_secondary = LV_MakeLayer(false);
    l_dialog = LV_MakeLayer(false);
    LI_Init();

    LV_LoadFont(&small_font, "F_SML", 4, 8);
    LV_LoadFont(&large_font, "F_LRG", 7, 16);

    for (int i = MENU_MAIN; i < NUM_MENUS; ++i)
        menus[i].data.layer = l_primary;

    menus[MENU_MAIN].data.cursor = 0;
    menus[MENU_MAIN].initfunc(&menus[MENU_MAIN].data);
    anim_step = 0;

    LV_SetBrightness(l_bg_primary, 0, 0);
    LV_SetBrightness(l_bg_primary, 255, 4);

    // Temporary...
    LV_DrawPatch(l_bg_primary, 94+160, 10, W_CacheLumpName("LN_DOOM1", PU_CACHE));

    while (true)
    {
        const int cur_menu = menu_stack[menu_stack_pos];

        pulsating_color = 16 + (finesine[(int)((SDL_GetTicks() % 1000) * 8.200200020002f)] >> 12);

        LI_HandleEvents();
        LV_ClearLayer(menus[cur_menu].data.layer);

        if (dialog_open)
            LN_HandleDialog();
        else
            menus[cur_menu].inputfunc(&menus[cur_menu].data);            

        if (menus[cur_menu].data.target_list)
            StandardMenuDraw(&menus[cur_menu].data);
        if (menus[cur_menu].drawfunc)
            menus[cur_menu].drawfunc(&menus[cur_menu].data);

        if (++anim_step > 14)
            anim_step = 14;

        LV_RenderFrame();

        if (dialog_open)
            continue;

        if (next_menu) // Disable text input on any menu switch.
            LI_SetTextInput(NULL, 0);

        switch (next_menu)
        {
        case MENU_NONE:
            break;

        case MENUSPEC_EXECUTE_SETUP:
            LN_ExecuteSetup();
            break;

        case MENUSPEC_EXECUTE_GAME:
            LN_ExecuteGame(settings_to_execute);
            break;

        case MENUSPEC_BACK:
            if (menu_stack_pos <= 0)
                I_Quit();
            --menu_stack_pos;
            LV_SetBrightness(l_primary, 128, 0);
            LV_SetBrightness(l_primary, 255, 16);
            anim_step = 0;
            break;

        case MENUSPEC_REINIT:
            next_menu = menu_stack[menu_stack_pos];
            goto reinit;

        default:
            if (++menu_stack_pos >= 6)
                I_Error("Menus layered too deep!");
            menu_stack[menu_stack_pos] = next_menu;
            // fall through
        reinit:
            menus[next_menu].data.cursor = 0;
            menus[next_menu].initfunc(&menus[next_menu].data);
            LV_SetBrightness(menus[next_menu].data.layer, 128, 0);
            LV_SetBrightness(menus[next_menu].data.layer, 255, 16);
            anim_step = 0;

            // Set scrolling positions to equal destination from start
            if (menus[next_menu].data.scroll.enable)
            {
                menus[next_menu].data.scroll.cur = menus[next_menu].data.scroll.dest;
                menus[next_menu].data.scroll.inter = menus[next_menu].data.scroll.dest;
            }
            break; 
        }
        next_menu = MENU_NONE;
    }
}
