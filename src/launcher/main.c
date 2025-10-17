
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

layer_t *l_primary;
layer_t *l_background;
layer_t *l_dialog;

font_t large_font;
font_t small_font;

const ap_worldinfo_t *world_to_exec;

// ----------------------------------------------------------------------------

typedef struct {
    bool is_functional;
    char *error_reason;
} extrainfo_t;

int world_count;
static const ap_worldinfo_t **all_worlds = NULL;
extrainfo_t *extra_world_info = NULL;

bool TestIWAD(const char *iwad, char **error_str)
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
                "For newer rereleases, you want to use the IWAD file that is in the /base/ directory.";
        }
        *error_str = LN_allocsprintf("The IWAD for this game, \xF2%s\xF0, could not be found.%s",
            iwad, descriptive_text);
        return false;
    }
    return true;
}

bool TestPWAD(const char **wad_list, char **error_str)
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

// ----------------------------------------------------------------------------

typedef enum {
    MENU_EXECSETUP = -3,
    MENU_EXECGAME = -2,
    MENU_BACK = -1,
    MENU_NONE = 0,
    MENU_MAIN,
    MENU_SELECT_GAME,
    MENU_CONNECT,
    MENU_PRACTICE,
    MENU_ADVANCED_OPTIONS,
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
    int (*draw_handler)(int num, struct menudata_s *data);
} menutarget_t;

typedef struct menudata_s {
    int cursor;

    int target_count;
    const menutarget_t *target_list;
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

struct {
    void (*initfunc)(menudata_t *data);
    void (*drawfunc)(menudata_t *data);
    void (*inputfunc)(menudata_t *data);

    menudata_t data;
} menus[] = {
    {NULL, NULL, NULL},
    {Main_Init, Main_Draw, Main_Input},
    {SelectGame_Init, SelectGame_Draw, SelectGame_Input},
    {Connect_Init, Connect_Draw, Connect_Input},
    {Practice_Init, Practice_Draw, Practice_Input},
    {AdvancedOptions_Init, AdvancedOptions_Draw, AdvancedOptions_Input},
};

static int menu_stack_pos = 0;
static menulist_t menu_stack[6] = {MENU_MAIN};
static menulist_t next_menu = MENU_NONE;

// ----------------------------------------------------------------------------

int anim_text_move[15] = { 5, 10, 12, 14, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15};
int anim_bg_fade[15] =   { 2,  4,  6,  8,  9, 10, 11, 12, 13, 14, 14, 15, 15, 16, 16};
byte anim_step = 0;

void DrawHeader(int y, const char *txt)
{
    const int center_header = LV_TextWidth(&large_font, txt) / 2;
    LV_SetPalette(2);
    LV_PrintText(l_primary, (SCREEN_WIDTH/2)-center_header, y, &large_font, txt);
    LV_SetPalette(0);
}

void DrawMenuItem(int x, int y, int selected, const char *fmt, ...)
{
    if (selected)
        x += anim_text_move[anim_step];

    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    LV_PrintText(l_primary, x, y, &large_font, str);

    free(str);
}

void DrawLabel(int x, int y, const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    const int width = LV_TextWidth(&large_font, str);
    LV_PrintText(l_primary, (SCREEN_WIDTH-x)-width, y, &large_font, str);

    free(str);
}

void StandardMenuDraw(menudata_t *data)
{
    int pal = LV_GetPalette();
    for (int i = 0; i < data->target_count; ++i)
    {
        const menutarget_t *target = &data->target_list[i];

        if (data->cursor == i)
        {
            const int y = target->y;
            const int h = anim_bg_fade[anim_step];
            int slow_fade = 16 + (finesine[(int)((SDL_GetTicks() % 1000) * 8.200200020002f)] >> 12);
            uint32_t color = 0x000070FF + (slow_fade << 8) + (slow_fade << 17);

            LV_FillRect(l_primary, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 0x60000000 | color);
            LV_OutlineRect(l_primary, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 1, 0x40000000 | color);
        }

        if (target->draw_handler && target->draw_handler(i, data))
            continue;
        DrawMenuItem(target->x, target->y, data->cursor == i, target->text);
        LV_SetPalette(pal); // Restore palette, as draw handler may change it
    }
}

int StandardMenuInput(menudata_t *data, int *interaction_type)
{
    int oldcursor = data->cursor;

    if (interaction_type)
        *interaction_type = INTERACT_NONE;

    if (mouse.active)
    {
        for (int i = 0; i < data->target_count; ++i)
        {
            const menutarget_t *target = &data->target_list[i];

            if (mouse.x >= target->x - 5 && mouse.y >= target->y - 4 && mouse.y < target->y + 12)
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
            next_menu = MENU_BACK;
            return -1;
        }
    }
    else
    {
        if (nav[NAV_UP])        data->cursor += data->target_count - 1;
        else if (nav[NAV_DOWN]) ++data->cursor;
        data->cursor %= data->target_count;
    }

    if (data->cursor != oldcursor)
        anim_step = 0;

    if (nav[NAV_BACK])
    {
        next_menu = MENU_BACK;
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

static int TextInputDrawer(int num, menudata_t *data)
{
    const char *text;
    // TODO HACK
    switch (num)
    {
    case 1: text = exec_settings.slot_name; break;
    case 2: text = exec_settings.address; break;
    case 3: text = exec_settings.password; break;
    default: text = exec_settings.extra_cmdline; break;
    }

    LV_PrintText(l_primary, SCREEN_WIDTH/2, data->target_list[num].y + 3, &small_font, text);
    if (data->cursor == num && SDL_GetTicks() % 500 > 250)
    {
        const int width = LV_TextWidth(&small_font, text);
        LV_PrintText(l_primary, SCREEN_WIDTH/2 + width, data->target_list[num].y + 3, &small_font, "_");
    }
    return false;
}

// ----- Main Menu ------------------------------------------------------------

static int DrawDisabled(int num, menudata_t *data)
{
    LV_SetPalette(9);
    return 0;
}

static const menutarget_t MainTargets[] = {
    {60, 120, "Connect to Game"},
    {60, 140, "Load Previous Game", DrawDisabled},
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
    DrawMenuItem(40, 100, false, "\xF2" "Archipelago");
    DrawMenuItem(40, 180, false, "\xF2" "Offline");
}

static void Main_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);
    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_CONNECT; break;
    case 2: next_menu = MENU_PRACTICE; break;
    case 3: next_menu = MENU_EXECSETUP; break;
    case 4: next_menu = MENU_BACK; break;
    }
}

// ----- Select Game ----------------------------------------------------------

static int GameActionHandler(int num, menudata_t *data)
{
    if (!extra_world_info[num].is_functional)
        LV_SetPalette(9);
    return false;
}

static void SelectGame_Init(menudata_t *data)
{
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

    for (int i = 0; i < world_count; ++i)
    {
        if (world_to_exec == all_worlds[i])
            data->cursor = i;
    }
}

static void SelectGame_Draw(menudata_t *data)
{
    DrawHeader(100, "Select a Game");
}

static void SelectGame_Input(menudata_t *data)
{
    int result = StandardMenuInput(data, NULL);
    if (result < 0)
        return;

    if (extra_world_info[result].is_functional)
    {
        world_to_exec = all_worlds[result];
        next_menu = MENU_BACK;
    }
    else if (extra_world_info[result].error_reason)
        LN_OpenDialog(DIALOG_OK, "Can't Select Game", extra_world_info[result].error_reason);
}

// ----- Practice -------------------------------------------------------------

static int DrawGameName(int num, menudata_t *data)
{
    DrawLabel(data->target_list[num].x, data->target_list[num].y, "%c%s",
        (world_to_exec ? 0xF4 : 0xF9),
        (world_to_exec ? world_to_exec->fullname : "<no game selected>"));
    return false;
}

static int DisableStartIfNoWorld(int num, menudata_t *data)
{
    if (world_to_exec == NULL)
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

    exec_settings.practice_mode = true;

    exec_settings.skill = 3;
    exec_settings.monster_rando = 0;
    exec_settings.item_rando = 0;
    exec_settings.music_rando = 0;
    exec_settings.flip_levels = 0;
    exec_settings.reset_level = 0;
}

static void Practice_Draw(menudata_t *data)
{
    DrawHeader(100, "Setup Practice Game");
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
    case 1: next_menu = MENU_EXECGAME; break;
    case 2: next_menu = MENU_ADVANCED_OPTIONS; break;
    case 3: next_menu = MENU_BACK; break;
    }

    if (next_menu == MENU_EXECGAME && !world_to_exec)
        next_menu = MENU_NONE;
}

// ----- Connect --------------------------------------------------------------

static int IsReadyToConnect(void)
{
    return (world_to_exec && exec_settings.slot_name[0] && exec_settings.address[0]);
}

static int DisableStartIfNotReady(int num, menudata_t *data)
{
    if (!IsReadyToConnect())
        LV_SetPalette(9);
    return false;
}

static const menutarget_t ConnectTargets[] = {
    {40, 120, "Select Game...", DrawGameName},
    {40, 160, "Slot Name", TextInputDrawer},
    {40, 180, "Server Address", TextInputDrawer},
    {40, 200, "Server Password", TextInputDrawer},
    {40, 240, "Connect to Server", DisableStartIfNotReady},
    {40, 280, "Advanced Options..."},
    {40, 320, "Back"},
};

static void Connect_Init(menudata_t *data)
{
    data->target_count = 7;
    data->target_list = ConnectTargets;

    exec_settings.practice_mode = false;

    exec_settings.skill = -1;
    exec_settings.monster_rando = -1;
    exec_settings.item_rando = -1;
    exec_settings.music_rando = -1;
    exec_settings.flip_levels = -1;
    exec_settings.reset_level = -1;
    exec_settings.no_deathlink = -1;
}

static void Connect_Draw(menudata_t *data)
{
    DrawHeader(100, "Connect to Game");
}

static void Connect_Input(menudata_t *data)
{
    switch (data->cursor)
    {
    case 1:  LI_SetTextInput(exec_settings.slot_name, 16 + 1); break;
    case 2:  LI_SetTextInput(exec_settings.address, 128 + 1); break;
    case 3:  LI_SetTextInput(exec_settings.password, 128 + 1); break;
    default: LI_SetTextInput(NULL, 0); break;
    }

    int result = StandardMenuInput(data, NULL);
    if (result < 0)
        return;

    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_SELECT_GAME; break;
    case 4: next_menu = MENU_EXECGAME; break;
    case 5: next_menu = MENU_ADVANCED_OPTIONS; break;
    case 6: next_menu = MENU_BACK; break;
    }

    if (next_menu == MENU_EXECGAME && !IsReadyToConnect())
        next_menu = MENU_NONE;
}

// ----- Advanced Options -----

static int AdvOptDrawSkill(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    switch (exec_settings.skill)
    {
    case 1: text = "Baby"; break;
    case 2: text = "Easy"; break;
    case 3: text = "Medium"; break;
    case 4: text = "Hard"; break;
    case 5: text = "Nightmare"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawMonsterRando(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    switch (exec_settings.monster_rando)
    {
    case 0: text = "Off"; break;
    case 1: text = "Shuffle"; break;
    case 2: text = "Random Balanced"; break;
    case 3: text = "Random Chaotic"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawItemRando(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    switch (exec_settings.item_rando)
    {
    case 0: text = "Off"; break;
    case 1: text = "Shuffle"; break;
    case 2: text = "Random Balanced"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawMusicRando(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    switch (exec_settings.music_rando)
    {
    case 0: text = "Off"; break;
    case 1: text = "Shuffle Selected"; break;
    case 2: text = "Shuffle Game"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawFlipLevels(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    if (world_to_exec && !strcmp(world_to_exec->iwad, "HERETIC.WAD"))
    {
        LV_SetPalette(9);
        text = "\xF9<not available>";
    }
    else switch (exec_settings.flip_levels)
    {
    case 0: text = "Off"; break;
    case 1: text = "On"; break;
    case 2: text = "Random Mix"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawResetLevel(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    switch (exec_settings.reset_level)
    {
    case 0: text = "Off"; break;
    case 1: text = "On"; break;
    default: break;
    }
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static int AdvOptDrawDeathLink(int num, menudata_t *data)
{
    const char *text = "\xF9<unchanged>";
    if (exec_settings.practice_mode)
    {
        LV_SetPalette(9);
        text = "\xF9<not available>";
    }
    else if (exec_settings.no_deathlink > 0)
        text = "Force Off";
    DrawLabel(data->target_list[num].x, data->target_list[num].y, text);
    return false;
}

static const menutarget_t AdvancedOptsTargets[] = {
    {40, 120, "Skill",                AdvOptDrawSkill},
    {40, 140, "Random Monsters",      AdvOptDrawMonsterRando},
    {40, 160, "Random Pickups",       AdvOptDrawItemRando},
    {40, 180, "Random Music",         AdvOptDrawMusicRando},
    {40, 200, "Flip Levels",          AdvOptDrawFlipLevels},
    {40, 220, "Reset Level on Death", AdvOptDrawResetLevel},
    {40, 240, "DeathLink",            AdvOptDrawDeathLink},
    {40, 280, "Command Line Args.",   TextInputDrawer},
    {40, 320, "Back"}
};

struct {
    int *value;

    int range_min;
    int range_max;
} AdvOptValues[] = {
    {&exec_settings.skill,         1, 5},
    {&exec_settings.monster_rando, 0, 3},
    {&exec_settings.item_rando,    0, 2},
    {&exec_settings.music_rando,   0, 2},
    {&exec_settings.flip_levels,   0, 2},
    {&exec_settings.reset_level,   0, 1},
    {&exec_settings.no_deathlink,  1, 1},
};

static void AdvancedOptions_Init(menudata_t *data)
{
    data->target_count = 9;
    data->target_list = AdvancedOptsTargets;
}

static void AdvancedOptions_Draw(menudata_t *data)
{
    DrawHeader(100, "Option Overrides");
}

static void AdvancedOptions_Input(menudata_t *data)
{
    LI_SetTextInput((data->cursor == 7 ? exec_settings.extra_cmdline : NULL), 256 + 1);
    int interaction_type;
    int result = StandardMenuInput(data, &interaction_type);
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
            if (exec_settings.practice_mode && *AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_max;
        }
        else
        {
            if (*AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_min;
            else if (++*AdvOptValues[result].value > AdvOptValues[result].range_max)
                *AdvOptValues[result].value = -1;
            if (exec_settings.practice_mode && *AdvOptValues[result].value == -1)
                *AdvOptValues[result].value = AdvOptValues[result].range_min;
        }
        break;
    case 7:
        break;
    case 8:
        if (interaction_type == INTERACT_SELECT)
            next_menu = MENU_BACK;
        break;
    }
}

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
    l_background = LV_MakeLayer();
    l_primary = LV_MakeLayer();
    l_dialog = LV_MakeLayer();
    LI_Init();

    LV_LoadFont(&small_font, "F_SML", 4, 8);
    LV_LoadFont(&large_font, "F_LRG", 7, 16);

    menus[MENU_MAIN].data.cursor = 0;
    menus[MENU_MAIN].initfunc(&menus[MENU_MAIN].data);
    anim_step = 0;

    LV_SetBrightness(l_background, 0, 0);
    LV_SetBrightness(l_background, 255, 4);

    // Temporary...
    LV_DrawPatch(l_background, 94+160, 10, W_CacheLumpName("LN_DOOM1", PU_CACHE));

    while (true)
    {
        const int cur_menu = menu_stack[menu_stack_pos];

        LI_HandleEvents();
        LV_ClearLayer(l_primary);

        if (dialog_open)
            LN_HandleDialog();
        else
            menus[cur_menu].inputfunc(&menus[cur_menu].data);            

//        if (mouse.active)
//            LV_FormatText(l_primary, 0, 10, &small_font, "%d %d", mouse.x, mouse.y);

        if (menus[cur_menu].data.target_list)
            StandardMenuDraw(&menus[cur_menu].data);
        if (menus[cur_menu].drawfunc)
            menus[cur_menu].drawfunc(&menus[cur_menu].data);

        if (++anim_step > 14)
            anim_step = 14;

        LV_RenderFrame();

        if (dialog_open)
            continue;

        switch (next_menu)
        {
        case MENU_NONE:
            break;
        case MENU_EXECSETUP:
            LN_ExecuteSetup();
            break;
        case MENU_EXECGAME:
            LN_ExecuteWorld(world_to_exec);
            break;
        case MENU_BACK:
            if (menu_stack_pos <= 0)
                I_Quit();
            --menu_stack_pos;
            LV_SetBrightness(l_primary, 128, 0);
            LV_SetBrightness(l_primary, 255, 16);
            anim_step = 0;
            break;
        default:
            if (++menu_stack_pos >= 6)
                I_Error("Menus layered too deep!");
            menu_stack[menu_stack_pos] = next_menu;
            menus[next_menu].data.cursor = 0;
            menus[next_menu].initfunc(&menus[next_menu].data);
            LV_SetBrightness(l_primary, 128, 0);
            LV_SetBrightness(l_primary, 255, 16);
            anim_step = 0;
            break; 
        }
        next_menu = MENU_NONE;
    }
}
