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
// Common functions for handling notification boxes
//

#ifndef __APNOTIF_H__
#define __APNOTIF_H__

void APC_DrawNotifBox(int x, int y, const char *sprite, boolean disabled);
void APC_DrawNotifs(void (textfunc)(const char *, int, int));

#endif
