//
// Copyright(C) 2023 David St-Louis
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
// *Level select feature for archipelago*
//

#include "level_select.h"
#include "doomdef.h"
#include "s_sound.h"
#include "z_zone.h"
#include "v_video.h"
#include "doomkeys.h"
#include "apdoom.h"
#include "i_video.h"
#include "m_misc.h"
#include "ap_msg.h"
#include "m_controls.h"
#include "i_timer.h"

extern boolean automapactive;


typedef struct
{
    int x;
    int y;
    char* urhere_lump_name;
    int urhere_x_offset;
    int urhere_y_offset;
} level_pos_t;


typedef struct
{
    int x, y;
    int right_align;
} legende_t;


static legende_t legendes[5] = {
    {320, 40, 1},
    {0, 150, 0},
    {0, 200 - 8 * 3, 0},
    {0, 200 - 8 * 3, 0},
    {0, 200 - 8 * 3, 0}
};



const char* level_names[5][9] = {
    {
        "THE DOCKS - E1M1",
        "THE DUNGEONS - E1M2",
        "THE GATEHOUSE - E1M3",
        "THE GUARD TOWER - E1M4",
        "THE CITADEL - E1M5",
        "THE CATHEDRAL - E1M6",
        "THE CRYPTS - E1M7",
        "HELL'S MAW - E1M8",
        "THE GRAVEYARD - E1M9"
    },
    {
        "THE CRATER - E2M1",
        "THE LAVA PITS - E2M2",
        "THE RIVER OF FIRE - E2M3",
        "THE ICE GROTTO - E2M4",
        "THE CATACOMBS - E2M5",
        "THE LABYRINTH - E2M6",
        "THE GREAT HALL - E2M7",
        "THE PORTALS OF CHAOS - E2M8",
        "THE GLACIER - E2M9"
    },
    {
        "THE STOREHOUSE - E3M1",
        "THE CESSPOOL - E3M2",
        "THE CONFLUENCE - E3M3",
        "THE AZURE FORTRESS - E3M4",
        "THE OPHIDIAN LAIR - E3M5",
        "THE HALLS OF FEAR - E3M6",
        "THE CHASM - E3M7",
        "D'SPARIL'S KEEP - E3M8",
        "THE AQUIFIER - E3M9"
    },
    {
        "CATAFALQUE - E4M1",
        "BLOCKHOUSE - E4M2",
        "AMBULATORY - E4M3",
        "SEPULCHER - E4M4",
        "GREAT STAIR - E4M5",
        "HALLS OF THE APOSTATE - E4M6",
        "RAMPARTS OF PERDITION - E4M7",
        "SHATTERED BRIDGE - E4M8",
        "MAUSOLEUM - E4M9"
    },
    {
        "OCHRE CLIFFS - E5M1",
        "RAPIDS - E5M2",
        "QUAY - E5M3",
        "COURTYARD - E5M4",
        "HYDRATYR - E5M5",
        "COLONNADE - E5M6",
        "FOETID MANSE - E5M7",
        "FIELD OF JUDGEMENT - E5M8",
        "SKEIN OF D'SPARIL - E5M9"
    }
};


extern int cursor_x;
extern int cursor_y;

static level_pos_t level_pos_infos[5][9] =
{
    // Episode 1
    {
        { 172, 76, "IN_YAH", 0, 0 },
	    { 87, 93, "IN_YAH", 0, 0 },
	    { 73, 66, "IN_YAH", 0, 0 },
	    { 155, 100, "IN_YAH", 0, 0 },
	    { 147, 127, "IN_YAH", 0, 0 },
	    { 126, 51, "IN_YAH", 0, 0 },
	    { 127, 82, "IN_YAH", 0, 0 },
	    { 207, 138, "IN_YAH", 0, 0 },
	    { 53, 104, "IN_YAH", 0, 0 }
    },
    
    // Episode 2
    {
        { 218, 57, "IN_YAH", 0, 0 },
        { 137, 76, "IN_YAH", 0, 0 }, // 2
        { 155, 124, "IN_YAH", 0, 0 },
        { 171, 68, "IN_YAH", 0, 0 },
        { 250, 86, "IN_YAH", 0, 0 },
        { 136, 103, "IN_YAH", 0, 0 }, // 6
        { 203, 90, "IN_YAH", 0, 0 },
        { 220, 140, "IN_YAH", 0, 0 },
        { 279, 106, "IN_YAH", 0, 0 }
    },

    // Episode 3
    {
        { 86, 99, "IN_YAH", 0, 0 },
        { 124, 103, "IN_YAH", 0, 0 },
        { 154, 79, "IN_YAH", 0, 0 },
        { 202, 83, "IN_YAH", 0, 0 },
        { 178, 59, "IN_YAH", 0, 0 },
        { 142, 58, "IN_YAH", 0, 0 },
        { 219, 66, "IN_YAH", 0, 0 },
        { 247, 57, "IN_YAH", 0, 0 },
        { 107, 80, "IN_YAH", 0, 0 }
    },

    // Episode 4
    {
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 }
    },

    // Episode 5
    {
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 },
        { 0, 0, "IN_YAH", 0, 0 }
    }
};


int selected_level[4] = {0};
int selected_ep = 0;
int prev_ep = 0;
int ep_anim = 0;
int urh_anim = 0;



static const char* YELLOW_DIGIT_LUMP_NAMES[] = {
    "SMALLIN0", "SMALLIN1", "SMALLIN2", "SMALLIN3", "SMALLIN4", 
    "SMALLIN5", "SMALLIN6", "SMALLIN7", "SMALLIN8", "SMALLIN9"
};


void print_right_aligned_yellow_digit(int x, int y, int digit)
{
    x -= 4;

    if (!digit)
    {
        V_DrawPatch(x, y, W_CacheLumpName(YELLOW_DIGIT_LUMP_NAMES[0], PU_CACHE));
        return;
    }

    while (digit)
    {
        int i = digit % 10;
        V_DrawPatch(x, y, W_CacheLumpName(YELLOW_DIGIT_LUMP_NAMES[i], PU_CACHE));
        x -= 4;
        digit /= 10;
    }
}


void print_left_aligned_yellow_digit(int x, int y, int digit)
{
    if (!digit)
    {
        x += 4;
    }

    int len = 0;
    int d = digit;
    while (d)
    {
        len++;
        d /= 10;
    }
    print_right_aligned_yellow_digit(x + len * 4, y, digit);
}


void play_level(int ep, int lvl)
{
    // Check if level has a save file first
    char filename[260];
    snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_seed(), ep + 1, lvl + 1);
    if (M_FileExists(filename))
    {
        // We load
        extern char savename[256];
        snprintf(savename, 256, "%s", filename);
        gameaction = ga_loadgame;
        //G_DoLoadGame();
    }
    else
    {
        // If none, load it fresh
        G_DeferedInitNew(gameskill, ep + 1, lvl + 1);
    }

    //apdoom_check_victory(); // In case we had pending victory

    HU_ClearAPMessages();
}


static int get_episode_count()
{
    int ep_count = 0;
    if (gamemode != commercial)
        for (int i = 0; i < ap_episode_count; ++i)
            if (ap_state.episodes[i])
                ep_count++;
    return ep_count;
}


static void level_select_prev_episode()
{
    if (gamemode != shareware && get_episode_count() > 1)
    {
        prev_ep = selected_ep;
        ep_anim = -10;
        selected_ep--;
        if (selected_ep < 0) selected_ep = ap_episode_count - 1;
        while (!ap_state.episodes[selected_ep])
        {
            selected_ep--;
            if (selected_ep < 0) selected_ep = ap_episode_count - 1;
            if (selected_ep == prev_ep) // oops;
                break;
        }
        urh_anim = 0;
        S_StartSound(NULL, sfx_keyup);
    }
}


static void level_select_next_episode()
{
    if (gamemode != shareware && get_episode_count() > 1)
    {
        prev_ep = selected_ep;
        ep_anim = 10;
        selected_ep = (selected_ep + 1) % ap_episode_count;
        while (!ap_state.episodes[selected_ep])
        {
            selected_ep = (selected_ep + 1) % ap_episode_count;
            if (selected_ep == prev_ep) // oops;
                break;
        }
        urh_anim = 0;
        S_StartSound(NULL, sfx_keyup);
    }
}


void select_map_dir(int dir)
{
    int from = selected_level[selected_ep];
    float fromx = (float)level_pos_infos[selected_ep][from].x;
    float fromy = (float)level_pos_infos[selected_ep][from].y;

    int best = from;
    float best_score = 0.0f;

    for (int i = 0; i < ap_map_count; ++i)
    {
        if (i == from) continue;

        float tox = (float)level_pos_infos[selected_ep][i].x;
        float toy = (float)level_pos_infos[selected_ep][i].y;
        float score = 0.0f;
        float dist = 10000.0f;

        switch (dir)
        {
            case 0: // Left
                if (tox >= fromx) continue;
                dist = fromx - tox;
                break;
            case 1: // Right
                if (tox <= fromx) continue;
                dist = tox - fromx;
                break;
            case 2: // Up
                if (toy >= fromy) continue;
                dist = fromy - toy;
                break;
            case 3: // Down
                if (toy <= fromy) continue;
                dist = toy - fromy;
                break;
        }
        score = 1.0f / dist;

        if (score > best_score)
        {
            best_score = score;
            best = i;
        }
    }

    if (best != from)
    {
        urh_anim = 0;
        S_StartSound(NULL, sfx_keyup);
        selected_level[selected_ep] = best;
    }
}


static void level_select_nav_enter()
{
    if (ap_get_level_state(selected_ep + 1, selected_level[selected_ep] + 1)->unlocked)
    {
        S_StartSound(NULL, sfx_dorcls);
        play_level(selected_ep, selected_level[selected_ep]);
    }
    else
    {
        S_StartSound(NULL, sfx_artiuse);
    }
}


boolean LevelSelectResponder(event_t* ev)
{
    if (ep_anim) return true;

    int ep_count = 0;
    if (gamemode != commercial)
        for (int i = 0; i < ap_episode_count; ++i)
            if (ap_state.episodes[i])
                ep_count++;

    switch (ev->type)
    {
        case ev_joystick:
        {
            if (ev->data4 < 0 || ev->data2 < 0)
            {
                select_map_dir(0);
                joywait = I_GetTime() + 5;
            }
            else if (ev->data4 > 0 || ev->data2 > 0)
            {
                select_map_dir(1);
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 < 0)
            {
                select_map_dir(2);
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                select_map_dir(3);
                joywait = I_GetTime() + 5;
            }

#define JOY_BUTTON_MAPPED(x) ((x) >= 0)
#define JOY_BUTTON_PRESSED(x) (JOY_BUTTON_MAPPED(x) && (ev->data1 & (1 << (x))) != 0)

            if (JOY_BUTTON_PRESSED(joybfire)) level_select_nav_enter();

            if (JOY_BUTTON_PRESSED(joybprevweapon)) level_select_prev_episode();
            else if (JOY_BUTTON_PRESSED(joybnextweapon)) level_select_next_episode();

            break;
        }
        case ev_keydown:
        {
            switch (ev->data1)
            {
                case KEY_LEFTARROW:
                case 'a':
                    select_map_dir(0);
                    break;
                case KEY_RIGHTARROW:
                case 'd':
                    select_map_dir(1);
                    break;
                case KEY_UPARROW:
                case 'w':
                    select_map_dir(2);
                    break;
                case KEY_DOWNARROW:
                case 's':
                    select_map_dir(3);
                    break;
                case '[':
                    level_select_prev_episode();
                    break;
                case ']':
                    level_select_next_episode();
                    break;
                case KEY_ENTER:
                case 'e':
                    level_select_nav_enter();
                    break;
            }
            break;
        }
    }

    return true;
}

void G_DoSaveGame(void);

void ShowLevelSelect()
{
    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame(); 

    S_StartSong(mus_intr, true);

    gameaction = ga_nothing;
    gamestate = GS_LEVEL_SELECT;
    viewactive = false;
    automapactive = false;

    ap_state.ep = 0;
    ap_state.map = 0;
    players[consoleplayer].centerMessage = NULL;

    if (gamemode == commercial)
    {
        selected_ep = 0;
    }
    else
    {
        while (!ap_state.episodes[selected_ep])
        {
            selected_ep = (selected_ep + 1) % ap_episode_count;
            if (selected_ep == 0) // oops;
                break;
        }
    }
}


void TickLevelSelect()
{
    if (ep_anim > 0)
        ep_anim -= 1;
    else if (ep_anim < 0)
        ep_anim += 1;
    urh_anim = (urh_anim + 1) % 35;
}


void draw_legend_line_right_aligned(const char* text, int x, int y)
{
    int w = MN_TextAWidth_len(text, strlen(text));
    MN_DrTextA(text, x - w, y);
}


void draw_legend_line(const char* text, int x, int y)
{
    MN_DrTextA(text, x, y);
}


void DrawEpisodicLevelSelectStats()
{
    int x, y;
    const int key_spacing = 5;
    const int stat_y_offset = 8;

    for (int i = 0; i < ap_map_count; ++i)
    {
        level_pos_t* level_pos = &level_pos_infos[selected_ep][i];
        ap_level_info_t* ap_level_info = ap_get_level_info(selected_ep + 1, i + 1);
        ap_level_state_t* ap_level_state = ap_get_level_state(selected_ep + 1, i + 1);

        x = level_pos->x;
        y = level_pos->y;

        int key_count = 0;
        for (int i = 0; i < 3; ++i)
            if (ap_level_info->keys[i])
                key_count++;

        const int key_start_offset = -key_spacing * key_count / 2 + (3 - key_count) * 2;
        
        // Level complete splash
        if (ap_level_state->completed)
            V_DrawPatch(x, y, W_CacheLumpName("IN_X", PU_CACHE));

        // Lock
        if (!ap_level_state->unlocked)
            V_DrawPatch(x, y, W_CacheLumpName("WILOCK", PU_CACHE));

        // Keys
        const char* key_lump_names[] = {"SELKEYY", "SELKEYG", "SELKEYB"};
        int key_y = y + key_start_offset - 1;
        int key_x = x + 9;
        for (int k = 0; k < 3; ++k)
        {
            if (ap_level_info->keys[k])
            {
                const char* key_lump_name = key_lump_names[k];
                V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
                if (ap_level_state->keys[k])
                    V_DrawPatch(key_x, key_y, W_CacheLumpName(key_lump_name, PU_CACHE));
                key_y += key_spacing;
            }
        }

        // Progress
        print_right_aligned_yellow_digit(x - 4, y + stat_y_offset, ap_level_state->check_count);
        V_DrawPatch(x - 3, y + stat_y_offset, W_CacheLumpName("STYSLASH", PU_CACHE));
        print_left_aligned_yellow_digit(x + 3, y + stat_y_offset, ap_level_info->check_count);

        // "You are here"
        if (i == selected_level[selected_ep] && urh_anim < 25)
        {
            int x_offset = 2;
            int y_offset = -2;
            V_DrawPatch(x + x_offset + level_pos->urhere_x_offset, 
                        y + y_offset + level_pos->urhere_y_offset, 
                        W_CacheLumpName(level_pos->urhere_lump_name, PU_CACHE));
        }
    }

    // Level name
    const char* level_name = level_names[selected_ep][selected_level[selected_ep]];
    int text_x = 160 - MN_TextBWidth(level_name) / 2;
    int text_y = 200 - 20;
    MN_DrTextB(level_name, text_x, text_y);

    // Legend
    int lx = legendes[selected_ep].x;
    int ly = legendes[selected_ep].y;

    typedef void (*draw_legend_line_fn_t)(const char* text, int x, int y);
    draw_legend_line_fn_t draw_legend_line_fn = draw_legend_line;
    if (legendes[selected_ep].right_align) draw_legend_line_fn = draw_legend_line_right_aligned;
    draw_legend_line_fn("~2Change map: ~3Arrows", lx, ly);
    draw_legend_line_fn("~2Change episode: ~3(~2, ~3)", lx, ly + 8);
    draw_legend_line_fn("~2Enter map: ~3Enter", lx, ly + 16);
}


void DrawLevelSelectStats()
{
    DrawEpisodicLevelSelectStats();
}


static const char* WIN_MAPS[5] = {
    "MAPE1",
    "MAPE2",
    "MAPE3",
    "AUTOPAGE",
    "AUTOPAGE"
};


const char* get_win_map(int ep)
{
    return WIN_MAPS[ep];
}


void DrawLevelSelect()
{
    int x_offset = ep_anim * 32;

    char lump_name[9];
    snprintf(lump_name, 9, "%s", get_win_map(selected_ep));
    
    // [crispy] fill pillarboxes in widescreen mode
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
    }

    V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    if (ep_anim == 0)
    {
        DrawLevelSelectStats();
    }
    else
    {
        snprintf(lump_name, 9, "%s", get_win_map(prev_ep));
        if (ep_anim > 0)
            x_offset = -(10 - ep_anim) * 32;
        else
            x_offset = (10 + ep_anim) * 32;
        V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    }
}
