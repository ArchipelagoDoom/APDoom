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
#include "doomstat.h"
#include "d_main.h"
#include "s_sound.h"
#include "z_zone.h"
#include "v_video.h"
#include "d_player.h"
#include "doomkeys.h"
#include "apdoom.h"
#include "i_video.h"
#include "g_game.h"
#include "m_misc.h"
#include "hu_lib.h"
#include "hu_stuff.h"
#include <math.h>
#include "m_controls.h"


void WI_initAnimatedBack(void);
void WI_updateAnimatedBack(void);
void WI_drawAnimatedBack(void);
void WI_initVariables(wbstartstruct_t* wbstartstruct);
void WI_loadData(void);

void G_DoSaveGame(void);

// Functions in "st_stuff.c" needed for drawing things using status bar graphics
void ST_DrawKey(int x, int y, int which, boolean is_skull);
void ST_RightAlignedShortNum(int x, int y, int digit);
void ST_LeftAlignedShortNum(int x, int y, int digit);

static wbstartstruct_t wiinfo;

extern int bcnt;

int selected_level[6] = {0};
int selected_ep = 0;
int prev_ep = 0;
int ep_anim = 0;
int urh_anim = 0;

void restart_wi_anims()
{
    wiinfo.epsd = selected_ep;
    WI_initVariables(&wiinfo);
    WI_loadData();
    WI_initAnimatedBack();
}

static int get_episode_count()
{
    int ep_count = 0;
    for (int i = 0; i < ap_episode_count; ++i)
        if (ap_state.episodes[i])
            ep_count++;
    return ep_count;
}

void HU_ClearAPMessages();

void play_level(int ep, int lvl)
{
    ap_level_index_t idx = { ep, lvl };
    ep = ap_index_to_ep(idx);
    lvl = ap_index_to_map(idx);

    // Check if level has a save file first
    char filename[260];
    if (gamemode != commercial)
        snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_seed(), ep, lvl);
    else
        snprintf(filename, 260, "%s/save_MAP%02i.dsg", apdoom_get_seed(), lvl);

    if (M_FileExists(filename))
    {
        // We load
        extern char savename[256];
        snprintf(savename, 256, "%s", filename);
        ap_state.player_state.powers[pw_strength] = 0;
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
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_stnmov);
        selected_level[selected_ep] = best;
    }
}


static void level_select_nav_left()
{
    select_map_dir(0);
}


static void level_select_nav_right()
{
    select_map_dir(1);
}


static void level_select_nav_up()
{
    select_map_dir(2);
}


static void level_select_nav_down()
{
    select_map_dir(3);
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
        restart_wi_anims();
        urh_anim = 0;
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
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
        restart_wi_anims();
        urh_anim = 0;
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
    }
}


static void level_select_nav_enter()
{
    ap_level_index_t idx = {selected_ep, selected_level[selected_ep]};
    if (ap_get_level_state(idx)->unlocked)
    {
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_swtchn);
        play_level(selected_ep, selected_level[selected_ep]);
    }
    else
    {
        S_StartSoundOptional(NULL, sfx_mnusli, sfx_noway);
    }
}


boolean LevelSelectResponder(event_t* ev)
{
    if (ep_anim) return true;

    //int ep_count = get_episode_count();

    switch (ev->type)
    {
        case ev_joystick:
        {
            if (ev->data4 < 0 || ev->data2 < 0)
            {
                level_select_nav_left();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data4 > 0 || ev->data2 > 0)
            {
                level_select_nav_right();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 < 0)
            {
                level_select_nav_up();
                joywait = I_GetTime() + 5;
            }
            else if (ev->data3 > 0)
            {
                level_select_nav_down();
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
            if (ev->data1 == key_left || ev->data1 == key_alt_strafeleft || ev->data1 == key_strafeleft) level_select_prev_episode();
            if (ev->data1 == key_right || ev->data1 == key_alt_straferight || ev->data1 == key_straferight) level_select_next_episode();
            if (ev->data1 == key_up || ev->data1 == key_alt_up) level_select_nav_up();
            if (ev->data1 == key_down || ev->data1 == key_alt_down) level_select_nav_down();
            if (ev->data1 == key_menu_forward || ev->data1 == key_use) level_select_nav_enter();
            break;
        }
    }

    return true;
}


void ShowLevelSelect()
{
    HU_ClearAPMessages();

    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame(); 

    if (crispy->ap_levelselectmusic)
        S_ChangeMusic(mus_read_m, true);
    else
        S_StopMusic();

    gameaction = ga_nothing;
    gamestate = GS_LEVEL_SELECT;
    viewactive = false;
    automapactive = false;

    ap_state.ep = 0;
    ap_state.map = 0;

    while (!ap_state.episodes[selected_ep])
    {
        selected_ep = (selected_ep + 1) % ap_episode_count;
        if (selected_ep == 0) // oops;
            break;
    }

    wiinfo.epsd = selected_ep;
    wiinfo.didsecret = false;
    wiinfo.last = -1;
    wiinfo.next = -1;
    wiinfo.maxkills = 0;
    wiinfo.maxitems = 0;
    wiinfo.maxsecret = 0;
    wiinfo.maxfrags = 0;
    wiinfo.partime = 0;
    wiinfo.pnum = 0;
    
    restart_wi_anims();
    bcnt = 0;
}


void TickLevelSelect()
{
    if (ep_anim > 0)
        ep_anim -= 1;
    else if (ep_anim < 0)
        ep_anim += 1;
    bcnt++;
    urh_anim = (urh_anim + 1) % 35;
    WI_updateAnimatedBack();
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
        if (screen_defs->map_names == 0 && mapinfo->map_name.graphic[0])
        {
            patch_t* patch = W_CacheLumpName(mapinfo->map_name.graphic, PU_CACHE);
            V_DrawPatch(x + mapinfo->map_name.x, y + mapinfo->map_name.y, patch);
            map_name_width = patch->width; // Store for later
        }
        
        // Level complete splash
        if (ap_level_state->completed)
            V_DrawPatch(x, y, W_CacheLumpName("WISPLAT", PU_CACHE));

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
                if (!ap_level_info->keys[k])
                    continue;

                V_DrawPatch(key_x, key_y, W_CacheLumpName("KEYBG", PU_CACHE));
                if (mapinfo->keys.use_checkmark)
                {
                    const int checkmark_x = key_x + mapinfo->keys.checkmark_x;
                    const int checkmark_y = key_y + mapinfo->keys.checkmark_y;

                    ST_DrawKey(key_x, key_y, k, ap_level_info->use_skull[k]);
                    if (ap_level_state->keys[k])
                        V_DrawPatch(checkmark_x, checkmark_y, W_CacheLumpName("CHECKMRK", PU_CACHE));
                }
                else
                {
                    if (ap_level_state->keys[k])
                        ST_DrawKey(key_x, key_y, k, ap_level_info->use_skull[k]);
                }

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
            ST_RightAlignedShortNum(progress_x, progress_y, ap_level_state->check_count);
            V_DrawPatch(progress_x + 1, progress_y, W_CacheLumpName("STYSLASH", PU_CACHE));
            ST_LeftAlignedShortNum(progress_x + 8, progress_y, ap_total_check_count(ap_level_info));
        }

        // You are here
        if (i == selected_level[selected_ep] && urh_anim < 25)
        {
            int offset_y = 0;

            if (strncmp(mapinfo->cursor.graphic, "WIURH", 5) == 0)
            {
                // I don't feel like de-hackifying this. It would make the level select json much more complex
                const int key_align = mapinfo->keys.align_y * key_count;
                switch (mapinfo->cursor.graphic[5])
                {
                    case '0': offset_y = (mapinfo->keys.x > 0) ? key_align : 0; break;
                    case '1': offset_y = (mapinfo->keys.x < 0) ? key_align : 0; break;
                    default:  break;
                }
            }

            V_DrawPatch(x + mapinfo->cursor.x, 
                        y + offset_y + mapinfo->cursor.y, 
                        W_CacheLumpName(mapinfo->cursor.graphic, PU_CACHE));
        }
    }

    // Level name (non-"Individual" modes)
    if (screen_defs->map_names != 0)
    {
        const int sel_idx = selected_level[selected_ep];
        const ap_levelselect_map_t* mapinfo = &screen_defs->map_info[sel_idx];

        if (mapinfo->map_name.graphic[0])
        {
            patch_t *patch = W_CacheLumpName(mapinfo->map_name.graphic, PU_CACHE);
            const int x = (ORIGWIDTH - patch->width) / 2;
            const int y = (screen_defs->map_names < 0) ? 2 : (ORIGHEIGHT - patch->height) - 2;

            V_DrawPatch(x, y, patch);
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

    V_DrawPatch(x_offset, 0, W_CacheLumpName(lump_name, PU_CACHE));
    if (ep_anim == 0)
    {
        WI_drawAnimatedBack();

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
