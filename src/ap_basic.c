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
#include "apzip.h"
#include "embedded_files.h"

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

    // If certain arguments are set don't attempt to initialize Archipelago.
    if (M_CheckParmWithArgs ("-playdemo", 1) || M_CheckParmWithArgs ("-timedemo", 1)
        || M_CheckParm("-testcontrols"))
    {
        printf("Not initializing Archipelago due to certain command line arguments being specified.\n");
        ap_practice_mode = true;
        ap_force_disable_behaviors = true;
        I_AtExit(apdoom_remove_save_dir, true);
        return;
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
    // @category archipelago
    //
    // Always shows obituary messages in the message log,
    // even when they would otherwise be disabled. (DeathLink off, or practice)
    //
    if (M_CheckParm("-obituaries"))
        ap_settings->always_show_obituaries = 1;

    //!
    // @category archipelago
    //
    // Runs the game without connecting to Archipelago, for practicing.
    //
    if (M_CheckParm("-practice"))
    {
        printf("Entering practice mode.\n");
        ap_practice_mode = true;
        I_AtExit(apdoom_remove_save_dir, true);
        return;
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

// ----------------------------------------------------------------------------

// Initializes BaseAssets.zip (APDoom assets),
// whether it's embedded in the executable or shipped alongside it.
void APC_InitAssets(void)
{
    APZipReader *assets;

#ifdef EMBEDDED_FILE_BASEASSETS_ZIP
    const embedded_file_t *file = &embedded_files[EMBEDDED_FILE_BASEASSETS_ZIP];
    assets = APZipReader_FromMemory(file->data, file->size);
    if (assets) goto check_assets;
    printf("warning: APDoom's assets (BaseAssets.zip) were embedded, but the embedded archive can't be loaded\n");
#endif

    // Check current working directory and some other common subdirectories
    assets = APZipReader_FromFile("./BaseAssets.zip");
    if (assets) goto check_assets;
    assets = APZipReader_FromFile("./embed/BaseAssets.zip");
    if (assets) goto check_assets;
    assets = APZipReader_FromFile("./data/BaseAssets.zip");
    if (assets) goto check_assets;
    I_Error("APDoom's assets (BaseAssets.zip) cannot be found.");

check_assets:
    // A list of files that must exist inside BaseAssets.wad for it to be considered valid.
    const char *files_to_check[] = {
        "ArchipelagoDoom.wad",
        "ArchipelagoHeretic.wad",
        "Launcher.wad",
        NULL,
    };

    for (int i = 0; files_to_check[i]; ++i)
    {
        if (!APZipReader_FileExists(assets, files_to_check[i]))
        {
            APZipReader_Close(assets);
            I_Error("APDoom's assets (BaseAssets.zip) are missing a required file: %s", files_to_check[i]);            
        }
    }

    // File looks good, so try to cache it for later
    if (!APZipReader_Cache(assets, ":assets:"))
    {
        APZipReader_Close(assets);
        I_Error("There was a problem when trying to cache APDoom's assets (BaseAssets.zip).");
    }
}

// Dumps all embedded files into an "embed" subdirectory.
void APC_DumpEmbeddedFiles(void)
{
    char *embed_dir = M_StringJoin(".", DIR_SEPARATOR_S, "embed", NULL);
    char *embed_path;

    int file_count = 0;
    int success_count = 0;

    printf("Dumping all embedded files to \"%s\"...\n", embed_dir);
    M_MakeDirectory(embed_dir);

    for (file_count = 0; file_count < NUM_EMBEDDED_FILES; ++file_count)
    {
        const embedded_file_t *file = &embedded_files[file_count];
        embed_path = M_StringJoin(embed_dir, DIR_SEPARATOR_S, file->name, NULL);
        if (M_FileExists(embed_path))
        {
            printf("  %s: Already exists, not dumping\n", file->name);
        }
        else if (!M_WriteFile(embed_path, file->data, file->size))
        {
            printf("  %s: Couldn't write file\n", file->name);
        }
        else
        {
            ++success_count;
            printf("  %s: OK\n", file->name);
        }

        free(embed_path);
    }

    printf("%d of %d files dumped successfully.\n", success_count, file_count);
    free(embed_dir);
}
