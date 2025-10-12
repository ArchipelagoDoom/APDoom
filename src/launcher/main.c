
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
    MENU_PRACTICE,
    NUM_MENUS
} menulist_t;

typedef enum {
    ACTION_DRAW_DISABLED_CHECK,
    ACTION_SELECT_DISABLED_CHECK,
    NUM_ACTIONS
} actiontype_t;

struct menudata_s;

typedef struct {
    int x;
    int y;
    const char *text;

    int (*action_handler)(int num, actiontype_t action, struct menudata_s *data);
} menutarget_t;

typedef struct menudata_s {
    int cursor;

    int target_count;
    const menutarget_t *target_list;

    int extra_data[3];
    const void *extra_ptr[3];
} menudata_t;

static void Main_Init(menudata_t *data);
static void Main_Draw(menudata_t *data);
static void Main_Input(menudata_t *data);

static void SelectGame_Init(menudata_t *data);
static void SelectGame_Draw(menudata_t *data);
static void SelectGame_Input(menudata_t *data);

static void Practice_Init(menudata_t *data);
static void Practice_Draw(menudata_t *data);
static void Practice_Input(menudata_t *data);

struct {
    void (*initfunc)(menudata_t *data);
    void (*drawfunc)(menudata_t *data);
    void (*inputfunc)(menudata_t *data);

    menudata_t data;
} menus[] = {
    {NULL, NULL, NULL},
    {Main_Init, Main_Draw, Main_Input},
    {SelectGame_Init, SelectGame_Draw, SelectGame_Input},
    {Practice_Init, Practice_Draw, Practice_Input},
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

void DrawMenuItem(int x, int y, int disabled, int selected, const char *fmt, ...)
{
    if (selected)
    {
        int h = anim_bg_fade[anim_step];
        int slow_fade = 16 + (finesine[(int)((SDL_GetTicks() % 1000) * 8.200200020002f)] >> 12);
        uint32_t color = 0x000070FF + (slow_fade << 8) + (slow_fade << 17);

        LV_FillRect(l_primary, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 0x60000000 | color);
        LV_OutlineRect(l_primary, -1, y + 5 - (h/2), SCREEN_WIDTH+2, h, 1, 0x40000000 | color);        

        x += anim_text_move[anim_step];
    }

    va_list args;

    va_start(args, fmt);
    char *str = LN_allocvsprintf(fmt, args);
    va_end(args);

    LV_SetPalette(disabled ? 9 : 0);
    LV_PrintText(l_primary, x, y, &large_font, str);
    LV_SetPalette(0);

    free(str);
}

void StandardMenuDraw(menudata_t *data)
{
    for (int i = 0; i < data->target_count; ++i)
    {
        const menutarget_t *target = &data->target_list[i];
        int disabled = false;
        if (target->action_handler)
            disabled = target->action_handler(i, ACTION_DRAW_DISABLED_CHECK, data);
        DrawMenuItem(target->x, target->y, disabled, data->cursor == i, target->text);
    }
}

int StandardMenuInput(menudata_t *data)
{
    int oldcursor = data->cursor;

    if (mouse.active)
    {
        for (int i = 0; i < data->target_count; ++i)
        {
            const menutarget_t *target = &data->target_list[i];

            if (mouse.x > target->x - 5 && mouse.y > target->y - 4 && mouse.y < target->y + 12)
            {
                data->cursor = i;
                if (mouse.primary)
                {
                    if (!(target->action_handler
                        && target->action_handler(i, ACTION_SELECT_DISABLED_CHECK, data)))
                    {
                        return i;
                    }
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

    if (nav[NAV_PRIMARY])
    {
        const menutarget_t *target = &data->target_list[data->cursor];
        if (!(target->action_handler
            && target->action_handler(data->cursor, ACTION_SELECT_DISABLED_CHECK, data)))
        {
            return data->cursor;
        }
    }
    else if (nav[NAV_BACK])
        next_menu = MENU_BACK;
    return -1;
}

// ----- Main Menu ------------------------------------------------------------

static int LPG_Disabled(int num, actiontype_t action, menudata_t *data)
{
    return true;
}

static const menutarget_t MainTargets[] = {
    {60, 120, "Connect to Game"},
    {60, 140, "Load Previous Game", LPG_Disabled},
    {60, 200, "Practice"},
    {60, 220, "Launch Setup"},
    {40, 280, "Quit"},
};

static void Main_Init(menudata_t *data)
{
    data->cursor = 0;
    data->target_count = 5;
    data->target_list = MainTargets;
}

static void Main_Draw(menudata_t *data)
{
    DrawMenuItem(40, 100, false, false, "\xF2" "Archipelago");
    DrawMenuItem(40, 180, false, false, "\xF2" "Offline");
}

static void Main_Input(menudata_t *data)
{
    int result = StandardMenuInput(data);
    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_SELECT_GAME; break;
    case 2: next_menu = MENU_PRACTICE; break;
    case 3: next_menu = MENU_EXECSETUP; break;
    case 4: next_menu = MENU_BACK; break;
    }
}

// ----- Select Game ----------------------------------------------------------

static int GameActionHandler(int num, actiontype_t action, menudata_t *data)
{
    if (action != ACTION_DRAW_DISABLED_CHECK)
        return 0;

    return !extra_world_info[num].is_functional;
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
            newtargets[i].action_handler = GameActionHandler;
        }
        data->target_list = newtargets;
        data->target_count = world_count;
    }

    data->cursor = 0;
}

static void SelectGame_Draw(menudata_t *data)
{
    DrawHeader(100, "Select a Game");
}

static void SelectGame_Input(menudata_t *data)
{
    int result = StandardMenuInput(data);
    if (result < 0)
        return;

    if (extra_world_info[result].is_functional)
    {
        //menus[menu_stack[menu_stack_pos - 1]].data.extra_ptr[0] = all_worlds[result];
        world_to_exec = all_worlds[result];
        next_menu = MENU_BACK;
    }
    else if (extra_world_info[result].error_reason)
        LN_OpenDialog(DIALOG_OK, "Can't Select Game", extra_world_info[result].error_reason);
}

// ----- Setup Game (generic) -------------------------------------------------

static int StartActionHandler(int num, actiontype_t action, menudata_t *data)
{
    if (action == ACTION_DRAW_DISABLED_CHECK || action == ACTION_SELECT_DISABLED_CHECK)
        return (world_to_exec == NULL);
    return 0;
}

static const menutarget_t PracticeTargets[] = {
    {40, 120, "Select Game..."},
//    {40, 160, "Slot Name"},
//    {40, 180, "Server Address"},
//    {40, 200, "Server Password"},
    {40, 240, "Advanced Options..."},
    {40, 280, "Start", StartActionHandler}
};

static void Practice_Init(menudata_t *data)
{
    exec_settings.practice_mode = true;
    data->target_count = 3;
    data->target_list = PracticeTargets;

    data->cursor = (!world_to_exec) ? 0 : 2;
}

static void Practice_Draw(menudata_t *data)
{
    DrawHeader(100, "Setup Practice Game");

    const char *name;
    if (!world_to_exec)
    {
        name = "<no game selected>";
        LV_SetPalette(9);
    }
    else
    {
        name = world_to_exec->fullname;
        LV_SetPalette(4);
    }

    const int name_width = LV_TextWidth(&large_font, name);
    LV_PrintText(l_primary, (SCREEN_WIDTH-40)-name_width, 120, &large_font, name);

    LV_SetPalette(0);
}

static void Practice_Input(menudata_t *data)
{
    int result = StandardMenuInput(data);
    if (result < 0)
        return;

    switch (result)
    {
    default: break;
    case 0: next_menu = MENU_SELECT_GAME; break;
    case 2: next_menu = MENU_EXECGAME; break;
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

    menus[MENU_MAIN].initfunc(&menus[MENU_MAIN].data);
    anim_step = 0;

    LV_SetBrightness(l_background, 0, 0);
    LV_SetBrightness(l_background, 255, 4);

    // Temporary...
    LV_DrawPatch(l_background, 94+160, 2, W_CacheLumpName("LN_DOOM1", PU_CACHE));

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
            menus[next_menu].initfunc(&menus[next_menu].data);
            LV_SetBrightness(l_primary, 128, 0);
            LV_SetBrightness(l_primary, 255, 16);
            anim_step = 0;
            break; 
        }
        next_menu = MENU_NONE;
    }
}
