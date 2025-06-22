//
// This source file is copyright (C) 2024 Kay "Kaito" Sinclaire,
// released under the terms of the GNU General Public License, version 2 or later.
//
// This code reads AP Doom information from a Json blob,
// replacing hardcoded C headers which previously accomplished this task.
//

#ifndef _APDOOM_PWAD_
#define _APDOOM_PWAD_

#include <vector>
#include <set>
#include <map>
#include <json/json.h>

// ===== JSON PARSING =========================================================

typedef std::vector<ap_hint_autocomplete_t>
	hint_autocomplete_storage_t;
typedef std::vector<ap_levelselect_t>
	level_select_storage_t;
typedef std::map<int, std::map<int, std::vector<ap_maptweak_t>>>
	map_tweaks_storage_t;
typedef std::set<int>
	location_types_storage_t;
typedef std::map<int, std::map<int, std::map<int, int64_t>>>
	location_table_storage_t;
typedef std::map<int64_t, ap_item_t>
	item_table_storage_t;
typedef std::map<int, std::string>
	type_sprites_storage_t;
typedef std::vector<std::vector<ap_level_info_t>>
	level_info_storage_t;

int json_parse_game_info(Json::Value json, ap_gameinfo_t &output);
int json_parse_hint_autocomplete(Json::Value json, hint_autocomplete_storage_t &output);
int json_parse_level_select(Json::Value json, level_select_storage_t &output);
int json_parse_map_tweaks(Json::Value json, map_tweaks_storage_t &output);
int json_parse_location_types(Json::Value json, location_types_storage_t &output);
int json_parse_location_table(Json::Value json, location_table_storage_t &output);
int json_parse_item_table(Json::Value json, item_table_storage_t &output);
int json_parse_type_sprites(Json::Value json, type_sprites_storage_t &output);
int json_parse_level_info(Json::Value json, level_info_storage_t &output);

// ===== OTHER THINGS =========================================================

std::string do_hint_replacement(const char *msg, hint_autocomplete_storage_t &ac_list);

#endif // _APDOOM_PWAD_
