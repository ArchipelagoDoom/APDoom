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

