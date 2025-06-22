//
// Copyright(C) 2023 David St-Louis
// Copyright(C) 2024 Kay "Kaito" Sinclaire
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
// C side AP functions available to all games
//

#ifndef __AP_BASIC_H__
#define __AP_BASIC_H__

#include "apdoom.h"

void APC_ParseCommandLine(ap_settings_t *ap_settings, const char *default_game_defs);

#endif //__AP_BASIC_H__
