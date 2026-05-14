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
#include "ap_lsel.h"
#include "level_select.h"

#include "doomdef.h"
#include "doomkeys.h"
#include "doomstat.h"
#include "i_system.h"
#include "i_video.h"
#include "d_main.h"
#include "d_player.h"
#include "g_game.h"
#include "m_misc.h"
#include "m_controls.h"
#include "hu_stuff.h"
#include "hu_lib.h"
#include "s_sound.h"
#include "v_video.h"
#include "v_trans.h"
#include "w_wad.h"
#include "z_zone.h"


void WI_initAnimatedBack(void);
void WI_updateAnimatedBack(void);
void WI_drawAnimatedBack(void);
void WI_initVariables(wbstartstruct_t* wbstartstruct);
void WI_loadData(void);

void G_DoSaveGame(void);
void set_ap_player_states(void);

// Functions in "st_stuff.c" needed for drawing things using status bar graphics
void ST_DrawKey(int x, int y, int which);
void ST_RightAlignedShortNum(int x, int y, int digit);
void ST_LeftAlignedShortNum(int x, int y, int digit);

int M_StringWidth(const char *string);

extern int bcnt;

static int wi_animnum;
int ep_anim = 0;
int initial_delay = 0;
int urh_anim = 0;

// These key graphics are added for APDoom, so we don't need to work around PU_CACHE
const char* KEY_LUMP_NAMES[] = {"LSKEY0", "LSKEY1", "LSKEY2", "LSKEY3", "LSKEY4", "LSKEY5"};


static void restart_wi_anims(void)
{
    static wbstartstruct_t wi_info;
    memset(&wi_info, 0, sizeof(wbstartstruct_t));

    wi_animnum = 0;
    const char *bg = LS_CurrentEpisodeInfo()->background_image;
    if (strncmp(bg, "WIMAP", 5) != 0 || (bg[5] - '0') < 0)
        return; // Not a wi anim background
    wi_animnum = (bg[5] - '0') + 1;

    wi_info.epsd = wi_animnum - 1;
    wi_info.next = (wi_animnum == 2 ? 5 : 1);
    wi_info.last = -1;
    WI_initVariables(&wi_info);
    WI_loadData();
    WI_initAnimatedBack();
}


void play_level(int ep, int lvl)
{
    ap_level_index_t idx = { ep, lvl };
    ep = ap_index_to_ep(idx);
    lvl = ap_index_to_map(idx);

    // Check if level has a save file first
    char filename[260];
    if (gamemode != commercial)
        snprintf(filename, 260, "%s/save_E%iM%i.dsg", apdoom_get_save_dir(), ep, lvl);
    else
        snprintf(filename, 260, "%s/save_MAP%02i.dsg", apdoom_get_save_dir(), lvl);

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
    int new_ep = LS_PrevEpisode();
    if (new_ep == selected_ep)
        return;

    selected_ep = new_ep;
    ep_anim = -10;
    urh_anim = 0;
    restart_wi_anims();
    S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
}


static void level_select_next_episode()
{
    int new_ep = LS_NextEpisode();
    if (new_ep == selected_ep)
        return;

    selected_ep = new_ep;
    ep_anim = 10;
    urh_anim = 0;
    restart_wi_anims();
    S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
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
        S_StartSound(NULL, sfx_noway);
    }
}


boolean LevelSelectResponder(event_t* ev)
{
    if (menuactive) return false; // don't eat events when menu is up
    if (ep_anim || initial_delay) return true;

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
        default:
            break;
    }

    return true;
}


void ShowLevelSelect()
{
    LS_Start();
    HU_ClearAPMessages();

    // Level Select removes Berserk power from the player.
    ap_state.player_state.powers[pw_strength] = 0;
    players[consoleplayer].powers[pw_strength] = 0;

    // If in a level, save current level
    if (gamestate == GS_LEVEL)
        G_DoSaveGame();

    if (crispy->ap_levelselectmusic)
    {
        if (ap_game_info.levelsel_music_id != -1)
            S_ChangeMusic(ap_game_info.levelsel_music_id, true);
        else
            S_ChangeMusic((gamemode == commercial) ? mus_read_m : mus_inter, true);
    }
    else
        S_StopMusic();

    gameaction = ga_nothing;
    gamestate = GS_LEVEL_SELECT;
    viewactive = false;
    automapactive = false;
    
    restart_wi_anims();
    bcnt = 0;
    ep_anim = 0;
    initial_delay = 2; // Just a couple frames of no input to prevent accidental movement

    // Necessary to ensure player info is correct for EnergyLink, etc
    set_ap_player_states();
}


void TickLevelSelect()
{
    if (initial_delay)    --initial_delay;
    if (ep_anim > 0)      --ep_anim;
    else if (ep_anim < 0) ++ep_anim;
    bcnt++;
    urh_anim = (urh_anim + 1) % 35;

    if (wi_animnum != 0)
        WI_updateAnimatedBack();
}


int DrawLSPatch(const ap_levelselect_patch_t *lspatch, int x, int y)
{
    patch_t* patch = W_CacheLumpName(lspatch->graphic, PU_CACHE);
    V_DrawPatch(x + lspatch->x, y + lspatch->y, patch);
    return patch->width; // Save width if needed
}


int DrawLSText(const ap_levelselect_text_t *lstext, int x, int y)
{
    hu_forced_color = true; // Always respect color
    HUlib_drawText(lstext->text, x + lstext->x, y + lstext->y);
    hu_forced_color = false;
    return M_StringWidth(lstext->text); // Save width if needed
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
                int realkey = k + (ap_level_info->use_skull[k] ? 3 : 0);

                V_DrawPatch(key_x, key_y, W_CacheLumpName("LSKEYBG", PU_CACHE));
                if (mapinfo->keys.use_checkmark)
                {
                    const int checkmark_x = key_x + mapinfo->keys.checkmark_x;
                    const int checkmark_y = key_y + mapinfo->keys.checkmark_y;

                    if (mapinfo->keys.use_custom_gfx)
                        V_DrawPatch(key_x, key_y, W_CacheLumpName(KEY_LUMP_NAMES[realkey], PU_CACHE));
                    else
                        ST_DrawKey(key_x, key_y, realkey);
                    if (ap_level_state->keys[k])
                        V_DrawPatch(checkmark_x, checkmark_y, W_CacheLumpName("CHECKMRK", PU_CACHE));
                }
                else
                {
                    if (ap_level_state->keys[k])
                    {
                        if (mapinfo->keys.use_custom_gfx)
                            V_DrawPatch(key_x, key_y, W_CacheLumpName(KEY_LUMP_NAMES[realkey], PU_CACHE));
                        else
                            ST_DrawKey(key_x, key_y, realkey);
                    }
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
            ST_RightAlignedShortNum(progress_x, progress_y, ap_level_state->check_count);
            V_DrawPatch(progress_x + 1, progress_y, W_CacheLumpName("STYSLASH", PU_CACHE));
            ST_LeftAlignedShortNum(progress_x + 8, progress_y, ap_total_check_count(ap_level_info));
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
            const int y = (mapinfo->map_name_display == LS_MAP_DISPLAY_UPPER) ? 2 : (ORIGHEIGHT - patch->height) - 2;
            DrawLSPatch(&mapinfo->map_name, x, y);
        }
        if (mapinfo->map_text_display > LS_MAP_DISPLAY_NONE)
        {
            const int x = (ORIGWIDTH - M_StringWidth(mapinfo->map_text.text)) / 2;
            const int y = (mapinfo->map_text_display == LS_MAP_DISPLAY_UPPER) ? 2 : (ORIGHEIGHT - 8) - 2;
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
    V_DrawFilledBox(0, 0, SCREENWIDTH, SCREENHEIGHT, 0);
    V_DrawPatch(ep_anim * 32, 0, primary_image);

    if (ep_anim == 0)
    {
        // We may have room to draw the images for previous and next episodes...
        if (SCREENWIDTH != NONWIDEWIDTH)
        {
            patch_t *left_image = W_CacheLumpName(LS_PrevEpisodeInfo()->background_image, PU_CACHE);
            patch_t *right_image = W_CacheLumpName(LS_NextEpisodeInfo()->background_image, PU_CACHE);

            dp_translation = cr[CR_DARK];
            V_DrawPatch(-320, 0, left_image);
            V_DrawPatch(320, 0, right_image);
            dp_translation = NULL;
        }

        if (wi_animnum != 0)
            WI_drawAnimatedBack();
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
