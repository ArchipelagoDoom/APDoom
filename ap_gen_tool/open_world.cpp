#include <onut/onut.h>
#include <onut/Settings.h>
#include <onut/Renderer.h>
#include <onut/PrimitiveBatch.h>
#include <onut/Window.h>
#include <onut/Input.h>
#include <onut/SpriteBatch.h>
#include <onut/Texture.h>
#include <onut/Json.h>
#include <onut/Dialogs.h>
#include <onut/Random.h>
#include <onut/Timing.h>
#include <onut/Font.h>

#include <imgui/imgui.h>

#include <vector>
#include <set>

#include "maps.h"
#include "generate.h"
#include "defs.h"
#include "data.h"

#include "message.hpp"


enum class state_t
{
    idle,
    panning,
    //gen,
    //gen_panning,
    move_bb,
    move_rule,
    connecting_rule,
    //set_rules
};


enum class tool_t : int
{
    bb,
    region,
    rules,
    locations,
    access
};


#define RULES_W 1024
#define RULES_H 400
#define RULE_CONNECTION_OFFSET 64.0f
#define BIG_DOOR_W 128
#define BIG_DOOR_H 128
#define SMALL_DOOR_W 64
#define SMALL_DOOR_H 72




static region_t world_region = {
    "Hub",
    {},
    Color(0.6f, 0.6f, 0.6f, 1.0f),
    {}
};

static region_t exit_region = {
    "Exit",
    {},
    Color(0.6f, 0.6f, 0.6f, 1.0f),
    {}
};



level_index_t active_level;

static active_source_t active_source = active_source_t::current;
static state_t state = state_t::idle;
static tool_t tool = tool_t::locations;
static Vector2 mouse_pos;
static Vector2 mouse_pos_on_down;
static Vector2 cam_pos_on_down;
static bb_t bb_on_down;
static map_state_t* map_state = nullptr;
static map_view_t* map_view = nullptr;
static map_history_t* map_history = nullptr;
static OTextureRef ap_icon;
static OTextureRef ap_deathlogic_icon;
static OTextureRef ap_unreachable_icon;
static OTextureRef ap_check_sanity_icon;
static OTextureRef ap_player_start_icon;
static OTextureRef ap_wing_icon;
static int mouse_hover_bb = -1;
static int mouse_hover_sector = -1;
static int moving_edge = -1;
#if defined(WIN32)
static HCURSOR arrow_cursor = 0;
static HCURSOR we_cursor = 0;
static HCURSOR ns_cursor = 0;
static HCURSOR nswe_cursor = 0;
#endif
//static bool generating = true;
//static map_state_t* flat_levels[EP_COUNT * MAP_COUNT]; // For DOOM1 open world
//static int gen_step_count = 0;
static bool painted = false;
static int moving_rule = -3;
static int mouse_hover_connection = -1;
static int mouse_hover_connection_rule = -3;
static Point rule_pos_on_down;
static int mouse_hover_rule = -3;
static int connecting_rule_from = -3;
static int set_rule_rule = -3;
static int set_rule_connection = -1;
//static int mouse_hover_access = -1;
static int mouse_hover_location = -1;


map_view_t* get_view(const level_index_t& idx)
{
    auto game = get_game(idx);
    if (!game) return nullptr;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return nullptr;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return nullptr;
    return &game->episodes[idx.ep][idx.map].view;
}

map_history_t* get_history(const level_index_t& idx)
{
    auto game = get_game(idx);
    if (!game) return nullptr;
    if (idx.ep < 0 || idx.ep >= (int)game->episodes.size()) return nullptr;
    if (idx.map < 0 || idx.map >= (int)game->episodes[idx.ep].size()) return nullptr;
    return &game->episodes[idx.ep][idx.map].history;
}


const char* ERROR_STR = "ERROR";


const char* get_doom_type_name(const level_index_t& idx, int doom_type)
{
    auto game = get_game(idx);
    auto it = game->location_doom_types.find(doom_type);
    if (it == game->location_doom_types.end()) return ERROR_STR;
    return it->second.c_str();
}


Json::Value serialize_rules(const rule_region_t& rules)
{
    Json::Value json;

    json["x"] = rules.x;
    json["y"] = rules.y;

    Json::Value connections_json(Json::arrayValue);
    for (const auto& connection : rules.connections)
    {
        Json::Value connection_json;

        connection_json["target_region"] = connection.target_region;

        {
            Json::Value requirements_json(Json::arrayValue);
            for (auto requirement : connection.requirements_or)
            {
                requirements_json.append(requirement);
            }
            connection_json["requirements_or"] = requirements_json;
        }

        {
            Json::Value requirements_json(Json::arrayValue);
            for (auto requirement : connection.requirements_and)
            {
                requirements_json.append(requirement);
            }
            connection_json["requirements_and"] = requirements_json;
        }

        connections_json.append(connection_json);
    }
    json["connections"] = connections_json;

    return json;
}


rule_region_t deserialize_rules(const Json::Value& json)
{
    rule_region_t rules;

    rules.x = json.get("x", 0).asInt();
    rules.y = json.get("y", 0).asInt();

    const auto& connections_json = json["connections"];
    for (const auto& connection_json : connections_json)
    {
        rule_connection_t connection;

        connection.target_region = connection_json.get("target_region", -1).asInt();
        
        {
            const auto& requirements_json = connection_json["requirements_or"];
            for (const auto& requirement_json : requirements_json)
            {
                connection.requirements_or.push_back(requirement_json.asInt());
            }
        }
        {
            const auto& requirements_json = connection_json["requirements_and"];
            for (const auto& requirement_json : requirements_json)
            {
                connection.requirements_and.push_back(requirement_json.asInt());
            }
        }

        rules.connections.push_back(connection);
    }

    return rules;
}


void save(game_t* game)
{
    Json::Value _json;

    Json::Value eps_json(Json::arrayValue);
    int i = 0;
    int ep = 0;
    for (const auto& episode : game->episodes)
    {
        int lvl = 0;
        for (const auto& meta : episode)
        {
            Json::Value _map_json;
            Json::Value bbs_json(Json::arrayValue);
            auto state = &meta.state;
            for (const auto& bb : state->bbs)
            {
                Json::Value bb_json(Json::arrayValue);
                bb_json.append(bb.x1);
                bb_json.append(bb.y1);
                bb_json.append(bb.x2);
                bb_json.append(bb.y2);
                bb_json.append(bb.region);
                bbs_json.append(bb_json);
            }
            _map_json["bbs"] = bbs_json;

            Json::Value regions_json(Json::arrayValue);
            for (const auto& region : state->regions)
            {
                Json::Value region_json;
                region_json["name"] = region.name;
                region_json["tint"] = onut::serializeFloat4(&region.tint.r);

                Json::Value sectors_json(Json::arrayValue);
                for (auto sectori : region.sectors)
                    sectors_json.append(sectori);
                region_json["sectors"] = sectors_json;

                region_json["rules"] = serialize_rules(region.rules);

                regions_json.append(region_json);
            }
            _map_json["regions"] = regions_json;

            Json::Value accesses_json(Json::arrayValue);
            for (auto access : state->accesses)
            {
                accesses_json.append(access);
            }
            _map_json["accesses"] = accesses_json;

            Json::Value locations_json(Json::arrayValue);
            for (const auto& kv : state->locations)
            {
                Json::Value location_json;
                location_json["index"] = kv.first;
                location_json["death_logic"] = kv.second.death_logic;
                location_json["unreachable"] = kv.second.unreachable;
                location_json["check_sanity"] = kv.second.check_sanity;
                location_json["name"] = kv.second.name;
                location_json["description"] = kv.second.description;
                locations_json.append(location_json);
            }
            _map_json["locations"] = locations_json;

            _map_json["world_rules"] = serialize_rules(state->world_rules);
            _map_json["exit_rules"] = serialize_rules(state->exit_rules);

            _map_json["ep"] = ep;
            _map_json["map"] = lvl;

            eps_json.append(_map_json);

            ++i;
            ++lvl;
        }
        ++ep;
    }

    _json["maps"] = eps_json;

    std::string filename = "data/" + game->short_name + ".data.json";
    // Output styled, for ease of source control
    onut::saveJson(_json, filename, true);

    OnScreenMessages::Add("Saved '" + filename + "'.", ImColor(0.0f, 0.3f, 0.0f));
}


void load(game_t* game)
{
    Json::Value json;
    std::string filename = "data/" + game->short_name + ".data.json";
    if (!onut::loadJson(json, filename))
    {
        OnScreenMessages::Add("WARNING: " + filename + " not found.\n(If you just created this game, then it's fine. Otherwise, scream.)", ImColor(0.3f, 0.3f, 0.0f));
        //onut::showMessageBox("Warning", "Warning: File not found. (If you just created this game, then it's fine. Otherwise, scream).\n" + filename);
        return;
    }

    Json::Value json_maps = json["maps"];

    for (const auto& _map_json : json_maps)
    {
        int ep = _map_json["ep"].asInt();
        int lvl = _map_json["map"].asInt();
        if (ep == 0 && lvl >= (int)game->episodes[ep].size())
        {
            // Could be in DOOM2's old format, remap it
            for (auto& episode : game->episodes)
            {
                if (lvl < (int)episode.size())
                {
                    break;
                }
                lvl -= (int)episode.size();
                ++ep;
            }
        }
        auto meta = get_meta({game->short_name, ep, lvl});
        auto _map_state = &meta->state;

        const auto& bbs_json = _map_json["bbs"];
        for (const auto& bb_json : bbs_json)
        {
            _map_state->bbs.push_back({
                bb_json[0].asInt(),
                bb_json[1].asInt(),
                bb_json[2].asInt(),
                bb_json[3].asInt(),
                bb_json.isValidIndex(4) ? bb_json[4].asInt() : -1,
            });
        }

        const auto& regions_json = _map_json["regions"];
        for (const auto& region_json : regions_json)
        {
            region_t region;

            region.name = region_json.get("name", "BAD_NAME").asString();
            onut::deserializeFloat4(&region.tint.r, region_json["tint"]);

            const auto& sectors_json = region_json["sectors"];
            for (const auto& sector_json : sectors_json)
                region.sectors.insert(sector_json.asInt());

            region.rules = deserialize_rules(region_json["rules"]);

            _map_state->regions.push_back(region);
        }

        const auto& accesses_json = _map_json["accesses"];
        for (const auto& access_json : accesses_json)
        {
            _map_state->accesses.insert(access_json.asInt());
        }

        // Default locations from maps
        auto map = &meta->map;
        for (int i = 0; i < (int)map->things.size(); ++i)
        {
            const auto& thing = map->things[i];
            if (thing.flags & 0x0010) continue; // Thing is not in single player
            if (game->location_doom_types.find(thing.type) != game->location_doom_types.end())
            {
                location_t location;
                _map_state->locations[i] = location;
            }
        }
            
        const auto& locations_json = _map_json["locations"];
        for (const auto& location_json : locations_json)
        {
            location_t location;
            int index = location_json["index"].asInt();
            const auto& thing = map->things[index];
            if (thing.flags & 0x0010) continue; // Thing is not in single player
            if (game->location_doom_types.find(thing.type) != game->location_doom_types.end())
            {
                location.death_logic = location_json["death_logic"].asBool();
                location.unreachable = location_json["unreachable"].asBool();
                location.check_sanity = location_json["check_sanity"].asBool();
                if (location.check_sanity) _map_state->check_sanity_count++;
                location.name = location_json["name"].asString();
                location.description = location_json["description"].asString();
                _map_state->locations[index] = location;
            }
        }

        _map_state->world_rules = deserialize_rules(_map_json["world_rules"]);
        _map_state->exit_rules = deserialize_rules(_map_json["exit_rules"]);

        meta->view.cam_pos = Vector2((float)(map->bb[2] + map->bb[0]) / 2, -(float)(map->bb[3] + map->bb[1]) / 2);
    }
}


void update_window_title()
{
    std::string title;

    if (active_level.ep != -1)
    {
        game_t *game = get_game(active_level);
        meta_t *meta = get_meta(active_level);
        title += game->full_name;
        title += " - ";
        title += meta->name;
        title += " - ";
    }
    title += oSettings->getGameName();

    oWindow->setCaption(title.c_str());
}


// Undo/Redo shit
void push_undo()
{
    if (map_history == nullptr)
        return;
    if (map_history->history_point < (int)map_history->history.size() - 1)
        map_history->history.erase(map_history->history.begin() + (map_history->history_point + 1), map_history->history.end());
    map_history->history.push_back(*map_state);
    map_history->history_point = (int)map_history->history.size() - 1;
}


void clear_map()
{
    mouse_hover_sector = -1;
    mouse_hover_bb = -1;
    set_rule_rule = -3;
    set_rule_connection = -1;
    mouse_hover_location = -1;

    active_level = {"", -1, -1};
    map_state = nullptr;
    map_view = nullptr;
    map_history = nullptr;
}

void select_map(game_t* game, int ep, int map)
{
    clear_map();

    active_level = {game->short_name, ep, map};
    map_state = get_state(active_level, active_source);
    map_view = get_view(active_level);
    map_history = get_history(active_level);

    update_window_title();
    if (map_history->history.empty())
        push_undo();
}


void undo()
{
    if (map_history == nullptr)
        return;
    if (map_history->history_point > 0)
    {
        map_history->history_point--;
        *map_state = map_history->history[map_history->history_point];

        map_state->check_sanity_count = 0;
        for (const auto& loc : map_state->locations)
        {
            (void)loc;
            map_state->check_sanity_count++;
        }
    }
    else
        OnScreenMessages::Add("No action to undo");
}


void redo()
{
    if (map_history == nullptr)
        return;
    if (map_history->history_point < (int)map_history->history.size() - 1)
    {
        map_history->history_point++;
        *map_state = map_history->history[map_history->history_point];
    }
    else
        OnScreenMessages::Add("No action to redo");
}


void initSettings()
{
    oSettings->setGameName("APDoom Gen Tool v2.0");
    oSettings->setResolution({ 1600, 900 });
    oSettings->setBorderlessFullscreen(false);
    oSettings->setIsResizableWindow(true);
    oSettings->setIsFixedStep(false);
    oSettings->setShowFPS(false);
    oSettings->setAntiAliasing(true);
    oSettings->setShowOnScreenLog(false);
#ifdef WIN32
    oSettings->setStartMaximized(true);
#else
    oSettings->setStartMaximized(false);
#endif
}


void regen() // Doom1 only
{
    //gen_step_count = 0;
    //generating = true;

    //for (int ep = 0; ep < EP_COUNT; ++ep)
    //{
    //    for (int lvl = 0; lvl < MAP_COUNT; ++lvl)
    //    {
    //        auto map = &maps[ep][lvl];
    //        auto mid = Vector2((float)(map->bb[2] + map->bb[0]) / 2, (float)(map->bb[3] + map->bb[1]) / 2);
    //        get_state({ep, lvl, -1})->pos = -mid + onut::rand2f(Vector2(-1000, -1000), Vector2(1000, 1000));
    //    }
    //}
}


void init()
{
    oGenerateMipmaps = false;

#if defined(WIN32)
    arrow_cursor = LoadCursor(nullptr, IDC_ARROW);
    nswe_cursor = LoadCursor(nullptr, IDC_SIZEALL);
    we_cursor = LoadCursor(nullptr, IDC_SIZEWE);
    ns_cursor = LoadCursor(nullptr, IDC_SIZENS);
#endif
    
    ap_icon = OGetTexture("ap.png");
    ap_deathlogic_icon = OGetTexture("deathlogic.png");
    ap_unreachable_icon = OGetTexture("unreachable.png");
    ap_check_sanity_icon = OGetTexture("check_sanity.png");
    ap_player_start_icon = OGetTexture("player_start.png");
    ap_wing_icon = OGetTexture("wings.png");

    init_data();

    // Doom1 only
    //for (int ep = 0; ep < EP_COUNT; ++ep)
    //{
    //    for (int lvl = 0; lvl < MAP_COUNT; ++lvl)
    //    {
    //        //auto k = ep * 9 + lvl;
    //        //map_states[ep][lvl].pos = Vector2(
    //        //    -16384 + 3000 + ((k % 5) * 6000),
    //        //    -16384 + 3000 + ((k / 5) * 6000)
    //        //);

    //        auto map = &maps[ep][lvl];
    //        flat_levels[ep * MAP_COUNT + lvl] = get_state({ep, lvl, -1});
    //        get_view({ep, lvl, -1})->cam_pos = Vector2((float)(map->bb[2] + map->bb[0]) / 2, -(float)(map->bb[3] + map->bb[1]) / 2);
    //    }
    //}

    // Load states
    for (auto& kv : games)
    {
        auto game = &kv.second;
        load(game);
    }
    //load("regions_new.json", &metas_new);

    // Mark dirty levels
    //auto levels_idx = get_all_levels_idx();
    //for (const auto& idx : levels_idx)
    //{
    //    auto a = get_state(idx, &metas);
    //    auto b = get_state(idx, &metas_new);
    //    a->different = !(*a == *b);
    //}

    clear_map();
    //select_map(&games.begin()->second, 0, 0);

    regen();

    OnScreenMessages::Add("Welcome to the APDoom Gen Tool");
}


void shutdown() // lol
{
}


void add_bounding_box()
{
    auto map = get_map(active_level);
    map_state->bbs.push_back({map->bb[0], map->bb[1], map->bb[2], map->bb[3]});
    map_state->selected_bb = (int)map_state->bbs.size() - 1;
    push_undo();
}


rule_region_t* get_rules(int idx)
{
    switch (idx)
    {
        case -1: return &map_state->world_rules;
        case -2: return &map_state->exit_rules;
        default: return &map_state->regions[idx].rules;
    }
}


void delete_selected()
{
    switch (tool)
    {
        case tool_t::bb:
            if (map_state->selected_bb != -1)
            {
                map_state->bbs.erase(map_state->bbs.begin() + map_state->selected_bb);
                map_state->selected_bb = -1;
                push_undo();
            }
            break;
        case tool_t::rules:
            if (set_rule_rule != -3 && set_rule_connection != -1)
            {
                auto rules = get_rules(set_rule_rule);
                if (rules)
                {
                    rules->connections.erase(rules->connections.begin() + set_rule_connection);
                    set_rule_rule = -3;
                    set_rule_connection = -1;
                    push_undo();
                }
            }
            break;
        default:
            break;
    }
}


void reset_level()
{
    auto state = get_state(active_level);
    auto map = get_map(active_level);

    state->bbs.clear();
    state->selected_bb = -1;
    state->selected_region = -1;
    state->selected_location = -1;
    state->accesses.clear();
    state->regions.clear();

    region_t main_region;
    main_region.name = "Main";
    state->regions.push_back(main_region);
    for (int i = 0, len = (int)map->sectors.size(); i < len; ++i)
        state->regions[0].sectors.insert(i);

    Point rules_pos = {
        (int)map->bb[0] - RULES_W * 2,
        (int)map->bb[1] + ((int)map->bb[3] - (int)map->bb[1]) / 2
    };
    state->regions[0].rules.x = rules_pos.x;
    state->regions[0].rules.y = rules_pos.y;
    state->regions[0].rules.connections.push_back({-1});
    state->regions[0].rules.connections.push_back({-2});

    state->exit_rules.x = rules_pos.x;
    state->exit_rules.y = rules_pos.y + RULES_H * 3;

    state->world_rules.x = rules_pos.x;
    state->world_rules.y = rules_pos.y - RULES_H * 3;
    state->world_rules.connections.push_back({0});

    // Check for keycards, and create colored regions
    bool keycards[3] = {false, false, false};
    auto game = get_game(active_level);
    for (int i = 0; i < (int)map->things.size(); ++i)
    {
        const auto& thing = map->things[i];
        if (thing.flags & 0x0010)
        {
            continue; // Thing is not in single player
        }
        for (const auto& key_item : game->keys)
        {
            if (thing.type == key_item.item.doom_type)
            {
                keycards[key_item.key] = true;
                break;
            }
        }
    }

    for (int i = 0; i < 3; ++i)
    {
        if (keycards[i])
        {
            ap_key_def_t key_item;
            for (const auto& key_item_s : game->keys)
            {
                if (key_item_s.key == i)
                {
                    key_item = key_item_s;
                    break;
                }
            }
            region_t region;
            region.name = key_item.region_name;
            region.tint = key_item.color;
            region.rules.x = rules_pos.x - (RULES_W * 3 / 2);
            region.rules.y = rules_pos.y - RULES_H * 3 + i * RULES_H * 3;
            state->regions[0].rules.connections.push_back({(int)state->regions.size()});
            state->regions.push_back(region);
        }
    }

    OnScreenMessages::Add("Initialized default region/rules for level.");
    push_undo();
}


void update_shortcuts()
{
    if (active_level.ep == -1)
        return;

    auto ctrl = OInputPressed(OKeyLeftControl);
    auto shift = OInputPressed(OKeyLeftShift);
    auto alt = OInputPressed(OKeyLeftAlt);

    if (!ctrl && !shift && !alt && OInputJustPressed(OKey1)) { tool = tool_t::bb;        OnScreenMessages::Add("New tool: Bounding Box"); }
    if (!ctrl && !shift && !alt && OInputJustPressed(OKey2)) { tool = tool_t::region;    OnScreenMessages::Add("New tool: Region");       }
    if (!ctrl && !shift && !alt && OInputJustPressed(OKey3)) { tool = tool_t::rules;     OnScreenMessages::Add("New tool: Rules");        }
    if (!ctrl && !shift && !alt && OInputJustPressed(OKey4)) { tool = tool_t::locations; OnScreenMessages::Add("New tool: Locations");    }
#if 0 // Hidden, apparently unused?
    if (!ctrl && !shift && !alt && OInputJustPressed(OKey5)) { tool = tool_t::access;    OnScreenMessages::Add("New tool: Access");       }
#endif

    if (ctrl && !shift && !alt && OInputJustPressed(OKeyZ)) undo();
    if (ctrl && shift && !alt && OInputJustPressed(OKeyZ)) redo();
    if (!ctrl && !shift && !alt && OInputJustPressed(OKeyB)) add_bounding_box();
    if (!ctrl && !shift && !alt && OInputJustPressed(OKeyDelete)) delete_selected();
    if (ctrl && !shift && !alt && OInputJustPressed(OKeyS)) save(get_game(active_level));
    if (!ctrl && !shift && !alt && OInputJustPressed(OKeySpaceBar)) regen();
    if (ctrl && !shift && !alt && OInputJustPressed(OKeyR)) reset_level();
    if (!ctrl && !shift && !alt && OInputJustPressed(OKeyF1))
    {
        active_source = active_source_t::current;
        map_state = get_state(active_level, active_source);
    }
    if (!ctrl && !shift && !alt && OInputJustPressed(OKeyF2))
    {
        active_source = active_source_t::target;
        map_state = get_state(active_level, active_source);
    }
}


bool test_bb(const bb_t& bb, const Vector2& pos, float zoom, int &edge)
{
    float edge_size = 32.0f / zoom;
    if (pos.x >= bb.x1 - edge_size && pos.x <= bb.x1 + edge_size && pos.y <= -bb.y1 && pos.y >= -bb.y2)
    {
        edge = 0;
        return true;
    }
    if (pos.y <= -bb.y1 + edge_size && pos.y >= -bb.y1 - edge_size && pos.x >= bb.x1 && pos.x <= bb.x2)
    {
        edge = 1;
        return true;
    }
    if (pos.x >= bb.x2 - edge_size && pos.x <= bb.x2 + edge_size && pos.y <= -bb.y1 && pos.y >= -bb.y2)
    {
        edge = 2;
        return true;
    }
    if (pos.y <= -bb.y2 + edge_size && pos.y >= -bb.y2 - edge_size && pos.x >= bb.x1 && pos.x <= bb.x2)
    {
        edge = 3;
        return true;
    }
    if (pos.x >= bb.x1 && pos.x <= bb.x2 && pos.y <= -bb.y1 && pos.y >= -bb.y2)
    {
        return true;
    }
    return false;
}


int get_bb_at(const Vector2& pos, float zoom, int &edge)
{
    edge = -1;
    if (map_state->selected_bb != -1)
    {
        if (test_bb(map_state->bbs[map_state->selected_bb], pos, zoom, edge))
            return map_state->selected_bb;
    }
    for (int i = 0; i < (int)map_state->bbs.size(); ++i)
    {
        if (test_bb(map_state->bbs[i], pos, zoom, edge))
            return i;
    }
    return -1;
}


int get_loc_at(const Vector2& pos)
{
    auto map = get_map(active_level);
    auto game = get_game(active_level);

    int index = 0;
    for (const auto& thing : map->things)
    {
        if (thing.flags & 0x0010)
        {
            ++index;
            continue; // Thing is not in single player
        }
        if (game->location_doom_types.find(thing.type) != game->location_doom_types.end())
        {
            Rect rect((float)thing.x - 32.0f, (float)-thing.y - 32.0f, 64.0f, 64.0f);
            if (rect.Contains(pos))
            {
                return index;
            }
        }
        ++index;
    }

    return -1;
}


// -1 = world, -2 = exit, -3 = not found
int get_rule_at(const Vector2& pos)
{
    if (pos.x >= (float)map_state->world_rules.x - RULES_W * 0.5f &&
        pos.x <= (float)map_state->world_rules.x + RULES_W * 0.5f &&
        pos.y <= -(float)map_state->world_rules.y + RULES_H * 0.5f &&
        pos.y >= -(float)map_state->world_rules.y - RULES_H * 0.5f)
        return -1;

    if (pos.x >= (float)map_state->exit_rules.x - RULES_W * 0.5f &&
        pos.x <= (float)map_state->exit_rules.x + RULES_W * 0.5f &&
        pos.y <= -(float)map_state->exit_rules.y + RULES_H * 0.5f &&
        pos.y >= -(float)map_state->exit_rules.y - RULES_H * 0.5f)
        return -2;

    for (int i = (int)map_state->regions.size() - 1; i >= 0; --i)
    {
        const auto& region = map_state->regions[i];
        if (pos.x >= (float)region.rules.x - RULES_W * 0.5f &&
            pos.x <= (float)region.rules.x + RULES_W * 0.5f &&
            pos.y <= -(float)region.rules.y + RULES_H * 0.5f &&
            pos.y >= -(float)region.rules.y - RULES_H * 0.5f)
            return i;
    }

    return -3;
}


Vector2 get_rect_edge_pos(Vector2 from, Vector2 to, float side_offset, bool invert_offset)
{
    const auto RECT_HW = RULES_W * 0.5f + 32.0f;
    const auto RECT_HH = RULES_H * 0.5f + 32.0f;

    Vector2 dir = to - from;
    Vector2 right(-dir.y, dir.x);
    right.Normalize();

    Vector2 offset = right * side_offset;
    if (invert_offset) offset = -offset;

    // Left
    if (dir.x + offset.x < -RECT_HW)
    {
        auto d1 = offset.x + RECT_HW;
        auto d2 = -RECT_HW - (dir.x + offset.x);
        auto t = d1 / (d1 + d2);
        auto p = offset + dir * t;
        if (p.y >= -RECT_HH && p.y < RECT_HH) return from + p;
    }

    // Right
    if (dir.x + offset.x > RECT_HW)
    {
        auto d1 = RECT_HW - offset.x;
        auto d2 = (dir.x + offset.x) - RECT_HW;
        auto t = d1 / (d1 + d2);
        auto p = offset + dir * t;
        if (p.y >= -RECT_HH && p.y < RECT_HH) return from + p;
    }

    // Top
    if (dir.y + offset.y < -RECT_HH)
    {
        auto d1 = offset.y + RECT_HH;
        auto d2 = -RECT_HH - (dir.y + offset.y);
        auto t = d1 / (d1 + d2);
        auto p = offset + dir * t;
        if (p.x >= -RECT_HW && p.x < RECT_HW) return from + p;
    }

    // Bottom
    if (dir.y + offset.y > RECT_HH)
    {
        auto d1 = RECT_HH - offset.y;
        auto d2 = (dir.y + offset.y) - RECT_HH;
        auto t = d1 / (d1 + d2);
        auto p = offset + dir * t;
        if (p.x >= -RECT_HW && p.x < RECT_HW) return from + p;
    }

    // Shouldn't happen
    return from;
}


// Thanks, chatGPT
double segment_point_distance(const Vector2& p1, const Vector2& p2, const Vector2& point)
{
    double segmentLength = sqrt(pow(p2.x - p1.x, 2) + pow(p2.y - p1.y, 2));
    double u = ((point.x - p1.x) * (p2.x - p1.x) + (point.y - p1.y) * (p2.y - p1.y)) / (segmentLength * segmentLength);

    if (u < 0.0) {
        return sqrt(pow(point.x - p1.x, 2) + pow(point.y - p1.y, 2));
    }
    if (u > 1.0) {
        return sqrt(pow(point.x - p2.x, 2) + pow(point.y - p2.y, 2));
    }

    double intersectionX = p1.x + u * (p2.x - p1.x);
    double intersectionY = p1.y + u * (p2.y - p1.y);

    return sqrt(pow(point.x - intersectionX, 2) + pow(point.y - intersectionY, 2));
}


// -1 = not found
int get_connection_at(const Vector2& pos, const rule_region_t& rules)
{
    Vector2 center((float)rules.x, -(float)rules.y);

    int i = 0;
    for (const auto& connection : rules.connections)
    {
        auto other_rules = get_rules(connection.target_region);
        if (!other_rules) continue;

        Vector2 other_center((float)other_rules->x, -(float)other_rules->y);
        Vector2 from = get_rect_edge_pos(center, other_center, RULE_CONNECTION_OFFSET, false);
        Vector2 to = get_rect_edge_pos(other_center, center, RULE_CONNECTION_OFFSET, true);
        auto d = segment_point_distance(from, to, {pos.x, pos.y});
        if (d <= 24.0f / map_view->cam_zoom) return i;
        i++;
    }
    return -1;
}

void get_connection_at(const Vector2& pos, int& rule, int& connection)
{
    connection = get_connection_at(pos, map_state->world_rules);
    if (connection != -1)
    {
        rule = -1;
        return;
    }

    for (int i = 0; i < (int)map_state->regions.size(); ++i)
    {
        connection = get_connection_at(pos, map_state->regions[i].rules);
        if (connection != -1)
        {
            rule = i;
            return;
        }
    }

    connection = get_connection_at(pos, map_state->exit_rules);
    if (connection != -1)
    {
        rule = -2;
        return;
    }

    rule = -3;
    connection = -1;
}


void update_gen()
{
    //bool overlapped = false;
    //auto dt = 0.01f;
    //// TODO: Optimize that with chunks? Only care about bounding boxes and not levels?
    //for (int i = 0; i < EP_COUNT * MAP_COUNT; ++i)
    //{
    //    for (int k = 0; k < (int)flat_levels[i]->bbs.size(); ++k)
    //    {
    //        // try to pull back to the middle
    //        //if (gen_step_count > 100 && gen_step_count < 200)
    //        //    flat_levels[i]->pos *= (1.0f - dt * 0.5f);

    //        for (int j = 0; j < EP_COUNT * MAP_COUNT; ++j)
    //        {
    //            if (j == i) continue;
    //            for (int l = 0; l < (int)flat_levels[j]->bbs.size(); ++l)
    //            {
    //                auto bb1 = flat_levels[i]->bbs[k] + flat_levels[i]->pos;
    //                auto bb2 = flat_levels[j]->bbs[l] + flat_levels[j]->pos;
    //                auto penetration = bb1.overlaps(bb2);
    //                if (penetration)
    //                {
    //                    overlapped = true;
    //                    auto dir = bb1.center() - bb2.center();
    //                    dir.Normalize();
    //                    dir += onut::rand2f(Vector2(-1, -1), Vector2(1, 1));
    //                    dir *= (float)penetration;
    //                    flat_levels[i]->pos += dir * dt;
    //                    flat_levels[j]->pos -= dir * dt;
    //                }
    //            }
    //        }
    //    }
    //}
    //if (overlapped) gen_step_count++;
}


void update()
{
    if (map_view == nullptr)
        return;

    // Update mouse pos in world
    auto cam_matrix = Matrix::Create2DTranslationZoom(OScreenf, map_view->cam_pos, map_view->cam_zoom);
    auto inv_cam_matrix = cam_matrix.Invert();
    mouse_pos = Vector2::Transform(OGetMousePos(), inv_cam_matrix);

    // Update shortcuts
    switch (state)
    {
        case state_t::idle:
        {
            if (!ImGui::GetIO().WantCaptureMouse)
            {
                if (OInputJustPressed(OMouse3) || OInputJustPressed(OKeySpaceBar))
                {
                    state = state_t::panning;
                    mouse_pos_on_down = OGetMousePos();
                    cam_pos_on_down = map_view->cam_pos;
                }
                else if (oInput->getStateValue(OMouseZ) > 0.0f)
                {
                    map_view->cam_zoom *= 1.2f;
                }
                else if (oInput->getStateValue(OMouseZ) < 0.0f)
                {
                    map_view->cam_zoom /= 1.2f;
                }
                //else if (OInputJustPressed(OKeyTab))
                //{
                //    state = state_t::gen;
                //}
                else if (tool == tool_t::bb)
                {
                    mouse_hover_bb = get_bb_at(mouse_pos, map_view->cam_zoom, moving_edge);
#if defined(WIN32)
                    if (mouse_hover_bb != -1)
                    {
                        switch (moving_edge)
                        {
                            case -1: SetCursor(nswe_cursor); break;
                            case 0: SetCursor(we_cursor); break;
                            case 1: SetCursor(ns_cursor); break;
                            case 2: SetCursor(we_cursor); break;
                            case 3: SetCursor(ns_cursor); break;
                        }
                    }
                    else
                    {
                        SetCursor(arrow_cursor);
                    }
#endif

                    for (int i = 0; i < 9; ++i)
                        if (OInputJustPressed((onut::Input::State)((int)OKey1 + i)) && map_state->selected_bb != -1)
                            map_state->bbs[map_state->selected_bb].region = i;
                    if (OInputJustPressed(OKey0) && map_state->selected_bb != -1)
                        map_state->bbs[map_state->selected_bb].region = -1;

                    if (OInputJustPressed(OMouse1))
                    {
                        if (mouse_hover_bb != -1)
                        {
                            map_state->selected_bb = mouse_hover_bb;
                            state = state_t::move_bb;
                            bb_on_down = map_state->bbs[mouse_hover_bb];
                            mouse_pos_on_down = OGetMousePos();
                        }
                        else if (map_state->selected_bb != -1)
                        {
                            map_state->selected_bb = -1;
                            push_undo();
                        }
                    }
                }
                else if (tool == tool_t::region)
                {
                    int x = (int)mouse_pos.x;
                    int y = (int)-mouse_pos.y;
                    mouse_hover_sector = sector_at(x, y, get_map(active_level));

                    if ((OInputJustReleased(OMouse1) || OInputJustReleased(OMouse2)) && painted)
                    {
                        painted = false;
                        push_undo();
                    }
                    
                    if (OInputPressed(OMouse1))
                    {
                        // "paint" selected region
                        if (mouse_hover_sector != -1 && map_state->selected_region != -1)
                        {
                            for (auto& region : map_state->regions) region.sectors.erase(mouse_hover_sector);
                            map_state->regions[map_state->selected_region].sectors.insert(mouse_hover_sector);
                            painted = true;
                        }
                    }
                    else if (OInputPressed(OMouse2))
                    {
                        // "erase" selected region
                        if (mouse_hover_sector != -1)
                        {
                            for (auto& region : map_state->regions) region.sectors.erase(mouse_hover_sector);
                            painted = true;
                        }
                    }
                    else if (OInputJustPressed(OKeyF) && map_state->selected_region != -1)
                    {
                        // Fill selected region
                        for (auto& region : map_state->regions) region.sectors.clear();
                        for (int i = 0, len = (int)get_map(active_level)->sectors.size(); i < len; ++i)
                            map_state->regions[map_state->selected_region].sectors.insert(i);
                        painted = true;
                    }
                }
                else if (tool == tool_t::rules)
                {
                    mouse_hover_rule = get_rule_at(mouse_pos);
                    if (mouse_hover_rule != -3)
                    {
                        mouse_hover_connection_rule = -3;
                        mouse_hover_connection = -1;
                    }
                    else
                    {
                        get_connection_at(mouse_pos, mouse_hover_connection_rule, mouse_hover_connection);
                    }

                    if (OInputJustPressed(OMouse1))
                    {
                        if (mouse_hover_rule != -3)
                        {
                            moving_rule = mouse_hover_rule;
                            state = state_t::move_rule;
                            mouse_pos_on_down = OGetMousePos();
                            auto rules = get_rules(mouse_hover_rule);
                            rule_pos_on_down.x = rules->x;
                            rule_pos_on_down.y = rules->y;
                            mouse_hover_rule = -3;
                            set_rule_rule = -3;
                            set_rule_connection = -1;
                        }
                        else if (mouse_hover_connection != -1)
                        {
                            set_rule_rule = mouse_hover_connection_rule;
                            set_rule_connection = mouse_hover_connection;
                        }
                        else
                        {
                            set_rule_rule = -3;
                            set_rule_connection = -1;
                        }
                        //else
                        //{
                        //    box_to = box_from = mouse_pos;
                        //    state = state_t::selecting_rules;
                        //}
                    }
                    else if (OInputJustPressed(OMouse2))
                    {
                        if (mouse_hover_rule != -3)
                        {
                            set_rule_rule = -3;
                            set_rule_connection = -1;
                            state = state_t::connecting_rule;
                            connecting_rule_from = mouse_hover_rule;
                        }
                    }
                }
                else if (tool == tool_t::locations)
                {
                    mouse_hover_location = get_loc_at(mouse_pos);
                    if (OInputJustPressed(OMouse1))
                    {
                        if (mouse_hover_location == -1)
                        {
                            map_state->selected_location = -1;
                        }
                        else
                        {
                            map_state->selected_location = mouse_hover_location;
                        }
                    }
                }
            }
            if (!ImGui::GetIO().WantCaptureKeyboard)
            {
                update_shortcuts();
            }
            break;
        }
        case state_t::panning:
        {
            ImGui::GetIO().WantCaptureMouse = false;
            ImGui::GetIO().WantCaptureKeyboard = false;
            auto diff = OGetMousePos() - mouse_pos_on_down;
            map_view->cam_pos = cam_pos_on_down - diff / map_view->cam_zoom;
            if (OInputJustReleased(OMouse3) || OInputJustReleased(OKeySpaceBar))
                state = state_t::idle;
            break;
        }
        case state_t::move_bb:
        {
            auto diff = (OGetMousePos() - mouse_pos_on_down) / map_view->cam_zoom;
            switch (moving_edge)
            {
                case -1:
                    map_state->bbs[map_state->selected_bb].x1 = bb_on_down.x1 + (int)diff.x;
                    map_state->bbs[map_state->selected_bb].x2 = bb_on_down.x2 + (int)diff.x;
                    map_state->bbs[map_state->selected_bb].y1 = bb_on_down.y1 - (int)diff.y;
                    map_state->bbs[map_state->selected_bb].y2 = bb_on_down.y2 - (int)diff.y;
                    break;
                case 0:
                    map_state->bbs[map_state->selected_bb].x1 = bb_on_down.x1 + (int)diff.x;
                    break;
                case 1:
                    map_state->bbs[map_state->selected_bb].y1 = bb_on_down.y1 - (int)diff.y;
                    break;
                case 2:
                    map_state->bbs[map_state->selected_bb].x2 = bb_on_down.x2 + (int)diff.x;
                    break;
                case 3:
                    map_state->bbs[map_state->selected_bb].y2 = bb_on_down.y2 - (int)diff.y;
                    break;
            }
            if (OInputJustReleased(OMouse1))
            {
                push_undo();
                state = state_t::idle;
            }
            break;
        }
        case state_t::move_rule:
        {
            auto diff = (OGetMousePos() - mouse_pos_on_down) / map_view->cam_zoom;
            auto rules = get_rules(moving_rule);
            rules->x = rule_pos_on_down.x + (int)diff.x;
            rules->y = rule_pos_on_down.y - (int)diff.y;
            if (OInputJustReleased(OMouse1))
            {
                push_undo();
                state = state_t::idle;
            }
            break;
        }
        case state_t::connecting_rule:
        {
            mouse_hover_rule = get_rule_at(mouse_pos);

            if (OInputJustReleased(OMouse2))
            {
                if (mouse_hover_rule != -3 && connecting_rule_from != mouse_hover_rule)
                {
                    auto rules_from = get_rules(connecting_rule_from);
                    //auto rules_to = get_rules(mouse_hover_rule); // Unnecessary

                    // Check if connection not already there
                    bool already_connected = false;
                    for (const auto& connection : rules_from->connections)
                    {
                        if (connection.target_region == mouse_hover_rule)
                        {
                            already_connected = true;
                            break;
                        }
                    }

                    if (!already_connected)
                    {
                        rule_connection_t connection;
                        connection.target_region = mouse_hover_rule;
                        rules_from->connections.push_back(connection);
                        push_undo();

                        set_rule_rule = connecting_rule_from;
                        set_rule_connection = (int)rules_from->connections.size() - 1;
                    }
                }
                state = state_t::idle;
            }
            break;
        }
        //case state_t::gen:
        //{
        //    update_shortcuts();
        //    update_gen();
        //    if (OInputJustPressed(OMouse3))
        //    {
        //        state = state_t::gen_panning;
        //        mouse_pos_on_down = OGetMousePos();
        //        cam_pos_on_down = map_view->cam_pos;
        //    }
        //    else if (oInput->getStateValue(OMouseZ) > 0.0f)
        //    {
        //        map_view->cam_zoom *= 1.2f;
        //    }
        //    else if (oInput->getStateValue(OMouseZ) < 0.0f)
        //    {
        //        map_view->cam_zoom /= 1.2f;
        //    }
        //    else if (OInputJustPressed(OKeyTab))
        //    {
        //        state = state_t::idle;
        //    }
        //    break;
        //}
        //case state_t::gen_panning:
        //{
        //    update_gen();
        //    auto diff = OGetMousePos() - mouse_pos_on_down;
        //    map_view->cam_pos = cam_pos_on_down - diff / map_view->cam_zoom;
        //    if (OInputJustReleased(OMouse3))
        //        state = state_t::gen;
        //    break;
        //}
    }
}


void draw_guides()
{
    auto pb = oPrimitiveBatch.get();

    pb->draw(Vector2(0, -16384), Color(0.15f)); pb->draw(Vector2(0, 16384), Color(0.15f));
    pb->draw(Vector2(-16384, 0), Color(0.15f)); pb->draw(Vector2(16384, 0), Color(0.15f));

    pb->draw(Vector2(-16384, -16384), Color(1, 0.5f, 0)); pb->draw(Vector2(-16384, 16384), Color(1, 0.5f, 0));
    pb->draw(Vector2(-16384, 16384), Color(1, 0.5f, 0)); pb->draw(Vector2(16384, 16384), Color(1, 0.5f, 0));
    pb->draw(Vector2(16384, 16384), Color(1, 0.5f, 0)); pb->draw(Vector2(16384, -16384), Color(1, 0.5f, 0));
    pb->draw(Vector2(16384, -16384), Color(1, 0.5f, 0)); pb->draw(Vector2(-16384, -16384), Color(1, 0.5f, 0));

    pb->draw(Vector2(-32768, -32768), Color(1, 0, 0)); pb->draw(Vector2(-32768, 32768), Color(1, 0, 0));
    pb->draw(Vector2(-32768, 32768), Color(1, 0, 0)); pb->draw(Vector2(32768, 32768), Color(1, 0, 0));
    pb->draw(Vector2(32768, 32768), Color(1, 0, 0)); pb->draw(Vector2(32768, -32768), Color(1, 0, 0));
    pb->draw(Vector2(32768, -32768), Color(1, 0, 0)); pb->draw(Vector2(-32768, -32768), Color(1, 0, 0));
}


region_t* get_region_for_sector(map_state_t* map_state, int sector)
{
    for (auto& region : map_state->regions)
    {
        if (region.sectors.count(sector))
        {
            return &region;
        }
    }
    return nullptr;
}


void draw_rules(const rule_region_t& rules, const region_t& region, bool mouse_hover)
{
    auto sb = oSpriteBatch.get();

    Rect rect(rules.x - RULES_W * 0.5f, -rules.y - RULES_H * 0.5f, RULES_W, RULES_H);
    sb->drawRect(nullptr, rect, Color(0, 0, 0, 0.75f));
    sb->drawInnerOutlineRect(rect, 1.0f / map_view->cam_zoom * 2.0f, region.tint);
    if (mouse_hover)
    {
        sb->drawInnerOutlineRect(rect.Grow(1.0f / map_view->cam_zoom * 2.0f), 1.0f / map_view->cam_zoom * 2.0f, Color(0, 1, 1));
    }
}


void draw_rules_name(const rule_region_t& rules, const region_t& region)
{
    auto sb = oSpriteBatch.get();
    auto pFont = OGetFont("font.fnt");

    sb->begin(
        Matrix::CreateScale(10.0f) * 
        Matrix::CreateTranslation(Vector2(rules.x, -rules.y)) *
        Matrix::Create2DTranslationZoom(OScreenf, map_view->cam_pos, map_view->cam_zoom)
    );
    sb->drawText(pFont, region.name, Vector2::Zero, OCenter, Color::White);
    sb->end();
}


void draw_connections(const rule_region_t& rules, int rule_idx)
{
    auto pb = oPrimitiveBatch.get();

    Vector2 center((float)rules.x, -(float)rules.y);

    int i = 0;
    for (const auto& connection : rules.connections)
    {
        auto other_rules = get_rules(connection.target_region);
        if (!other_rules) continue;
        Rect other_rect(other_rules->x - RULES_W * 0.5f, -other_rules->y - RULES_H * 0.5f, RULES_W, RULES_H);
        other_rect.Grow(128);

        Vector2 other_center((float)other_rules->x, -(float)other_rules->y);
        Vector2 from = get_rect_edge_pos(center, other_center, RULE_CONNECTION_OFFSET, false);
        Vector2 to = get_rect_edge_pos(other_center, center, RULE_CONNECTION_OFFSET, true);
        Vector2 dir = to - from;
        dir.Normalize();
        Vector2 right(-dir.y, dir.x);

        Color color = Color::White;
        if (mouse_hover_connection_rule == rule_idx && i == mouse_hover_connection)
        {
            color = Color(0, 1, 1);
        }
        if (rule_idx == set_rule_rule && i == set_rule_connection)
        {
            color = Color(1, 0, 0);
        }               

        pb->draw(from, color); pb->draw(to, color);
        pb->draw(to, color); pb->draw(to - dir * 32.0f - right * 32.0f, color);
        pb->draw(to, color); pb->draw(to - dir * 32.0f + right * 32.0f, color);

        ++i;
    }
}


#define REQUIREMENT_SIZE 96.0f


OTextureRef get_requirement_icon(game_t* game, int doom_type)
{
    for (const auto& requirement : game->item_requirements)
        if (requirement.doom_type == doom_type)
            return requirement.icon;
    return nullptr;
}


float get_sprite_scale(const OTextureRef& tex)
{
    float biggest = onut::max(tex->getSizef().x, tex->getSizef().y);
    return 1.0f / biggest * 128.0f;
}


void draw_connections_requirements(const rule_region_t& rules, int rule_idx)
{
    auto sb = oSpriteBatch.get();
    auto game = get_game(active_level);

    Vector2 center((float)rules.x, -(float)rules.y);

    int i = 0;
    for (const auto& connection : rules.connections)
    {
        auto other_rules = get_rules(connection.target_region);
        if (!other_rules) continue;
        Rect other_rect(other_rules->x - RULES_W * 0.5f, -other_rules->y - RULES_H * 0.5f, RULES_W, RULES_H);
        other_rect.Grow(128);

        Vector2 other_center((float)other_rules->x, -(float)other_rules->y);
        Vector2 from = get_rect_edge_pos(center, other_center, RULE_CONNECTION_OFFSET, false);
        Vector2 to = get_rect_edge_pos(other_center, center, RULE_CONNECTION_OFFSET, true);
        Vector2 dir = to - from;
        dir.Normalize();
        Vector2 right(-dir.y, dir.x);

        auto count = connection.requirements_or.size() + connection.requirements_and.size();
        Vector2 pos = (to - from) * 0.5f + right * REQUIREMENT_SIZE - dir * ((float)(count) * 0.5f * REQUIREMENT_SIZE - 0.5f * REQUIREMENT_SIZE);
        for (auto requirement : connection.requirements_or)
        {
            auto tex = get_requirement_icon(game, requirement);
            sb->drawSprite(tex, pos + from, Color::White, 0.0f, get_sprite_scale(tex));
            pos += dir * REQUIREMENT_SIZE;
        }
        for (auto requirement : connection.requirements_and)
        {
            auto tex = get_requirement_icon(game, requirement);
            sb->drawSprite(tex, pos + from, Color::White, 0.0f, get_sprite_scale(tex));
            pos += dir * REQUIREMENT_SIZE;
        }

        ++i;
    }
}


void draw_rules()
{
    int i = 0;

    auto sb = oSpriteBatch.get();
    auto pb = oPrimitiveBatch.get();

    auto transform = Matrix::Create2DTranslationZoom(OScreenf, map_view->cam_pos, map_view->cam_zoom);

    // Draw connections
    pb->begin(OPrimitiveLineList, nullptr, transform);
    draw_connections(map_state->world_rules, -1);
    i = 0;
    for (const auto& region : map_state->regions)
    {
        draw_connections(region.rules, i);
        ++i;
    }
    draw_connections(map_state->exit_rules, -2);
    pb->end();

    // Draw connection requirements
    sb->begin(transform);
    draw_connections_requirements(map_state->world_rules, -1);
    i = 0;
    for (const auto& region : map_state->regions)
    {
        draw_connections_requirements(region.rules, i);
        ++i;
    }
    draw_connections_requirements(map_state->exit_rules, -2);
    sb->end();


    // Draw boxes
    sb->begin(transform);
    draw_rules(map_state->world_rules, world_region, mouse_hover_rule == -1);
    i = 0;
    for (const auto& region : map_state->regions)
    {
        draw_rules(region.rules, region, mouse_hover_rule == i);
        ++i;
    }
    draw_rules(map_state->exit_rules, exit_region, mouse_hover_rule == -2);
    sb->end();

    // Draw names
    draw_rules_name(map_state->world_rules, world_region);
    for (const auto& region : map_state->regions)
    {
        draw_rules_name(region.rules, region);
    }
    draw_rules_name(map_state->exit_rules, exit_region);
}


void draw_level(const level_index_t& idx, const Vector2& pos, float angle, bool draw_tools)
{
    Color bound_color(1.0f);
    Color step_color(0.35f);
    Color bb_color(0.5f);

    auto pb = oPrimitiveBatch.get();
    auto sb = oSpriteBatch.get();
    auto game = get_game(idx);
    auto map = get_map(idx);
    auto map_state = get_state(idx, active_source);
    oRenderer->renderStates.backFaceCull = false;

    auto transform = 
              Matrix::CreateRotationZ(angle) *
              Matrix::CreateTranslation(Vector2(pos.x, -pos.y)) *
              Matrix::Create2DTranslationZoom(OScreenf, map_view->cam_pos, map_view->cam_zoom);

    // Sectors
    if (draw_tools)
    {
        pb->begin(OPrimitiveTriangleList, nullptr, transform);
        int i = 0;
        for (const auto& sector : map->sectors)
        {
            region_t* region = get_region_for_sector(map_state, i);
            if (region)
            {
                Color color = region->tint * 0.5f;
                for (int i = 0, len = (int)sector.vertices.size(); i < len; i += 3)
                {
                    const auto& v1 = map->vertexes[sector.vertices[i + 0]];
                    const auto& v2 = map->vertexes[sector.vertices[i + 1]];
                    const auto& v3 = map->vertexes[sector.vertices[i + 2]];

                    pb->draw(Vector2(v1.x, -v1.y), color);
                    pb->draw(Vector2(v2.x, -v2.y), color);
                    pb->draw(Vector2(v3.x, -v3.y), color);
                }
            }
            ++i;
        }
        pb->end();
    }

    // Geometry
    pb->begin(OPrimitiveLineList, nullptr, transform);

    // Bounding box
    //pb->draw(Vector2(map->bb[0], -map->bb[1]), bb_color); pb->draw(Vector2(map->bb[0], -map->bb[3]), bb_color);
    //pb->draw(Vector2(map->bb[0], -map->bb[3]), bb_color); pb->draw(Vector2(map->bb[2], -map->bb[3]), bb_color);
    //pb->draw(Vector2(map->bb[2], -map->bb[3]), bb_color); pb->draw(Vector2(map->bb[2], -map->bb[1]), bb_color);
    //pb->draw(Vector2(map->bb[2], -map->bb[1]), bb_color); pb->draw(Vector2(map->bb[0], -map->bb[1]), bb_color);

    // Geometry
    int i = 0;
    bool is_heretic = game->iwad_name == "HERETIC.WAD";
    for (const auto& line : map->linedefs)
    {
        Color color = bound_color;
        if (line.back_sidedef != -1) color = step_color;

        if (draw_tools)
        {
            if (is_heretic)
            {
                if (line.special_type == LT_DR_DOOR_RED_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_RED_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_RED_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_RED_OPEN_STAY_FAST)
                    color = game->key_colors[1];
                else if (line.special_type == LT_DR_DOOR_YELLOW_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_YELLOW_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_YELLOW_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_YELLOW_OPEN_STAY_FAST)
                    color = game->key_colors[0];
                else if (line.special_type == LT_DR_DOOR_BLUE_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_BLUE_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_BLUE_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_BLUE_OPEN_STAY_FAST)
                    color = game->key_colors[2];
            }
            else
            {
                if (line.special_type == LT_DR_DOOR_RED_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_RED_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_RED_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_RED_OPEN_STAY_FAST)
                    color = game->key_colors[2];
                else if (line.special_type == LT_DR_DOOR_YELLOW_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_YELLOW_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_YELLOW_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_YELLOW_OPEN_STAY_FAST)
                    color = game->key_colors[1];
                else if (line.special_type == LT_DR_DOOR_BLUE_OPEN_WAIT_CLOSE ||
                    line.special_type == LT_D1_DOOR_BLUE_OPEN_STAY ||
                    line.special_type == LT_SR_DOOR_BLUE_OPEN_STAY_FAST ||
                    line.special_type == LT_S1_DOOR_BLUE_OPEN_STAY_FAST)
                    color = game->key_colors[0];
            }

            if (line.special_type == LT_DR_DOOR_OPEN_WAIT_CLOSE_ALSO_MONSTERS ||
                line.special_type == LT_DR_DOOR_OPEN_WAIT_CLOSE_FAST ||
                line.special_type == LT_SR_DOOR_OPEN_WAIT_CLOSE ||
                line.special_type == LT_SR_DOOR_OPEN_WAIT_CLOSE_FAST ||
                line.special_type == LT_S1_DOOR_OPEN_WAIT_CLOSE ||
                line.special_type == LT_S1_DOOR_OPEN_WAIT_CLOSE_FAST ||
                line.special_type == LT_WR_DOOR_OPEN_WAIT_CLOSE ||
                line.special_type == LT_WR_DOOR_OPEN_WAIT_CLOSE_FAST ||
                line.special_type == LT_W1_DOOR_OPEN_WAIT_CLOSE_ALSO_MONSTERS ||
                line.special_type == LT_W1_DOOR_OPEN_WAIT_CLOSE_FAST ||
                line.special_type == LT_D1_DOOR_OPEN_STAY ||
                line.special_type == LT_D1_DOOR_OPEN_STAY_FAST ||
                line.special_type == LT_SR_DOOR_OPEN_STAY ||
                line.special_type == LT_SR_DOOR_OPEN_STAY_FAST ||
                line.special_type == LT_S1_DOOR_OPEN_STAY ||
                line.special_type == LT_S1_DOOR_OPEN_STAY_FAST ||
                line.special_type == LT_GR_DOOR_OPEN_STAY ||
                line.special_type == LT_SR_DOOR_CLOSE_STAY ||
                line.special_type == LT_SR_DOOR_CLOSE_STAY_FAST ||
                line.special_type == LT_S1_DOOR_CLOSE_STAY ||
                line.special_type == LT_S1_DOOR_CLOSE_STAY_FAST)
                color = Color(0, 1, 1);
            else if (line.special_type == LT_S1_EXIT_LEVEL ||
                line.special_type == LT_W1_EXIT_LEVEL ||
                line.special_type == LT_S1_EXIT_LEVEL_GOES_TO_SECRET_LEVEL ||
                line.special_type == LT_W1_EXIT_LEVEL_GOES_TO_SECRET_LEVEL)
                color = Color(0, 0.5f, 1);
        }

        if (draw_tools && tool == tool_t::region)
        {
            if (mouse_hover_sector != -1)
            {
                if ((line.back_sidedef != -1 && map->sidedefs[line.back_sidedef].sector == mouse_hover_sector) ||
                    (line.front_sidedef != -1 && map->sidedefs[line.front_sidedef].sector == mouse_hover_sector))
                {
                    color = Color(0, 1, 1);
                }
            }
        }
        
        pb->draw(Vector2(map->vertexes[line.start_vertex].x, -map->vertexes[line.start_vertex].y), color);
        pb->draw(Vector2(map->vertexes[line.end_vertex].x, -map->vertexes[line.end_vertex].y), color);

        ++i;
    }

    // Arrows
    for (const auto& arrow : map->arrows)
    {
        pb->draw(arrow.from, arrow.color);
        pb->draw(arrow.to, arrow.color);

        Vector2 dir = arrow.to - arrow.from;
        dir.Normalize();
        Vector2 right(-dir.y, dir.x);

#define ARROW_HEAD_SIZE 8.0f
        pb->draw(arrow.to, arrow.color); pb->draw(arrow.to - dir * ARROW_HEAD_SIZE - right * ARROW_HEAD_SIZE, arrow.color);
        pb->draw(arrow.to, arrow.color); pb->draw(arrow.to - dir * ARROW_HEAD_SIZE + right * ARROW_HEAD_SIZE, arrow.color);
    }

    // Bounding boxes
    if (draw_tools && tool == tool_t::bb)
    {
        int i = 0;
        for (const auto& bb : map_state->bbs)
        {
            Color color = bb_color;
            if (bb.region != -1 && bb.region < (int)map_state->regions.size()) color = map_state->regions[bb.region].tint;
            //if (i == map_state->selected_bb) color = Color(1, 0, 0);
            pb->draw(Vector2(bb.x1, -bb.y1), color); pb->draw(Vector2(bb.x1, -bb.y2), color);
            pb->draw(Vector2(bb.x1, -bb.y2), color); pb->draw(Vector2(bb.x2, -bb.y2), color);
            pb->draw(Vector2(bb.x2, -bb.y2), color); pb->draw(Vector2(bb.x2, -bb.y1), color);
            pb->draw(Vector2(bb.x2, -bb.y1), color); pb->draw(Vector2(bb.x1, -bb.y1), color);
            ++i;
        }
    }

    pb->end();

    // Selected bb
    if (draw_tools && tool == tool_t::bb)
    {
        sb->begin(transform);
        if (map_state->selected_bb != -1)
        {
            const auto& bb = map_state->bbs[map_state->selected_bb];
            sb->drawRect(nullptr, Rect(bb.x1, -bb.y1 - (bb.y2 - bb.y1), bb.x2 - bb.x1, bb.y2 - bb.y1), Color(0.5f, 0, 0, 0.5f));
        }
        sb->end();
    }

    // Selected location
    if (draw_tools && tool == tool_t::locations)
    {
        sb->begin(transform);
        if (mouse_hover_location != -1)
        {
            auto thing = &map->things[mouse_hover_location];
            Rect rect((float)thing->x - 32.0f, (float)-thing->y - 32.0f, 64.0f, 64.0f);
            sb->drawOutterOutlineRect(rect, 2.0f / map_view->cam_zoom, Color(1, 1, 0));
        }
        if (map_state->selected_location != -1)
        {
            auto thing = &map->things[map_state->selected_location];
            Rect rect((float)thing->x - 32.0f, (float)-thing->y - 32.0f, 64.0f, 64.0f);
            sb->drawOutterOutlineRect(rect, 2.0f / map_view->cam_zoom, Color(1, 0, 0));
        }
        sb->end();
    }

    // Vertices
    //sb->begin(transform);
    //for (const auto& v : map->vertexes)
    //{
    //    sb->drawSprite(nullptr, Vector2(v.x, -v.y), bound_color, 0.0f, 3.0f);
    //}
    //sb->end();

    // Items
    sb->begin(transform);
    oRenderer->renderStates.sampleFiltering = OFilterNearest;
    i = -1;
    for (const auto& thing : map->things)
    {
        ++i;
        if (thing.flags & 0x0010) continue; // Thing is not in single player
        if (game->location_doom_types.find(thing.type) != game->location_doom_types.end())
        {
            //ap_deathlogic_icon
            if (map_state->locations[i].death_logic)
                sb->drawSprite(ap_deathlogic_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 1.0f);
            else
                sb->drawSprite(ap_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 2.0f);
            if (map_state->locations[i].unreachable)
                sb->drawSprite(ap_unreachable_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 1.0f);
            else if (map_state->locations[i].check_sanity)
                sb->drawSprite(ap_check_sanity_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 1.0f);
        }
        else if (thing.type == 1) // Player start
        {
            sb->drawSprite(ap_player_start_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 2.5f);
        }
        else if (thing.type == 83 && game->iwad_name == "HERETIC.WAD") // Wings
        {
            sb->drawSprite(ap_wing_icon, Vector2(thing.x, -thing.y), Color::White, 0.0f, 2.5f);
        }
    }
    sb->end();
}


void render()
{
    oRenderer->clear(Color::Black);
    oRenderer->renderStates.sampleFiltering = OFilterNearest;

    if (map_view == nullptr)
        return;

    auto pb = oPrimitiveBatch.get();
    auto sb = oSpriteBatch.get();

    pb->begin(OPrimitiveLineList, nullptr, Matrix::Create2DTranslationZoom(OScreenf, map_view->cam_pos, map_view->cam_zoom));
    draw_guides();
    pb->end();

    switch (state)
    {
        //case state_t::gen:
        //case state_t::gen_panning: // Doom1 only
        //{
        //    for (int ep = 0; ep < EP_COUNT; ++ep)
        //    {
        //        for (int map = 0; map < MAP_COUNT; ++map)
        //        {
        //            draw_level(ep, map, -1, metas.d1_metas[ep][map].state.pos, metas.d1_metas[ep][map].state.angle, true);
        //        }
        //    }
        //    sb->begin();
        //    sb->drawText(OGetFont("font.fnt"), "Step Count = " + std::to_string(gen_step_count), Vector2(0, 50));
        //    sb->end();
        //    break;
        //}
        default:
        {
            draw_level(active_level, {0, 0}, 0, true);
            draw_rules();

            if (active_source == active_source_t::target)
            {
                sb->begin();
                sb->drawInnerOutlineRect(Rect(0, 20, OScreenWf - 1, OScreenHf - 20 - 1), 4.0f, Color(1, 0, 0));
                sb->end();
            }
            break;
        }
    }

}


#if 0 // unused function?
std::string unique_name(const Json::Value& dict, const std::string& name)
{
    auto names = dict.getMemberNames();
    int i = 1;
    bool found = false;
    std::string new_name = name;
    while (true)
    {
        for (const auto& other_name : names)
        {
            if (other_name == name)
            {
                i++;
                new_name = name + " " + std::to_string(i);
                continue;
            }
        }
        break;
    }
    return new_name;
}
#endif

static void renderGames(bool is_in_submenu)
{
    for (auto& kv : games)
    {
        if (!is_in_submenu)
            ImGui::Separator();

        auto game = &kv.second;
        if (ImGui::BeginMenu(game->full_name.c_str()))
        {
            int episode_check_count = 0;
            int episode_check_sanity_count = 0;
            ImVec4 total_checks_col(0.6f, 0.6f, 0.6f, 1.0f);
            int ep = 0;
            for (const auto& episode : game->episodes)
            {
                int map = 0;
                for (const auto& meta : episode)
                {
                    auto map_state = &meta.state;
                    bool selected = &meta == get_meta(active_level);
                    if (ep != 0 && map == 0)
                    {
                        ImGui::TextColored(total_checks_col, "Total checks: %i-%i=(%i)", episode_check_count, episode_check_sanity_count, episode_check_count - episode_check_sanity_count);
                        episode_check_count = 0;
                        episode_check_sanity_count = 0;
                        ImGui::Separator();
                    }
                    episode_check_count += meta.map.check_count;
                    episode_check_sanity_count += meta.state.check_sanity_count;
                    std::string displayed_checks = std::to_string(meta.map.check_count) + "-" + std::to_string(meta.state.check_sanity_count) + "=(" + std::to_string(meta.map.check_count - meta.state.check_sanity_count) + ")";

                    if (ImGui::MenuItem((meta.name + (map_state->different ? "*" : "")).c_str(), displayed_checks.c_str(), &selected))
                        select_map(game, ep, map);
                    ++map;
                }
                ++ep;
            }
            ImGui::TextColored(total_checks_col, "Total checks: %i-%i=(%i)", episode_check_count, episode_check_sanity_count, episode_check_count - episode_check_sanity_count);
            ImGui::EndMenu();
        }
    }
}

void renderUI()
{
    bool game_loaded = (active_level.ep != -1);

    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File"))
    {
        //ImGui::MenuItem("Open...", "Ctrl+O", false, false);

        if (!game_loaded)
            ImGui::MenuItem("Save", "Ctrl+S", false, false);
        else if (ImGui::MenuItem(("Save " + get_game(active_level)->full_name).c_str(), "Ctrl+S"))
            save(get_game(active_level));

        ImGui::Separator();

        if (ImGui::BeginMenu("Generate APWorld..."))
        {
            if (games.empty())
                ImGui::MenuItem("No games are loaded", NULL, false, false);
            else for (auto& kv : games)
            {
                auto game = &kv.second;
                if (ImGui::MenuItem(("Generate " + game->full_name).c_str()))
                {
                    save(game);
                    generate(game);
                }
            }
            ImGui::EndMenu();
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Exit"))
            OQuit();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Tools"))
    {
        if (ImGui::MenuItem("Bounding Box", "1", tool == tool_t::bb))        tool = tool_t::bb;
        if (ImGui::MenuItem("Region",       "2", tool == tool_t::region))    tool = tool_t::region;
        if (ImGui::MenuItem("Rules",        "3", tool == tool_t::rules))     tool = tool_t::rules;
        if (ImGui::MenuItem("Location",     "4", tool == tool_t::locations)) tool = tool_t::locations;
#if 0 // Hidden, apparently unused?
        if (ImGui::MenuItem("Access",       "5", tool == tool_t::access))    tool = tool_t::access;
#endif

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit"))
    {
        if (ImGui::MenuItem("Undo", "Ctrl+Z",             false, game_loaded)) undo();
        if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z",       false, game_loaded)) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Add Bounding Box", "B",      false, game_loaded)) add_bounding_box();
        ImGui::Separator();
        if (ImGui::MenuItem("Initialize Level", "Ctrl-R", false, game_loaded)) reset_level();

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Diff"))
    {
        {
            bool selected = active_source == active_source_t::current;
            if (ImGui::MenuItem("Show Current", "F1", &selected, game_loaded))
            {
                active_source = active_source_t::current;
                map_state = get_state(active_level, active_source);
            }
        }
        {
            bool selected = active_source == active_source_t::target;
            if (ImGui::MenuItem("Show Target", "F2", &selected, game_loaded))
            {
                active_source = active_source_t::target;
                map_state = get_state(active_level, active_source);
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Apply Target", NULL, false, game_loaded))
        {
            *get_state(active_level, active_source_t::current) = *get_state(active_level, active_source_t::target);
            map_state = get_state(active_level);
            push_undo();
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    ImVec4 info_colored(0.6f, 0.6f, 0.6f, 1.0f);
    ImGui::TextColored(info_colored, "Maps:");

    if (games.size() >= 8)
    {
        // If the game list gets too large, we need to make a new menu for it.
        if (ImGui::BeginMenu(("All Loaded Games (" + std::to_string(games.size()) + ")").c_str()))
        {
            renderGames(true);
            ImGui::EndMenu();
        }
    }
    else
    {
        // If there's not too many, just render the games on the main title bar.
        renderGames(false);
    }

    ImGui::EndMainMenuBar();

    //if (state != state_t::gen && state != state_t::gen_panning)
    {
        if (ImGui::Begin("Regions") && game_loaded)
        {
            static char region_name[260] = {'\0'};
            ImGui::InputText("##region_name", region_name, 260);
            ImGui::SameLine();
            if (ImGui::Button("Add") && strlen(region_name) > 0)
            {
                region_t region;
                region.name = region_name;
                map_state->regions.push_back(region);
                region_name[0] = '\0';
                map_state->selected_region = (int)map_state->regions.size() - 1;
                push_undo();
            }
            {
                int to_move_up = -1;
                int to_move_down = -1;
                int to_delete = -1;
                for (int i = 0; i < (int)map_state->regions.size(); ++i)
                {
                    const auto& region = map_state->regions[i];
                    bool selected = map_state->selected_region == i;
                    if (ImGui::Selectable((std::to_string(i + 1) + " " + region.name).c_str(), selected, 0, ImVec2(150, 22)))
                    {
                        map_state->selected_region = i;
                    }
                    if (map_state->selected_region == i)
                    {
                        ImGui::SameLine(); if (ImGui::Button(" ^ ")) to_move_up = i;
                        ImGui::SameLine(); if (ImGui::Button(" v ")) to_move_down = i;
                        ImGui::SameLine(); if (ImGui::Button("X")) to_delete = i;
                        if (map_state->selected_bb != -1 && tool == tool_t::bb)
                        {
                            ImGui::SameLine();
                            if (ImGui::Button("Assign"))
                            {
                                map_state->bbs[map_state->selected_bb].region = i;
                            }
                        }
                    }
                }
                if (to_move_up > 0)
                {
                    for (auto& bb : map_state->bbs)
                    {
                        if (bb.region == to_move_up)
                            bb.region--;
                        else if (bb.region == to_move_up - 1)
                            bb.region++;
                    }
                    region_t region = map_state->regions[to_move_up];
                    map_state->regions.erase(map_state->regions.begin() + to_move_up);
                    map_state->regions.insert(map_state->regions.begin() + (to_move_up - 1), region);
                    map_state->selected_region = to_move_up - 1;
                    push_undo();
                }
                if (to_move_down != -1 && to_move_down < (int)map_state->regions.size() - 1)
                {
                    for (auto& bb : map_state->bbs)
                    {
                        if (bb.region == to_move_down)
                            bb.region++;
                        else if (bb.region == to_move_down + 1)
                            bb.region--;
                    }
                    region_t region = map_state->regions[to_move_down];
                    map_state->regions.erase(map_state->regions.begin() + to_move_down);
                    map_state->regions.insert(map_state->regions.begin() + (to_move_down + 1), region);
                    map_state->selected_region = to_move_down + 1;
                    push_undo();
                }
                if (to_delete != -1)
                {
                    for (auto& bb : map_state->bbs)
                    {
                        if (bb.region == to_delete)
                            bb.region = -1;
                    }
                    for (auto& region : map_state->regions)
                    {
                        for (int c = 0; c < (int)region.rules.connections.size(); ++c)
                        {
                            auto& connection = region.rules.connections[c];
                            if (connection.target_region == to_delete)
                            {
                                region.rules.connections.erase(region.rules.connections.begin() + c);
                                --c;
                                continue;
                            }
                            if (connection.target_region > to_delete)
                            {
                                connection.target_region--;
                            }
                        }
                    }
                    map_state->regions.erase(map_state->regions.begin() + to_delete);
                    map_state->selected_region = onut::min((int)map_state->regions.size() - 1, map_state->selected_region);
                    push_undo();
                }
            }
        }
        ImGui::End();

        if (ImGui::Begin("Region") && game_loaded)
        {
            if (map_state->selected_region != -1)
            {
                auto& region = map_state->regions[map_state->selected_region];

                static char region_name[260] = {'\0'};
                snprintf(region_name, 260, "%s", region.name.c_str());
                if (ImGui::InputText("Name", region_name, 260, ImGuiInputTextFlags_EnterReturnsTrue))
                {
                    region.name = region_name;
                    push_undo();
                }

                ImGui::ColorEdit4("Tint", &region.tint.r, ImGuiColorEditFlags_NoInputs);
                if (ImGui::IsItemDeactivatedAfterEdit()) push_undo();
            }
        }
        ImGui::End();

        if (ImGui::Begin("Connection") && game_loaded)
        {
            if (set_rule_rule != -3 && set_rule_connection != -1)
            {
                auto rules = get_rules(set_rule_rule);
                if (rules)
                {
                    if (ImGui::Button("Remove"))
                    {
                        rules->connections.erase(rules->connections.begin() + set_rule_connection);
                        set_rule_rule = -3;
                        set_rule_connection = -1;
                        push_undo();
                    }
                    else
                    {
                        auto& connection = rules->connections[set_rule_connection];
                        auto game = get_game(active_level);

                        ImGui::Columns(2);
                        ImGui::Text("OR");
                        ImGui::NextColumn();
                        ImGui::Text("AND");
                        ImGui::NextColumn();

                        for (const auto& requirement : game->item_requirements)
                        {
                            float biggest = requirement.icon->getSizef().x;
                            ImVec2 img_scale(requirement.icon->getSizef().x / biggest * 64.0f, requirement.icon->getSizef().y / biggest * 64.0f);

                            // Extra requirements are AND only, they don't function in OR slots, so hide them
                            if (requirement.doom_type > 0)
                            {
                                ImVec4 tint(0.25f, 0.25f, 0.25f, 1);
                                bool has_requirement = false;
                                auto it = std::find(connection.requirements_or.begin(), connection.requirements_or.end(), requirement.doom_type);
                                if (it != connection.requirements_or.end())
                                {
                                    has_requirement = true;
                                    tint = {1,1,1,1};
                                }

                                if (ImGui::ImageButton(
                                    ("or_btn_" + std::to_string(requirement.doom_type)).c_str(), // str_id
                                    (ImTextureID)&requirement.icon, // user_texture_id
                                    img_scale, // size
                                    ImVec2(0, 0), // uv0
                                    ImVec2(1, 1), // uv1
                                    ImVec4(0, 0, 0, 0), // bgcolor
                                    tint)) // tint
                                {
                                    if (has_requirement)
                                    {
                                        connection.requirements_or.erase(it);
                                    }
                                    else
                                    {
                                        connection.requirements_or.push_back(requirement.doom_type);
                                    }
                                    push_undo();
                                }
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("OR: %s", requirement.name.c_str());
                            }
                            ImGui::NextColumn();
                            {
                                ImVec4 tint(0.25f, 0.25f, 0.25f, 1);
                                bool has_requirement = false;
                                auto it = std::find(connection.requirements_and.begin(), connection.requirements_and.end(), requirement.doom_type);
                                if (it != connection.requirements_and.end())
                                {
                                    has_requirement = true;
                                    tint = {1,1,1,1};
                                }
                                if (ImGui::ImageButton(
                                    ("and_btn_" + std::to_string(requirement.doom_type)).c_str(), // str_id
                                    (ImTextureID)&requirement.icon, // user_texture_id
                                    img_scale, // size
                                    ImVec2(0, 0), // uv0
                                    ImVec2(1, 1), // uv1
                                    ImVec4(0, 0, 0, 0), // bgcolor
                                    tint)) // tint
                                {
                                    if (has_requirement)
                                    {
                                        connection.requirements_and.erase(it);
                                    }
                                    else
                                    {
                                        connection.requirements_and.push_back(requirement.doom_type);
                                    }
                                    push_undo();
                                }
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("AND: %s", requirement.name.c_str());
                            }
                            ImGui::NextColumn();
                        }
                    }
                }
            }
        }
        ImGui::End();

        if (ImGui::Begin("Map") && game_loaded)
        {
            auto map = get_map(active_level);
            auto game = get_game(active_level);
            ImGui::Text("Check count: %i", map->check_count);
            ImGui::Separator();
            int index = 0;
            for (const auto& thing : map->things)
            {
                if (thing.flags & 0x0010)
                {
                    index++;
                    continue; // Thing is not in single player
                }
                auto str = get_doom_type_name(active_level, thing.type);
                if (str == ERROR_STR)
                {
                    index++;
                    continue;
                }

                bool selected = map_state->selected_location == index;
                if (ImGui::Selectable((str + ("##loc_idx" + std::to_string(index))).c_str(), &selected))
                {
                    if (selected)
                    {
                        map_state->selected_location = index;
                    }
                }
                if (game->location_doom_types.find(thing.type) != game->location_doom_types.end())
                {
                    if (map_state->locations[index].unreachable)
                    {
                        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
                        cursorScreenPos.y -= 10;
                        ImGui::GetWindowDrawList()->AddLine(cursorScreenPos, ImVec2(cursorScreenPos.x + 150, cursorScreenPos.y), IM_COL32(255, 255, 255, 255), 1.0f);
                    }
                    if (map_state->locations[index].check_sanity)
                    {
                        ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
                        cursorScreenPos.y -= 10;
                        ImGui::GetWindowDrawList()->AddLine(cursorScreenPos, ImVec2(cursorScreenPos.x + 150, cursorScreenPos.y), IM_COL32(0, 255, 0, 255), 1.0f);
                    }
                }
                index++;
            }
        }
        ImGui::End();

#if 0
        if (ImGui::Begin("Game") && game_loaded)
        {
            auto game = get_game(active_level);
            for (const auto& kv : game->total_doom_types)
                ImGui::LabelText(("Doom Type " + std::to_string(kv.first)).c_str(), "%i", kv.second);
        }
        ImGui::End();
#endif

        if (ImGui::Begin("Location") && game_loaded)
        {
            if (map_state->selected_location != -1)
            {
                ImGui::Text("Thing Index: %i", map_state->selected_location);
                ImGui::Text("Thing Type: %s", get_doom_type_name(active_level, get_map(active_level)->things[map_state->selected_location].type));

                auto& location = map_state->locations[map_state->selected_location];
                
                if (ImGui::Checkbox("Death Logic", &location.death_logic))
                {
                    push_undo();
                }
                
                if (ImGui::Checkbox("Unreachable", &location.unreachable))
                {
                    push_undo();
                }

                if (ImGui::Checkbox("check_sanity", &location.check_sanity))
                {
                    if (location.check_sanity) map_state->check_sanity_count++;
                    else map_state->check_sanity_count--;
                    push_undo();
                }

                {
                    static char buf[260] = {'\0'};
                    snprintf(buf, 260, "%s", location.name.c_str());
                    if (ImGui::InputText("Name", buf, 260, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        location.name = buf;
                        push_undo();
                    }
                }
                {
                    static char buf[1000] = {'\0'};
                    snprintf(buf, 1000, "%s", location.description.c_str());
                    if (ImGui::InputText("Description", buf, 1000, ImGuiInputTextFlags_EnterReturnsTrue))
                    {
                        location.description = buf;
                        push_undo();
                    }
                }
            }
        }
        ImGui::End();
    }

    // Display text
    OnScreenMessages::Render();
}


void postRender()
{
}
