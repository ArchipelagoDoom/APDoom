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
// *Generates python files for Archipelago, also C and C++ headers for APDOOM*
//

#include <stdio.h>
#include <cinttypes>
#include <string.h>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <fstream>
#include <onut/onut.h>
#include <onut/Files.h>
#include <onut/Json.h>
#include <onut/Strings.h>
#include <onut/Log.h>

#ifndef _WIN32
#include <sys/stat.h>
#endif

#include "maps.h"
#include "generate.h"
#include "data.h"

#include <algorithm>

#include "message.hpp"
#include "python.hpp"
#include "zip.hpp"


#define DOOM_TYPE_LEVEL_UNLOCK   -1
#define DOOM_TYPE_LEVEL_COMPLETE -2

enum item_classification_t
{
    FILLER          = 0b00000, // aka trash, as in filler items like ammo, currency etc,
    PROGRESSION     = 0b00001, // Item that is logically relevant
    USEFUL          = 0b00010, // Item that is generally quite useful, but not required for anything logical
    TRAP            = 0b00100, // detrimental or entirely useless (nothing) item
    SKIP_BALANCING  = 0b01000,
    DEPRIORITIZED   = 0b10000,
};
inline item_classification_t operator|(item_classification_t a, item_classification_t b)
{
    return static_cast<item_classification_t>(static_cast<int>(a) | static_cast<int>(b));
}

struct ap_item_t
{
    int64_t id;
    std::string name;
    level_index_t idx;
    int doom_type;
    int count;
    bool is_key;
    item_classification_t classification;
};


struct ap_location_t
{
    int64_t id;
    location_t* loc_state = nullptr;
    std::string name;
    int x, y;
    level_index_t idx;
    int doom_thing_index;
    int doom_type;
    std::vector<int64_t> required_items;
    int sector;
    int region = -1;
    std::string region_name;
    bool check_sanity = false;
};


struct connection_t
{
    int sector;
    std::vector<int64_t> required_items; // OR
};


struct level_sector_t
{
    bool visited = false;
    std::vector<connection_t> connections;
    std::vector<int> locations;
};


struct level_t
{
    level_index_t idx;
    std::string name;
    std::string group_name; //lump_name
    std::vector<level_sector_t> sectors;
    int starting_sector = -1;
    bool keys[3] = {false};
    int location_count = 0;
    bool use_skull[3] = {false};
    map_t* map = nullptr;
    map_state_t* map_state = nullptr;
};

int64_t location_next_id = 0;

std::vector<ap_item_t> ap_items;
std::vector<ap_location_t> ap_locations;
std::map<std::string, std::set<std::string>> item_name_groups;
std::map<uintptr_t, std::map<int, int64_t>> level_to_keycards;
std::map<std::string, ap_item_t*> item_map;

static bool use_extended_names = false; // gross hack, but whatever

const char* get_doom_type_name(int doom_type);

static inline int get_location_id_base(level_index_t& idx)
{
    return ((idx.ep + 1) * 100'000) + ((idx.map + 1) * 1'000);
}

static inline int get_item_id_base(level_index_t& idx)
{
    return ((idx.ep + 1) * 10'000'000) + ((idx.map + 1) * 100'000);
}

static std::string get_group_name(meta_t& metamap)
{
    // In the overwhelming majority of cases, this will just wind up matching lump_name.
    // However, in some rare circumstances (D2+NRFTL), we need to display a "fake" lump name to the player
    // to mask the fact that we're loading some maps into other slots.
    // This lets us get away with that little bit of cheating.
    const auto pos_begin = metamap.name.find_last_of('(');
    const auto pos_end = metamap.name.find_last_of(')');
    if (pos_begin == std::string::npos || pos_end == std::string::npos)
        return metamap.lump_name;
    return metamap.name.substr(pos_begin + 1, pos_end - (pos_begin + 1));
}

static std::string get_requirement_name(game_t* game, const std::string& level_name, int doom_type)
{
    for (const auto& item : game->unique_progression)
        if (item.doom_type == doom_type)
            return level_name + " - " + item.name;

    for (const auto& item : game->keys)
        if (item.item.doom_type == doom_type)
            return level_name + " - " + item.item.name;

    for (const auto& requirement : game->item_requirements)
        if (requirement.doom_type == doom_type)
            return requirement.name;

    return "ERROR";
}

// For option-based requirements that cause a connection to get entirely removed if not true
static std::string get_extra_requirement_name(game_t* game, int doom_type)
{
    for (const auto& requirement : game->extra_connection_requirements)
        if (requirement.doom_type == doom_type)
            return requirement.name;

    return "ERROR";
}


bool loc_name_taken(const std::string& name)
{
    for (const auto& loc : ap_locations)
    {
        if (loc.name == name) return true;
    }
    return false;
}

void add_loc(const std::string& name, const map_thing_t& thing, level_t* level, int index, int id)
{
    location_t *loc_state = &level->map_state->locations[index];
    // Make sure it's not unreachable
    if (loc_state->unreachable) return;
    if (location_next_id > 999)
    {
        OLogE("Maximum number of locations reached for Episode "
            + std::to_string(level->idx.ep + 1)
            + " Map "
            + std::to_string(level->idx.map + 1)
        );
        return;
    }        

    int count = 0;

    std::string extended_name = loc_state->name;

    std::string loc_name = name;
    if (use_extended_names && extended_name.length() > 0)
        loc_name = name + " (" + extended_name + ")";

    while (loc_name_taken(loc_name))
    {
        ++count;
        if (use_extended_names && extended_name.length() > 0)
            loc_name = name + " " + std::to_string(count + 1) + " (" + extended_name + ")";
        else
            loc_name = name + " " + std::to_string(count + 1);
    }

    ap_location_t loc;
    loc.name = loc_name;
    loc.idx = level->idx;
    loc.id = get_location_id_base(loc.idx) + id;
    loc.doom_thing_index = index;
    loc.doom_type = thing.type; // Index can be a risky one. We could replace the item by it's type if it's unique enough
    loc.x = thing.x << 16;
    loc.y = thing.y << 16;
    loc.check_sanity = loc_state->check_sanity;
    loc.loc_state = loc_state;
    ap_locations.push_back(loc);

    level->location_count++;
}

void add_item_name_groups(const std::string name, const std::vector<std::string> groups, level_t *level = nullptr)
{
    std::string replacement = (level) ? level->group_name : "NULL";

    for (const std::string &group : groups)
    {
        if (group.empty())
            continue;

        std::string new_group = group;
        size_t map_marker = new_group.find("%MAP%");
        if (map_marker != std::string::npos)
            new_group.replace(map_marker, 5, replacement);

        item_name_groups[new_group].insert(name);
    }
}

int64_t add_unique(const ap_key_def_t &key_def, item_classification_t classification, const map_thing_t& thing, level_t* level, int index)
{
    std::string name = level->name + std::string(" - ") + key_def.item.name;

    for (const auto& other_item : ap_items)
    {
        if (other_item.name == name)
            return other_item.id;
    }

    ap_item_t item;
    item.is_key = true;
    item.count = 1;
    item.classification = classification;

    item.name = name;
    item.idx = level->idx;
    item.doom_type = key_def.item.doom_type;
    item.id = get_item_id_base(item.idx) + item.doom_type;

    add_item_name_groups(name, key_def.item.groups, level);
    ap_items.push_back(item);
    return item.id;
}

ap_item_t& add_item(const ap_item_def_t &item_def, item_classification_t classification, level_t* level = nullptr)
{
    ap_item_t item;
    item.is_key = false;
    item.count = item_def.count;
    item.classification = classification;
    item.doom_type = item_def.doom_type;

    int base_item_id = item.doom_type;
    if (item.doom_type < 0) switch (item.doom_type)
    {
    case DOOM_TYPE_LEVEL_UNLOCK:   base_item_id = 0;     break;
    case DOOM_TYPE_LEVEL_COMPLETE: base_item_id = 99999; break;
    default: OLogE("Unknown special doom_type " + std::to_string(item.doom_type)); break;
    }

    if (level)
    {
        item.name = item_def.name.empty()
            ? level->name
            : level->name + std::string(" - ") + item_def.name;
        item.idx = level->idx;
        item.id = get_item_id_base(item.idx) + base_item_id;
    }
    else
    {
        item.name = item_def.name;
        item.idx = level_index_t{"",-2,-2};
        item.id = base_item_id;
    }

    add_item_name_groups(item.name, item_def.groups, level);
    ap_items.push_back(item);
    return ap_items.back();
}


std::string escape_csv(const std::string& str)
{
    std::string ret = str;
    for (int i = 0; i < (int)ret.size(); ++i)
    {
        auto c = ret[i];
        if (c == '"')
        {
            ret.insert(ret.begin() + i, '"');
            ++i;
        }
    }
    return "\"" + ret + "\"";
}


typedef std::map<int, std::vector<level_t*>> level_map_t;

//
Json::Value make_connection(game_t *game, const rule_connection_t& connection,
    const std::string& level_name, const std::string &region_name)
{
    Json::Value connection_json;
    connection_json["_target"] = region_name;

    // For option requirements. These discard the entire connection if the option isn't set
    Json::Value requires_json = Json::arrayValue;

    // For regular rules. Stored as an array, to allow for the possibility of multiple sets of rules
    Json::Value rules_json = Json::arrayValue;
    std::vector<std::string> rules_and;
    std::vector<std::string> rules_or;

    for (int doom_type : connection.requirements_and)
    {
        if (doom_type < 0)
            requires_json.append(get_extra_requirement_name(game, doom_type));
        else
            rules_and.push_back(get_requirement_name(game, level_name, doom_type));
    }
    for (int doom_type : connection.requirements_or)
        rules_or.push_back(get_requirement_name(game, level_name, doom_type));

    if (!rules_and.empty())
    {
        rules_json[0]["and"] = Json::arrayValue;
        for (const std::string &rule : rules_and)
            rules_json[0]["and"].append(rule);
    }
    if (!rules_or.empty())
    {
        rules_json[0]["or"] = Json::arrayValue;
        for (const std::string &rule : rules_or)
            rules_json[0]["or"].append(rule);
    }

    if (!requires_json.empty())
        connection_json["requires"] = requires_json;
    if (!rules_json.empty())
        connection_json["rules"] = rules_json;

    return connection_json;
}

// --------------

Json::Value generate_apworld_manifest(game_t *game, const Json::Value &apdoom_json)
{
    static char buf[9];
    time_t dt = time(NULL);
    strftime(buf, sizeof(buf), "%Y%m%d", localtime(&dt));

    Json::Value json;
    json["version"] = 7;
    json["compatible_version"] = 7;
    json["game"] = game->ap_name;
    json["world_version"] = "2.0." + std::string(buf);
    json["__apdoom"] = apdoom_json;
    return json;
}

Json::Value generate_game_defs_json(game_t *game, level_map_t& levels_map)
{
    Json::Value defs_json;

    { // Output location table
        for (const auto& loc : ap_locations)
        {
            const std::string& episode = std::to_string(loc.idx.ep + 1);
            const std::string& map = std::to_string(loc.idx.map + 1);
            const std::string& thing_id = std::to_string(loc.doom_thing_index);
            defs_json["location_table"][episode][map][thing_id] = loc.id;
        }
    }

    { // Output item table
        for (const auto& item : ap_items)
        {
            const std::string& item_id = std::to_string(item.id);
            defs_json["item_table"][item_id][0] = item.name;
            defs_json["item_table"][item_id][1] = item.doom_type;
            if (item.idx.ep >= 0)
            {
                defs_json["item_table"][item_id][2] = item.idx.ep + 1;
                defs_json["item_table"][item_id][3] = item.idx.map + 1;
            }
        }
    }

    // Output level info
    for (int ep = 0; ep < game->ep_count; ++ep)
    {
        int map = 0;
        for (const auto& meta : game->episodes[ep])
        {
            level_t *level = levels_map[ep][map];
            Json::Value json_level;

            json_level["_name"] = level->name;
            json_level["key"][0] = level->keys[0];
            json_level["key"][1] = level->keys[1];
            json_level["key"][2] = level->keys[2];
            json_level["use_skull"][0] = level->use_skull[0];
            json_level["use_skull"][1] = level->use_skull[1];
            json_level["use_skull"][2] = level->use_skull[2];

            // Split out lump name into gameepisode/gamemap that can easily be used by APDoom
            const char *lump_name = meta.lump_name.c_str();
            if (strncmp(lump_name, "MAP", 3) == 0)
                json_level["game_map"][0] = 1;
            else
                json_level["game_map"][0] = (lump_name[1] - '0');
            json_level["game_map"][1] = std::atoi(lump_name + 3);

            Json::Value json_mts;
            int idx = 0;
            for (const auto& thing : level->map->things)
            {
                for (const auto& loc : ap_locations)
                {
                    if (loc.idx == level->idx && loc.doom_thing_index == idx)
                    {
                        json_mts[idx][0] = thing.type;
                        json_mts[idx][1] = loc.id;
                        break;
                    }
                }
                if (json_mts[idx].isNull())
                    json_mts[idx] = thing.type;

                ++idx;
            }
            json_level["thing_list"] = json_mts;

            defs_json["level_info"][ep][map++] = json_level;
        }
    }

    // Output item sprites (used by notification icons)
    {
        for (const auto& item : game->progression)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& item : game->useful)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& item : game->filler)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& item : game->unique_progression)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& item : game->unique_useful)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& item : game->unique_filler)
            defs_json["type_sprites"][std::to_string(item.doom_type)] = item.sprite;
        for (const auto& key : game->keys)
            defs_json["type_sprites"][std::to_string(key.item.doom_type)] = key.item.sprite;
    }

    // Output AP location types
    {
        int idx = 0;
        for (const auto& location_doom_type_kv : game->location_doom_types)
            defs_json["ap_location_types"][idx++] = location_doom_type_kv.first;
    }

    // Extra structures in data JSON file intended for use in APDoom
    if (!game->json_game_info.isNull())
        defs_json["game_info"] = game->json_game_info;
    if (!game->json_map_tweaks.isNull())
        defs_json["map_tweaks"] = game->json_map_tweaks;
    if (!game->json_level_select.isNull())
        defs_json["level_select"] = game->json_level_select;
    if (!game->json_rename_lumps.isNull())
        defs_json["rename_lumps"] = game->json_rename_lumps;

    return defs_json;
}

// --------------

// This is a mess. Many refactors. Sorry...
// This function is bulky... I've tried to split it up where I can, but there's still a lot. -KS
int generate(game_t* game)
{
    OLog("AP Gen Tool version 2.0");
    onut::createFolder("output");
    long runtime_start = get_runtime_us();

    ap_items.clear();
    ap_locations.clear();
    item_name_groups.clear();
    level_to_keycards.clear();
    item_map.clear();

    int warning_count_no_exit = 0;
    int warning_count_empty_region = 0;
    int warning_count_unknown_world_options = WorldOptions_Init(game);

    ap_locations.reserve(600);
    ap_items.reserve(300);

    use_extended_names = game->extended_names;

    for (const auto& def : game->progression)
        add_item(def, PROGRESSION);
    for (const auto& def : game->useful)
        add_item(def, USEFUL);
    for (const auto& def : game->filler)
        add_item(def, FILLER);
    
    std::vector<level_t*> levels;
    std::map<int, std::vector<level_t*>> levels_map;
    int ep = 0;
    for (auto& episode : game->episodes)
    {
        int map = 0;
        for (auto& meta : episode)
        {
            level_t* level = new level_t();
            level->idx = {game->short_name, ep, map};
            level->name = meta.name;
            level->group_name = get_group_name(meta);
            level->map = &meta.map;
            level->map_state = &meta.state;
            levels.push_back(level);
            levels_map[ep].push_back(level);
            ++map;
        }
        ++ep;
    }

    auto get_level = [&levels_map](const level_index_t& idx) -> level_t*
    {
        return levels_map[idx.ep][idx.map];
    };
    
    // Keycard n such
    for (auto level : levels)
    {
        int next_loc = 1;

        auto map = level->map;
        level->sectors.resize(map->sectors.size());
        std::string lvl_prefix = level->name + std::string(" - ");

        for (int i = 0, len = (int)level->map->things.size(); i < len; ++i)
        {
            const auto& thing = level->map->things[i];
            auto loc_it = game->location_doom_types.find(thing.type);

            if (
                loc_it == game->location_doom_types.end() // Not a location
                || (thing.flags & 0x0010) // Multiplayer only flag set
            )
                continue;

            for (const auto& key_def : game->keys)
            {
                if (key_def.item.doom_type == thing.type)
                {
                    level_to_keycards[(uintptr_t)level][0] = add_unique(key_def, PROGRESSION, thing, level, i);
                    level->keys[key_def.key] = true;
                    level->use_skull[key_def.key] = key_def.use_skull;
                    break;
                }
            }

            add_loc(lvl_prefix + loc_it->second, thing, level, i, next_loc++);
        }

        // Make exit location
        ap_location_t complete_loc;
        complete_loc.doom_thing_index = -1;
        complete_loc.doom_type = DOOM_TYPE_LEVEL_COMPLETE;
        complete_loc.idx = level->idx;
        complete_loc.x = complete_loc.y = -1;
        complete_loc.name = lvl_prefix + "Exit";
        complete_loc.id = get_location_id_base(complete_loc.idx);

        for (const auto& region : level->map_state->regions)
        {
            const std::string region_name = level->name + std::string(" @ ") + region.name;

            for (const auto& connection : region.rules.connections)
            {
                if (connection.target_region == -2)
                {
                    complete_loc.region_name = region_name;
                    goto found_exit_connection;
                }
            }
        }
        OLogW(level->name + " has no region that connects to the Exit.");
        complete_loc.region_name = "Hub @ Entrance to " + level->name;
        ++warning_count_no_exit;

    found_exit_connection:
        ap_locations.push_back(complete_loc);
    }

    // Lastly, add level items. We want to add more levels in the future and not shift all existing item IDs
    ap_item_def_t level_unlock_item;
    level_unlock_item.doom_type = -1;
    level_unlock_item.count = 1;
    level_unlock_item.groups.push_back("Levels");
    level_unlock_item.groups.push_back("%MAP%");

    ap_item_def_t level_complete_item;
    level_complete_item.doom_type = -2;
    level_complete_item.count = 0;
    level_complete_item.name = "Complete";

    for (auto level : levels)
    {
        add_item(level_unlock_item, PROGRESSION|USEFUL, level);
        add_item(level_complete_item, PROGRESSION, level);

        for (const auto& def : game->unique_progression)
            add_item(def, PROGRESSION, level);
        for (const auto& def : game->unique_useful)
            add_item(def, USEFUL, level);
        for (const auto& def : game->unique_filler)
            add_item(def, FILLER, level);
    }

    // Temporary(?) support for old remapping IDs
    if (!game->location_remap.empty())
    {
        int64_t next_location_id = 0;
        std::vector<int> unmapped_locations;
        int i = 0;
        for (const auto& kv : game->location_remap)
            next_location_id = std::max(next_location_id, kv.second + 1);
        for (auto& location : ap_locations)
        {
            auto it = game->location_remap.find(location.name);
            if (it != game->location_remap.end())
                location.id = game->location_remap[location.name];
            else
                unmapped_locations.push_back(i);
            ++i;
        }
        for (auto unmapped_location : unmapped_locations)
            ap_locations[unmapped_location].id = next_location_id++;
    }
    if (!game->item_remap.empty())
    {
        int64_t next_itemn_id = 0;
        std::vector<int> unmapped_items;
        int i = 0;
        for (const auto &kv : game->item_remap)
            next_itemn_id = std::max(next_itemn_id, kv.second + 1);
        for (auto& item : ap_items)
        {
            auto it = game->item_remap.find(item.name);
            if (it != game->item_remap.end())
                item.id = game->item_remap[item.name];
            else
                unmapped_items.push_back(i);
            ++i;
        }
        for (auto unmapped_item : unmapped_items)
            ap_items[unmapped_item].id = next_itemn_id++;
    }

    // Sort item and location IDs for cleanliness
    std::sort(ap_locations.begin(), ap_locations.end(), [](const ap_location_t& a, const ap_location_t& b) { return a.id < b.id; });
    std::sort(ap_items.begin(), ap_items.end(), [](const ap_item_t& a, const ap_item_t& b) { return a.id < b.id; });

    // Fill in locations into level's sectors
    for (int i = 0, len = (int)ap_locations.size(); i < len; ++i)
    {
        auto& loc = ap_locations[i];
        if (loc.doom_thing_index < 0) continue;
        auto level = get_level(loc.idx);
        auto subsector = point_in_subsector(loc.x, loc.y, get_map(loc.idx));
        if (subsector)
        {
            level->sectors[subsector->sector].locations.push_back(i);
            loc.sector = subsector->sector;
        }
        else
        {
            OLogE("Cannot find sector for location: " + loc.name);
        }
    }

    // Last minute checks
#define ERROR_JUNK_GROUP      0b001
#define ERROR_MAJOR_EPISODE   0b010
#define ERROR_DEFAULT_EPISODE 0b100
    unsigned int errored = 0b111;

    for (const auto& kv : item_name_groups)
    {
        if (kv.first == "Junk")
            errored &= ~ERROR_JUNK_GROUP;
    }
    for (const auto &episode : game->episode_info)
    {
        if (!episode.is_minor_episode)
            errored &= ~ERROR_MAJOR_EPISODE;
        if (episode.default_enabled)
            errored &= ~ERROR_DEFAULT_EPISODE;
    }

    if (errored)
    {
        std::vector<std::string> error_list;
        error_list.push_back("The following errors prevented generation of an APWorld:");
        if (errored & ERROR_JUNK_GROUP) error_list.push_back("A 'Junk' item group must exist.");
        if (errored & ERROR_MAJOR_EPISODE) error_list.push_back("There must be at least one major episode.");
        if (errored & ERROR_DEFAULT_EPISODE) error_list.push_back("There must be at least one episode enabled by default.");

        for (const std::string& error : error_list)
        {
            OLogE(error);
            OnScreenMessages::AddError(error);
        }

        for (auto level : levels) delete level;
        return 1;
    }

    OLog(std::to_string(ap_locations.size()) + " locations, " + std::to_string(ap_items.size()) + " items");

    // ------------------------------------------------------------------------
    // APWorld output begins here
    // ------------------------------------------------------------------------
    long runtime_output = get_runtime_us();

    const std::string zip_world_path = game->ap_world_name + "/";
    const std::string zip_wad_path = zip_world_path + "wad/";
    ZipFile world;

    Json::Value ap_json;
    { // Regions
        Json::Value allregions_json = Json::arrayValue;

        // We split up the Hub like so:
        // - The Hub is one giant region at the start of the file
        // - Each level present in the game has a subregion in the Hub called "Entrance to (level)"
        //   - The main Hub region connects to every single one of these subregions
        //   - The above connection contains the level unlock item requirement
        // - The "Entrance to (level)" subregion rules are stored with each level's rules
        //   - This is to keep all weapon/key logic for a level together
        {
            Json::Value hubregion_json;
            hubregion_json["_name"] = "Hub";
            hubregion_json["connections"] = Json::arrayValue;
            for (auto level : levels)
            {
                Json::Value connection_json;
                connection_json["_target"] = "Hub @ Entrance to " + level->name;
                connection_json["rules"][0]["and"][0] = level->name;
                hubregion_json["connections"].append(connection_json);
            }
            allregions_json.append(hubregion_json);
        }

        // Regions
        for (auto level : levels)
        {
            const std::string& level_name = level->name;

            {
                Json::Value region_json;
                region_json["_name"] = "Hub @ Entrance to " + level_name;
                region_json["exmx"][0] = level->idx.ep + 1;
                region_json["exmx"][1] = level->idx.map + 1;

                Json::Value allconnections_json = Json::arrayValue;

                for (const rule_connection_t& world_connection : level->map_state->world_rules.connections)
                {
                    const region_t& target_region = level->map_state->regions[world_connection.target_region];
                    const std::string region_name = level_name + " @ " + target_region.name;

                    allconnections_json.append(make_connection(game, world_connection, level_name, region_name));
                }

                region_json["connections"] = allconnections_json;
                allregions_json.append(region_json);
            }

            for (std::size_t region_i = 0; region_i < level->map_state->regions.size(); ++region_i)
            {
                const region_t& region = level->map_state->regions[region_i];
                Json::Value region_json;
                const std::string region_name = level_name + " @ " + region.name;

                // Set up region name in locations for later.
                for (auto sectori : region.sectors)
                {
                    for (auto loci : level->sectors[sectori].locations)
                        ap_locations[loci].region_name = region_name;
                }

                // Gather all connections.
                Json::Value allconnections_json = Json::arrayValue;
                for (const rule_connection_t& connection : region.rules.connections)
                {
                    std::string target_region_name;
                    if (connection.target_region == -2)
                        continue; // Connection to Exit -- Not actually needed due to exits being event locations
                    if (connection.target_region == -1)
                        target_region_name = "Hub @ Entrance to " + level_name;
                    else
                        target_region_name = level_name + " @ " + level->map_state->regions[connection.target_region].name;

                    allconnections_json.append(make_connection(game, connection, level_name, target_region_name));
                }

                region_json["_name"] = region_name;
                region_json["exmx"][0] = level->idx.ep + 1;
                region_json["exmx"][1] = level->idx.map + 1;
                region_json["connections"] = allconnections_json;
                allregions_json.append(region_json);
            }
        }

        ap_json["regions"] = allregions_json;
        //world.AddJson(zip_world_path + "regions.json", ap_json, false);
    }
    { // Items
        Json::Value itemtable_json;
        Json::Value itemgroups_json;

        for (const auto& item : ap_items)
        {
            const std::string &item_id = std::to_string(item.id);

            Json::Value itemdata_json;
            itemdata_json["_name"] = item.name;
            itemdata_json["classification"] = item.classification;
            itemdata_json["doom_type"] = item.doom_type;
            if (item.count > 0)
                itemdata_json["count"] = item.count;
            if (item.idx.ep >= 0)
            {
                itemdata_json["exmx"][0] = item.idx.ep + 1;
                itemdata_json["exmx"][1] = item.idx.map + 1;
            }

            itemtable_json[item_id] = itemdata_json;
        }

        // item_name_groups
        for (const auto& kv : item_name_groups)
        {
            itemgroups_json[kv.first] = Json::arrayValue;
            for (const auto& item_name : kv.second)
                itemgroups_json[kv.first].append(item_name);
        }

        ap_json["item_table"] = itemtable_json;
        ap_json["item_name_groups"] = itemgroups_json;
        //world.AddJson(zip_world_path + "items.json", ap_json, false);
    }
    { // Locations
        Json::Value locations_json;
        Json::Value locgroups_json;
        Json::Value deathlogic_json;

        for (const auto& location : ap_locations)
        {
            const std::string &level_name = get_level_name(location.idx);
            const std::string &loc_id = std::to_string(location.id);
            std::string region_name = location.region_name;

            if (region_name.empty())
            {
                OLogW("Location '" + location.name + "' is not marked as unreachable, and is not associated with a region.");
                region_name = "Hub @ Entrance to " + level_name;
                ++warning_count_empty_region;
            }

            // Location data
            Json::Value locdata_json = Json::objectValue;
            locdata_json["_name"] = location.name;
            locdata_json["doom_type"] = location.doom_type;
            locdata_json["exmx"][0] = location.idx.ep + 1;
            locdata_json["exmx"][1] = location.idx.map + 1;
            //locdata_json["index"] = location.doom_thing_index;
            locdata_json["region"] = region_name;
            if (game->check_sanity && location.check_sanity)
                locdata_json["check_sanity"] = true;
            locations_json[loc_id] = locdata_json;


            // Location name groups
            if (locgroups_json[level_name].isNull())
                locgroups_json[level_name] = Json::arrayValue;
            locgroups_json[level_name].append(location.name);

            // Death Logic locations
            if (location.loc_state && location.loc_state->death_logic)
                deathlogic_json.append(location.name);
        }

        ap_json["location_table"] = locations_json;
        ap_json["location_name_groups"] = locgroups_json;
        ap_json["death_logic_excluded_locations"] = deathlogic_json;
        //world.AddJson(zip_world_path + "locations.json", ap_json, false);
    }
    { // Starting levels
        Json::Value startlevels_json = Json::objectValue;
        for (size_t ep = 0; ep < game->episode_info.size(); ++ep)
        {
            int start_level = game->episode_info[ep].starting_level - 1;
            if (start_level >= (int)levels_map[ep].size() || start_level < 0)
                continue;
            startlevels_json[std::to_string(ep + 1)] = levels_map[ep][start_level]->name;
        }

        ap_json["starting_levels_by_episode"] = startlevels_json;
        //world.AddJson(zip_world_path + "start.json", ap_json, false);
    }
    { // World info
        Json::Value customratio_json = Json::objectValue;
        Json::Value filleritem_json = Json::objectValue;

        for (const auto& pool : game->custom_pool_ratio)
        {
            std::string key = std::to_string(pool.first);
            customratio_json[key] = Json::arrayValue;
            customratio_json[key][0] = pool.second[0];
            customratio_json[key][1] = pool.second[1];
        }
        for (const auto& weight : game->filler_item_weight)
            filleritem_json[weight.first] = weight.second;

        if (!customratio_json.empty())
            ap_json["custom_pool_ratio"] = customratio_json;
        if (!filleritem_json.empty())
            ap_json["filler_item_weight"] = filleritem_json;
        //world.AddJson(zip_world_path + "filler.json", ap_json, false);
    }
    world.AddJson(zip_world_path + game->short_name + ".data.json", ap_json, false);

    // ========================================================================

    // Start making the info json that the launcher uses
    // It needs a lot of varying info about other parts of the world
    Json::Value info_json;
    info_json["short_name"] = game->short_name;
    info_json["iwad"] = game->iwad_name;

    if (game->full_name != game->ap_name)
        info_json["full_name"] = game->full_name;

    if (!game->required_wads.empty())
    {
        info_json["wads_required"] = Json::arrayValue;
        for (std::string &wad : game->required_wads)
            info_json["wads_required"].append(wad);
    }
    if (!game->optional_wads.empty())
    {
        info_json["wads_optional"] = Json::arrayValue;
        for (std::string &wad : game->optional_wads)
            info_json["wads_optional"].append(wad);
    }

    // Include extra data wads
    for (const std::string &wad_path : game->included_wads)
    {
        const auto dirsep = wad_path.find_last_of('/');
        std::string wad_name = wad_path.substr(dirsep == std::string::npos ? 0 : dirsep + 1);
        if (!world.AddFile(zip_wad_path + wad_name, wad_path))
        {
            OnScreenMessages::AddError("Couldn't add " + wad_path + " to the APWorld!");
            continue;
        }

        if (info_json["wads_included"].isNull())
            info_json["wads_included"] = Json::arrayValue;
        info_json["wads_included"].append(zip_wad_path + wad_name);
    }

    // Generate the game def json that contains all the info for apdoom
    std::string defs_path = zip_world_path + game->short_name + ".game.json";
    world.AddJson(defs_path, generate_game_defs_json(game, levels_map));
    info_json["definitions"] = defs_path;

    // Lastly generate the apworld manifest
    world.AddJson(zip_world_path + "archipelago.json", generate_apworld_manifest(game, info_json));

    // ========================================================================

    std::vector<PyOption> opts;
    { // Insert goal options
        PyOption& opt_numlevels = opts.emplace_back("goal_num_levels", "Goal: Number of Levels", PyOptionType::Range);
        opt_numlevels.docstring.push_back("If 'Complete Some Levels' or 'Complete Random Levels' are chosen as the goal, this is how many levels must be completed.");
        opt_numlevels.option_group = "Goal Options";
        opt_numlevels.range_start = 1;
        opt_numlevels.range_end = levels.size();
        opt_numlevels.default_int = levels.size();

        PyOption& opt_speclevels = opts.emplace_back("goal_specific_levels", "Goal: Specific Levels", PyOptionType::OptionSet);
        opt_speclevels.docstring.push_back("If 'Complete Specific Levels' is chosen as the goal, all levels chosen here must be completed.");
        opt_speclevels.option_group = "Goal Options";
        for (auto level : levels)
            opt_speclevels.option_list.push_back(level->name);
        for (size_t ep = 0; ep < game->episode_info.size(); ++ep)
        {
            int boss_level = game->episode_info[ep].boss_level - 1;
            if (boss_level >= (int)levels_map[ep].size() || boss_level < 0)
                continue;
            opt_speclevels.default_list.push_back(levels_map[ep][boss_level]->name);
        }
    }

    // Insert episode options, if there's more than one episode (It's pointless for just one)
    if (game->episode_info.size() > 1)
    {
        for (size_t ep = 0; ep < game->episode_info.size(); ++ep)
        {
            PyOption &opt_ep = opts.emplace_back("episode" + std::to_string(ep + 1), "Episode " + std::to_string(ep + 1), PyOptionType::Episode);
            opt_ep.option_group = "Episodes to Play";
            opt_ep.docstring.push_back(game->episode_info[ep].name + ".");
            if (game->episode_info[ep].is_minor_episode)
                opt_ep.docstring.push_back("This is a minor episode. Another episode must be played alongside this one.");
            opt_ep.docstring.push_back("");
            opt_ep.docstring.push_back("This episode includes the following levels:");
            for (auto level : levels_map[ep])
                opt_ep.docstring.push_back("- " + level->name);
            opt_ep.is_minor_episode = game->episode_info[ep].is_minor_episode;
            opt_ep.default_int = (game->episode_info[ep].default_enabled ? 1 : 0);
        }
    }

    // Heretic doesn't support level flipping.
    if (game->iwad_name == "HERETIC.WAD")
        opts.emplace_back("flip_levels", PyOptionType::Removed);

    // Add in "check sanity" option if the game uses it.
    if (game->check_sanity)
        opts.emplace_back("check_sanity", PyOptionType::CheckSanity);

    WorldOptions_MixinPyOptions(game, opts);

    world.AddSStream(zip_world_path + "options.py", Py_CreateOptionsPy(game, opts));
    world.AddSStream(zip_world_path + "__init__.py", Py_CreateInitPy(game));

    int result = 0;
    result += world.AddFile(zip_world_path + "id1common/__init__.py", "./assets/py/id1common/__init__.py");
    result += world.AddFile(zip_world_path + "id1common/options.py", "./assets/py/id1common/options.py");
    result += world.AddFile(zip_world_path + "id1common/LICENSE", "./assets/py/id1common/LICENSE");
    if (result != 3)
        OnScreenMessages::AddError("Couldn't add the id1common Python module to the APWorld!");

    // ========================================================================

#if 0
    // Generate location CSV that will be used for names
    {
        FILE* fout = fopen((pop_tracker_data_dir + game->short_name + "_location_names.csv").c_str(), "w");

        fprintf(fout, "Map,Type,Index,Name,Description\n");

        for (auto level : levels)
        {
            fprintf(fout, ",,,,\n");

            for (const auto& location_kv : level->map_state->locations)
            {
                const auto& location = location_kv.second;
                if (location.unreachable) continue;

                int index = location_kv.first;

                fprintf(fout, "%s,", level->name.c_str());
                fprintf(fout, "%s,", game->location_doom_types[level->map->things[index].type].c_str());
                fprintf(fout, "%i,", index);
                fprintf(fout, "%s,", escape_csv(location.name).c_str());
                fprintf(fout, "%s,\n", escape_csv(location.description).c_str());
            }
        }
        fclose(fout);
    }
#endif

    long runtime_end = get_runtime_us();

    if (warning_count_unknown_world_options)
        OnScreenMessages::AddWarning(std::to_string(warning_count_unknown_world_options) + " unknown world option(s) found.");        
    if (warning_count_no_exit)
        OnScreenMessages::AddWarning(std::to_string(warning_count_no_exit) + " level(s) are missing Exit connections.");
    if (warning_count_empty_region)
        OnScreenMessages::AddWarning(std::to_string(warning_count_empty_region) + " location(s) are not associated with any regions.");

    std::string apworld_zip_path = "./output/" + game->ap_world_name + ".apworld";
    if (!world.Output(apworld_zip_path))
        OnScreenMessages::AddError("Couldn't create file '" + apworld_zip_path + "'.");
    OnScreenMessages::AddNotice("Created world '" + apworld_zip_path + "' successfully (" + compare_runtime(runtime_start, runtime_end) + "sec.)");

    OLog("Generation complete: " 
        + compare_runtime(runtime_start, runtime_end) + " sec. total, "
        + compare_runtime(runtime_start, runtime_output) + " sec. assembling, "
        + compare_runtime(runtime_output, runtime_end) + " sec. output");

    // TODO: Pop tracker logic

    // Clean up
    for (auto level : levels) delete level;
    return 0;
}
