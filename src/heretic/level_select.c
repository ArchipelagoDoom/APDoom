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

// Functions in "sb_bar.c" needed for drawing things using status bar graphics
void SB_RightAlignedSmallNum(int x, int y, int digit);
void SB_LeftAlignedSmallNum(int x, int y, int digit);

int selected_level[6] = {0};
int selected_ep = 0;
int prev_ep = 0;
int ep_anim = 0;
int urh_anim = 0;
int activating_level_select_anim = 200;

// These key graphics are added for APDoom, so we don't need to work around PU_CACHE
const char* KEY_LUMP_NAMES[] = {"SELKEYY", "SELKEYG", "SELKEYB"};

void play_level(int ep, int lvl)
{
    ap_level_index_t idx = { ep, lvl };
    ep = ap_index_to_ep(idx);
    lvl = ap_index_to_map(idx);

    // Check if level has a save file first
    char filename[260];
    snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_seed(), ep, lvl);
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
        G_DeferedInitNew(gameskill, ep, lvl);
    }
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
    const ap_levelselect_t* screen_defs = ap_get_level_select_info(selected_ep);

    int from = selected_level[selected_ep];
    float fromx = (float)screen_defs->map_info[from].x;
    float fromy = (float)screen_defs->map_info[from].y;

    int best = from;
    int top_most = 200;
    int top_most_idx = -1;
    int bottom_most = 0;
    int bottom_most_idx = -1;
    float best_score = 0.0f;
    
    int map_count = ap_get_map_count(selected_ep + 1);
    for (int i = 0; i < map_count; ++i)
    {
        if (screen_defs->map_info[i].y < top_most)
        {
            top_most = screen_defs->map_info[i].y;
            top_most_idx = i;
        }
        if (screen_defs->map_info[i].y > bottom_most)
        {
            bottom_most = screen_defs->map_info[i].y;
            bottom_most_idx = i;
        }
        if (i == from) continue;

        float tox = (float)screen_defs->map_info[i].x;
        float toy = (float)screen_defs->map_info[i].y;
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

    // Are we at the top? go to the bottom
    if (from == top_most_idx && dir == 2)
    {
        best = bottom_most_idx;
    }
    else if (from == bottom_most_idx && dir == 3)
    {
        best = top_most_idx;
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
    if (ap_get_level_state(ap_make_level_index(selected_ep + 1, selected_level[selected_ep] + 1))->unlocked)
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
    if (activating_level_select_anim) return true;
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
                //select_map_dir(2);
                level_select_prev_episode();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                //select_map_dir(3);
                joywait = I_GetTime() + 5;
                level_select_next_episode();
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
            if (ev->data1 == key_left || ev->data1 == key_alt_strafeleft || ev->data1 == key_strafeleft) level_select_prev_episode();
            if (ev->data1 == key_right || ev->data1 == key_alt_straferight || ev->data1 == key_straferight) level_select_next_episode();
            if (ev->data1 == key_up || ev->data1 == key_alt_up) select_map_dir(2);
            if (ev->data1 == key_down || ev->data1 == key_alt_down) select_map_dir(3);
            if (ev->data1 == key_menu_forward || ev->data1 == key_use) level_select_nav_enter();
            break;
        }
    }

    return true;
}

void G_DoSaveGame(void);

void ShowLevelSelect()
{
    HU_ClearAPMessages();

    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame(); 

    if (crispy->ap_levelselectmusic)
        S_StartSong(mus_intr, true);
    else
    {
        extern int mus_song;
        mus_song = -1;
        I_StopSong();
    }

    gameaction = ga_nothing;
    gamestate = GS_LEVEL_SELECT;
    viewactive = false;
    automapactive = false;

    activating_level_select_anim = 200;
    ap_state.ep = 0;
    ap_state.map = 0;
    ep_anim = 0;
    players[consoleplayer].centerMessage = NULL;

    while (!ap_state.episodes[selected_ep])
    {
        selected_ep = (selected_ep + 1) % ap_episode_count;
        if (selected_ep == 0) // oops;
            break;
    }
}


void TickLevelSelect()
{
    if (activating_level_select_anim > 0)
    {
        activating_level_select_anim -= 6;
        if (activating_level_select_anim < 0)
            activating_level_select_anim = 0;
        else
            return;
    }
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
    const ap_levelselect_t* screen_defs = ap_get_level_select_info(selected_ep);
    const int map_count = ap_get_map_count(selected_ep + 1);

    for (int i = 0; i < map_count; ++i)
    {
        ap_level_index_t idx = {selected_ep, i};
        ap_level_info_t* ap_level_info = ap_get_level_info(idx);
        ap_level_state_t* ap_level_state = ap_get_level_state(idx);

        const ap_levelselect_map_t* mapinfo = &screen_defs->map_info[i];
        const int x = mapinfo->x;
        const int y = mapinfo->y;

        int key_x, key_y;
        int map_name_width = 0;
        int key_count = 0;

        for (int i = 0; i < 3; ++i)
            if (ap_level_info->keys[i])
                key_count++;

        // Level name display ("Individual" mode)
        if (screen_defs->map_names == 0 && mapinfo->map_name.text)
        {
            MN_DrTextB(mapinfo->map_name.text, x + mapinfo->map_name.x, y + mapinfo->map_name.y);
            map_name_width = MN_TextBWidth(mapinfo->map_name.text); // Store for later
        }

        // Level complete splash
        if (ap_level_state->completed)
            V_DrawPatch(x, y, W_CacheLumpName("IN_X", PU_CACHE));

        // Lock
        if (!ap_level_state->unlocked)
            V_DrawPatch(x, y, W_CacheLumpName("WILOCK", PU_CACHE));

        // Keys
        {
            key_x = x + mapinfo->keys.x + (mapinfo->keys.align_x * key_count);
            key_y = y + mapinfo->keys.y + (mapinfo->keys.align_y * key_count);
            switch (mapinfo->keys.relative_to)
            {
                default:
                    break;
                case 2:
                    key_x += map_name_width;
                    // fall through
                case 1:
                    key_x += mapinfo->map_name.x;
                    key_y += mapinfo->map_name.y;
                    break;
            }

            for (int k = 0; k < 3; ++k)
            {
                const char* key_lump_name = KEY_LUMP_NAMES[k];
                if (!ap_level_info->keys[k])
                    continue;

                V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
                if (ap_level_state->keys[k])
                    V_DrawPatch(key_x, key_y, W_CacheLumpName(key_lump_name, PU_CACHE));                

                key_x += mapinfo->keys.spacing_x;
                key_y += mapinfo->keys.spacing_y;
            }
        }

        // Progress
        {
            int progress_x = x + mapinfo->checks.x;
            int progress_y = y + mapinfo->checks.y;
            switch (mapinfo->checks.relative_to)
            {
                default:
                    break;
                case 2:
                    progress_x += map_name_width;
                    // fall through
                case 1:
                    progress_x += mapinfo->map_name.x;
                    progress_y += mapinfo->map_name.y;
                    break;
                case 3:
                    progress_x += mapinfo->keys.x;
                    progress_y += mapinfo->keys.y;
                    break;
                case 4:
                    progress_x = key_x + mapinfo->checks.x;
                    progress_y = key_y + mapinfo->checks.y;
                    break;
            }
            SB_RightAlignedSmallNum(progress_x, progress_y, ap_level_state->check_count);
            V_DrawPatch(progress_x + 1, progress_y, W_CacheLumpName("STYSLASH", PU_CACHE));
            SB_LeftAlignedSmallNum(progress_x + 7, progress_y, ap_total_check_count(ap_level_info));
        }
    }

    // Stuff relating only to selected level
    {
        const int sel_idx = selected_level[selected_ep];
        const ap_levelselect_map_t* mapinfo = &screen_defs->map_info[sel_idx];

        // Level name (non-"Individual" modes)
        if (screen_defs->map_names != 0 && mapinfo->map_name.text)
        {
            const int x = (ORIGWIDTH - MN_TextBWidth(mapinfo->map_name.text)) / 2;
            const int y = (screen_defs->map_names < 0) ? 2 : ORIGHEIGHT - 20;

            MN_DrTextB(mapinfo->map_name.text, x, y);
        }

        // You are here
        if (urh_anim < 25)
        {
            V_DrawPatch(mapinfo->x + mapinfo->cursor.x, 
                        mapinfo->y + mapinfo->cursor.y, 
                        W_CacheLumpName(mapinfo->cursor.graphic, PU_CACHE));
        }
    }
}


void DrawLevelSelectStats()
{
    DrawEpisodicLevelSelectStats();
}


void DrawLevelSelect()
{
    int x_offset = ep_anim * 32;

    char lump_name[9];
    snprintf(lump_name, 9, "%s", ap_get_level_select_info(selected_ep)->background_image);
    
    // [crispy] fill pillarboxes in widescreen mode
    if (SCREENWIDTH != NONWIDEWIDTH)
    {
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
    }

    V_DrawPatch(x_offset, activating_level_select_anim, W_CacheLumpName(lump_name, PU_CACHE));
    if (ep_anim == 0)
    {
        if (activating_level_select_anim == 0)
            DrawLevelSelectStats();
    }
    else
    {
        snprintf(lump_name, 9, "%s", ap_get_level_select_info(prev_ep)->background_image);
        if (ep_anim > 0)
            x_offset = -(10 - ep_anim) * 32;
        else
            x_offset = (10 + ep_anim) * 32;
        V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    }
}
