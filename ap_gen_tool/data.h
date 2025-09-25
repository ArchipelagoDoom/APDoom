#pragma once


#include <onut/Maths.h>
#include <onut/Vector2.h>
#include <onut/Texture.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "maps.h"


typedef std::map<std::string, std::vector<std::string>> world_hook_list_t;


struct rule_connection_t
{
    int target_region = -1;
    std::vector<int> requirements_or;
    std::vector<int> requirements_and;

    bool operator==(const rule_connection_t& other) const
    {
        return 
            target_region == other.target_region &&
            requirements_or == other.requirements_or &&
            requirements_and == other.requirements_and;
    }
};


struct rule_region_t
{
    int x = 0, y = 0;
    std::vector<rule_connection_t> connections;

    bool operator==(const rule_region_t& other) const
    {
        return 
            x == other.x &&
            y == other.y &&
            connections == other.connections;
    }
};

struct bb_t
{
    int x1, y1, x2, y2;
    int region = -1;

    int overlaps(const bb_t& other) const
    {
        auto d1 = other.x2 - x1;
        if (d1 < 0) return 0;

        auto d2 = x2 - other.x1;
        if (d2 < 0) return 0;

        auto d3 = other.y2 - y1;
        if (d3 < 0) return 0;

        auto d4 = y2 - other.y1;
        if (d4 < 0) return 0;

        return onut::max(d1, d2, d3, d4);

        //return (x1 <= other.x2 && x2 >= other.x1 && 
        //        y1 <= other.y2 && y2 >= other.y1);
    }

    bb_t operator+(const Vector2& v) const
    {
        return {
            (int)(x1 + v.x),
            (int)(y1 + v.y),
            (int)(x2 + v.x),
            (int)(y2 + v.y)
        };
    }

    Vector2 center() const
    {
        return {
            (float)(x1 + x2) * 0.5f,
            (float)(y1 + y2) * 0.5f
        };
    }

    bool operator==(const bb_t& other) const
    {
        return 
            x1 == other.x1 &&
            y1 == other.y1 &&
            x2 == other.x2 &&
            y2 == other.y2 &&
            region == other.region;
    }
};


struct region_t
{
    std::string name;
    std::set<int> sectors;
    Color tint = Color::White;
    rule_region_t rules;

    bool operator==(const region_t& other) const
    {
        return 
            name == other.name &&
            sectors == other.sectors &&
            tint == other.tint &&
            rules == other.rules;
    }
};


struct location_t
{
    bool death_logic = false;
    bool unreachable = false; // Check will be removed (Unreachable area)
    bool check_sanity = false; // Removed, but option to put it back
    std::string name;
    std::string description;

    bool operator==(const location_t& other) const
    {
        return 
            death_logic == other.death_logic &&
            unreachable == other.unreachable &&
            name == other.name &&
            description == other.description;
    }
};


struct map_state_t
{
    Vector2 pos;
    float angle = 0.0f;
    int selected_bb = -1;
    int selected_region = -1;
    int selected_location = -1;
    std::vector<bb_t> bbs;
    std::vector<region_t> regions;
    rule_region_t world_rules;
    rule_region_t exit_rules;
    std::set<int> accesses;
    std::map<int, location_t> locations;
    bool different = false;

    int true_check_count; // Map check count, minus unreachable
    int check_sanity_count; // Number of check_sanity locations

    bool operator==(const map_state_t& other) const
    {
        return 
            bbs == other.bbs &&
            regions == other.regions &&
            world_rules == other.world_rules &&
            exit_rules == other.exit_rules &&
            accesses == other.accesses &&
            locations == other.locations;
    }
};


struct map_view_t
{
    Vector2 cam_pos;
    float cam_zoom = 0.25f;
};


struct map_history_t
{
    std::vector<map_state_t> history;
    int history_point = 0;
};


struct meta_t // Bad name, but everything about a level
{
    std::string name; // Name of the level
    std::string wad_name; // Which WAD it comes from
    std::string lump_name; // The lump name in the above WAD

    map_t map; // As loaded from the wad
    map_state_t state; // What we play with
    map_state_t state_new; // For diffing
    map_view_t view; // Camera zoom/position
    map_history_t history; // History of map_state_t for undo/redo (It's infinite!)
};


struct episode_info_t // separate struct because I didn't want to redo everything using the above
{
    std::string name;
    int starting_level = 0; // Only applies for major episodes, the map given at the start
    int boss_level = 0; // The default level added to "complete_specific_levels", roughly old boss levels
    bool is_minor_episode; // e.g. secret levels, too short to be a "real" episode
    bool default_enabled; // If the episode should be enabled by default in the template
};


struct ap_item_def_t
{
    int doom_type = -1;
    std::string name;
    std::string sprite;
    OTextureRef icon;

    std::vector<std::string> groups;
    int count = 0;
};


struct ap_key_def_t
{
    ap_item_def_t item;
    int key = -1;
    bool use_skull = false; // Only relevent for doom games
    std::string region_name;
    Color color;
};


struct game_t
{
    std::string full_name; // Game's canonical full name, used in the launcher
    std::string short_name; // Short name for the game, used for '-game' param

    std::string iwad_name; // Name of IWAD file that this WAD needs to run
    std::vector<std::string> required_wads; // PWAD files that are required for this game to run
    std::vector<std::string> optional_wads;
    std::vector<std::string> included_wads;

    // APWorld related things
    std::string ap_name; // Full name used on Archipelago.
    std::string ap_world_name; // Short name used by the apworld/directory.
    std::string ap_class_name; // Class name prefixed to python classes in the apworld.
    std::vector<std::string> description; // Docstring for the world class
    std::map<std::string, std::vector<std::string>> world_hooks;
    std::map<std::string, int> filler_item_weight;
    std::map<int, std::vector<int>> custom_pool_ratio;
    // Not stored in a map because we want to preserve order.
    Json::Value json_world_options;

    std::map<int, std::string> location_doom_types;
    std::vector<ap_item_def_t> extra_connection_requirements;
    std::vector<ap_item_def_t> progression;
    std::vector<ap_item_def_t> useful;
    std::vector<ap_item_def_t> filler;
    std::vector<ap_item_def_t> unique_progression;
    std::vector<ap_item_def_t> unique_useful;
    std::vector<ap_item_def_t> unique_filler;
    std::vector<ap_key_def_t> keys;
    // Should remove these two after 2.0, as they're no longer necessary
    std::map<std::string, int64_t> location_remap;
    std::map<std::string, int64_t> item_remap;

    Color key_colors[3];
    int ep_count = -1;
    std::vector<std::vector<meta_t>> episodes;
    std::vector<episode_info_t> episode_info;
    std::vector<ap_item_def_t> item_requirements;
    std::map<int, int> total_doom_types; // Count of every doom types in the game

    // Settings
    bool check_sanity = false;
    bool extended_names = false;

    // JSON structures which need to be preserved unchanged and put in output
    Json::Value json_rename_lumps;
    Json::Value json_game_info;
    Json::Value json_map_tweaks;
    Json::Value json_level_select;
};


struct level_index_t
{
    std::string game_name;
    int ep = -1;
    int map = -1;

    bool operator==(const level_index_t& other) const
    {
        return game_name == other.game_name && ep == other.ep && map == other.map;
    }

    bool operator!() const
    {
        return ep < 0 && map < 0;
    }
};


enum class active_source_t
{
    current,
    target
};


extern std::map<std::string, game_t> games;


void init_data();
game_t* get_game(const level_index_t& idx);
meta_t* get_meta(const level_index_t& idx, active_source_t source = active_source_t::current);
map_state_t* get_state(const level_index_t& idx, active_source_t source = active_source_t::current);
map_t* get_map(const level_index_t& idx);
const std::string& get_level_name(const level_index_t& idx);
const std::string& get_item_name(game_t* game, int doom_num);

long get_runtime_us(void);
const std::string compare_runtime(long start, long end = get_runtime_us());
