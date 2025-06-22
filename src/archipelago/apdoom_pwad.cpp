//
// This source file is copyright (C) 2024 Kay "Kaito" Sinclaire,
// released under the terms of the GNU General Public License, version 2 or later.
//
// This code reads AP Doom information from a Json blob,
// replacing hardcoded C headers which previously accomplished this task.
//

#include "apdoom.h"
#include "apdoom_pwad.hpp"

#include <vector>
#include <set>
#include <map>
#include <deque>
#include <string>
#include <sstream>
#include <json/json.h>

// Quick and easy storage for strings that need to be put into C structs as const char *
// Stored in a deque so we can quickly add a new name to the back, do back().c_str(), and never think about it again
static std::deque<std::string> cstring_storage;

const char *string_to_const_char_ptr(std::string element)
{
	cstring_storage.emplace_back(element);
	return cstring_storage.back().c_str();
}

// Stores the name of a lump into a 9-byte char array. Does nothing if src is not a string.
static void store_lump_name(char *dest, Json::Value src)
{
	if (!src.isString())
		return;

	strncpy(dest, src.asCString(), 8);
	dest[8] = 0;
}

// Gets a level index from a lump name such as "MAP15" or "E2M4"
static ap_level_index_t ap_get_index_from_map_name(const char *lump_name)
{
	if (strlen(lump_name) < 4)
		return {-1, -1};

	int gameepisode = 0;
	int gamemap = atoi(&lump_name[3]);

	if (strncmp(lump_name, "MAP", 3) == 0)
		gameepisode = 1;
	else if (lump_name[0] == 'E' && lump_name[1] >= '1' && lump_name[1] <= '9' && lump_name[2] == 'M')
		gameepisode = (lump_name[1] - '0');
	else
		return {-1, -1};
	return ap_try_make_level_index(gameepisode, gamemap);
}


// ============================================================================
// Base game info - Stuff like weapon and ammo names, etc
// (json: "game_info")
// ============================================================================

int json_parse_game_info(Json::Value json, ap_gameinfo_t &output)
{
	std::map<std::string, int> reverse_ammo_map;

	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'game_info'.\n");
		return 0;
	}

	output.named_ammo_count = (int)json["ammo"].size();
	output.ammo_types = new ap_ammo_info_t[output.named_ammo_count];
	for (int i = 0; i < output.named_ammo_count; ++i)
	{
		Json::Value json_ammo = json["ammo"][i];
		std::string ammo_type_str = json_ammo["name"].asString();

		output.ammo_types[i].name = string_to_const_char_ptr(ammo_type_str);
		output.ammo_types[i].max_ammo = json_ammo["max"].asInt();

		reverse_ammo_map.insert({ammo_type_str, i});
	}

	output.named_weapon_count = (int)json["weapons"].size();
	output.weapons = new ap_weapon_info_t[output.named_weapon_count];
	for (int i = 0; i < output.named_weapon_count; ++i)
	{
		Json::Value json_weapon = json["weapons"][i];

		output.weapons[i].name = string_to_const_char_ptr(json_weapon["name"].asString());

		if (json_weapon["ammo_type"].isNull())
		{
			output.weapons[i].ammo_type = -1;
			output.weapons[i].start_ammo = 0;
		}
		else if (json_weapon["ammo_type"].isInt())
		{
			output.weapons[i].ammo_type = json_weapon["ammo_type"].asInt() - 1;
			output.weapons[i].start_ammo = json_weapon.get("starting_ammo", 0).asInt();
		}
		else
		{
			std::string ammo_type_str = json_weapon["ammo_type"].asString();
			if (reverse_ammo_map.count(ammo_type_str) == 0)
			{
				printf("APDOOM: Ammo type '%s' doesn't exist.\n", ammo_type_str.c_str());
				return 0;
			}
			output.weapons[i].ammo_type = reverse_ammo_map[ammo_type_str];
			output.weapons[i].start_ammo = json_weapon.get("starting_ammo", 0).asInt();

		}
	}

	output.start_health = json.get("starting_health", 100).asInt();
	output.start_armor = json.get("starting_armor", 0).asInt();

	if (!json["pausepic"].isNull())
		output.pausepic = string_to_const_char_ptr(json["pausepic"].asString());
	else
		output.pausepic = NULL;

	return 1;
}


// ============================================================================
// Hint auto completion
// (json: "game_info/hint_auto_complete")
// ============================================================================

int json_parse_hint_autocomplete(Json::Value json, hint_autocomplete_storage_t &output)
{
	if (json.isNull())
		return 1; // Optional

	const int hint_count = (int)json.size();
	output.resize(hint_count);

	int idx = 0;
	for (std::string &key_input : json.getMemberNames())
	{
		output[idx].input = string_to_const_char_ptr(key_input);

		if (json[key_input].isArray())
		{
			if (key_input == "RED")
				output[idx].key_id = 2;
			else if (key_input == "YELLOW")
				output[idx].key_id = 1;
			else
				output[idx].key_id = 0;
			output[idx].replace_normal = string_to_const_char_ptr(json[key_input][0].asString());
			output[idx].replace_skull = string_to_const_char_ptr(json[key_input][1].asString());
		}
		else
		{
			output[idx].key_id = -1;
			output[idx].replace_normal = string_to_const_char_ptr(json[key_input].asString());
			output[idx].replace_skull = NULL;
		}
		++idx;
	}
	return 1;
}


// ============================================================================
// Level Select screen definitions
// (json: "level_select")
// ============================================================================

// Parses one mapinfo structure from a JSON blob, while taking care to not overwrite
// any default options that may have already been set.
static void json_parse_single_mapinfo(ap_levelselect_map_t *info, const Json::Value json)
{
	info->x = json.get("x", info->x).asInt();
	info->y = json.get("y", info->y).asInt();

	if (!json["cursor"].isNull())
	{
		store_lump_name(info->cursor.graphic, json["cursor"]["graphic"]);
		info->cursor.x = json["cursor"].get("x", info->cursor.x).asInt();
		info->cursor.y = json["cursor"].get("y", info->cursor.y).asInt();
	}

	if (!json["map_name"].isNull())
	{
		if (!json["map_name"]["text"].isNull())
		{
			info->map_name.text = string_to_const_char_ptr(json["map_name"]["text"].asString());
			info->map_name.graphic[0] = '\0';
		}
		else if (!json["map_name"]["graphic"].isNull())
		{
			store_lump_name(info->map_name.graphic, json["map_name"]["graphic"]);
			info->map_name.text = NULL;
		}
		info->map_name.x = json["map_name"].get("x", info->map_name.x).asInt();
		info->map_name.y = json["map_name"].get("y", info->map_name.y).asInt();
	}

	if (!json["keys"].isNull())
	{
		if (!json["keys"]["relative_to"].isNull())
		{
			std::string result = json["keys"]["relative_to"].asString();
			if (result == "map")                 info->keys.relative_to = 0;
			else if (result == "map-name")       info->keys.relative_to = 1;
			else if (result == "map-name-right") info->keys.relative_to = 2;
		}
		info->keys.x = json["keys"].get("x", info->keys.x).asInt();
		info->keys.y = json["keys"].get("y", info->keys.y).asInt();
		info->keys.spacing_x = json["keys"].get("spacing_x", info->keys.spacing_x).asInt();
		info->keys.spacing_y = json["keys"].get("spacing_y", info->keys.spacing_y).asInt();
		info->keys.align_x = json["keys"].get("align_x", info->keys.align_x).asInt();
		info->keys.align_y = json["keys"].get("align_y", info->keys.align_y).asInt();
		info->keys.checkmark_x = json["keys"].get("checkmark_x", info->keys.checkmark_x).asInt();
		info->keys.checkmark_y = json["keys"].get("checkmark_y", info->keys.checkmark_y).asInt();		
		info->keys.use_checkmark = json["keys"].get("use_checkmark", info->keys.use_checkmark).asBool();
	}

	if (!json["checks"].isNull())
	{
		if (!json["checks"]["relative_to"].isNull())
		{
			std::string result = json["checks"]["relative_to"].asString();
			if (result == "map")                 info->checks.relative_to = 0;
			else if (result == "map-name")       info->checks.relative_to = 1;
			else if (result == "map-name-right") info->checks.relative_to = 2;
			else if (result == "keys")           info->checks.relative_to = 3;
			else if (result == "keys-last")      info->checks.relative_to = 4;
		}
		info->checks.x = json["checks"].get("x", info->checks.x).asInt();
		info->checks.y = json["checks"].get("y", info->checks.y).asInt();
	}
}

int json_parse_level_select(Json::Value json, level_select_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'level_select'.\n");
		return 0;
	}

	// Defaults for level select mapinfo, if not specified anywhere else.
	char default_map_image[9] = "INTERPIC";
	int default_map_names = -1; // Top
	ap_levelselect_map_t default_mapinfo;
	memset(&default_mapinfo, 0, sizeof(ap_levelselect_map_t));

	// Specifying defaults?
	if (!json["defaults"].isNull())
	{
		json_parse_single_mapinfo(&default_mapinfo, json["defaults"]["maps"]);
		store_lump_name(default_map_image, json["defaults"]["background_image"]);

		if (!json["defaults"]["map_name_position"].isNull())
		{
			std::string pos_str = json["defaults"]["map_name_position"].asString();
			if (pos_str == "top")             default_map_names = -1;
			else if (pos_str == "bottom")     default_map_names = 1;
			else if (pos_str == "individual") default_map_names = 0;
		}
	}

	const int ep_count = (int)json["episodes"].size();
	output.resize(ep_count);

	for (int idx = 0; idx < ep_count; ++idx)
	{
		Json::Value episode_defs = json["episodes"][idx];

		if (!episode_defs["background_image"].isNull())
			store_lump_name(output[idx].background_image, episode_defs["background_image"]);
		else
			memcpy(output[idx].background_image, default_map_image, 9);

		if (!episode_defs["map_name_position"].isNull())
		{
			std::string pos_str = episode_defs["map_name_position"].asString();
			if (pos_str == "top")             output[idx].map_names = -1;
			else if (pos_str == "bottom")     output[idx].map_names = 1;
			else if (pos_str == "individual") output[idx].map_names = 0;
		}
		else
			output[idx].map_names = default_map_names;

		const int map_count = (int)episode_defs["maps"].size();
		for (int map_idx = 0; map_idx < map_count; ++map_idx)
		{
			memcpy(&output[idx].map_info[map_idx], &default_mapinfo, sizeof(ap_levelselect_map_t));
			json_parse_single_mapinfo(&output[idx].map_info[map_idx], episode_defs["maps"][map_idx]);
		}
	}
	return 1;
}


// ============================================================================
// Map tweaks - softlock removal, other AP quality of life things
// (json: "map_tweaks")
// ============================================================================

static void insert_new_tweak(std::vector<ap_maptweak_t> &tweak_list, allowed_tweaks_t type, int target, Json::Value value)
{
	if (value.isNull())
		return;

	ap_maptweak_t new_tweak = {type, target, 0, ""};
	if (value.isString())
		store_lump_name(new_tweak.string, value);
	else if (value.isInt())
		new_tweak.value = value.asInt();
	else if (value.isBool())
		new_tweak.value = value.asBool();

	tweak_list.emplace_back(new_tweak);
}

static void parse_hub_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	// There's only one thing that can be tweaked with the hub, so the target is ignored
	insert_new_tweak(tweak_list, TWEAK_HUB_X, 0, json["x"]);
	insert_new_tweak(tweak_list, TWEAK_HUB_Y, 0, json["y"]);	
}

static void parse_things_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_X,     target, json[key_target]["x"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_Y,     target, json[key_target]["y"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_TYPE,  target, json[key_target]["type"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_ANGLE, target, json[key_target]["angle"]);
	}
}

static void parse_sectors_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_SPECIAL,     target, json[key_target]["special"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_TAG,         target, json[key_target]["tag"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_FLOOR,       target, json[key_target]["floor"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_FLOOR_PIC,   target, json[key_target]["floor_pic"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_CEILING,     target, json[key_target]["ceiling"]);
		insert_new_tweak(tweak_list, TWEAK_SECTOR_CEILING_PIC, target, json[key_target]["ceiling_pic"]);
	}
}

static void parse_linedefs_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_SPECIAL, target, json[key_target]["special"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_TAG,     target, json[key_target]["tag"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_FLAGS,   target, json[key_target]["flags"]);
	}
}

static void parse_sidedefs_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_LOWER,  target, json[key_target]["lower"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_MIDDLE, target, json[key_target]["middle"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_UPPER,  target, json[key_target]["upper"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_X,      target, json[key_target]["x"]);
		insert_new_tweak(tweak_list, TWEAK_SIDEDEF_Y,      target, json[key_target]["y"]);
	}
}

static void parse_metadata_tweak_block(Json::Value json, std::vector<ap_maptweak_t> &tweak_list)
{
	// Metadata is level-wide stuff, so the target is ignored
	insert_new_tweak(tweak_list, TWEAK_META_BEHAVES_AS, 0, json["behaves_as"]);
}

int json_parse_map_tweaks(Json::Value json, map_tweaks_storage_t &output)
{
	if (json.isNull())
		return 1; // Optional

	for (std::string &map_lump_name : json.getMemberNames())
	{
		ap_level_index_t idx = ap_get_index_from_map_name(map_lump_name.c_str());
		if (idx.ep == -1)
		{
			printf("APDOOM: 'map_tweaks' contains invalid map name '%s'.\n", map_lump_name.c_str());
			return 0;
		}

		output.insert({idx.ep, {}});
		output[idx.ep].insert({idx.map, {}});

		for (std::string &tweak_type : json[map_lump_name].getMemberNames())
		{
			if (tweak_type == "hub")
				parse_hub_tweak_block(json[map_lump_name]["hub"], output[idx.ep][idx.map]);
			else if (tweak_type == "things")
				parse_things_tweak_block(json[map_lump_name]["things"], output[idx.ep][idx.map]);
			else if (tweak_type == "sectors")
				parse_sectors_tweak_block(json[map_lump_name]["sectors"], output[idx.ep][idx.map]);
			else if (tweak_type == "linedefs")
				parse_linedefs_tweak_block(json[map_lump_name]["linedefs"], output[idx.ep][idx.map]);
			else if (tweak_type == "sidedefs")
				parse_sidedefs_tweak_block(json[map_lump_name]["sidedefs"], output[idx.ep][idx.map]);
			else if (tweak_type == "metadata")
				parse_metadata_tweak_block(json[map_lump_name]["metadata"], output[idx.ep][idx.map]);
			else
				printf("APDOOM: Unknown tweak section '%s', ignoring\n", tweak_type.c_str());
		}
	}
#if 0
	for (auto &it_ep : output)
	{
		for (auto &it_map : it_ep.second)
		{
			for (ap_maptweak_t &it : it_map.second)
			{
				printf("(%i, %i): [%02x] %i %i %s\n", it_ep.first, it_map.first, it.type, it.target, it.value, it.string);
			}
		}
	}
#endif
	return 1;
}


// ============================================================================
// Location type list - replaces "is_<game>_type_ap_location"
// (json: "location_types")
// ============================================================================

int json_parse_location_types(Json::Value json, location_types_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'location_types'.\n");
		return 0;
	}

	for (auto &doomednum : json)
		output.emplace(doomednum.asInt());
	return 1;
}


// ============================================================================
// Location table - list of all AP location IDs assigned to each level
// (json: "location_table")
// ============================================================================

int json_parse_location_table(Json::Value json, location_table_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'location_table'.\n");
		return 0;
	}

	for (std::string &key_episode : json.getMemberNames())
	{
		const int episode_num = std::stoi(key_episode);
		output.insert({episode_num, {}});

		for (std::string &key_map : json[key_episode].getMemberNames())
		{
			const int map_num = std::stoi(key_map);
			output[episode_num].insert({map_num, {}});

			Json::Value items_in_map = json[key_episode][key_map];
			for (std::string &key_item_idx : items_in_map.getMemberNames())
			{
				const int item_idx = std::stoi(key_item_idx);
				const int64_t ap_item_id = items_in_map[key_item_idx].asInt64();
				output[episode_num][map_num].insert({item_idx, ap_item_id});
			}
		}
	}
	return 1;
}


// ============================================================================
// Item table - list of all items we can receive from AP
// (json: "item_table")
// ============================================================================

int json_parse_item_table(Json::Value json, item_table_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'item_table'.\n");
		return 0;
	}

	for (std::string &json_key : json.getMemberNames())
	{
		const int64_t ap_item_id = std::stoll(json_key);

		Json::Value json_value = json[json_key];
		const int doomednum = json_value[0].asInt();
		const int ep = json_value.get(1, -1).asInt();
		const int map = json_value.get(2, -1).asInt();

		output.insert({ap_item_id, {doomednum, ep, map}});
	}	
	return 1;
}


// ============================================================================
// Type sprites - used for the notification icons, maps AP items to sprites
// (json: "type_sprites")
// ============================================================================

int json_parse_type_sprites(Json::Value json, type_sprites_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'type_sprites'.\n");
		return 0;
	}

	for (std::string &json_key : json.getMemberNames())
	{
		const int doomednum = std::stoi(json_key);
		output.insert({doomednum, json[json_key].asString()});
	}
	return 1;
}


// ============================================================================
// Level info - big autogenerated list of details APDoom needs for each level
// (json: "level_info")
// ============================================================================

int json_parse_level_info(Json::Value json, level_info_storage_t &output)
{
	if (json.isNull())
	{
		printf("APDOOM: Definitions missing required 'level_info'.\n");
		return 0;
	}

	const int episode_count = (int)json.size();
	output.resize(episode_count);

	ap_level_info_t new_level;
	for (int ep = 0; ep < episode_count; ++ep)
	{
		const int map_count = (int)json[ep].size();
		output[ep].resize(map_count);

		for (int map = 0; map < map_count; ++map)
		{
			Json::Value map_info = json[ep][map];

			cstring_storage.emplace_back(map_info["_name"].asString());
			new_level.name = cstring_storage.back().c_str();

			new_level.game_episode = map_info["game_map"][0].asInt();
			new_level.game_map = map_info["game_map"][1].asInt();
			new_level.keys[0] = map_info["key"][0].asBool();
			new_level.keys[1] = map_info["key"][1].asBool();
			new_level.keys[2] = map_info["key"][2].asBool();
			new_level.use_skull[0] = map_info["use_skull"][0].asBool();
			new_level.use_skull[1] = map_info["use_skull"][1].asBool();
			new_level.use_skull[2] = map_info["use_skull"][2].asBool();

			// These used to be stored in the structures, but are now recalculated as we load.
			new_level.thing_count = map_info["thing_list"].size();
			new_level.check_count = 0;
			new_level.sanity_check_count = 0;

			if (new_level.thing_count > AP_MAX_THING)
			{
				printf("APDOOM: %s: Too many things! The max is %i\n", new_level.name, AP_MAX_THING);
				return 0;
			}

			new_level.thing_infos = new ap_thing_info_t[new_level.thing_count];
			Json::Value map_things = map_info["thing_list"];
			for (int idx = 0; idx < new_level.thing_count; ++idx)
			{
				new_level.thing_infos[idx].index = idx;
				if (map_things[idx].isInt())
				{
					// Things which are not AP items are only stored as their doomednum.
					new_level.thing_infos[idx].doom_type = map_things[idx].asInt();
					new_level.thing_infos[idx].check_sanity = false;
					new_level.thing_infos[idx].unreachable = true;
				}
				else
				{
					// Things which _are_ AP items are stored as an array.
					// [0] is the doomednum, [1] is the checksanity boolean.
					new_level.thing_infos[idx].doom_type = map_things[idx][0].asInt();
					new_level.thing_infos[idx].check_sanity = map_things[idx][1].asBool();
					new_level.thing_infos[idx].unreachable = false;
					++new_level.check_count;
					// sanity_check_count was always handled later, anyway.
				}
			}

			// Copy structure into our vector
			output[ep][map] = new_level;
		}
	}
	return 1;
}


// ============================================================================
// Other functions, not directly related to parsing definitions
// ============================================================================

std::string do_hint_replacement(const char *msg, hint_autocomplete_storage_t &ac_list)
{
	std::stringstream s(msg);

	// Note: The Doom engine always converts chat text to all caps.
	if (strncmp(msg, "!HINT ", 6) != 0)
		return s.str();

	const char *p = msg + 6;
	while (*p && *p == ' ')
		++p; // Advance to next non-space

	// Get the map at the given index; if there isn't one, do not change anything
	ap_level_index_t idx = ap_get_index_from_map_name(p);
	if (idx.ep >= 0)
	{
		ap_level_info_t *level_info = ap_get_level_info(idx);

		while (*p && *p != ' ')
			++p; // Advance to next space
		while (*p && *p == ' ')
			++p; // Advance to next non-space

		if (*p)
		{
			// If the string continues on, string compare what's left with our hint_auto_completes.
			for (ap_hint_autocomplete_t &hint : ac_list)
			{
				if (strcmp(p, hint.input) == 0)
				{
					s.str("");
					s << "!hint " << level_info->name << " - ";
					if (hint.key_id < 0 || !level_info->use_skull[hint.key_id])
						s << hint.replace_normal;
					else
						s << hint.replace_skull;

					break;
				}
			}
		}
		else
		{
			// If it doesn't continue on from here, we wanted to hint the level unlock item.
			s.str("");
			s << "!hint " << level_info->name;
		}
	}
	return s.str();
}
