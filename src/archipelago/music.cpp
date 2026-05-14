//
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
// Functions for music and music rando.
//

#include "local.hpp"

#include <string>
#include <vector>

static const std::vector<const char *> doom_music_lumps = {
	"INVALIDLUMP",
	// Doom 1
	"D_E1M1",   "D_E1M2",   "D_E1M3",   "D_E1M4",   "D_E1M5",   "D_E1M6",   "D_E1M7",   "D_E1M8",   "D_E1M9",
	"D_E2M1",   "D_E2M2",   "D_E2M3",   "D_E2M4",   "D_E2M5",   "D_E2M6",   "D_E2M7",   "D_E2M8",   "D_E2M9",
	"D_E3M1",   "D_E3M2",   "D_E3M3",   "D_E3M4",   "D_E3M5",   "D_E3M6",   "D_E3M7",   "D_E3M8",   "D_E3M9",
    "D_E4M1",   "D_E4M2",   "D_E4M3",   "D_E4M4",   "D_E4M5",   "D_E4M6",   "D_E4M7",   "D_E4M8",   "D_E4M9",
    "D_E5M1",   "D_E5M2",   "D_E5M3",   "D_E5M4",   "D_E5M5",   "D_E5M6",   "D_E5M7",   "D_E5M8",   "D_E5M9",
    "D_E6M1",   "D_E6M2",   "D_E6M3",   "D_E6M4",   "D_E6M5",   "D_E6M6",   "D_E6M7",   "D_E6M8",   "D_E6M9",
    "D_SIGINT", "D_SG2INT", "D_INTER",  "D_INTRO",  "D_BUNNY",  "D_VICTOR", "D_INTROA",
    // Doom 2
    "D_RUNNIN", "D_STALKS", "D_COUNTD", "D_BETWEE", "D_DOOM",   "D_THE_DA", "D_SHAWN",  "D_DDTBLU", "D_IN_CIT",
    "D_DEAD",   "D_STLKS2", "D_THEDA2", "D_DOOM2",  "D_DDTBL2", "D_RUNNI2", "D_DEAD2",  "D_STLKS3", "D_ROMERO",
    "D_SHAWN2", "D_MESSAG", "D_COUNT2", "D_DDTBL3", "D_AMPIE",  "D_THEDA3", "D_ADRIAN", "D_MESSG2", "D_ROMER2",
    "D_TENSE",  "D_SHAWN3", "D_OPENIN", "D_EVIL",   "D_ULTIMA", "D_READ_M", "D_DM2TTL", "D_DM2INT",
    "D_NRFTL1", "D_NRFTL2", "D_NRFTL3", "D_NRFTL4", "D_NRFTL5", "D_NRFTL6", "D_NRFTL7", "D_NRFTL8", "D_NRFTL9",
    "D_MLK1",   "D_MLK2",   "D_MLK3",   "D_MLK4",   "D_MLK5",   "D_MLK6",   "D_MLK7",   "D_MLK8",   "D_MLK9",
    "D_MLK10",  "D_MLK11",  "D_MLK12",  "D_MLK13",  "D_MLK14",  "D_MLK15",  "D_MLK16",  "D_MLK17",  "D_MLK18",
    "D_MLK19",  "D_MLK20",  "D_MLK21",
};

static const std::vector<const char *> heretic_music_lumps = {
	"MUS_E1M1", "MUS_E1M2", "MUS_E1M3", "MUS_E1M4", "MUS_E1M5", "MUS_E1M6", "MUS_E1M7", "MUS_E1M8", "MUS_E1M9",
	"MUS_E2M1", "MUS_E2M2", "MUS_E2M3", "MUS_E2M4", "MUS_E2M5", "MUS_E2M6", "MUS_E2M7", "MUS_E2M8", "MUS_E2M9",
	"MUS_E3M1", "MUS_E3M2", "MUS_E3M3", "MUS_E3M4", "MUS_E3M5", "MUS_E3M6", "MUS_E3M7", "MUS_E3M8", "MUS_E3M9",
    "MUS_E4M1", "MUS_E4M2", "MUS_E4M3", "MUS_E4M4", "MUS_E4M5", "MUS_E4M6", "MUS_E4M7", "MUS_E4M8", "MUS_E4M9",
    "MUS_E5M1", "MUS_E5M2", "MUS_E5M3", "MUS_E5M4", "MUS_E5M5", "MUS_E5M6", "MUS_E5M7", "MUS_E5M8", "MUS_E5M9",
    "MUS_E6M1", "MUS_E6M2", "MUS_E6M3",
    "MUS_TITL", "MUS_INTR", "MUS_CPTD",
};

static const std::vector<const char *> null_music_lumps = {};

static const std::vector<const char *> &get_music_list(void)
{
	switch (ap_base_game)
	{
	case ap_game_t::doom:    // fall through
	case ap_game_t::doom2:   return doom_music_lumps;
	case ap_game_t::heretic: return heretic_music_lumps;
	default:                 return null_music_lumps;
	}
}

int get_vanilla_music_id(int ep, int map)
{
	--ep, --map;
	switch (ap_base_game)
	{
		case ap_game_t::doom: // Correcting Episode 4's music is no longer needed
			return 1 + (ep * 9) + map;
		case ap_game_t::doom2:
			return 62 + map;
		case ap_game_t::heretic:
			return (ep * 9) + map;
		default: // Unrecognized game
			return -1;
	}
}

int get_named_music_id(const std::string name)
{
	const std::vector<const char *> &music_list = get_music_list();
	for (int i = 0; i < (int)music_list.size(); ++i)
	{
		if (name == music_list[i])
			return i;
	}
	return -1;
}

const char *music_id_to_name(int id)
{
	const std::vector<const char *> &music_list = get_music_list();
	return (id >= 0 && id < (int)music_list.size()) ? music_list[id] : "UNKNOWN";
}
