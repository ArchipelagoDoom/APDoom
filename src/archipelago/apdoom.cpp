//
// Copyright(C) 2023 David St-Louis
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
// *Source file for interfacing with archipelago*
//

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <io.h>
#ifdef _MSC_VER
#include <direct.h>
#endif
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif


#include "apdoom.h"
#include "Archipelago.h"
#include <json/json.h>
#include <filesystem>
#include <memory.h>
#include <chrono>
#include <thread>
#include <vector>
#include <fstream>
#include <sstream>
#include <set>

#include "apdoom_pwad.hpp"
#include "apzip.h"

#if defined(_WIN32)
static wchar_t *ConvertMultiByteToWide(const char *str, UINT code_page)
{
    wchar_t *wstr = NULL;
    int wlen = 0;

    wlen = MultiByteToWideChar(code_page, 0, str, -1, NULL, 0);

    if (!wlen)
    {
        errno = EINVAL;
        printf("APDOOM: ConvertMultiByteToWide: Failed to convert path to wide encoding.\n");
        return NULL;
    }

    wstr = (wchar_t*)malloc(sizeof(wchar_t) * wlen);

    if (!wstr)
    {
        printf("APDOOM: ConvertMultiByteToWide: Failed to allocate new string.");
        return NULL;
    }

    if (MultiByteToWideChar(code_page, 0, str, -1, wstr, wlen) == 0)
    {
        errno = EINVAL;
        printf("APDOOM: ConvertMultiByteToWide: Failed to convert path to wide encoding.\n");
        free(wstr);
        return NULL;
    }

    return wstr;
}

static wchar_t *AP_ConvertUtf8ToWide(const char *str)
{
    return ConvertMultiByteToWide(str, CP_UTF8);
}
#endif



//
// Create a directory
//

static void AP_MakeDirectory(const char *path)
{
#ifdef _WIN32
    wchar_t *wdir;

    wdir = AP_ConvertUtf8ToWide(path);

    if (!wdir)
    {
        return;
    }

    _wmkdir(wdir);

    free(wdir);
#else
    mkdir(path, 0755);
#endif
}


static FILE *AP_fopen(const char *filename, const char *mode)
{
#ifdef _WIN32
    FILE *file;
    wchar_t *wname = NULL;
    wchar_t *wmode = NULL;

    wname = AP_ConvertUtf8ToWide(filename);

    if (!wname)
    {
        return NULL;
    }

    wmode = AP_ConvertUtf8ToWide(mode);

    if (!wmode)
    {
        free(wname);
        return NULL;
    }

    file = _wfopen(wname, wmode);

    free(wname);
    free(wmode);

    return file;
#else
    return fopen(filename, mode);
#endif
}


static int AP_FileExists(const char *filename)
{
    FILE *fstream;

    fstream = AP_fopen(filename, "r");

    if (fstream != NULL)
    {
        fclose(fstream);
        return true;
    }
    else
    {
        // If we can't open because the file is a directory, the 
        // "file" exists at least!

        return errno == EISDIR;
    }
}


static Json::Value AP_ReadJson(const char *data, size_t size)
{
	static Json::CharReaderBuilder builder;
	std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

	Json::Value json;
	reader->parse(data, data+size, &json, NULL);
	return json;
}


static Json::Value AP_ReadJson(const std::string &data)
{
	return AP_ReadJson(data.data(), data.size());
}


enum class ap_game_t
{
	doom,
	doom2,
	heretic
};


const ap_worldinfo_t *ap_world_info;
ap_gameinfo_t ap_game_info;
ap_state_t ap_state;
int ap_is_in_game = 0;
int ap_episode_count = -1;

int ap_practice_mode = false; // Not connected to a server.
int ap_force_disable_behaviors = false; // For demo compatibility.

static bool detected_old_apworld = false;

static ap_game_t ap_base_game;
static int ap_weapon_count = -1;
static int ap_ammo_count = -1;
static int ap_powerup_count = -1;
static int ap_inventory_count = -1;
static int max_map_count = -1;
static ap_settings_t ap_settings;
static AP_RoomInfo ap_room_info;
static std::vector<int64_t> ap_item_queue; // We queue when we're in the menu.
static bool ap_was_connected = false; // Got connected at least once. That means the state is valid
static std::set<int64_t> ap_progressive_locations;
static std::set<int64_t> suppressed_locations; // Locations that don't exist in current multiworld (checksanity, etc)
static bool ap_initialized = false;
static std::vector<std::string> ap_cached_messages;
static std::string ap_seed_string;
static std::string ap_save_dir_name;
static std::vector<ap_notification_icon_t> ap_notification_icons;

#define SLOT_DATA_CALLBACK(func_name, output, condition) void func_name (int result) { if (condition) output = result; }

SLOT_DATA_CALLBACK(f_difficulty, ap_state.difficulty, (!ap_settings.override_skill) );
SLOT_DATA_CALLBACK(f_random_monsters, ap_state.random_monsters, (!ap_settings.override_monster_rando) );
SLOT_DATA_CALLBACK(f_random_items, ap_state.random_items, (!ap_settings.override_item_rando) );
SLOT_DATA_CALLBACK(f_random_music, ap_state.random_music, (!ap_settings.override_music_rando) );
SLOT_DATA_CALLBACK(f_flip_levels, ap_state.flip_levels, (!ap_settings.override_flip_levels) );
SLOT_DATA_CALLBACK(f_reset_level_on_death, ap_state.reset_level_on_death, (!ap_settings.override_reset_level_on_death) );

void f_goal(std::string json_blob)
{
	Json::Value json = AP_ReadJson(json_blob);
	if (json.isInt())
	{
		detected_old_apworld = true;
		return;
	}

	ap_state.goal = json["type"].asInt();
	switch (ap_state.goal)
	{
	case 3: // Specific Levels
	case 2: // Random Levels
		{
			ap_state.goal_level_count = (int)json["levels"].size();
			ap_state.goal_level_list = new ap_level_index_t[ap_state.goal_level_count];
			for (int i = 0; i < ap_state.goal_level_count; ++i)
			{
				const Json::Value& j_idx = json["levels"][i];
				ap_state.goal_level_list[i].ep = j_idx[0].asInt() - 1;
				ap_state.goal_level_list[i].map = j_idx[1].asInt() - 1;
			}
		}
		break;
	case 1: // Some number of levels
		ap_state.goal_level_count = json["count"].asInt();
		break;
	default:
		break;
	}
}

void f_suppressed_locations(std::string json_blob)
{
	Json::Value json = AP_ReadJson(json_blob);
	for (const Json::Value& loc_id : json)
		suppressed_locations.insert(loc_id.asInt());
}

void f_episodes(std::string json_blob)
{
	Json::Value json = AP_ReadJson(json_blob);
	for (const Json::Value& episode : json)
	{
		int ep_int = episode.asInt() - 1;
		if (ep_int >= 0 && ep_int < ap_episode_count)
			ap_state.episodes[ep_int] = 1;
	}
}

void f_ammo_start(std::string json_blob)
{
	Json::Value json = AP_ReadJson(json_blob);
	for (int i = 0; i < ap_ammo_count && i < (int)json.size(); ++i)
		ap_state.max_ammo_start[i] = json[i].asInt();
}

void f_ammo_add(std::string json_blob)
{
	Json::Value json = AP_ReadJson(json_blob);
	for (int i = 0; i < ap_ammo_count && i < (int)json.size(); ++i)
		ap_state.max_ammo_add[i] = json[i].asInt();
}

void f_itemclr();
void f_itemrecv(int64_t item_id, int player_id, bool notify_player);
void f_locrecv(int64_t loc_id);
void f_locinfo(std::vector<AP_NetworkItem> loc_infos);
void load_state();
void save_state();
void APSend(std::string msg);



// ===== PWAD SUPPORT =========================================================
// All of these are loaded from json on game startup.
// The following are large tables of game data, usually auto-generated by ap_gen_tool,
// that are parsed in apdoom_pwad.cpp.

static level_select_storage_t level_select_screens; // index:screen_num<ap_levelselect_t>
static map_tweaks_storage_t map_tweak_list; // <int episode, <int map, <ap_maptweak_t>>>
static level_info_storage_t preloaded_level_info; // index:episode<index:map<ap_level_info_t>>
static location_types_storage_t preloaded_location_types; // <int doomednum>
static location_table_storage_t preloaded_location_table; // <int episode, <int map, <int index, int64_t ap_id>>>
static item_table_storage_t preloaded_item_table; // <int64_t ap_id, ap_item_t>
static type_sprites_storage_t preloaded_type_sprites; // <int doomednum, std::string sprite_lump_name>

// ----------------------------------------------------------------------------

Json::Value open_defs(const char *defs_file)
{
	APZipReader *world = APZipReader_FetchFromCache(":world:");
	APZipFile *f = APZipReader_GetFile(world, defs_file);
	if (!f)
	{
		printf("Definitions file '%s' is missing...\n", defs_file);
		return Json::nullValue;
	}

	Json::Value json = AP_ReadJson(f->data, f->size);
	if (json.isNull())
		printf("Failed to initialize game definitions\n");
	return json;
}

// Returns positive on successful load, 0 for failure.
int ap_preload_defs_for_game(const char *game_name)
{
	ap_world_info = ap_get_world(game_name);
	if (!ap_world_info)
	{
		printf("APDOOM: No valid apworld for the game '%s' exists.\n    Currently available games are:\n", game_name);
		const ap_worldinfo_t **games_list = ap_list_worlds();
		for (int i = 0; games_list[i]; ++i)
			printf("    - '%s' -> %s\n", games_list[i]->shortname, games_list[i]->fullname);
		return 0;
	}

	if (!ap_load_world(ap_world_info->shortname))
		return 0;

	Json::Value defs_json = open_defs(ap_world_info->definitions);
	if (defs_json.isNull())
		return 0;

	{ // Recognize supported IWADs, and set up game info for them automatically.
		std::string iwad_name = std::string(ap_world_info->iwad);
		if (iwad_name == "HERETIC.WAD")
			ap_base_game = ap_game_t::heretic;
		else if (iwad_name == "DOOM.WAD" || iwad_name == "CHEX.WAD")
			ap_base_game = ap_game_t::doom;
		else // All others are Doom 2 based, I think?
			ap_base_game = ap_game_t::doom2;
	}

	ap_game_info.ammo_types = new ap_ammo_info_t[ap_episode_count * max_map_count];

	if (!json_parse_location_types(defs_json["ap_location_types"], preloaded_location_types)
		|| !json_parse_type_sprites(defs_json["type_sprites"], preloaded_type_sprites)
		|| !json_parse_item_table(defs_json["item_table"], preloaded_item_table)
		|| !json_parse_location_table(defs_json["location_table"], preloaded_location_table)
		|| !json_parse_level_info(defs_json["level_info"], preloaded_level_info)
		|| !json_parse_map_tweaks(defs_json["map_tweaks"], map_tweak_list)
		|| !json_parse_level_select(defs_json["level_select"], level_select_screens)
		|| !json_parse_game_info(defs_json["game_info"], ap_game_info)
		// TODO rename_lumps
	)
	{
		printf("APDOOM: Errors occurred while loading \"%s\".\n", game_name);
		return 0;
	}
	return 1;
} 

// ----------------------------------------------------------------------------

const ap_worldinfo_t *ap_loaded_world_info(void)
{
	return ap_world_info;
}

int ap_is_location_type(int doom_type)
{
	return preloaded_location_types.count(doom_type);
}

ap_levelselect_t *ap_get_level_select_info(unsigned int ep)
{
	if (ep >= level_select_screens.size())
		return NULL;
	return &level_select_screens[ep];
}

// ----------------------------------------------------------------------------

// These are used to do iteration with ap_get_map_tweaks
static ap_level_index_t gmt_level;
static allowed_tweaks_t gmt_type_mask;
static unsigned int gmt_i;

void ap_init_map_tweaks(ap_level_index_t idx, allowed_tweaks_t type_mask)
{
	gmt_i = 0;
	gmt_level.ep = idx.ep;
	gmt_level.map = idx.map;
	gmt_type_mask = type_mask;
}

ap_maptweak_t *ap_get_map_tweaks()
{
	// If map isn't present (has no tweaks), do nothing.
	if (map_tweak_list.count(gmt_level.ep) == 0
		|| map_tweak_list[gmt_level.ep].count(gmt_level.map) == 0)
	{
		return NULL;		
	}

	std::vector<ap_maptweak_t> &tweak_list = map_tweak_list[gmt_level.ep][gmt_level.map];
	while (gmt_i < tweak_list.size())
	{
		ap_maptweak_t *tweak = &tweak_list[gmt_i++];
		if ((tweak->type & TWEAK_TYPE_MASK) != gmt_type_mask)
			continue;
		return tweak;
	}
	return NULL;
}

// ----------------------------------------------------------------------------
// Keeping these getter functions around makes management easier.

static std::vector<std::vector<ap_level_info_t>>& get_level_info_table()
{
	return preloaded_level_info;
}

static const std::map<int64_t, ap_item_t>& get_item_type_table()
{
	return preloaded_item_table;
}

static const std::map<int /* ep */, std::map<int /* map */, std::map<int /* index */, int64_t /* loc id */>>>& get_location_table()
{
	return preloaded_location_table;
}

static const std::map<int, std::string>& get_sprites()
{
	return preloaded_type_sprites;
}

const char* get_weapon_name(int weapon)
{
	return (weapon >= 0 && weapon < ap_weapon_count) ? ap_game_info.weapons[weapon].name : "UNKNOWN";
}

const char* get_ammo_name(int weapon)
{
	return (weapon >= 0 && weapon < ap_ammo_count) ? ap_game_info.ammo_types[weapon].name : "UNKNOWN";
}

// ============================================================================

static int get_original_music_for_level(int ep, int map)
{
	switch (ap_base_game)
	{
		case ap_game_t::doom:
		{
			int ep4_music[] = {
				// Song - Who? - Where?

				2 * 9 + 3 + 1, //mus_e3m4,        // American     e4m1
				2 * 9 + 1 + 1, //mus_e3m2,        // Romero       e4m2
				2 * 9 + 2 + 1, //mus_e3m3,        // Shawn        e4m3
				0 * 9 + 4 + 1, //mus_e1m5,        // American     e4m4
				1 * 9 + 6 + 1, //mus_e2m7,        // Tim          e4m5
				1 * 9 + 3 + 1, //mus_e2m4,        // Romero       e4m6
				1 * 9 + 5 + 1, //mus_e2m6,        // J.Anderson   e4m7 CHIRON.WAD
				1 * 9 + 4 + 1, //mus_e2m5,        // Shawn        e4m8
				0 * 9 + 8 + 1  //mus_e1m9,        // Tim          e4m9
			};

			if (ep == 4) return ep4_music[map - 1];
			return 1 + (ep - 1) * ap_get_map_count(ep) + (map - 1);
		}
		case ap_game_t::doom2:
		{
			return 52 + ap_index_to_map({ep - 1, map - 1}) - 1;
		}
		case ap_game_t::heretic:
		{
			return (ep - 1) * 9 + (map - 1);
		}
	}

	// For now for doom and heretic
	return -1;
}


int ap_get_map_count(int ep)
{
	--ep;
	auto& level_info_table = get_level_info_table();
	if (ep < 0 || ep >= (int)level_info_table.size()) return -1;
	return (int)level_info_table[ep].size();
}


int ap_total_check_count(const ap_level_info_t *level_info)
{
	return level_info->true_check_count;
}


ap_level_info_t* ap_get_level_info(ap_level_index_t idx)
{
	auto& level_info_table = get_level_info_table();
	if (idx.ep < 0 || idx.ep >= (int)level_info_table.size()) return nullptr;
	if (idx.map < 0 || idx.map >= (int)level_info_table[idx.ep].size()) return nullptr;
	return &level_info_table[idx.ep][idx.map];
}


ap_level_state_t* ap_get_level_state(ap_level_index_t idx)
{
	return &ap_state.level_states[idx.ep * max_map_count + idx.map];
}


std::string string_to_hex(const char* str)
{
    static const char hex_digits[] = "0123456789ABCDEF";

	std::string out;
	std::string in = str;

    out.reserve(in.length() * 2);
    for (unsigned char c : in)
    {
        out.push_back(hex_digits[c >> 4]);
        out.push_back(hex_digits[c & 15]);
    }

    return out;
}


void recalc_max_ammo()
{
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		const int recalc_max = ap_state.max_ammo_start[i]
		    + (ap_state.max_ammo_add[i] * ap_state.player_state.capacity_upgrades[i]);

		ap_state.player_state.max_ammo[i] = (recalc_max > 999) ? 999 : recalc_max;
	}
}

int validate_doom_location(ap_level_index_t idx, int index)
{
    ap_level_info_t* level_info = ap_get_level_info(idx);
    if (index >= level_info->thing_count) return 0;
	if (level_info->thing_infos[index].location_id <= 0) return 0;
	if (suppressed_locations.count(level_info->thing_infos[index].location_id)) return 0;
    return 1;
}


int apdoom_init(ap_settings_t* settings)
{
	printf("%s\n", APDOOM_VERSION_FULL_TEXT);

#if 0 // If there's an invalid memory access being done here, I want to find it.
	ap_notification_icons.reserve(4096); // 1MB. A bit exessive, but I got a crash with invalid strings and I cannot figure out why. Let's not take any chances...
#endif
	memset(&ap_state, 0, sizeof(ap_state));

	settings->game = ap_world_info->apname;
	if (ap_base_game == ap_game_t::heretic)
	{
		ap_weapon_count = 9;
		ap_ammo_count = 6;
		ap_powerup_count = 9;
		ap_inventory_count = 14;
	}
	else // Doom or Doom 2, both use the same variables here
	{
		ap_weapon_count = 9;
		ap_ammo_count = 4;
		ap_powerup_count = 6;
		ap_inventory_count = 0;
	}

	const auto& level_info_table = get_level_info_table();
	ap_episode_count = (int)level_info_table.size();
	max_map_count = 0; // That's really the map count
	for (const auto& episode_level_info : level_info_table)
	{
		max_map_count = std::max(max_map_count, (int)episode_level_info.size());
	}

	ap_state.level_states = new ap_level_state_t[ap_episode_count * max_map_count];
	ap_state.episodes = new int[ap_episode_count];
	ap_state.player_state.powers = new int[ap_powerup_count];
	ap_state.player_state.weapon_owned = new int[ap_weapon_count];
	ap_state.player_state.ammo = new int[ap_ammo_count];
	ap_state.player_state.max_ammo = new int[ap_ammo_count];
	ap_state.player_state.inventory = ap_inventory_count ? new ap_inventory_slot_t[ap_inventory_count] : nullptr;

	memset(ap_state.level_states, 0, sizeof(ap_level_state_t) * ap_episode_count * max_map_count);
	memset(ap_state.episodes, 0, sizeof(int) * ap_episode_count);
	memset(ap_state.player_state.powers, 0, sizeof(int) * ap_powerup_count);
	memset(ap_state.player_state.weapon_owned, 0, sizeof(int) * ap_weapon_count);
	memset(ap_state.player_state.ammo, 0, sizeof(int) * ap_ammo_count);
	memset(ap_state.player_state.max_ammo, 0, sizeof(int) * ap_ammo_count);
	if (ap_inventory_count)
		memset(ap_state.player_state.inventory, 0, sizeof(ap_inventory_slot_t) * ap_inventory_count);

	ap_state.player_state.health = ap_game_info.start_health;
	ap_state.player_state.armor_points = ap_game_info.start_armor;
	ap_state.player_state.armor_type = 1;

	ap_state.player_state.ready_weapon = 1;
	ap_state.player_state.weapon_owned[0] = 1; // Fist
	ap_state.player_state.weapon_owned[1] = 1; // Pistol
	ap_state.player_state.ammo[0] = ap_game_info.weapons[1].start_ammo; // Clip

	// Ammo capacity management
	ap_state.max_ammo_start = new int[ap_ammo_count];
	ap_state.max_ammo_add = new int[ap_ammo_count];
	ap_state.player_state.capacity_upgrades = new int[ap_ammo_count];

	// default to regular max ammos for games without custom max ammo set
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		ap_state.max_ammo_start[i] = ap_game_info.ammo_types[i].max_ammo;
		ap_state.max_ammo_add[i] = ap_game_info.ammo_types[i].max_ammo;
	}
	memset(ap_state.player_state.capacity_upgrades, 0, sizeof(int) * ap_ammo_count);

	for (int ep = 0; ep < ap_episode_count; ++ep)
	{
		int map_count = ap_get_map_count(ep + 1);
		for (int map = 0; map < map_count; ++map)
		{
			for (int k = 0; k < AP_CHECK_MAX; ++k)
			{
				ap_state.level_states[ep * max_map_count + map].checks[k] = -1;
			}
		}
	}

	ap_settings = *settings;

	if (ap_settings.override_skill)
		ap_state.difficulty = ap_settings.skill;
	if (ap_settings.override_monster_rando)
		ap_state.random_monsters = ap_settings.monster_rando;
	if (ap_settings.override_item_rando)
		ap_state.random_items = ap_settings.item_rando;
	if (ap_settings.override_music_rando)
		ap_state.random_music = ap_settings.music_rando;
	if (ap_settings.override_flip_levels)
		ap_state.flip_levels = ap_settings.flip_levels;
	if (ap_settings.override_reset_level_on_death)
		ap_state.reset_level_on_death = ap_settings.reset_level_on_death;

	if (ap_practice_mode) // Practice game, no connection
	{
		// Select all episodes.
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			ap_state.episodes[ep] = 1;

			// Enable all maps.
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
			{
				auto level_state = ap_get_level_state(ap_level_index_t{ep, map});
				level_state->unlocked = 1;
				level_state->has_map = 1;

				auto level_info = ap_get_level_info(ap_level_index_t{ep, map});
				level_info->true_check_count = level_info->check_count;

				ap_state.level_states[ep * max_map_count + map].music = get_original_music_for_level(ep + 1, map + 1);
			}

		}

		recalc_max_ammo();

		ap_seed_string = "practmp_" + std::to_string(rand());
		ap_save_dir_name = ap_seed_string;
		if (!AP_FileExists(ap_save_dir_name.c_str()))
			AP_MakeDirectory(ap_save_dir_name.c_str());

		ap_initialized = true;
		return 1;
	}

	printf("APDOOM: Initializing Game: \"%s\", Server: %s, Slot: %s\n", settings->game, settings->ip, settings->player_name);
	AP_NetworkVersion version = {0, 6, 3};
	AP_SetClientVersion(&version);
    AP_Init(ap_settings.ip, ap_settings.game, ap_settings.player_name, ap_settings.passwd);
	AP_SetDeathLinkSupported(ap_settings.force_deathlink_off ? false : true);
	AP_SetItemClearCallback(f_itemclr);
	AP_SetItemRecvCallback(f_itemrecv);
	AP_SetLocationCheckedCallback(f_locrecv);
	AP_SetLocationInfoCallback(f_locinfo);
	AP_RegisterSlotDataRawCallback("goal", f_goal);
	AP_RegisterSlotDataIntCallback("difficulty", f_difficulty);
	AP_RegisterSlotDataIntCallback("reset_level_on_death", f_reset_level_on_death);
	AP_RegisterSlotDataIntCallback("random_monsters", f_random_monsters);
	AP_RegisterSlotDataIntCallback("random_pickups", f_random_items);
	AP_RegisterSlotDataIntCallback("random_music", f_random_music);
	AP_RegisterSlotDataIntCallback("flip_levels", f_flip_levels);
	AP_RegisterSlotDataRawCallback("suppressed_locations", f_suppressed_locations);
	AP_RegisterSlotDataRawCallback("episodes", f_episodes);
	AP_RegisterSlotDataRawCallback("ammo_start", f_ammo_start);
	AP_RegisterSlotDataRawCallback("ammo_add", f_ammo_add);
    AP_Start();

	// Block DOOM until connection succeeded or failed
	auto start_time = std::chrono::steady_clock::now();
	while (true)
	{
		bool should_break = false;
		switch (AP_GetConnectionStatus())
		{
			case AP_ConnectionStatus::Authenticated:
			{
				if (detected_old_apworld)
				{
					printf("APDOOM: Older versions of the APWorld are not supported.\n");
					printf("  Please use APDOOM 1.2.0 to connect to this slot.\n");
					return 0;
				}

				printf("APDOOM: Authenticated\n");
				AP_GetRoomInfo(&ap_room_info);

				printf("APDOOM: Room Info:\n");
				printf("  Network Version: %i.%i.%i\n", ap_room_info.version.major, ap_room_info.version.minor, ap_room_info.version.build);
				printf("  Tags:\n");
				for (const auto& tag : ap_room_info.tags)
					printf("    %s\n", tag.c_str());
				printf("  Password required: %s\n", ap_room_info.password_required ? "true" : "false");
				printf("  Permissions:\n");
				for (const auto& permission : ap_room_info.permissions)
					printf("    %s = %i:\n", permission.first.c_str(), permission.second);
				printf("  Hint cost: %i\n", ap_room_info.hint_cost);
				printf("  Location check points: %i\n", ap_room_info.location_check_points);
				printf("  Data package checksums:\n");
				for (const auto& kv : ap_room_info.datapackage_checksums)
					printf("    %s = %s:\n", kv.first.c_str(), kv.second.c_str());
				printf("  Seed name: %s\n", ap_room_info.seed_name.c_str());
				printf("  Time: %f\n", ap_room_info.time);

				ap_was_connected = true;

				ap_seed_string = "AP_" + ap_room_info.seed_name + "_" + string_to_hex(ap_settings.player_name);
				if (ap_settings.save_dir != NULL)
					ap_save_dir_name = std::string(ap_settings.save_dir) + "/" + ap_seed_string;
				else
					ap_save_dir_name = ap_seed_string;

				// Create a directory where saves will go for this AP seed.
				printf("APDOOM: Save directory: %s\n", ap_save_dir_name.c_str());
				if (!AP_FileExists(ap_save_dir_name.c_str()))
				{
					printf("  Doesn't exist, creating...\n");
					AP_MakeDirectory(ap_save_dir_name.c_str());
				}

				// Make sure that ammo starts at correct base values no matter what
				recalc_max_ammo();

				load_state();
				should_break = true;
				break;
			}
			case AP_ConnectionStatus::ConnectionRefused:
				printf("APDOOM: Failed to connect, connection refused\n");
				return 0;
			default: // Explicitly do not handle
				break;
		}
		if (should_break) break;
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
		{
			printf("APDOOM: Failed to connect, timeout 10s\n");
			return 0;
		}
	}

	// If none episode is selected, select the first one.
	int ep_count = 0;
	for (int i = 0; i < ap_episode_count; ++i)
		if (ap_state.episodes[i])
			ep_count++;
	if (!ep_count)
	{
		printf("APDOOM: No episode selected, selecting episode 1\n");
		ap_state.episodes[0] = 1;
	}

	// Set up true check counts now
	for (int ep = 0; ep < ap_episode_count; ++ep)
	{
		int map_count = ap_get_map_count(ep + 1);
		for (int map = 0; map < map_count; ++map)
		{
			auto level_info = ap_get_level_info(ap_level_index_t{ep, map});
			level_info->true_check_count = level_info->check_count;
			for (int k = 0; k < level_info->thing_count; ++k)
			{
				if (suppressed_locations.count(level_info->thing_infos[k].location_id))
				{
					--level_info->true_check_count;
				}
			}
		}
	}

	// Seed for random features
	ap_srand(31337);

	// Randomly flip levels based on the seed
	if (ap_state.flip_levels == 1)
	{
		printf("APDOOM: All levels flipped\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
				ap_state.level_states[ep * max_map_count + map].flipped = 1;
		}
	}
	else if (ap_state.flip_levels == 2)
	{
		printf("APDOOM: Levels randomly flipped\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
				ap_state.level_states[ep * max_map_count + map].flipped = ap_rand() % 2;
		}
	}

	// Map original music to every level to start
	for (int ep = 0; ep < ap_episode_count; ++ep)
	{
		int map_count = ap_get_map_count(ep + 1);
		for (int map = 0; map < map_count; ++map)
			ap_state.level_states[ep * max_map_count + map].music = get_original_music_for_level(ep + 1, map + 1);
	}

	// Randomly shuffle music 
	if (ap_state.random_music > 0)
	{
		// Collect music for all selected levels
		std::vector<int> music_pool;
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep] || ap_state.random_music == 2)
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
					music_pool.push_back(ap_state.level_states[ep * max_map_count + map].music);
			}
		}

		// Shuffle
		printf("APDOOM: Random Music:\n");
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (ap_state.episodes[ep])
			{
				int map_count = ap_get_map_count(ep + 1);
				for (int map = 0; map < map_count; ++map)
				{
					int rnd = ap_rand() % (int)music_pool.size();
					int mus = music_pool[rnd];
					music_pool.erase(music_pool.begin() + rnd);
					ap_state.level_states[ep * max_map_count + map].music = mus;

					switch (ap_base_game)
					{
						case ap_game_t::doom:
							printf("  E%iM%i = E%iM%i\n", ep + 1, map + 1, ((mus - 1) / max_map_count) + 1, ((mus - 1) % max_map_count) + 1);
							break;
						case ap_game_t::doom2:
							printf("  MAP%02i = MAP%02i\n", map + 1, mus);
							break;
						case ap_game_t::heretic:
							printf("  E%iM%i = E%iM%i\n", ep + 1, map + 1, (mus / max_map_count) + 1, (mus % max_map_count) + 1);
							break;
					}
				}
			}
		}
	}

	// Scout locations to see which are progressive
	if (ap_progressive_locations.empty())
	{
		std::vector<int64_t> location_scouts;

		const auto& loc_table = get_location_table();
		for (const auto& kv1 : loc_table)
		{
			if (!ap_state.episodes[kv1.first - 1])
				continue;
			for (const auto& kv2 : kv1.second)
			{
				for (const auto& kv3 : kv2.second)
				{
					if (kv3.first == -1) continue;

					if (validate_doom_location({kv1.first - 1, kv2.first - 1}, kv3.first))
					{
						location_scouts.push_back(kv3.second);
					}
				}
			}
		}
		
		printf("APDOOM: Scouting for %i locations...\n", (int)location_scouts.size());
		AP_SendLocationScouts(location_scouts, 0);

		// Wait for location infos
		start_time = std::chrono::steady_clock::now();
		while (ap_progressive_locations.empty())
		{
			apdoom_update();
		
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			if (std::chrono::steady_clock::now() - start_time > std::chrono::seconds(10))
			{
				printf("APDOOM: Timeout waiting for LocationScouts. 10s\n  Do you have a VPN active?\n  Checks will all look non-progression.");
				break;
			}
		}
	}
	else
	{
		printf("APDOOM: Scout locations cached loaded\n");
	}
	
	printf("APDOOM: Initialized\n");
	ap_initialized = true;
	return 1;
}


static bool is_loc_checked(ap_level_index_t idx, int index)
{
	auto level_state = ap_get_level_state(idx);
	for (int i = 0; i < level_state->check_count; ++i)
	{
		if (level_state->checks[i] == index) return true;
	}
	return false;
}


void apdoom_shutdown()
{
	if (ap_was_connected)
		save_state();
}


void apdoom_save_state()
{
	if (ap_was_connected)
		save_state();
}


static void json_get_int(const Json::Value& json, int& out_or_default)
{
	if (json.isInt())
		out_or_default = json.asInt();
}


static void json_get_bool_or(const Json::Value& json, int& out_or_default)
{
	if (json.isInt())
		out_or_default |= json.asInt();
}


const char* get_power_name(int weapon)
{
	switch (weapon)
	{
		case 0: return "Invulnerability";
		case 1: return "Strength";
		case 2: return "Invisibility";
		case 3: return "Hazard suit";
		case 4: return "Computer area map";
		case 5: return "Infrared";
		default: return "UNKNOWN";
	}
}


void load_state()
{
	printf("APDOOM: Load state\n");

	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ifstream f(filename);
	if (!f.is_open())
	{
		printf("  None found.\n");
		return; // Could be no state yet, that's fine
	}
	Json::Value json;
	f >> json;
	f.close();

	// Player state
	json_get_int(json["player"]["health"], ap_state.player_state.health);
	json_get_int(json["player"]["armor_points"], ap_state.player_state.armor_points);
	json_get_int(json["player"]["armor_type"], ap_state.player_state.armor_type);
	json_get_int(json["player"]["ready_weapon"], ap_state.player_state.ready_weapon);
	json_get_int(json["player"]["kill_count"], ap_state.player_state.kill_count);
	json_get_int(json["player"]["item_count"], ap_state.player_state.item_count);
	json_get_int(json["player"]["secret_count"], ap_state.player_state.secret_count);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_get_int(json["player"]["powers"][i], ap_state.player_state.powers[i]);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_get_bool_or(json["player"]["weapon_owned"][i], ap_state.player_state.weapon_owned[i]);
	for (int i = 0; i < ap_ammo_count; ++i)
	{
		json_get_int(json["player"]["ammo"][i], ap_state.player_state.ammo[i]);

		// This will get overwritten later,
		// but it must be saved if the player is in game before all their items have been re-received.
		json_get_int(json["player"]["max_ammo"][i], ap_state.player_state.max_ammo[i]);		
	}
	for (int i = 0; i < ap_inventory_count; ++i)
	{
		const auto& inventory_slot = json["player"]["inventory"][i];
		json_get_int(inventory_slot["type"], ap_state.player_state.inventory[i].type);
		json_get_int(inventory_slot["count"], ap_state.player_state.inventory[i].count);
	}

	printf("  Player State:\n");
	printf("    Health %i:\n", ap_state.player_state.health);
	printf("    Armor points %i:\n", ap_state.player_state.armor_points);
	printf("    Armor type %i:\n", ap_state.player_state.armor_type);
	printf("    Ready weapon: %s\n", get_weapon_name(ap_state.player_state.ready_weapon));
	printf("    Kill count %i:\n", ap_state.player_state.kill_count);
	printf("    Item count %i:\n", ap_state.player_state.item_count);
	printf("    Secret count %i:\n", ap_state.player_state.secret_count);
	printf("    Active powerups:\n");
	for (int i = 0; i < ap_powerup_count; ++i)
		if (ap_state.player_state.powers[i])
			printf("    %s\n", get_power_name(i));
	printf("    Owned weapons:\n");
	for (int i = 0; i < ap_weapon_count; ++i)
		if (ap_state.player_state.weapon_owned[i])
			printf("      %s\n", get_weapon_name(i));
	printf("    Ammo:\n");
	for (int i = 0; i < ap_ammo_count; ++i)
		printf("      %s = %i / %i\n", get_ammo_name(i),
			ap_state.player_state.ammo[i],
			ap_state.player_state.max_ammo[i]);

	// Level states
	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			auto level_state = ap_get_level_state(ap_level_index_t{i, j});
			json_get_bool_or(json["episodes"][i][j]["completed"], level_state->completed);
			json_get_bool_or(json["episodes"][i][j]["keys0"], level_state->keys[0]);
			json_get_bool_or(json["episodes"][i][j]["keys1"], level_state->keys[1]);
			json_get_bool_or(json["episodes"][i][j]["keys2"], level_state->keys[2]);
			//json_get_bool_or(json["episodes"][i][j]["check_count"], level_state->check_count);
			json_get_bool_or(json["episodes"][i][j]["has_map"], level_state->has_map);
			json_get_bool_or(json["episodes"][i][j]["unlocked"], level_state->unlocked);
			json_get_bool_or(json["episodes"][i][j]["special"], level_state->special);

			//int k = 0;
			//for (const auto& json_check : json["episodes"][i][j]["checks"])
			//{
			//	json_get_bool_or(json_check, level_state->checks[k++]);
			//}
		}
	}

	// Item queue
	for (const auto& item_id_json : json["item_queue"])
	{
		ap_item_queue.push_back(item_id_json.asInt64());
	}

	json_get_int(json["ep"], ap_state.ep);
	printf("  Enabled episodes: ");
	int first = 1;
	for (int i = 0; i < ap_episode_count; ++i)
	{
		json_get_int(json["enabled_episodes"][i], ap_state.episodes[i]);
		if (ap_state.episodes[i])
		{
			if (!first) printf(", ");
			first = 0;
			printf("%i", i + 1);
		}
	}
	printf("\n");
	json_get_int(json["map"], ap_state.map);
	printf("  Episode: %i\n", ap_state.ep);
	printf("  Map: %i\n", ap_state.map);

	for (const auto& prog_json : json["progressive_locations"])
	{
		ap_progressive_locations.insert(prog_json.asInt64());
	}
	
	json_get_bool_or(json["victory"], ap_state.victory);
	printf("  Victory state: %s\n", ap_state.victory ? "true" : "false");
}


static Json::Value serialize_level(int ep, int map)
{
	auto level_state = ap_get_level_state(ap_level_index_t{ep - 1, map - 1});

	Json::Value json_level;

	json_level["completed"] = level_state->completed;
	json_level["keys0"] = level_state->keys[0];
	json_level["keys1"] = level_state->keys[1];
	json_level["keys2"] = level_state->keys[2];
	json_level["check_count"] = level_state->check_count;
	json_level["has_map"] = level_state->has_map;
	json_level["unlocked"] = level_state->unlocked;
	json_level["special"] = level_state->special;

	Json::Value json_checks(Json::arrayValue);
	for (int k = 0; k < AP_CHECK_MAX; ++k)
	{
		if (level_state->checks[k] == -1)
			continue;
		json_checks.append(level_state->checks[k]);
	}
	json_level["checks"] = json_checks;

	return json_level;
}


std::vector<ap_level_index_t> get_level_indices()
{
	std::vector<ap_level_index_t> ret;

	for (int i = 0; i < ap_episode_count; ++i)
	{
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			ret.push_back({i + 1, j + 1});
		}
	}

	return ret;
}


void save_state()
{
	std::string filename = ap_save_dir_name + "/apstate.json";
	std::ofstream f(filename);
	if (!f.is_open())
	{
		printf("Failed to save AP state.\n");
#if WIN32
		MessageBoxA(nullptr, "Failed to save player state. That's bad.", "Error", MB_OK);
#endif
		return; // Ok that's bad. we won't save player state
	}

	// Player state
	Json::Value json;
	Json::Value json_player;
	json_player["health"] = ap_state.player_state.health;
	json_player["armor_points"] = ap_state.player_state.armor_points;
	json_player["armor_type"] = ap_state.player_state.armor_type;
	json_player["ready_weapon"] = ap_state.player_state.ready_weapon;
	json_player["kill_count"] = ap_state.player_state.kill_count;
	json_player["item_count"] = ap_state.player_state.item_count;
	json_player["secret_count"] = ap_state.player_state.secret_count;

	Json::Value json_powers(Json::arrayValue);
	for (int i = 0; i < ap_powerup_count; ++i)
		json_powers.append(ap_state.player_state.powers[i]);
	json_player["powers"] = json_powers;

	Json::Value json_weapon_owned(Json::arrayValue);
	for (int i = 0; i < ap_weapon_count; ++i)
		json_weapon_owned.append(ap_state.player_state.weapon_owned[i]);
	json_player["weapon_owned"] = json_weapon_owned;

	Json::Value json_ammo(Json::arrayValue);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_ammo.append(ap_state.player_state.ammo[i]);
	json_player["ammo"] = json_ammo;

	Json::Value json_max_ammo(Json::arrayValue);
	for (int i = 0; i < ap_ammo_count; ++i)
		json_max_ammo.append(ap_state.player_state.max_ammo[i]);
	json_player["max_ammo"] = json_max_ammo;

	Json::Value json_inventory(Json::arrayValue);
	for (int i = 0; i < ap_inventory_count; ++i)
	{
		if (ap_state.player_state.inventory[i].type == 9) // Don't include wings to player inventory, they are per level
			continue;
		Json::Value json_inventory_slot;
		json_inventory_slot["type"] = ap_state.player_state.inventory[i].type;
		json_inventory_slot["count"] = ap_state.player_state.inventory[i].count;
		json_inventory.append(json_inventory_slot);
	}
	json_player["inventory"] = json_inventory;

	json["player"] = json_player;

	// Level states
	Json::Value json_episodes(Json::arrayValue);
	for (int i = 0; i < ap_episode_count; ++i)
	{
		Json::Value json_levels(Json::arrayValue);
		int map_count = ap_get_map_count(i + 1);
		for (int j = 0; j < map_count; ++j)
		{
			json_levels.append(serialize_level(i + 1, j + 1));
		}
		json_episodes.append(json_levels);
	}
	json["episodes"] = json_episodes;

	// Item queue
	Json::Value json_item_queue(Json::arrayValue);
	for (auto item_id : ap_item_queue)
	{
		json_item_queue.append(item_id);
	}
	json["item_queue"] = json_item_queue;

	json["ep"] = ap_state.ep;
	for (int i = 0; i < ap_episode_count; ++i)
		json["enabled_episodes"][i] = ap_state.episodes[i] ? true : false;
	json["map"] = ap_state.map;

	// Progression items (So we don't scout everytime we connect)
	for (auto loc_id : ap_progressive_locations)
	{
		json["progressive_locations"].append(loc_id);
	}

	json["victory"] = ap_state.victory;

	json["version"] = APDOOM_VERSION_FULL_TEXT;

	f << json;
}


void f_itemclr()
{
	// This gets called when (re)connecting to the server.
	// Any items that we need to keep track of, that can be collected multiple times,
	// need to be cleared out here; otherwise, we will double count them on reconnect.
	memset(ap_state.player_state.capacity_upgrades, 0, sizeof(int) * ap_ammo_count);
}


static const std::map<int, int> doom_keys_map = {{5, 0}, {40, 0}, {6, 1}, {39, 1}, {13, 2}, {38, 2}};
static const std::map<int, int> doom2_keys_map = {{5, 0}, {40, 0}, {6, 1}, {39, 1}, {13, 2}, {38, 2}};
static const std::map<int, int> heretic_keys_map = {{80, 0}, {73, 1}, {79, 2}};


const std::map<int, int>& get_keys_map()
{
	switch (ap_base_game)
	{
		default: // Indeterminate state? Default to Doom 1
		case ap_game_t::doom: return doom_keys_map;
		case ap_game_t::doom2: return doom2_keys_map;
		case ap_game_t::heretic: return heretic_keys_map;
	}
}


int get_map_doom_type()
{
	switch (ap_base_game)
	{
		default: // Indeterminate state? Default to Doom 1
		case ap_game_t::doom: return 2026;
		case ap_game_t::doom2: return 2026;
		case ap_game_t::heretic: return 35;
	}
}


static const std::map<int, int> doom_weapons_map = {{2001, 2}, {2002, 3}, {2003, 4}, {2004, 5}, {2006, 6}, {2005, 7}};
static const std::map<int, int> doom2_weapons_map = {{2001, 2}, {2002, 3}, {2003, 4}, {2004, 5}, {2006, 6}, {2005, 7}, {82, 8}};
static const std::map<int, int> heretic_weapons_map = {{2005, 7}, {2001, 2}, {53, 3}, {2003, 5}, {2002, 6}, {2004, 4}};


const std::map<int, int>& get_weapons_map()
{
	switch (ap_base_game)
	{
		default: // Indeterminate state? Default to Doom 1
		case ap_game_t::doom: return doom_weapons_map;
		case ap_game_t::doom2: return doom2_weapons_map;
		case ap_game_t::heretic: return heretic_weapons_map;
	}
}


std::string get_exmx_name(const std::string& name)
{
	auto pos = name.find_first_of('(');
	if (pos == std::string::npos) return name;
	return name.substr(pos);
}


// Split from f_itemrecv so that the item queue can call it without side-effects
// This handles everything that requires us be in game, notification icons included
static void process_received_item(int64_t item_id)
{
	const auto& item_type_table = get_item_type_table();
	auto it = item_type_table.find(item_id);
	if (it == item_type_table.end())
		return; // Skip -- This is probably redundant, but whatever

	ap_item_t item = it->second;
	std::string notif_text;

	if (ap_practice_mode)
	{
		// We have no AP server to give us item messages, so let's pretend we got one.
		std::string colored_msg = std::string("~2Received ~9") + item.name + "~2 from ~4Player";
		ap_settings.message_callback(colored_msg.c_str());
	}

	// If the item has an associated episode/map, note that
	if (item.ep != -1)
	{
		ap_level_index_t idx = {item.ep - 1, item.map - 1};
		ap_level_info_t* level_info = ap_get_level_info(idx);

		notif_text = get_exmx_name(level_info->name);
	}

	// Give item to in-game player
	ap_settings.give_item_callback(item.doom_type, item.ep, item.map);

	// Add notification icon
	const auto& sprite_map = get_sprites();
	auto sprite_it = sprite_map.find(item.doom_type);
	if (sprite_it != sprite_map.end())
	{
		ap_notification_icon_t notif;
		snprintf(notif.sprite, 9, "%s", sprite_it->second.c_str());
		notif.t = 0;
		notif.text[0] = '\0'; // For now
		if (notif_text != "")
		{
			snprintf(notif.text, 260, "%s", notif_text.c_str());
		}
		notif.xf = AP_NOTIF_SIZE / 2 + AP_NOTIF_PADDING;
		notif.yf = -200.0f + AP_NOTIF_SIZE / 2;
		notif.state = AP_NOTIF_STATE_PENDING;
		notif.velx = 0.0f;
		notif.vely = 0.0f;
		notif.x = (int)notif.xf;
		notif.y = (int)notif.yf;
		ap_notification_icons.push_back(notif);
	}
}

void f_itemrecv(int64_t item_id, int player_id, bool notify_player)
{
	const auto& item_type_table = get_item_type_table();
	auto it = item_type_table.find(item_id);
	if (it == item_type_table.end())
		return; // Skip
	ap_item_t item = it->second;

	ap_level_index_t idx = {item.ep - 1, item.map - 1};
	auto level_state = ap_get_level_state(idx);

	// Backpack?
	if (item.doom_type == 8)
	{
		for (int i = 0; i < ap_ammo_count; ++i)
			++ap_state.player_state.capacity_upgrades[i];
		recalc_max_ammo();
	}

	// Single ammo capacity upgrade?
	if (item.doom_type >= 65001 && item.doom_type <= 65006)
	{
		int ammo_num = item.doom_type - 65001;
		if (ammo_num < ap_ammo_count)
			++ap_state.player_state.capacity_upgrades[ammo_num];
		recalc_max_ammo();
	}

	// Key?
	const auto& keys_map = get_keys_map();
	auto key_it = keys_map.find(item.doom_type);
	if (key_it != keys_map.end())
		level_state->keys[key_it->second] = 1;

	// Weapon?
	const auto& weapons_map = get_weapons_map();
	auto weapon_it = weapons_map.find(item.doom_type);
	if (weapon_it != weapons_map.end())
		ap_state.player_state.weapon_owned[weapon_it->second] = 1;

	// Map?
	if (item.doom_type == get_map_doom_type())
		level_state->has_map = 1;

	// Level unlock?
	if (item.doom_type == -1)
		level_state->unlocked = 1;

	// Level complete?
	if (item.doom_type == -2)
		level_state->completed = 1;

	// Ignore inventory items, the game will add them up

	if (!notify_player) return;

	if (!ap_is_in_game)
		ap_item_queue.push_back(item_id);
	else
		process_received_item(item_id);
}


bool find_location(int64_t loc_id, int &ep, int &map, int &index)
{
	ep = -1;
	map = -1;
	index = -1;

	const auto& loc_table = get_location_table();
	for (const auto& loc_map_table : loc_table)
	{
		for (const auto& loc_index_table : loc_map_table.second)
		{
			for (const auto& loc_index : loc_index_table.second)
			{
				if (loc_index.second == loc_id)
				{
					ep = loc_map_table.first;
					map = loc_index_table.first;
					index = loc_index.first;
					break;
				}
			}
			if (ep != -1) break;
		}
		if (ep != -1) break;
	}
	return (ep > 0);
}


void f_locrecv(int64_t loc_id)
{
	// Find where this location is
	int ep = -1;
	int map = -1;
	int index = -1;
	if (!find_location(loc_id, ep, map, index))
	{
		printf("APDOOM: In f_locrecv, loc id not found: %i\n", (int)loc_id);
		return; // Loc not found
	}

	ap_level_index_t idx = {ep - 1, map - 1};

	// Make sure we didn't already check it
	if (is_loc_checked(idx, index)) return;
	if (index < 0) return;

	auto level_state = ap_get_level_state(idx);
	level_state->checks[level_state->check_count] = index;
	level_state->check_count++;
}


void f_locinfo(std::vector<AP_NetworkItem> loc_infos)
{
	for (const auto& loc_info : loc_infos)
	{
		if (loc_info.flags & 1)
			ap_progressive_locations.insert(loc_info.location);
	}
}


const char* apdoom_get_save_dir()
{
	return ap_save_dir_name.c_str();
}


void apdoom_remove_save_dir(void)
{
	if (ap_save_dir_name.substr(0, 8) != "practmp_")
		return;

	const std::filesystem::path save_path(ap_save_dir_name);
	std::filesystem::remove_all(save_path);
}


void apdoom_check_location(ap_level_index_t idx, int index)
{
	int64_t id = 0;
	const auto& loc_table = get_location_table();

	auto it1 = loc_table.find(idx.ep + 1);
	if (it1 == loc_table.end()) return;

	auto it2 = it1->second.find(idx.map + 1);
	if (it2 == it1->second.end()) return;

	auto it3 = it2->second.find(index);
	if (it3 == it2->second.end()) return;

	id = it3->second;
	if (suppressed_locations.count(id))
		return;

	if (ap_practice_mode)
	{
		f_locrecv(id);

		// Get the item that's supposed to be in that location.
		ap_level_info_t* level_info = ap_get_level_info(idx);
		int item_id = level_info->thing_infos[index].doom_type;

		// If it exists in the item table already, great.
		// If not, append the episode and map numbers and try again.
		const auto& item_type_table = get_item_type_table();
		if (!item_type_table.count(item_id))
		{
			item_id += (idx.ep + 1) * 10'000'000;
			item_id += (idx.map + 1) * 100'000;
		}

		// Send the item to ourselves as if we were playing.
		if (item_type_table.count(item_id))
		{
			f_itemrecv(item_id, 1, true);
		}
		return;
	}

	if (index >= 0)
	{
		if (is_loc_checked(idx, index))
		{
			printf("APDOOM: Location already checked\n");
		}
		else
		{ // We get back from AP
			//auto level_state = ap_get_level_state(ep, map);
			//level_state->checks[level_state->check_count] = index;
			//level_state->check_count++;
		}
	}
	AP_SendItem(id);
}


int apdoom_is_location_progression(ap_level_index_t idx, int index)
{
	const auto& loc_table = get_location_table();

	auto it1 = loc_table.find(idx.ep + 1);
	if (it1 == loc_table.end()) return 0;

	auto it2 = it1->second.find(idx.map + 1);
	if (it2 == it1->second.end()) return 0;

	auto it3 = it2->second.find(index);
	if (it3 == it2->second.end()) return 0;

	int64_t id = it3->second;

	return ap_progressive_locations.count(id);
}

void apdoom_complete_level(ap_level_index_t idx)
{
	//if (ap_state.level_states[ep - 1][map - 1].completed) return; // Already completed
    ap_get_level_state(idx)->completed = 1;
	apdoom_check_location(idx, -1); // -1 is complete location
}


// Attempt to make level index; failure is a possibility
ap_level_index_t ap_try_make_level_index(int gameepisode, int gamemap)
{
	// For PWAD support: Level info struct has gameepisode/gamemap, don't make assumptions
	const auto& table = get_level_info_table();
	for (int ep = 0; ep < (int)table.size(); ++ep)
	{
		for (int map = 0; map < (int)table[ep].size(); ++map)
		{
			if (table[ep][map].game_episode == gameepisode && table[ep][map].game_map == gamemap)
				return {ep, map};
		}
	}
	return {-1, -1};
}

// For when failure can't be a possibility due to array access; default to episode 1 map 1
ap_level_index_t ap_make_level_index(int gameepisode, int gamemap)
{
	ap_level_index_t idx = ap_try_make_level_index(gameepisode, gamemap);
	if (idx.ep == -1)
	{
		printf("APDOOM: Episode %d, Map %d isn't in the Archipelago level table!\n", gameepisode, gamemap);
		return {0, 0};
	}
	return idx;
}

int ap_index_to_ep(ap_level_index_t idx)
{
	const auto& table = get_level_info_table();
	return table[idx.ep][idx.map].game_episode;
}


int ap_index_to_map(ap_level_index_t idx)
{
	const auto& table = get_level_info_table();
	return table[idx.ep][idx.map].game_map;
}


void apdoom_check_victory()
{
	if (ap_state.victory)
	{
		// Silently resend victory state, just in case connection got dropped as we actually won
		AP_StoryComplete();
		return;
	}

	int complete_level_count = 0;

	switch (ap_state.goal)
	{
	case 3: // Specific levels
	case 2: // Random levels
		for (int i = 0; i < ap_state.goal_level_count; ++i)
		{
			if (!ap_get_level_state(ap_state.goal_level_list[i])->completed)
				return;
		}
		break;
	case 1: // Some count of levels
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (!ap_state.episodes[ep]) continue;
		
			const int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
			{
				if (ap_get_level_state(ap_level_index_t{ep, map})->completed)
					++complete_level_count;
			}
		}
		if (complete_level_count < ap_state.goal_level_count)
			return;
		break;
	default: // All levels
		for (int ep = 0; ep < ap_episode_count; ++ep)
		{
			if (!ap_state.episodes[ep]) continue;
		
			const int map_count = ap_get_map_count(ep + 1);
			for (int map = 0; map < map_count; ++map)
			{
				if (!ap_get_level_state(ap_level_index_t{ep, map})->completed)
					return;
			}
		}
		break;
	}

	ap_state.victory = 1;

	AP_StoryComplete();
	ap_settings.victory_callback();
}


void apdoom_send_message(const char* msg)
{
	Json::Value say_packet;
	say_packet[0]["cmd"] = "Say";
	say_packet[0]["text"] = msg;
	Json::FastWriter writer;
	APSend(writer.write(say_packet));
}


void apdoom_on_death()
{
	AP_DeathLinkSend();
}


void apdoom_clear_death()
{
	AP_DeathLinkClear();
}


int apdoom_should_die()
{
	return AP_DeathLinkPending() ? 1 : 0;
}


const ap_notification_icon_t* ap_get_notification_icons(int* count)
{
	*count = (int)ap_notification_icons.size();
	return ap_notification_icons.data();
}


int ap_get_highest_episode()
{
	int highest = 0;
	for (int i = 0; i < ap_episode_count; ++i)
		if (ap_state.episodes[i])
			highest = i;
	return highest;
}


int ap_validate_doom_location(ap_level_index_t idx, int doom_type, int index)
{
	ap_level_info_t* level_info = ap_get_level_info(idx);
    if (index >= level_info->thing_count) return -1;
	if (level_info->thing_infos[index].doom_type != doom_type) return -1;
	if (level_info->thing_infos[index].location_id <= 0) return 0;
	if (suppressed_locations.count(level_info->thing_infos[index].location_id)) return 0;
    return 1;
}


/*
    black: "000000"
    red: "EE0000"
    green: "00FF7F"  # typically a location
    yellow: "FAFAD2"  # typically other slots/players
    blue: "6495ED"  # typically extra info (such as entrance)
    magenta: "EE00EE"  # typically your slot/player
    cyan: "00EEEE"  # typically regular item
    slateblue: "6D8BE8"  # typically useful item
    plum: "AF99EF"  # typically progression item
    salmon: "FA8072"  # typically trap item
    white: "FFFFFF"  # not used, if you want to change the generic text color change color in Label

    (byte *) &cr_none, // 0 (RED)
    (byte *) &cr_dark, // 1 (DARK RED)
    (byte *) &cr_gray, // 2 (WHITE) normal text
    (byte *) &cr_green, // 3 (GREEN) location
    (byte *) &cr_gold, // 4 (YELLOW) player
    (byte *) &cr_red, // 5 (RED, same as cr_none)
    (byte *) &cr_blue, // 6 (BLUE) extra info such as Entrance
    (byte *) &cr_red2blue, // 7 (BLUE) items
    (byte *) &cr_red2green // 8 (DARK EDGE GREEN)
*/
void apdoom_update()
{
	if (ap_initialized)
	{
		if (!ap_cached_messages.empty())
		{
			for (const auto& cached_msg : ap_cached_messages)
				ap_settings.message_callback(cached_msg.c_str());
			ap_cached_messages.clear();
		}
	}

	while (AP_IsMessagePending())
	{
		AP_Message* msg = AP_GetLatestMessage();

		std::string colored_msg;

		switch (msg->type)
		{
			case AP_MessageType::ItemSend:
			{
				AP_ItemSendMessage* o_msg = static_cast<AP_ItemSendMessage*>(msg);
				colored_msg = "~9" + o_msg->item + "~2 was sent to ~4" + o_msg->recvPlayer;
				break;
			}
			case AP_MessageType::ItemRecv:
			{
				AP_ItemRecvMessage* o_msg = static_cast<AP_ItemRecvMessage*>(msg);
				colored_msg = "~2Received ~9" + o_msg->item + "~2 from ~4" + o_msg->sendPlayer;
				break;
			}
			case AP_MessageType::Hint:
			{
				AP_HintMessage* o_msg = static_cast<AP_HintMessage*>(msg);
				colored_msg = "~9" + o_msg->item + "~2 from ~4" + o_msg->sendPlayer + "~2 to ~4" + o_msg->recvPlayer + "~2 at ~3" + o_msg->location + (o_msg->checked ? " (Checked)" : " (Unchecked)");
				break;
			}
			default:
			{
				colored_msg = "~2" + msg->text;
				break;
			}
		}

		printf("APDOOM: %s\n", msg->text.c_str());

		if (ap_initialized)
			ap_settings.message_callback(colored_msg.c_str());
		else
			ap_cached_messages.push_back(colored_msg);

		AP_ClearLatestMessage();
	}

	// Check if we're in game, then dequeue the items
	if (ap_is_in_game)
	{
		while (!ap_item_queue.empty())
		{
			auto item_id = ap_item_queue.front();
			ap_item_queue.erase(ap_item_queue.begin());
			process_received_item(item_id);
		}
	}

	// Update notification icons
	float previous_y = 2.0f;
	for (auto it = ap_notification_icons.begin(); it != ap_notification_icons.end();)
	{
		auto& notification_icon = *it;

		if (notification_icon.state == AP_NOTIF_STATE_PENDING && previous_y > -100.0f)
		{
			notification_icon.state = AP_NOTIF_STATE_DROPPING;
		}
		if (notification_icon.state == AP_NOTIF_STATE_PENDING)
		{
			++it;
			continue;
		}

		if (notification_icon.state == AP_NOTIF_STATE_DROPPING)
		{
			notification_icon.vely += 0.15f + (float)(ap_notification_icons.size() / 4) * 0.25f;
			if (notification_icon.vely > 8.0f) notification_icon.vely = 8.0f;
			notification_icon.yf += notification_icon.vely;
			if (notification_icon.yf >= previous_y - AP_NOTIF_SIZE - AP_NOTIF_PADDING)
			{
				notification_icon.yf = previous_y - AP_NOTIF_SIZE - AP_NOTIF_PADDING;
				notification_icon.vely *= -0.3f / ((float)(ap_notification_icons.size() / 4) * 0.05f + 1.0f);

				notification_icon.t += ap_notification_icons.size() / 4 + 1; // Faster the more we have queued (4 can display on screen)
				if (notification_icon.t > 350 * 3 / 4) // ~7.5sec
				{
					notification_icon.state = AP_NOTIF_STATE_HIDING;
				}
			}
		}

		if (notification_icon.state == AP_NOTIF_STATE_HIDING)
		{
			notification_icon.velx -= 0.14f + (float)(ap_notification_icons.size() / 4) * 0.1f;
			notification_icon.xf += notification_icon.velx;
			if (notification_icon.xf < -AP_NOTIF_SIZE / 2)
			{
				it = ap_notification_icons.erase(it);
				continue;
			}
		}

		notification_icon.x = (int)notification_icon.xf;
		notification_icon.y = (int)notification_icon.yf;
		previous_y = notification_icon.yf;

		++it;
	}
}

// Remote data per slot
void ap_remote_set(const char *key, int per_slot, int value)
{
	if (ap_practice_mode)
		return;

	AP_SetServerDataRequest rq;
	if (per_slot)
		rq.key += "<Slot" + std::to_string(AP_GetPlayerID()) + ">";
	rq.key += key;
	rq.operations = { {"replace", &value} };
	rq.default_value = 0;
	rq.type = AP_DataType::Int;
	rq.want_reply = false;

	AP_SetServerData(&rq);
}

// ----------------------------------------------------------------------------
// Consistent randomness based on seed (xorshift64*)
static uint64_t xorshift_base = 0;
static uint64_t xorshift_seed = 1;

static uint64_t hash_seed(const char *str)
{
    uint64_t hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

void ap_srand(int hash)
{
	if (!xorshift_base)
		xorshift_base = hash_seed(ap_seed_string.c_str());

	xorshift_seed = xorshift_base;
	do {
		xorshift_seed += (hash * 19937) + 1;
	} while (!xorshift_seed);
}

unsigned int ap_rand(void)
{
	xorshift_seed ^= xorshift_seed << 17;
	xorshift_seed ^= xorshift_seed >> 31;
	xorshift_seed ^= xorshift_seed << 8;
	return (unsigned int)((xorshift_seed * 1181783497276652981LL) >> 32);
}
