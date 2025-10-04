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

// ===== LUMP REMAPPING =======================================================

struct remap_entry_t
{
    char _from[8]; // not null terminated
    char _to[8]; // not null terminated

    remap_entry_t(const char *from, const char *to)
    {
        strncpy(_from, from, 8);
        strncpy(_to, to, 8);
    }

    bool rename(char *lump_name)
    {
        char copy[8]; // not null terminated

        for (int i = 0, copy_pos = 0; i < 8; ++i)
        {
            if (_from[i] == '?')
                copy[copy_pos++] = lump_name[i];
            else if (_from[i] != lump_name[i])
                return false;
            else if (_from[i] == '\0')
                break;
        }
        for (int i = 0, copy_pos = 0; i < 8; ++i)
        {
            lump_name[i] = (_to[i] == '?' ? copy[copy_pos++] : _to[i]);
            if (_to[i] == '\0')
                break;
        }
        return true;
    }
};

// ===== OBITUARIES ===========================================================

class obituary_t
{
    int _score;
    std::set<std::string> tags;
    std::string obituary;

    // Used to bias certain tag matches over others.
    static int bias_score(const std::string &new_tag)
    {
        if (new_tag == "TELEFRAG")
            return 10000;
        if (new_tag == "SPLASH")
            return 1000;
        if (new_tag.substr(0, 10) == "INFLICTOR_" || new_tag == "SUICIDE")
            return 100;
        if (new_tag.substr(0, 7) == "SOURCE_" || new_tag == "CRUSHER")
            return 10;
        return 1;
    }

public:
    obituary_t(const std::string &tag_list, const std::string &text) : obituary(text)
    {
        size_t origpos = 0;
        size_t newpos = tag_list.find(',');
        while (newpos != std::string::npos)
        {
            tags.insert(tag_list.substr(origpos, newpos-origpos));
            origpos = newpos + 1;
            newpos = tag_list.find(',', origpos);
        }
        tags.insert(tag_list.substr(origpos));

        _score = 0;
        for (const std::string& tag : tags)
            _score += bias_score(tag);
    }

    int score(const std::set<std::string>& wanted_tags)
    {
        for (const std::string& tag : tags)
        {
            if (!wanted_tags.count(tag))
                return -1;
        }
        return _score;
    }

    const std::string& get_text()
    {
        return obituary;
    }
};

// ===== JSON PARSING =========================================================

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
typedef std::map<std::string, std::vector<remap_entry_t>>
    rename_lumps_storage_t;
typedef std::vector<obituary_t>
    obituary_storage_t;

int json_parse_game_info(const Json::Value& json, ap_gameinfo_t &output);
int json_parse_level_select(const Json::Value& json, level_select_storage_t &output);
int json_parse_map_tweaks(const Json::Value& json, map_tweaks_storage_t &output);
int json_parse_location_types(const Json::Value& json, location_types_storage_t &output);
int json_parse_location_table(const Json::Value& json, location_table_storage_t &output);
int json_parse_item_table(const Json::Value& json, item_table_storage_t &output);
int json_parse_type_sprites(const Json::Value& json, type_sprites_storage_t &output);
int json_parse_level_info(const Json::Value& json, level_info_storage_t &output);
int json_parse_rename_lumps(const Json::Value& json, rename_lumps_storage_t &output);
int json_parse_obituaries(const Json::Value& json, obituary_storage_t &output);

#endif // _APDOOM_PWAD_
