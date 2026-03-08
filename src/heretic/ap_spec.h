//
// Copyright(C) 2023 David St-Louis
// Copyright(C) 2026 Kay "Kaito" Sinclaire
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
// Game specific AP related functions
//

#ifndef __APSPEC_H__
#define __APSPEC_H__

#include "doomtype.h"

// Callback functions
void APC_OnMessage(const char *text);
void APC_OnVictory(void);
int APC_OnGiveItem(int doom_type, int ep, int map);

// Helper function: returns positive if item can be given, zero if not
boolean APC_CanGiveItem(int doom_type);

// Helper function: Returns EnergyLink credit cost of item
int64_t APC_EnergyLinkItemCost(int doom_type);

#endif
