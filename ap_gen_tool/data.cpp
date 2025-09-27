#include "data.h"
#include "maps.h"

#include <chrono>

#include <onut/Files.h>
#include <onut/Json.h>
#include <onut/Log.h>
#include <json/json.h>

#include "message.hpp"

std::map<std::string, game_t> games;

void parse_item(ap_item_def_t &item, const Json::Value &json)
{
    item.doom_type = json["doom_type"].asInt();
    item.name = json["name"].asString();
    item.sprite = json["sprite"].asString();
    item.count = json.get("count", 1).asInt();

    if (!json["group"].isNull())
    {
        if (json["group"].isString())
        {
            item.groups.resize(1);
            item.groups[0] = json["group"].asString();
        }
        else
        {
            item.groups.resize(json["group"].size());
            for (unsigned int i = 0; i < json["group"].size(); ++i)
                item.groups[i] = json["group"][i].asString();
        }
    }
}

void parse_items(std::vector<ap_item_def_t> &items, const Json::Value &json)
{
    if (!json.isArray())
        return;
    for (const auto &value : json)
    {
        ap_item_def_t item;
        parse_item(item, value);
        items.push_back(item);
    }
}

void stringarray_to_vector(std::vector<std::string> &entry, const Json::Value &json)
{
    if (json.isString())
    {
        // Interpret a string as a single-entry array.
        entry.push_back(json.asString());
    }
    else if (json.isArray())
    {
        entry.resize(json.size());
        for (unsigned int i = 0; i < json.size(); ++i)
            entry[i] = json[i].asString();
    }
}

void init_data()
{
    long start_time = get_runtime_us();

    auto game_json_files = onut::findAllFiles("./games/", "json", false);
    for (const auto& game_json_file : game_json_files)
    {
        Json::Value game_json;
        if (!onut::loadJson(game_json, game_json_file))
        {
            OnScreenMessages::AddError(
                "Can't load '" + game_json_file + "': Json parse error.\n"
                "The terminal may have further information about this error.");
            continue;
        }
        if (
            // Test required fields
            !game_json["short_name"].isString()
            || !game_json["iwad"].isString()
            || !game_json["episodes"].isArray()
        )
        {
            OnScreenMessages::AddError(
                "Can't load '" + game_json_file + "': Missing required fields.\n"
                "The terminal may have further information about this error.");
            printf("%s : Missing a required field.\n"
                "  At minimum, the following fields are required:\n"
                "  - short_name (string)\n"
                "  - iwad (string)\n"
                "  - episodes (array of objects)\n",
                game_json_file.c_str());
            continue;
        }

        game_t game;

        // The name of the game, in various forms.
        game.ap_name = game_json.get("ap_name", "Unnamed id1 Game").asString();
        game.ap_world_name = game_json.get("ap_world_name", "id1_game").asString();
        game.ap_class_name = game_json.get("ap_class_name", "id1Game").asString();
        game.full_name = game_json.get("full_name", game.ap_name).asString();
        game.short_name = game_json["short_name"].asString();

        game.iwad_name = game_json["iwad"].asString(); // The IWAD, lumps get loaded from this if missing in PWAD
        stringarray_to_vector(game.required_wads, game_json["required_wads"]);
        stringarray_to_vector(game.optional_wads, game_json["optional_wads"]);
        stringarray_to_vector(game.included_wads, game_json["included_wads"]);

        std::string primary_wad = game.iwad_name;
        if (!game.required_wads.empty())
        {
            // Assume that if a PWAD is required, the maps we want to analyze come from that PWAD by default.
            primary_wad = game.required_wads[0];
        }

        if (!game_json["settings"].isNull())
        {
            game.check_sanity = game_json["settings"].get("check_sanity", false).asBool();
            game.extended_names = game_json["settings"].get("extended_names", false).asBool();
        }

        // Sections reserved unchanged
        game.json_rename_lumps = game_json["rename_lumps"];
        game.json_game_info = game_json["game_info"];
        game.json_map_tweaks = game_json["map_tweaks"];
        game.json_level_select = game_json["level_select"];

        game.ep_count = (int)game_json["episodes"].size();
        game.episodes.resize(game.ep_count);
        game.episode_info.resize(game.ep_count);

        int ep = 0;
        for (const auto &episode_json : game_json["episodes"])
        {
            game.episode_info[ep].name = episode_json.get("name", "Episode " + std::to_string(ep + 1)).asString();
            game.episode_info[ep].is_minor_episode = episode_json.get("minor", false).asBool();
            game.episode_info[ep].default_enabled = episode_json.get("default", true).asBool();

            if (episode_json["maps"].isArray())
            {
                int map = 0;

                game.episodes[ep].resize(episode_json["maps"].size());
                for (const auto& mapname_json : episode_json["maps"])
                {
                    game.episodes[ep][map].name = mapname_json["name"].asString();
                    game.episodes[ep][map].lump_name = mapname_json["lump"].asString();
                    game.episodes[ep][map].wad_name = (mapname_json["wad"].isNull() ? primary_wad : mapname_json["wad"].asString());
                    ++map;
                }
                if (!game.episode_info[ep].is_minor_episode)
                {
                    game.episode_info[ep].starting_level = episode_json.get("start_level", 1).asInt();
                    game.episode_info[ep].boss_level =  episode_json.get("boss_level", map).asInt();                    
                }
            }
            ++ep;
        }

        const auto& doom_types_ids = game_json["location_doom_types"].getMemberNames();
        for (const auto& doom_types_id : doom_types_ids)
        {
            game.location_doom_types[std::stoi(doom_types_id)] = game_json["location_doom_types"][doom_types_id].asString();
        }

        parse_items(game.extra_connection_requirements, game_json["extra_connection_requirements"]);
        parse_items(game.progression, game_json["progression"]);
        parse_items(game.useful, game_json["useful"]);
        parse_items(game.filler, game_json["filler"]);
        parse_items(game.unique_progression, game_json["unique_progression"]);
        parse_items(game.unique_useful, game_json["unique_useful"]);
        parse_items(game.unique_filler, game_json["unique_filler"]);

        for (const auto& key_json : game_json["keys"])
        {
            ap_key_def_t item;
            parse_item(item.item, key_json);
            item.key = key_json["key"].asInt();
            item.use_skull = key_json["use_skull"].asBool();
            item.region_name = key_json["region_name"].asString();
            item.color = Color(key_json["color"][0].asFloat(), key_json["color"][1].asFloat(), key_json["color"][2].asFloat());
            game.key_colors[item.key] = item.color;
            game.keys.push_back(item);
        }

        game.item_requirements.insert(game.item_requirements.end(), game.extra_connection_requirements.begin(), game.extra_connection_requirements.end());
        game.item_requirements.insert(game.item_requirements.end(), game.progression.begin(), game.progression.end());
        game.item_requirements.insert(game.item_requirements.end(), game.unique_progression.begin(), game.unique_progression.end());
        for (const auto& key : game.keys)
            game.item_requirements.push_back(key.item);

        if (game_json["world_info"].isObject())
        {
            Json::Value& world_json = game_json["world_info"];

            // World description: used as the docstring for the world class
            stringarray_to_vector(game.description, world_json["description"]);

            // World options: Automatic addition of common hooks and options
            game.json_world_options = Json::arrayValue;
            if (world_json["world_options"].isArray())
                game.json_world_options = world_json["world_options"];

            // World hooks: allows some extra python code in certain places, if necessary
            if (world_json["hooks"].isObject())
            {
                const auto& hook_types = world_json["hooks"].getMemberNames();
                for (const auto& hook_type : hook_types)
                    stringarray_to_vector(game.world_hooks[hook_type], world_json["hooks"][hook_type]);
            }

            // Filler item weights: Lets worlds have a weighted "helpful" filler pool
            if (world_json["filler_item_weight"].isObject())
            {
                const auto& item_names = world_json["filler_item_weight"].getMemberNames();
                for (const auto& item_name : item_names)
                    game.filler_item_weight.try_emplace(item_name, world_json["filler_item_weight"][item_name].asInt());
            }

            // Custom pool ratio: Size of the helpful and random pools relative to number of locations
            if (world_json["custom_pool_ratio"].isObject())
            {
                const auto& difficulties = world_json["custom_pool_ratio"].getMemberNames();
                for (const auto& diff : difficulties)
                {
                    const Json::Value& customratio = world_json["custom_pool_ratio"][diff];
                    int diff_int = std::stoi(diff);

                    game.custom_pool_ratio[diff_int].push_back(customratio.get("helpful", 0).asInt());
                    game.custom_pool_ratio[diff_int].push_back(customratio.get("random", 0).asInt());
                }
            }
        }
        if (game.description.empty())
            game.description.push_back(game.ap_name + " is a game playable with APDoom version 2.0.0.");

        // Temporarily support old ID remapping
        if (game_json["loc_remap"].isObject())
        {
            for (const auto& loc_name : game_json["loc_remap"].getMemberNames())
                game.location_remap[loc_name] = game_json["loc_remap"][loc_name].asInt64();
        }
        if (game_json["item_remap"].isObject())
        {
            for (const auto& loc_name : game_json["item_remap"].getMemberNames())
                game.item_remap[loc_name] = game_json["item_remap"][loc_name].asInt64();
        }

        if (init_maps(game))
            games[game.short_name] = game;
        else
        {
            OnScreenMessages::AddError(
                "Can't load '" + game_json_file + "': Wad files missing.\n"
                "The terminal may have further information about this error.");
        }
    }

    OnScreenMessages::AddNotice("Game loading complete (" + compare_runtime(start_time) + " sec)");
}


game_t* get_game(const level_index_t& idx)
{
    auto it = games.find(idx.game_name);
    if (it == games.end()) return nullptr;
    return &it->second;
}


meta_t* get_meta(const level_index_t& idx, active_source_t source)
{
    auto game = get_game(idx);
    if (!game) return nullptr;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return nullptr;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return nullptr;
    if (source == active_source_t::current ||
        source == active_source_t::target) return &game->episodes[idx.ep][idx.map];
    return nullptr;
}

map_state_t* get_state(const level_index_t& idx, active_source_t source)
{
    auto game = get_game(idx);
    if (!game) return nullptr;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return nullptr;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return nullptr;
    if (source == active_source_t::current ||
        source == active_source_t::target) return &game->episodes[idx.ep][idx.map].state;
    return nullptr;
}

map_t* get_map(const level_index_t& idx)
{
    auto game = get_game(idx);
    if (!game) return nullptr;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return nullptr;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return nullptr;
    return &game->episodes[idx.ep][idx.map].map;
}

const std::string& get_level_name(const level_index_t& idx)
{
    auto game = get_game(idx);
    static std::string err_str = "ERROR";
    if (!game) return err_str;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return err_str;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return err_str;
    return game->episodes[idx.ep][idx.map].name;
}

static inline int get_item_index(const std::vector<ap_item_def_t>& items, int doom_type)
{
    for (int i = 0; i < (int)items.size(); ++i)
    {
        if (items[i].doom_type == doom_type)
            return i;
    }
    return -1;
}

const std::string& get_item_name(game_t *game, int doom_type)
{
    static std::string no_item_str = "(no item)";

    int ret;
    if ((ret = get_item_index(game->progression, doom_type)) != -1)
        return game->progression[ret].name;
    if ((ret = get_item_index(game->useful, doom_type)) != -1)
        return game->useful[ret].name;
    if ((ret = get_item_index(game->filler, doom_type)) != -1)
        return game->filler[ret].name;
    if ((ret = get_item_index(game->unique_progression, doom_type)) != -1)
        return game->unique_progression[ret].name;
    if ((ret = get_item_index(game->unique_useful, doom_type)) != -1)
        return game->unique_useful[ret].name;
    if ((ret = get_item_index(game->unique_filler, doom_type)) != -1)
        return game->unique_filler[ret].name;
    return no_item_str;
}


// I don't know where else to put these right now...
long get_runtime_us(void)
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

const std::string compare_runtime(long start, long end)
{
    static char buf[32];
    long dur = end-start;

    snprintf(buf, sizeof(buf), "%ld.%06ld", dur/1'000'000, dur%1'000'000);
    return std::string(buf);
}
