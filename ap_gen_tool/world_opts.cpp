#include <vector>
#include <string>

#include <onut/Log.h>
#include <onut/Json.h>

#include "data.h"
#include "python.hpp"

static std::string to_snake_case(const std::string& name)
{
    std::string ret;
    for (char c : name)
    {
        if (!(c >= '0' && c <= '9') && !(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z'))
        {
            if (ret.empty() || ret.back() != '_')
                ret += '_';
        }
        else
            ret += (char)tolower((unsigned char)c);
    }
    return ret;
}

static std::string to_title_case(const std::string& name)
{
    std::string ret;
    bool capitalize = true;

    for (char c : name)
    {
        if (capitalize)
            ret += (char)toupper((unsigned char)c);
        else
            ret += c;
        capitalize = (!(c >= 'A' && c <= 'Z') && !(c >= 'a' && c <= 'z'));
    }
    return ret;
}

void Null_InsertWorldHook(game_t *game, const std::string& hook_type, std::vector<std::string>& hook) {}

// ============================================================================
// ============================================================================

/*****************************
** World Option: Difficulty **
*****************************/

struct
{
    int preset;
    std::string skill_5_warning;
    std::vector<Json::Value> json; 
} diff_info;

struct default_difficulty_t {
    const char *name;
    const char *description;
};
const default_difficulty_t difficulties_doom[5] = {
    {"baby",      "Damage taken is halved. Ammo received from pickups is doubled."},
    {"easy",      "Lesser number or strength of monsters, and more pickups."},
    {"medium",    "The default skill. Balanced monsters and pickups."},
    {"hard",      "Greater number or strength of monsters, and less pickups."},
    {"nightmare", "Monsters are faster, more aggressive, and respawn."}
};
const default_difficulty_t difficulties_heretic[5] = {
    {"wet nurse",    "Damage taken is halved. Ammo received from pickups is doubled. Quartz Flasks and Mystic Urns are automatically used when the player nears death."},
    {"easy",         "Lesser number or strength of monsters, and more pickups."},
    {"medium",       "The default skill. Balanced monsters and pickups."},
    {"hard",         "Greater number or strength of monsters, and less pickups."},
    {"black plague", "Monsters are faster and more aggressive."}
};

void Difficulty_Init(game_t *game, const Json::Value& json)
{
    diff_info.json.clear();
    std::string preset = json.get("preset", "").asString();

    if (preset == "Doom")
        diff_info.preset = 1;
    else if (preset == "Heretic")
        diff_info.preset = 2;
    else
    {
        diff_info.preset = 0;
        diff_info.skill_5_warning = json.get("skill_5_warning", "").asString();
        diff_info.json.resize(5);
        if (json["list"].isArray())
        {
            for (int i = 0; i < 5; ++i)
                diff_info.json[i] = json["list"].get(i, Json::objectValue);
        }
    }
}

void Difficulty_InsertPyOptions(game_t *game, std::vector<PyOption>& options)
{
    if (diff_info.preset == 1)
    {
        PyOption& opt = options.emplace_back("difficulty", "DifficultyDoom", PyOptionType::InID1Common);
        opt.option_group = "Difficulty Options";
    }
    else if (diff_info.preset == 2)
    {
        PyOption& opt = options.emplace_back("difficulty", "DifficultyHeretic", PyOptionType::InID1Common);
        opt.option_group = "Difficulty Options";
    }
    else
    {
        const default_difficulty_t *diff_strings = (game->iwad_name == "HERETIC.WAD" ? difficulties_heretic : difficulties_doom);
        std::vector<std::string> choices;
        std::vector<std::string> aliases;

        PyOption& opt = options.emplace_back("difficulty", "Difficulty", PyOptionType::Choice);
        opt.option_group = "Difficulty Options";
        opt.docstring.push_back("Choose the game difficulty (skill level).");
        opt.docstring.push_back("");

        for (int i = 0; i < 5; ++i)
        {
            std::string full_name = diff_info.json[i].get("full_name", "").asString();
            std::string opt_name = diff_info.json[i].get("option_name", diff_strings[i].name).asString();
            if (full_name == "")
                continue;

            opt.docstring.push_back("**" + opt_name + "**: (" + full_name + ") - " + diff_strings[i].description);
            choices.push_back("option_" + to_snake_case(opt_name) + " = " + std::to_string(i));
            for (const auto& alias : diff_info.json[i].get("aliases", Json::arrayValue))
                aliases.push_back("alias_" + to_snake_case(alias.asString()) + " = " + std::to_string(i));
        }
        if (diff_info.skill_5_warning != "")
            opt.option_list.push_back("skill_5_warning = " + Py_QuoteString(diff_info.skill_5_warning));
        opt.option_list.insert(opt.option_list.end(), choices.begin(), choices.end());
        opt.option_list.insert(opt.option_list.end(), aliases.begin(), aliases.end());
        opt.default_int = 2;
    }
}

/**********************************
** World Option: Start with Maps **
**********************************/

struct
{
    int doom_type;
    std::string plural_name;
    std::string class_name;
} swm_info;

void StartWithMaps_Init(game_t *game, const Json::Value& json)
{
    swm_info.doom_type = json.get("doom_type", (game->iwad_name == "HERETIC.WAD" ? 35 : 2026)).asInt();

    std::string singular = get_item_name(game, swm_info.doom_type);
    swm_info.plural_name = json.get("plural_name", singular + "s").asString();
    swm_info.class_name = "start_with_" + to_snake_case(swm_info.plural_name);
}

void StartWithMaps_InsertWorldHook(game_t *game, const std::string& hook_type, std::vector<std::string>& hook)
{
    if (hook_type != "create_items")
        return;

    hook.push_back("map_opt = self.options." + swm_info.class_name);
    hook.push_back("if map_opt.value:");
    hook.push_back("    map_items = [pop_from_pool(i.name) for i in self.matching_items(doom_type=map_opt.doom_type).values()]");
    hook.push_back("    [self.multiworld.push_precollected(self.create_item(n)) for n in map_items if n is not None]");
}

void StartWithMaps_InsertPyOptions(game_t *game, std::vector<PyOption>& options)
{
    // If defaults, use the common types
    if (swm_info.doom_type == 2026 && swm_info.plural_name == "Computer area maps")
    {
        PyOption& opt = options.emplace_back(swm_info.class_name, "StartWithComputerAreaMaps", PyOptionType::InID1Common);
        opt.option_group = "Randomizer Options";
    }
    else if (swm_info.doom_type == 35 && swm_info.plural_name == "Map Scrolls")
    {
        PyOption& opt = options.emplace_back(swm_info.class_name, "StartWithMapScrolls", PyOptionType::InID1Common);
        opt.option_group = "Randomizer Options";
    }
    else
    {
        std::string public_name = "Start With " + to_title_case(swm_info.plural_name);
        PyOption& opt = options.emplace_back(swm_info.class_name, public_name, PyOptionType::StartWithMaps);
        opt.option_group = "Randomizer Options";
        opt.docstring.push_back("If enabled, all " + swm_info.plural_name + " will be given to the player from the start.");
        opt.doom_type = swm_info.doom_type;
    }
}

/********************************
** World Option: Invis as Trap **
********************************/

struct
{
    int doom_type;
    std::string class_name;
} invis_info;

void InvisAsTrap_Init(game_t *game, const Json::Value& json)
{
    invis_info.doom_type = json.get("doom_type", 2024).asInt();
    invis_info.class_name = to_snake_case(get_item_name(game, invis_info.doom_type)) + "_as_trap";
}

void InvisAsTrap_InsertWorldHook(game_t *game, const std::string& hook_type, std::vector<std::string>& hook)
{
    if (hook_type != "create_item")
        return;

    hook.push_back("invis_trap = self.options." + invis_info.class_name);
    hook.push_back("if invis_trap.value and item_data.doom_type == invis_trap.doom_type:");
    hook.push_back("    classification = AP.ItemClassification.trap");
}

void InvisAsTrap_InsertPyOptions(game_t *game, std::vector<PyOption>& options)
{
    const std::string& invis_name = get_item_name(game, invis_info.doom_type);

    // If defaults, use the common type
    if (invis_info.doom_type == 2024 && invis_name == "Partial invisibility")
    {
        PyOption& opt = options.emplace_back(invis_info.class_name, PyOptionType::InvisibilityTrap);
        opt.option_group = "Randomizer Options";
    }
    else
    {
        std::string public_name = to_title_case(invis_name) + " as Trap";

        PyOption& opt = options.emplace_back(invis_info.class_name, public_name, PyOptionType::InvisibilityTrap);
        opt.option_group = "Randomizer Options";
        opt.docstring.push_back("If enabled, " + invis_name +" will be classified as a trap, rather than just filler.");
        opt.docstring.push_back("This does not change how the item behaves, only how Archipelago sees it.");
        opt.doom_type = invis_info.doom_type;        
    }
}

/***************************************
** World Option: Custom Ammo Capacity **
***************************************/

struct capacitytype_t
{
    std::string name;
    std::string class_suffix;
    int capacity;
};
static std::vector<capacitytype_t> cac_ammo_types;

void CustomAmmoCapacity_Init(game_t *game, const Json::Value& json)
{
    (void)json; // not used
    cac_ammo_types.clear();

    const Json::Value& all_ammo = game->json_game_info.get("ammo", Json::arrayValue);
    for (const Json::Value &ammo : all_ammo)
    {
        capacitytype_t newcap;
        newcap.name = ammo.get("name", "(no name)").asString();
        newcap.class_suffix = to_snake_case(newcap.name);
        newcap.capacity = ammo.get("max", 0).asInt();
        cac_ammo_types.push_back(newcap);
    }
}

void CustomAmmoCapacity_InsertWorldHook(game_t *game, const std::string& hook_type, std::vector<std::string>& hook)
{
    if (hook_type != "fill_slot_data")
        return;

    hook.push_back("slot_data[\"ammo_start\"] = [");
    for (const capacitytype_t& ammo_type : cac_ammo_types)
        hook.push_back("    self.options.max_ammo_" + ammo_type.class_suffix + ".value,");
    hook.push_back("]");
    hook.push_back("slot_data[\"ammo_add\"] = [");
    for (const capacitytype_t& ammo_type : cac_ammo_types)
        hook.push_back("    self.options.added_ammo_" + ammo_type.class_suffix + ".value,");
    hook.push_back("]");
}

void CustomAmmoCapacity_InsertPyOptions(game_t *game, std::vector<PyOption>& options)
{
    for (const capacitytype_t& ammo_type : cac_ammo_types)
    {
        PyOption& max_opt = options.emplace_back(
            "max_ammo_" + ammo_type.class_suffix,
            "Max Ammo - " + ammo_type.name,
            PyOptionType::Range
        );
        max_opt.docstring.push_back("Set the starting capacity for " + ammo_type.name + ".");
        max_opt.option_group = "Ammo Capacity";
        max_opt.range_start = ammo_type.capacity;
        max_opt.range_end = 999;
        max_opt.default_int = ammo_type.capacity;
    }
    for (const capacitytype_t& ammo_type : cac_ammo_types)
    {
        PyOption& added_opt = options.emplace_back(
            "added_ammo_" + ammo_type.class_suffix,
            "Added Ammo - " + ammo_type.name,
            PyOptionType::Range
        );
        added_opt.docstring.push_back("Set how much capacity for " + ammo_type.name + " will be added when a capacity upgrade is obtained.");
        added_opt.option_group = "Ammo Capacity";
        added_opt.range_start = ammo_type.capacity / 10;
        added_opt.range_end = 999;
        added_opt.default_int = ammo_type.capacity;
    }
}

/************************************
** World Option: Capacity Upgrades **
************************************/

struct
{
    int doom_type;
    int item_count;
    std::string plural_name;

    std::string split_class;
    std::string count_class;
} capupg_info;

void CapacityUpgrades_Init(game_t *game, const Json::Value& json)
{
    capupg_info.doom_type = json.get("doom_type", 8).asInt();

    std::string singular = get_item_name(game, capupg_info.doom_type);
    capupg_info.plural_name = json.get("combined_plural_name", singular + "s").asString();

    capupg_info.split_class = "split_" + to_snake_case(singular);
    capupg_info.count_class = to_snake_case(singular) + "_count";

    capupg_info.item_count = json.get("item_count", (game->iwad_name == "HERETIC.WAD" ? 6 : 4)).asInt();
}

void CapacityUpgrades_InsertWorldHook(game_t *game, const std::string& hook_type, std::vector<std::string>& hook)
{
    if (hook_type != "create_items")
        return;

    hook.push_back("split_opt = self.options." + capupg_info.split_class);
    hook.push_back("split_items = [i for i in self.matching_items(doom_type=split_opt.split_doom_types).values()]");
    hook.push_back("combined_items = [i for i in self.matching_items(doom_type=split_opt.doom_type).values()]");
    hook.push_back("");
    hook.push_back("# Remove stray capacity upgrades of all types from the pool");
    hook.push_back("item_names = [i.name for i in split_items] + [i.name for i in combined_items]");
    hook.push_back("itempool = [n for n in itempool if n not in item_names]");
    hook.push_back("");
    hook.push_back("# Insert requested types and count of capacity upgrades");
    hook.push_back("if split_opt.value:");
    hook.push_back("    itempool += [i.name for i in split_items for _ in range(self.options." + capupg_info.count_class + ".value)]");
    hook.push_back("else:");
    hook.push_back("    itempool += [i.name for i in combined_items for _ in range(self.options." + capupg_info.count_class + ".value)]");
}

void CapacityUpgrades_InsertPyOptions(game_t *game, std::vector<PyOption>& options)
{
    std::string singular = get_item_name(game, capupg_info.doom_type);

    // If defaults, use the common types
    if (capupg_info.doom_type == 8 && singular == "Backpack" && capupg_info.item_count == 4)
    {
        PyOption& split_opt = options.emplace_back(capupg_info.split_class, "SplitBackpack", PyOptionType::InID1Common);
        split_opt.option_group = "Randomizer Options";
        PyOption& count_opt = options.emplace_back(capupg_info.count_class, "BackpackCount", PyOptionType::InID1Common);
        count_opt.option_group = "Randomizer Options";
    }
    else if (capupg_info.doom_type == 8 && singular == "Bag of Holding" && capupg_info.item_count == 6)
    {
        PyOption& split_opt = options.emplace_back(capupg_info.split_class, "SplitBagOfHolding", PyOptionType::InID1Common);
        split_opt.option_group = "Randomizer Options";
        PyOption& count_opt = options.emplace_back(capupg_info.count_class, "BagOfHoldingCount", PyOptionType::InID1Common);
        count_opt.option_group = "Randomizer Options";
    }
    else
    {
        std::string public_split_name = "Split " + to_title_case(singular);
        std::string public_count_name = to_title_case(singular) + " Count";

        PyOption& split_opt = options.emplace_back(capupg_info.split_class, public_split_name, PyOptionType::CapacitySplit);
        split_opt.option_group = "Randomizer Options";
        split_opt.docstring.push_back("Split the " + singular + " into " + std::to_string(capupg_info.item_count) + " individual items, "
            "each one increasing ammo capacity for one type of weapon only.");
        split_opt.doom_type = capupg_info.doom_type;
        split_opt.split_item_count = capupg_info.item_count;

        PyOption& count_opt = options.emplace_back(capupg_info.count_class, public_count_name, PyOptionType::CapacityCount);
        count_opt.option_group = "Randomizer Options";
        count_opt.docstring.push_back("How many " + capupg_info.plural_name + " will be available.");
        count_opt.docstring.push_back("If " + public_split_name + " is set, this will be the number of each capacity upgrade available.");
    }

}

// ============================================================================
// ============================================================================

struct WorldOptHandlers {
    void (*init)(game_t *game, const Json::Value&);
    void (*mix_worldhooks)(game_t *game, const std::string&, std::vector<std::string>&);
    void (*mix_pyoptions)(game_t *game, std::vector<PyOption>&);
};

static std::map<std::string, WorldOptHandlers> handlers = {
    {"Difficulty", {Difficulty_Init, Null_InsertWorldHook, Difficulty_InsertPyOptions}},
    {"Start with Maps", {StartWithMaps_Init, StartWithMaps_InsertWorldHook, StartWithMaps_InsertPyOptions}},
    {"Invis as Trap", {InvisAsTrap_Init, InvisAsTrap_InsertWorldHook, InvisAsTrap_InsertPyOptions}},
    {"Custom Ammo Capacity", {CustomAmmoCapacity_Init, CustomAmmoCapacity_InsertWorldHook, CustomAmmoCapacity_InsertPyOptions}},
    {"Capacity Upgrades", {CapacityUpgrades_Init, CapacityUpgrades_InsertWorldHook, CapacityUpgrades_InsertPyOptions}}
};

static std::vector<std::string> initialized_world_options;

int WorldOptions_Init(game_t *game)
{
    int unknown = 0;

    initialized_world_options.clear();
    for (const Json::Value& option : game->json_world_options)
    {
        std::string option_name = option.get("name", "(no name)").asString();
        if (!handlers.count(option_name))
        {
            ++unknown;
            OLogW("Unknown world option '" + option_name + "'!");
            continue;
        }

        handlers[option_name].init(game, option);
        initialized_world_options.push_back(option_name);
    }
    return unknown;
}

std::vector<std::string>& WorldOptions_GetAllHooks(game_t *game, const std::string& hook_type, int line_breaks)
{
    static std::vector<std::string> content;

    content.clear();

    // Mix in hooks from the game itself
    if (game->world_hooks.count(hook_type))
    {
        content.push_back("######## Custom code for this world begins here ########");
        content.insert(content.end(), game->world_hooks[hook_type].begin(), game->world_hooks[hook_type].end());
        content.push_back("######## Custom code for this world ends here ########");
        content.resize(content.size() + line_breaks, "");
    }

    // Mix in option hooks
    for (const std::string& opt_name : initialized_world_options)
    {
        static std::vector<std::string> temp;
        temp.clear();
 
        handlers[opt_name].mix_worldhooks(game, hook_type, temp);
        if (temp.empty())
            continue;

        content.push_back("######## Custom code for world option '" + opt_name + "' begins here ########");
        content.insert(content.end(), temp.begin(), temp.end());
        content.push_back("######## Custom code for world option '" + opt_name + "' ends here ########");
        content.resize(content.size() + line_breaks, "");
    }

    return content;
}

void WorldOptions_MixinPyOptions(game_t *game, std::vector<PyOption>& pyopts)
{
    for (const std::string& opt_name : initialized_world_options)
        handlers[opt_name].mix_pyoptions(game, pyopts);
}
