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
// *Interface header with the underlying C++ archipelago library*
//

#ifndef _APDOOM_
#define _APDOOM_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#include "config.h"

// Displayed in menus, uses shortened "APDOOM" instead of full name.
#define APDOOM_VERSION_FULL_TEXT "APDOOM " PACKAGE_VERSION


#define AP_CHECK_MAX 999 // 999 is enforced by ID format
#define AP_MAX_THING 10240 // List is dynamically allocated; this is more to guard against malformed defs

typedef struct
{
    int doom_type;
    int index;
    int64_t location_id;
} ap_thing_info_t;


typedef struct
{
    const char* name;
    int keys[3];
    int use_skull[3];
    int check_count;
    int true_check_count; // check count minus suppressed locations
    int thing_count;
    ap_thing_info_t* thing_infos; // Dynamically allocated

    int game_episode;
    int game_map;
    int music; // music ID used in vanilla
} ap_level_info_t;


typedef struct
{
    int completed;
    int keys[3];
    int has_map;
    int unlocked;
    int special; // Berzerk or Wings
    int flipped;
    int music; // music ID we're using for this map, post-music rando

    int check_count;
    int max_check_count;
    int64_t* checks; // Dynamically allocated
} ap_level_state_t;


// Don't construct that manually, use ap_make_level_index()
typedef struct
{
    int ep; // 0-based
    int map; // 0-based
} ap_level_index_t;


typedef struct
{
    int type;
    int count;
} ap_inventory_slot_t;


typedef struct
{
    int health;
    int armor_points;
    int armor_type;
    int ready_weapon; // Last weapon held
    int kill_count; // We accumulate globally
    int item_count;
    int secret_count;
    int* powers;
    int* weapon_owned;
    int* ammo;
    int* max_ammo; // Kept for easing calculations
    int* capacity_upgrades; // Replaces bool "backpack", track for each weapon
    ap_inventory_slot_t* inventory;

} ap_player_state_t;


typedef struct
{
    ap_level_state_t* level_states;
    ap_player_state_t player_state;
    int ep; // Useful when reloading, to load directly where we left
    int map;
    int difficulty;
    int random_monsters;
    int random_items;
    int random_music;
    //int two_ways_keydoors; // no longer used
    int* episodes;
    int flip_levels;
    //int check_sanity;
    int reset_level_on_death;
    int* max_ammo_start; // Starting ammo max
    int* max_ammo_add; // Ammo max gained with backpack/bag of holding
    
    int victory;
    int goal; // 0: all, 1: count, 2 or 3: specific
    int goal_level_count;
    ap_level_index_t* goal_level_list;
} ap_state_t;


typedef struct
{
    const char* temp_init_file; // Used for launcher
    const char* extra_args; // Stored in save file

    const char* ip;
    const char* game;
    const char* player_name;
    const char* passwd;
    void (*message_callback)(const char*);
    void (*victory_callback)(void);
    int (*give_item_callback)(int doom_type, int ep, int map);

    const char* save_dir;

    int override_skill; int skill;
    int override_monster_rando; int monster_rando;
    int override_item_rando; int item_rando;
    int override_music_rando; int music_rando;
    int override_flip_levels; int flip_levels;
    int force_deathlink_off;
    int override_reset_level_on_death; int reset_level_on_death;

    int always_show_obituaries;
} ap_settings_t;


#define AP_NOTIF_STATE_PENDING 0
#define AP_NOTIF_STATE_DROPPING 1
#define AP_NOTIF_STATE_HIDING 2
#define AP_NOTIF_SIZE 30
#define AP_NOTIF_PADDING 2
#define AP_NOTIF_ICONSIZE (AP_NOTIF_SIZE-(AP_NOTIF_PADDING*2))


typedef struct
{
    char sprite[9];
    int x, y;
    float xf, yf;
    float velx, vely;
    char text[40];
    int t;
    int state;
    int disabled;
} ap_notification_icon_t;


// Map item id
typedef struct
{
    int doom_type;
    int ep; // If doom_type is a keycard
    int map; // If doom_type is a keycard

    const char *name; // Name of the item -- only used in practice mode
} ap_item_t;

// ===== PWAD version specific structures =====================================
// Info on basic game data
typedef enum
{
    RGROUP_SMALL,
    RGROUP_MEDIUM,
    RGROUP_BIG,
    RGROUP_BOSS,
    NUM_RGROUPS
} rando_group_t;

typedef enum
{
    RLEVEL_NONE,
    RLEVEL_SHUFFLE,
    RLEVEL_SAMETYPE,
    RLEVEL_BALANCED,
    RLEVEL_CHAOTIC
} rando_level_t;

typedef struct
{
    int doom_type;
    rando_group_t group;
} ap_itemrando_t;

typedef struct {
    const char *name;
    int max_ammo;
} ap_ammo_info_t;

typedef struct {
    const char *name;
    int ammo_type;
    int start_ammo;
} ap_weapon_info_t;

typedef struct {
    ap_ammo_info_t *ammo_types;
    ap_weapon_info_t *weapons;

    int named_ammo_count;
    int named_weapon_count;

    ap_itemrando_t *rand_monster_types;
    ap_itemrando_t *rand_pickup_types;

    int start_health;
    int start_armor;

    const char *pause_pic;
    const char *goal_menu_flat;

    int levelsel_music_id;
} ap_gameinfo_t;

typedef struct {
    char graphic[9]; // Lump name to display
    short x;         // X position - for maps, added to base X coordinate of map
    short y;         // Y position - for maps, added to base Y coordinate of map
} ap_levelselect_patch_t;

typedef struct {
    const char *text; // Text to display
    char size;        // 0 == small, 1 == big (if available)
    short x;          // X position - for maps, added to base X coordinate of map
    short y;          // Y position - for maps, added to base Y coordinate of map
} ap_levelselect_text_t;

#define LS_MAP_DISPLAY_INDIVIDUAL -1
#define LS_MAP_DISPLAY_NONE        0
#define LS_MAP_DISPLAY_UPPER       1
#define LS_MAP_DISPLAY_LOWER       2

#define LS_RELATIVE_MAP          0
#define LS_RELATIVE_IMAGE        1
#define LS_RELATIVE_IMAGE_RIGHT  2
#define LS_RELATIVE_KEYS         3
#define LS_RELATIVE_KEYS_LAST    4

// All info for a single map on the level select screen
typedef struct { // All info for a specific map on the level select screen
    short x;
    short y;

    ap_levelselect_patch_t cursor; // Selection cursor / "You are here"
    ap_levelselect_patch_t complete; // Level complete splat
    ap_levelselect_patch_t locked; // Locked level indicator
    ap_levelselect_patch_t map_name; // Map name (as patch)
    ap_levelselect_text_t  map_text; // Map name (as text)

    char map_name_display; // for map_name: -1 == individual, 0 == don't display, 1 == upper, 2 == lower
    char map_text_display; // as above but for map_text

    struct { // Display of keys in map
        char relative_to;    // 0 == map, 1 == image, 2 == image-right
        char use_checkmark;  // 1 == shows all keys, and a checkmark shows if obtained, 0 == only shows obtained keys
        char use_custom_gfx; // 1 == Use LSKEY# instead of normal key graphics
        short x;             // Added to base X coordinate of relative choice above
        short y;             // Added to base Y coordinate of relative choice above
        short spacing_x;     // Added to each additional key's X coordinate after the first
        short spacing_y;     // Added to each additional key's Y coordinate after the first
        short align_x;       // Added to the base X coordinate, multiplied by number of keys
        short align_y;       // Added to the base Y coordinate, multiplied by number of keys
        short checkmark_x;   // If checkmark is enabled, added to each additional key's X coordinate
        short checkmark_y;   // If checkmark is enabled, added to each additional key's Y coordinate
    } keys;

    struct { // Display of check count
        char relative_to; // 0 == map, 1 == image, 2 == image-right, 3 == keys, 4 == keys-last
        short x;          // Added to base X coordinate of relative choice above
        short y;          // Added to base Y coordinate of relative choice above
    } checks;
} ap_levelselect_map_t;

// A single screen for the level select
typedef struct
{
    char background_image[9]; // Lump name to use as background

    int num_map_info;
    int num_text;
    int num_patches;

    ap_levelselect_map_t *map_info; // Primary data for maps
    ap_levelselect_text_t *text; // Extra arbitrary text strings
    ap_levelselect_patch_t *patches; // Extra arbitrary patches
} ap_levelselect_t;

// List of all tweaks we allow definitions JSONs to do.
// X_TWEAKS is used as a mask.
typedef enum
{
    HUB_TWEAKS = 0x00,
    TWEAK_HUB_X,
    TWEAK_HUB_Y,
    TWEAK_HUB_ANGLE,

    MAPTHING_TWEAKS = 0x10,
    TWEAK_MAPTHING_X,
    TWEAK_MAPTHING_Y,
    TWEAK_MAPTHING_TYPE,
    TWEAK_MAPTHING_ANGLE,
    TWEAK_MAPTHING_FLAGS,
    TWEAK_MAPTHING_FLYING_ONLY,
    TWEAK_MAPTHING_DONT_RANDOMIZE,
    TWEAK_MAPTHING_VOODOO_NOITEMS,
    TWEAK_MAPTHING_VOODOO_NODAMAGE,

    SECTOR_TWEAKS = 0x20,
    TWEAK_SECTOR_SPECIAL,
    TWEAK_SECTOR_TAG,
    TWEAK_SECTOR_FLOOR,
    TWEAK_SECTOR_FLOOR_PIC,
    TWEAK_SECTOR_CEILING,
    TWEAK_SECTOR_CEILING_PIC,

    LINEDEF_TWEAKS = 0x30,
    TWEAK_LINEDEF_SPECIAL,
    TWEAK_LINEDEF_TAG,
    TWEAK_LINEDEF_FLAGS,

    SIDEDEF_TWEAKS = 0x40,
    TWEAK_SIDEDEF_LOWER,
    TWEAK_SIDEDEF_MIDDLE,
    TWEAK_SIDEDEF_UPPER,
    TWEAK_SIDEDEF_X,
    TWEAK_SIDEDEF_Y,

    META_TWEAKS = 0xA0,
    TWEAK_META_BEHAVES_AS,
    TWEAK_META_SECRET_EXIT,
    TWEAK_META_SKY_TEXTURE,

    TWEAK_TYPE_MASK = 0xF0,
} allowed_tweaks_t;

typedef struct
{
    allowed_tweaks_t type;
    int target;
    int value;
    char string[9];
} ap_maptweak_t;

// AP specific mapthing flags:
#define APMTF_MASK            0xF000 // Reserve the top 4 bits for AP specific thing tweaks

#define APMTF_VOODOO_NOITEMS  0x1000 // Voodoo doll with this flag can't pick up items
#define APMTF_VOODOO_NODAMAGE 0x2000 // Voodoo doll with this flag can't transfer damage to player
#define APMTF_FLYING_ONLY     0x1000 // Enemy rando must place a flying enemy here
#define APMTF_DONT_RANDOMIZE  0x8000 // No matter what this thing is, it must spawn vanilla

// ============================================================================

extern ap_state_t ap_state;
extern int ap_is_in_game; // Don't give items when in menu (Or when dead on the ground).
extern int ap_episode_count;
extern int ap_race_mode; // Read from server. Used to disable cheats
extern int ap_practice_mode; // Offline testing mode.
extern int ap_debug_mode; // Additional debug info.
extern int ap_force_disable_behaviors; // Demo compatibility, disable most apdoom stuff


int apdoom_init(ap_settings_t* settings);
void apdoom_shutdown();
void apdoom_save_state();
void apdoom_check_location(ap_level_index_t idx, int index);
int apdoom_is_location_progression(ap_level_index_t idx, int index);
void apdoom_check_victory();
void apdoom_update();
const char* apdoom_get_save_dir();
void apdoom_remove_save_dir(void);
void apdoom_send_message(const char* msg);
void apdoom_complete_level(ap_level_index_t idx);
ap_level_state_t* ap_get_level_state(ap_level_index_t idx); // 1-based
ap_level_info_t* ap_get_level_info(ap_level_index_t idx); // 1-based
const ap_notification_icon_t* ap_get_notification_icons(int* count);
int ap_get_highest_episode();
int ap_validate_doom_location(ap_level_index_t idx, int doom_type, int index);
int ap_get_map_count(int ep);
int ap_total_check_count(const ap_level_info_t *level_info);
int ap_is_location_checked(ap_level_index_t idx, int index);

ap_level_index_t ap_try_make_level_index(int ep /* 1-based */, int map /* 1-based */);
ap_level_index_t ap_make_level_index(int ep /* 1-based */, int map /* 1-based */);
int ap_index_to_ep(ap_level_index_t idx);
int ap_index_to_map(ap_level_index_t idx);

// Remote data storage (global, or just for our slot if per_slot)
void ap_remote_set(const char *key, int per_slot, int value);

// ===== PWAD SUPPORT =========================================================
extern ap_gameinfo_t ap_game_info;

ap_levelselect_t *ap_get_level_select_info(unsigned int ep);

void ap_init_map_tweaks(ap_level_index_t idx, allowed_tweaks_t type_mask);
ap_maptweak_t *ap_get_map_tweaks();

int ap_preload_defs_for_game(const char *game_name);
int ap_is_location_type(int doom_type);

ap_level_index_t *ap_get_all_levels(void);
ap_level_index_t *ap_get_available_levels(void);

const ap_item_t* ap_get_item(int item_id);
const char* ap_get_sprite(int doom_type);

// ===== RANDOMNESS ===========================================================
void ap_srand(int hash);
unsigned int ap_rand(void);
void ap_shuffle(int *arr, int len);

// =====

typedef struct {
    // Short name of the game (e.g. "doom", "doom2"); must be unique
    // Used for the "-game" param to select which game to play
    const char *shortname;

    // Full name of the game (e.g. "DOOM (1993)", "DOOM II: Hell on Earth")
    const char *fullname;

    // Name of the game that the AP server refers to it by, used to connect to the slot
    const char *apname;

    // Relative path in world zip to game definitions file
    const char *definitions;

    // Required IWAD file, special behavior may be inferred based on which IWAD is used
    const char *iwad;

    // NULL-terminated list of authors in the manifest
    const char **authors;

    // NULL-terminated list of PWADs that must be present
    const char **required_wads;

    // NULL-terminated list of PWADs which are auto-loaded if present, but aren't required
    // (e.g. community music WAD, graphics modifications, etc.)
    const char **optional_wads;

    // NULL-terminated list of PWADs included in the world zip, by relative path
    const char **included_wads;
} ap_worldinfo_t;

const ap_worldinfo_t **ap_list_worlds(void);
const ap_worldinfo_t *ap_get_world(const char *shortname);
int ap_load_world(const char *shortname);
const ap_worldinfo_t *ap_loaded_world_info(void);

void ap_init_remap(const char *filename);
int ap_do_remap(char *lump_name);

// ===== DEATHLINK & OBITUARIES ===============================================

const char* APDOOM_ReceiveDeath(); // NULL, or death reason
const char* APDOOM_SendDeath(); // Returns your obituary
void APDOOM_ClearDeath();

void APDOOM_ObitTags_Clear(void);
void APDOOM_ObitTags_Add(const char *fmt, ...);

// ===== SAVE DATA ============================================================

typedef struct {
    char slot_name[16 + 1]; // -applayerhex <s>
    char address[128 + 1]; // -apserver <s>
    char password[128 + 1]; // -password <s>

    int practice_mode; // -practice

    // Overrides of server options
    int skill; // -skill <n>
    int monster_rando; // -apmonsterrando <n>
    int item_rando; // -apitemrando <n>
    int music_rando; // -apmusicrando <n>
    int flip_levels; // -apfliplevels <n>
    int reset_level; // -apresetlevelondeath <n>
    int no_deathlink; // -apdeathlinkoff

    char extra_cmdline[256 + 1];

    // ------------------------------------------------------------------------

    const ap_worldinfo_t *world;
    char path[256 + 1];
    char description[64 + 1];
    long int initial_timestamp;
    long int last_timestamp;
    int victory;
} ap_savesettings_t;

const ap_savesettings_t *APDOOM_FindSaves(int *save_count);
const char *APDOOM_GetSaveMemo(const ap_savesettings_t *save);
int APDOOM_SetSaveMemo(const ap_savesettings_t *save, const char *str);
int APDOOM_DeleteSave(const ap_savesettings_t *save);

// ===== ENERGYLINK ===========================================================

#define AP_ENERGYLINK_RATIO                                 25000000LL // Joules per base displayed "point" (2.5e7)
#define AP_ENERGYLINK_MAX         (1000000000LL * AP_ENERGYLINK_RATIO) // Maximum energy we track
#define AP_ENERGYLINK_COST(value)      ((value) * AP_ENERGYLINK_RATIO)

int APDOOM_EnergyLink_Enabled(void);

void APDOOM_EnergyLink_GiveEnergy(int64_t energy);
int APDOOM_EnergyLink_TakeEnergyForItem(int64_t energy, int item);
int APDOOM_EnergyLink_DisplayEnergy(void);

// Returns a zero-terminated list of items that should be available in the shop.
const int* APDOOM_EnergyLink_ShopItemList(int* count);

#ifdef __cplusplus
}
#endif


#endif
