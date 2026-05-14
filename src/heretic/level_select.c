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

#include "apdoom.h"
#include "ap_msg.h"
#include "ap_lsel.h"
#include "level_select.h"

#include "doomdef.h"
#include "doomkeys.h"
#include "deh_str.h" // solely for PLAYPAL
#include "i_system.h"
#include "i_timer.h"
#include "i_video.h"
#include "m_misc.h"
#include "m_controls.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_trans.h"
#include "w_wad.h"
#include "z_zone.h"


extern boolean automapactive;

// Functions in "sb_bar.c" needed for drawing things using status bar graphics
void SB_RightAlignedSmallNum(int x, int y, int digit);
void SB_LeftAlignedSmallNum(int x, int y, int digit);

void G_DoSaveGame(void);
void set_ap_player_states(void);

int ep_anim = 0;
int urh_anim = 0;
int activating_level_select_anim = 200;

// These key graphics are added for APDoom, so we don't need to work around PU_CACHE
const char* KEY_LUMP_NAMES[] = {"LSKEY0", "LSKEY1", "LSKEY2"};


void play_level(int ep, int lvl)
{
    ap_level_index_t idx = { ep, lvl };
    ep = ap_index_to_ep(idx);
    lvl = ap_index_to_map(idx);

    // Check if level has a save file first
    char filename[260];
    snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_save_dir(), ep, lvl);
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


static void level_select_prev_episode()
{
    int new_ep = LS_PrevEpisode();
    if (new_ep == selected_ep)
        return;

    selected_ep = new_ep;
    ep_anim = -10;
    urh_anim = 0;
    S_StartSound(NULL, sfx_keyup);
}


static void level_select_next_episode()
{
    int new_ep = LS_NextEpisode();
    if (new_ep == selected_ep)
        return;

    selected_ep = new_ep;
    ep_anim = 10;
    urh_anim = 0;
    S_StartSound(NULL, sfx_keyup);
}


void select_map_dir(int dir)
{
    const ap_levelselect_t* screen_defs = LS_CurrentEpisodeInfo();

    int from = selected_level[selected_ep];
    float fromx = (float)screen_defs->map_info[from].x;
    float fromy = (float)screen_defs->map_info[from].y;

    int best = from;
    int top_most = 200;
    int top_most_idx = -1;
    int bottom_most = 0;
    int bottom_most_idx = -1;
    float best_score = 0.0f;
    
    for (int i = 0; i < screen_defs->num_map_info; ++i)
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
    if (MenuActive || askforquit) return false; // don't eat events when menu is up
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
        default:
            break;
    }

    return true;
}

void ShowLevelSelect()
{
    LS_Start();
    HU_ClearAPMessages();

    // Heretic doesn't reset the palette, we have to do it ourselves
#ifndef CRISPY_TRUECOLOR
    I_SetPalette(W_CacheLumpName(DEH_String("PLAYPAL"), PU_CACHE));
#else
    I_SetPalette(0);
#endif

    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame(); 

    if (paused)
    {
        paused = false;
        S_ResumeSound();
    }

    if (crispy->ap_levelselectmusic)
    {
        if (ap_game_info.levelsel_music_id != -1)
            S_StartSong(ap_game_info.levelsel_music_id, true);
        else
            S_StartSong(mus_intr, true);
    }
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
    ep_anim = 0;
    players[consoleplayer].centerMessage = NULL;

    // Necessary to ensure player info is correct for EnergyLink, etc
    set_ap_player_states();
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


int DrawLSPatch(const ap_levelselect_patch_t *lspatch, int x, int y)
{
    patch_t* patch = W_CacheLumpName(lspatch->graphic, PU_CACHE);
    V_DrawPatch(x + lspatch->x, y + lspatch->y, patch);
    return patch->width; // Save width if needed
}


int DrawLSText(const ap_levelselect_text_t *lstext, int x, int y)
{
    if (lstext->size == 1)
    {
        MN_DrTextB(lstext->text, x + lstext->x, y + lstext->y);
        return MN_TextBWidth(lstext->text); // Save width if needed
    }
    MN_DrTextA(lstext->text, x + lstext->x, y + lstext->y);
    return MN_TextAWidth(lstext->text); // Save width if needed
}


void DrawEpisodicLevelSelectStats()
{
    const ap_levelselect_t* screen_defs = LS_CurrentEpisodeInfo();

    for (int i = 0; i < screen_defs->num_map_info; ++i)
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

        if (mapinfo->map_name_display == LS_MAP_DISPLAY_INDIVIDUAL) // Level name display (as patch)
            map_name_width = DrawLSPatch(&mapinfo->map_name, x, y);
        if (mapinfo->map_text_display == LS_MAP_DISPLAY_INDIVIDUAL) // Level name display (as text)
            map_name_width = DrawLSText(&mapinfo->map_text, x, y);
        if (ap_level_state->completed) // Level complete splat
            DrawLSPatch(&mapinfo->complete, x, y);
        if (!ap_level_state->unlocked)
            DrawLSPatch(&mapinfo->locked, x, y);

        // Keys
        {
            key_x = x + mapinfo->keys.x + (mapinfo->keys.align_x * key_count);
            key_y = y + mapinfo->keys.y + (mapinfo->keys.align_y * key_count);
            switch (mapinfo->keys.relative_to)
            {
                default:
                    break;
                case LS_RELATIVE_IMAGE_RIGHT:
                    key_x += map_name_width;
                    // fall through
                case LS_RELATIVE_IMAGE:
                    if (mapinfo->map_text_display == LS_MAP_DISPLAY_INDIVIDUAL)
                    {
                        key_x += mapinfo->map_text.x;
                        key_y += mapinfo->map_text.y;  
                    }
                    else if (mapinfo->map_name_display == LS_MAP_DISPLAY_INDIVIDUAL)
                    {
                        key_x += mapinfo->map_name.x;
                        key_y += mapinfo->map_name.y;
                    }
                    break;
            }

            for (int k = 0; k < 3; ++k)
            {
                if (!ap_level_info->keys[k])
                    continue;

                V_DrawPatch(key_x, key_y, W_CacheLumpName("LSKEYBG", PU_CACHE));
                if (ap_level_state->keys[k])
                    V_DrawPatch(key_x, key_y, W_CacheLumpName(KEY_LUMP_NAMES[k], PU_CACHE));

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
                case LS_RELATIVE_IMAGE_RIGHT:
                    progress_x += map_name_width;
                    // fall through
                case LS_RELATIVE_IMAGE:
                    if (mapinfo->map_text_display == LS_MAP_DISPLAY_INDIVIDUAL)
                    {
                        progress_x += mapinfo->map_text.x;
                        progress_y += mapinfo->map_text.y;  
                    }
                    else if (mapinfo->map_name_display == LS_MAP_DISPLAY_INDIVIDUAL)
                    {
                        progress_x += mapinfo->map_name.x;
                        progress_y += mapinfo->map_name.y;
                    }
                    break;
                case LS_RELATIVE_KEYS:
                    progress_x += mapinfo->keys.x;
                    progress_y += mapinfo->keys.y;
                    break;
                case LS_RELATIVE_KEYS_LAST:
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
        if (mapinfo->map_name_display > LS_MAP_DISPLAY_NONE)
        {
            patch_t *patch = W_CacheLumpName(mapinfo->map_name.graphic, PU_CACHE);
            const int x = (ORIGWIDTH - patch->width) / 2;
            const int y = (mapinfo->map_text_display == LS_MAP_DISPLAY_UPPER) ? 2 : (ORIGHEIGHT - patch->height) - 2;
            DrawLSPatch(&mapinfo->map_name, x, y);
        }
        if (mapinfo->map_text_display > LS_MAP_DISPLAY_NONE)
        {
            int x, y;
            if (mapinfo->map_text.size == 0)
            {
                x = (ORIGWIDTH - MN_TextAWidth(mapinfo->map_text.text)) / 2;
                y = (mapinfo->map_text_display == LS_MAP_DISPLAY_UPPER) ? 2 : (ORIGHEIGHT - 10) - 2;
            }
            else
            {
                x = (ORIGWIDTH - MN_TextBWidth(mapinfo->map_text.text)) / 2;
                y = (mapinfo->map_text_display == LS_MAP_DISPLAY_UPPER) ? 2 : ORIGHEIGHT - 20;
            }
            DrawLSText(&mapinfo->map_text, x, y);
        }

        // You are here
        if (urh_anim < 25)
            DrawLSPatch(&mapinfo->cursor, mapinfo->x, mapinfo->y);
    }

    for (int i = 0; i < screen_defs->num_patches; ++i)
        DrawLSPatch(&screen_defs->patches[i], 0, 0);

    for (int i = 0; i < screen_defs->num_text; ++i)
        DrawLSText(&screen_defs->text[i], 0, 0);
}


void DrawLevelSelectStats()
{
    DrawEpisodicLevelSelectStats();
}


void DrawLevelSelect()
{
    patch_t *primary_image = W_CacheLumpName(LS_CurrentEpisodeInfo()->background_image, PU_CACHE);

    // Just in case, always fill with black, then draw the current selected episode background
    // But for Heretic we need to not do this until the intro anim is over
    if (!activating_level_select_anim)
        V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);

    V_DrawPatch(ep_anim * 32, activating_level_select_anim, primary_image);

    if (ep_anim == 0)
    {
        // We may have room to draw the images for previous and next episodes...
        if (SCREENWIDTH != NONWIDEWIDTH)
        {
            patch_t *left_image = W_CacheLumpName(LS_PrevEpisodeInfo()->background_image, PU_CACHE);
            patch_t *right_image = W_CacheLumpName(LS_NextEpisodeInfo()->background_image, PU_CACHE);

            dp_translation = cr[CR_DARK];
            V_DrawPatch(-320, activating_level_select_anim, left_image);
            V_DrawPatch(320, activating_level_select_anim, right_image);
            dp_translation = NULL;
        }
        V_DrawPatch(0, activating_level_select_anim, primary_image);

        if (!activating_level_select_anim)
            DrawLevelSelectStats();
    }
    else if (ep_anim > 0)
    {
        patch_t *secondary_image = W_CacheLumpName(LS_PrevEpisodeInfo()->background_image, PU_CACHE);
        V_DrawPatch(-(10 - ep_anim) * 32, 0, secondary_image);
    }
    else // ep_anim < 0
    {
        patch_t *secondary_image = W_CacheLumpName(LS_NextEpisodeInfo()->background_image, PU_CACHE);
        V_DrawPatch((10 + ep_anim) * 32, 0, secondary_image);
    }
}
