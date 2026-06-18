//
// Copyright(C) 2023 David St-Louis
// Copyright(C) 2025 Kay "Kaito" Sinclaire
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
// Generic functions for Level Select
//

#include <stdlib.h>

#include "apdoom.h"
#include "ap_lsel.h"

#include "crispy.h"
#include "i_system.h"
#include "m_argv.h"

int *selected_level; // dynamically allocate due to variable episode count
int selected_ep = -1;


void LS_Start(void)
{
	if (M_CheckParm("-testcontrols"))
		I_Quit();

    if (!selected_level)
        selected_level = calloc(ap_episode_count, sizeof(int));

    // If we resumed a level upon starting, try to go to the episode screen that level is in.
    // Otherwise go to the first episode.
    if (selected_ep == -1)
    	selected_ep = (ap_state.ep > 0 ? ap_state.ep - 1 : 0);

    if (!ap_state.episodes[selected_ep])
    	selected_ep = LS_NextEpisode();

    ap_state.ep = 0;
    ap_state.map = 0;
}

int LS_MoveCursor(int dir)
{
    const ap_levelselect_t* screen_defs = LS_CurrentEpisodeInfo();
    const int from = selected_level[selected_ep];
    int best = from;

    switch (crispy->ap_levelselectorder)
    {
        case AP_LEVELSELECTORDER_MAP_ORDER:
        {
            if (screen_defs->map_info[screen_defs->num_map_info-1].y < screen_defs->map_info[0].y)
                dir *= -1; // Make menu work in reverse if maps are in reverse order.
            best += screen_defs->num_map_info + dir;
            best %= screen_defs->num_map_info;
            break;
        }
        case AP_LEVELSELECTORDER_POSITION:
        {
            // We score each level based on its base coordinates.
            // We *must* consider both X and Y coordinates, otherwise we might miss levels!
            int from_pos = (screen_defs->map_info[from].y << 16) + screen_defs->map_info[from].x;
            int top_pos = 100000000, bot_pos = -100000000;
            int top_id = -1, bot_id = -1;
            int best_score = 100000000;

            for (int i = 0; i < screen_defs->num_map_info; ++i)
            {
                int cur_pos = (screen_defs->map_info[i].y << 16) + screen_defs->map_info[i].x;
                if (cur_pos < top_pos)
                {
                    top_id = i;
                    top_pos = cur_pos;
                }
                if (cur_pos > bot_pos)
                {
                    bot_id = i;
                    bot_pos = cur_pos;
                }

                // We ignore negative "score", so, any move in the opposite direction we want to go.
                // (This also catches trying to move to the current location.)
                const int64_t score = (cur_pos - from_pos) * dir;
                if (score > 0 && score < best_score)
                {
                    best = i;
                    best_score = score;
                }
            }

            // If we didn't move because we're as far up/down as we can go,
            // then wrap around to the map on we found furthest from us
            if (best == from && from == top_id)
                best = bot_id;
            else if (best == from && from == bot_id)
                best = top_id;
            break;
        }
        default:
            break;
    }
    return best;
}

int LS_PrevEpisode(void)
{
    const int add = ap_episode_count - 1;
    int ep = (selected_ep + add) % ap_episode_count;
    for (; ep != selected_ep; ep = (ep + add) % ap_episode_count)
    {
        if (ap_state.episodes[ep])
            break;
    }
    return ep;
}

int LS_NextEpisode(void)
{
    const int add = 1;
    int ep = (selected_ep + add) % ap_episode_count;
    for (; ep != selected_ep; ep = (ep + add) % ap_episode_count)
    {
        if (ap_state.episodes[ep])
            break;
    }
    return ep;
}

ap_levelselect_t *LS_PrevEpisodeInfo(void)
{
    return ap_get_level_select_info(LS_PrevEpisode());
}

ap_levelselect_t *LS_CurrentEpisodeInfo(void)
{
    return ap_get_level_select_info(selected_ep);
}

ap_levelselect_t *LS_NextEpisodeInfo(void)
{
    return ap_get_level_select_info(LS_NextEpisode());
}

