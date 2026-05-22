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
// Dummy file until this game is supported.
//

// Run every gametic, should call a function to update message state.

#include "apdoom.h"
#include "ap_spec.h"

void tick_sticky_msgs(void)
{
	//HU_TickAPMessages();
}

// ============================================================================

// Callback function for "ap_settings.message_callback"
// Should add the message to the HUD's message queue.

void APC_OnMessage(const char* text)
{
    //HU_AddAPMessage(text); // This string is cached for several seconds
}

// ============================================================================

// Callback function for "ap_settings.victory_callback"
// Should play some sort of ending screen.

void APC_OnVictory(void)
{
    //F_StartFinale();
}

// ============================================================================

// Callback function for "ap_settings.give_item_callback"

int APC_OnGiveItem(int doom_type, int ep, int map)
{
	return false;
}
