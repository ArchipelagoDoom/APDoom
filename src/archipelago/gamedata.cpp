//
// Copyright(C) 2025 Kay "Kaito" Sinclaire
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
// Functions to handle reading game information from Json blobs.
// Replaces auto-generated C headers included at compile time.
//

#include "apdoom.h"
#include "local.hpp"

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

const char *string_to_const_char_ptr(const std::string &element)
{
	cstring_storage.emplace_back(element);
	return cstring_storage.back().c_str();
}

// Stores the name of a lump into a 9-byte char array. Does nothing if src is not a string.
static void store_lump_name(char *dest, const Json::Value& src)
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

const struct {
	rando_group_t group;
	const char *str;
} rgroup_types[NUM_RGROUPS + 1] = {
	{RGROUP_SMALL, "small"},
	{RGROUP_MEDIUM, "medium"},
	{RGROUP_BIG, "big"},
	{RGROUP_BOSS, "boss"},
	{NUM_RGROUPS}
};

ap_itemrando_t *json_parse_itemrando(const Json::Value& json)
{
	int size = 1;
	for (int i = 0; rgroup_types[i].group != NUM_RGROUPS; ++i)
	{
		if (!json.isObject() || !json.isMember(rgroup_types[i].str))
			continue;
		size += json[rgroup_types[i].str].size();
	}

	ap_itemrando_t *output = new ap_itemrando_t[size];
	int elem = 0;

	for (int i = 0; rgroup_types[i].group != NUM_RGROUPS; ++i)
	{
		if (!json.isObject() || !json.isMember(rgroup_types[i].str))
			continue;
		for (auto &element : json[rgroup_types[i].str])
		{
			output[elem].doom_type = element.asInt();
			output[elem].group = rgroup_types[i].group;
			++elem;
		}
	}

	// Write terminator.
	output[elem].doom_type = -1;
	output[elem].group = NUM_RGROUPS;

	return output;
}

int json_parse_game_info(const Json::Value& json, ap_gameinfo_t &output)
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

	output.rand_monster_types = json_parse_itemrando(json["monsters"]);
	output.rand_pickup_types = json_parse_itemrando(json["pickups"]);

	output.pause_pic = NULL;
	if (json["pausepic"].isString())
	{
		std::string pausepic = json["pausepic"].asString();
		if (!pausepic.empty())
			output.pause_pic = string_to_const_char_ptr(pausepic);
	}

	output.goal_menu_flat = NULL;
	if (json["goalmenuflat"].isString())
	{
		std::string goalmenuflat = json["goalmenuflat"].asString();
		if (!goalmenuflat.empty())
			output.goal_menu_flat = string_to_const_char_ptr(goalmenuflat);
	}

	return 1;
}


// ============================================================================
// Level Select screen definitions
// (json: "level_select")
// ============================================================================

// Parses one mapinfo structure from a JSON blob, while taking care to not overwrite
// any default options that may have already been set.
static void json_parse_single_mapinfo(ap_levelselect_map_t *info, const Json::Value& json)
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

int json_parse_level_select(const Json::Value& json, level_select_storage_t &output)
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

static void insert_new_tweak(std::vector<ap_maptweak_t> &tweak_list, allowed_tweaks_t type, int target, const Json::Value& value)
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

static void parse_hub_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
{
	// There's only one thing that can be tweaked with the hub, so the target is ignored
	insert_new_tweak(tweak_list, TWEAK_HUB_X, 0, json["x"]);
	insert_new_tweak(tweak_list, TWEAK_HUB_Y, 0, json["y"]);	
}

static void parse_things_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_X,     target, json[key_target]["x"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_Y,     target, json[key_target]["y"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_TYPE,  target, json[key_target]["type"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_ANGLE, target, json[key_target]["angle"]);
		insert_new_tweak(tweak_list, TWEAK_MAPTHING_FLAGS, target, json[key_target]["flags"]);
	}
}

static void parse_sectors_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
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

static void parse_linedefs_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
{
	for (std::string &key_target : json.getMemberNames())
	{
		const int target = std::stoi(key_target);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_SPECIAL, target, json[key_target]["special"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_TAG,     target, json[key_target]["tag"]);
		insert_new_tweak(tweak_list, TWEAK_LINEDEF_FLAGS,   target, json[key_target]["flags"]);
	}
}

static void parse_sidedefs_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
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

static void parse_metadata_tweak_block(const Json::Value& json, std::vector<ap_maptweak_t> &tweak_list)
{
	// Metadata is level-wide stuff, so the target is ignored
	insert_new_tweak(tweak_list, TWEAK_META_BEHAVES_AS, 0, json["behaves_as"]);
}

int json_parse_map_tweaks(const Json::Value& json, map_tweaks_storage_t &output)
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

int json_parse_location_types(const Json::Value& json, location_types_storage_t &output)
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

int json_parse_location_table(const Json::Value& json, location_table_storage_t &output)
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

int json_parse_item_table(const Json::Value& json, item_table_storage_t &output)
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
		std::string name = json_value[0].asString();
		const int doomednum = json_value[1].asInt();
		const int ep = json_value.get(2, -1).asInt();
		const int map = json_value.get(3, -1).asInt();

		output.insert({ap_item_id, {doomednum, ep, map, string_to_const_char_ptr(name)}});
	}	
	return 1;
}


// ============================================================================
// Type sprites - used for the notification icons, maps AP items to sprites
// (json: "type_sprites")
// ============================================================================

int json_parse_type_sprites(const Json::Value& json, type_sprites_storage_t &output)
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

int json_parse_level_info(const Json::Value& json, level_info_storage_t &output)
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
			new_level.true_check_count = 0;

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
					new_level.thing_infos[idx].location_id = -1;
				}
				else
				{
					// Things which _are_ AP items are stored as an array.
					// [0] is the doomednum, [1] is the location num.
					new_level.thing_infos[idx].doom_type = map_things[idx][0].asInt();
					new_level.thing_infos[idx].location_id = map_things[idx][1].asInt64();
					++new_level.check_count;
					// true_check_count is handled after connect (when we know suppressed locations)
				}
			}

			// Copy structure into our vector
			output[ep][map] = new_level;
		}
	}
	return 1;
}


// ============================================================================
// Rename lumps: Rarely used way of renaming lumps in loaded WADs
// (json: "rename_lumps")
// ============================================================================

int json_parse_rename_lumps(const Json::Value& json, rename_lumps_storage_t &output)
{
	if (json.isNull())
		return 1; // Optional

	for (const std::string &file_name : json.getMemberNames())
	{
		std::string lower_file_name;
		for (const unsigned char c : file_name)
			lower_file_name += tolower(c);

		auto result = output.try_emplace(lower_file_name);
		if (!result.second)
		{
			printf("APDOOM: Duplicate WAD file found in rename_lumps\n");
			return 0;
		}

		std::vector<remap_entry_t>& rename_list = (*result.first).second;
		for (const std::string &rename_from : json[file_name].getMemberNames())
			rename_list.emplace_back(rename_from.c_str(), json[file_name][rename_from].asCString());
	}
	return 1;
}


// ============================================================================
// Obituaries: Death messages for DeathLink
// (json: "game_info" -> "obituaries")
// ============================================================================

int json_parse_obituaries(const Json::Value& json, obituary_storage_t &output)
{
	if (json.isNull())
		return 1; // Optional

	for (const std::string &tag_list : json.getMemberNames())
	{
		std::string obituary_text = json[tag_list].asString();
		output.push_back({tag_list, obituary_text});
	}
	return 1;
}

