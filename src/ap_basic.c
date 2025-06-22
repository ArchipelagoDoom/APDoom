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

#include <stdlib.h>
#include <string.h>

#include "i_system.h"
#include "m_argv.h"
#include "m_misc.h"

#include "apdoom.h"

// Parses command line options common to all games' AP implementations.
// See each game's "d_main.c".
void APC_ParseCommandLine(ap_settings_t *ap_settings, const char *default_game_defs)
{
    int p;

    //!
    // @arg <game>
    // @category archipelago
    //
    // The game that you wish to play.
    // Can include the names of PWADs; see the "/defs" folder for available options.
    //
    if (!M_CheckParm("-game"))
    {
        if (!default_game_defs)
            I_Error("Required command line arguments are missing.\nThe '-game' parameter requires an argument.");
        if (!ap_preload_defs_for_game(default_game_defs))
            I_Error("Failed to initialize Archipelago.");            
    }
    else
    {
        p = M_CheckParmWithArgs("-game", 1);
        if (!p)
            I_Error("Required command line arguments are missing.\nThe '-game' parameter requires an argument.");
        if (!ap_preload_defs_for_game(myargv[p + 1]))
            I_Error("Failed to initialize Archipelago.");
    }

    //!
    // @arg <directory>
    // @category archipelago
    //
    // Change the subdirectory that Archipelago game saves are placed into.
    //
    if ((p = M_CheckParmWithArgs("-apsavedir", 1)) != 0)
    {
        ap_settings->save_dir = myargv[p + 1];
        M_MakeDirectory(ap_settings->save_dir);
    }

    //!
    // @arg <override_value>
    // @category archipelago
    //
    // Enable or disable monster rando,
    // overriding the settings specified by Archipelago at generation time.
    //
    if ((p = M_CheckParmWithArgs("-apmonsterrando", 1)) != 0)
    {
        ap_settings->override_monster_rando = 1;
        ap_settings->monster_rando = atoi(myargv[p + 1]);
    }

    //!
    // @arg <override_value>
    // @category archipelago
    //
    // Enable or disable item rando,
    // overriding the settings specified by Archipelago at generation time.
    //
    if ((p = M_CheckParmWithArgs("-apitemrando", 1)) != 0)
    {
        ap_settings->override_item_rando = 1;
        ap_settings->item_rando = atoi(myargv[p + 1]);
    }

    //!
    // @arg <override_value>
    // @category archipelago
    //
    // Enable or disable music rando,
    // overriding the settings specified by Archipelago at generation time.
    //
    if ((p = M_CheckParmWithArgs("-apmusicrando", 1)) != 0)
    {
        ap_settings->override_music_rando = 1;
        ap_settings->music_rando = atoi(myargv[p + 1]);
    }

    //!
    // @arg <override_value>
    // @category archipelago
    //
    // Enable or disable flipping levels,
    // overriding the settings specified by Archipelago at generation time.
    //
    if ((p = M_CheckParmWithArgs("-apfliplevels", 1)) != 0)
    {
        ap_settings->override_flip_levels = 1;
        ap_settings->flip_levels = atoi(myargv[p + 1]);
    }

    //!
    // @arg <override_value>
    // @category archipelago
    //
    // Enable or disable resetting level on death,
    // overriding the settings specified by Archipelago at generation time.
    //
    if ((p = M_CheckParmWithArgs("-apresetlevelondeath", 1)) != 0)
    {
        ap_settings->override_reset_level_on_death = 1;
        ap_settings->reset_level_on_death = atoi(myargv[p + 1]) ? 1 : 0;
    }

    //!
    // @category archipelago
    //
    // Forcibly disables DeathLink.
    //
    if (M_CheckParm("-apdeathlinkoff"))
        ap_settings->force_deathlink_off = 1;

    //!
    // @arg <server_address>
    // @category archipelago
    //
    // The Archipelago server to connect to.
    // Required.
    //
    p = M_CheckParmWithArgs("-apserver", 1);
    if (!p)
        I_Error("Required command line arguments are missing.\nThe '-apserver' parameter requires an argument.");
    ap_settings->ip = myargv[p + 1];

    //!
    // @arg <slot_name>
    // @category archipelago
    //
    // The name of the player/slot to connect to.
    // Required.
    //
    p = M_CheckParmWithArgs("-applayer", 1);
    if (!p)
    {
        //!
        // @arg <slot_name>
        // @category archipelago
        //
        // The name of the player/slot to connect to, specified in hex.
        //
        p = M_CheckParmWithArgs("-applayerhex", 1);
        if (!p)
            I_Error("Required command line arguments are missing.\nThe '-applayer' parameter requires an argument.");
        else
        {
            char* player_name = myargv[p + 1];
            int len = strlen(player_name) / 2;
            char byte_str[3] = {0};

            for (int i = 0; i < len; ++i)
            {
                memcpy(byte_str, player_name + (i * 2), 2);
                player_name[i] = strtol(byte_str, NULL, 16);
            }
            player_name[len] = '\0';
        }
    }
    ap_settings->player_name = myargv[p + 1];

    //!
    // @arg <password>
    // @category archipelago
    //
    // The password to connect to the Archipelago server.
    //
    if (M_CheckParm("-password"))
    {
        p = M_CheckParmWithArgs("-password", 1);
        if (!p)
            I_Error("Required command line arguments are missing.\nThe '-password' parameter requires an argument.");
        ap_settings->passwd = myargv[p + 1];
    }
    else
        ap_settings->passwd = "";
}
