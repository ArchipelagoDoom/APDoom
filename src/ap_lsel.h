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

#ifndef __AP_LSEL_H__
#define __AP_LSEL_H__

#include "apdoom.h"

extern int *selected_level;
extern int selected_ep;

void LS_Start(void);

int LS_PrevEpisode(void);
int LS_NextEpisode(void);

ap_levelselect_t *LS_PrevEpisodeInfo(void);
ap_levelselect_t *LS_CurrentEpisodeInfo(void);
ap_levelselect_t *LS_NextEpisodeInfo(void);

#endif
