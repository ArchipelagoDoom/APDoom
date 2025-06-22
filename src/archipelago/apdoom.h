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


#ifdef __cplusplus
extern "C"
{
#endif



#define APDOOM_MAJOR 1
#define APDOOM_MINOR 2
#define APDOOM_PATCH 0
#define APDOOM_STR(x) APDOOM_STR2(x)
#define APDOOM_STR2(x) #x
#define APDOOM_VERSION APDOOM_STR(APDOOM_MAJOR) "." APDOOM_STR(APDOOM_MINOR) "." APDOOM_STR(APDOOM_PATCH)
#define APDOOM_VERSION_TEXT APDOOM_VERSION ""
#define APDOOM_VERSION_FULL_TEXT "APDOOM " APDOOM_VERSION_TEXT " PWAD"


#define AP_CHECK_MAX 128 // Arbitrary number (raised from 64)
#define AP_MAX_THING 10240 // List is dynamically allocated; this is more to guard against malformed defs

typedef struct
{
    int doom_type;
    int index;
    int check_sanity;
    int unreachable;
} ap_thing_info_t;


typedef struct
{
    const char* name;
    int keys[3];
    int use_skull[3];
    int check_count;
    int thing_count;
    ap_thing_info_t* thing_infos; // Dynamically allocated
    int sanity_check_count;

    int game_episode;
    int game_map;
} ap_level_info_t;


typedef struct
{
    int completed;
    int keys[3];
    int check_count;
    int has_map;
    int unlocked;
    int checks[AP_CHECK_MAX];
    int special; // Berzerk or Wings
    int flipped;
    int music;

} ap_level_state_t;


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
    int two_ways_keydoors;
    int* episodes;
    int victory;
    int flip_levels;
    int check_sanity;
    int reset_level_on_death;
    int goal;
    int* max_ammo_start; // Starting ammo max
    int* max_ammo_add; // Ammo max gained with backpack/bag of holding
    
} ap_state_t;


typedef struct
{
    const char* ip;
    const char* game;
    const char* player_name;
    const char* passwd;
    void (*message_callback)(const char*);
    void (*give_item_callback)(int doom_type, int ep, int map);
    void (*victory_callback)();

    const char* save_dir;

    int override_skill; int skill;
    int override_monster_rando; int monster_rando;
    int override_item_rando; int item_rando;
    int override_music_rando; int music_rando;
    int override_flip_levels; int flip_levels;
    int force_deathlink_off;
    int override_reset_level_on_death; int reset_level_on_death;
} ap_settings_t;


#define AP_NOTIF_STATE_PENDING 0
#define AP_NOTIF_STATE_DROPPING 1
#define AP_NOTIF_STATE_HIDING 2
#define AP_NOTIF_SIZE 30
#define AP_NOTIF_PADDING 2


typedef struct
{
    char sprite[9];
    int x, y;
    float xf, yf;
    float velx, vely;
    char text[260];
    int t;
    int state;
} ap_notification_icon_t;


// Don't construct that manually, use ap_make_level_index()
typedef struct
{
    int ep; // 0-based
    int map; // 0-based
} ap_level_index_t;


// Map item id
typedef struct
{
    int doom_type;
    int ep; // If doom_type is a keycard
    int map; // If doom_type is a keycard
} ap_item_t;

// ===== PWAD version specific structures =====================================
// Info on basic game data
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

    int start_health;
    int start_armor;

    const char *pausepic;
} ap_gameinfo_t;

// All info for a single map on the level select screen
typedef struct { // All info for a specific map on the level select screen
    int x;
    int y;

    struct { // Selection cursor / "You are here"
        char graphic[9]; // Lump name to display when this map is selected
        int x;           // Added to base X coordinate of map
        int y;           // Added to base Y coordinate of map
    } cursor;

    struct { // Image or text of map name
        const char *text; // Name of map to display; use either this, or lump name below
        char graphic[9];  // Lump name to display; use either this, or text above
        int x;            // Added to base X coordinate of map, in individual mode
        int y;            // Added to base Y coordinate of map, in individual mode
    } map_name;

    struct { // Display of keys in map
        int relative_to;   // 0 == map, 1 == image, 2 == image-right
        int x;             // Added to base X coordinate of relative choice above
        int y;             // Added to base Y coordinate of relative choice above
        int spacing_x;     // Added to each additional key's X coordinate after the first
        int spacing_y;     // Added to each additional key's Y coordinate after the first
        int align_x;       // Added to the base X coordinate, multiplied by number of keys
        int align_y;       // Added to the base Y coordinate, multiplied by number of keys
        int checkmark_x;   // If checkmark is enabled, added to each additional key's X coordinate
        int checkmark_y;   // If checkmark is enabled, added to each additional key's Y coordinate
        int use_checkmark; // 1 == shows all keys, and a checkmark shows if obtained, 0 == only shows obtained keys
    } keys;

    struct { // Display of check count
        int relative_to; // 0 == map, 1 == image, 2 == image-right, 3 == keys, 4 == keys-last
        int x;           // Added to base X coordinate of relative choice above
        int y;           // Added to base Y coordinate of relative choice above
    } checks;
} ap_levelselect_map_t;

// A single screen for the level select
typedef struct
{
    char background_image[9]; // Lump name to use as background
    int map_names; // negative for upper, positive for lower, zero for individual display

    ap_levelselect_map_t map_info[12];
} ap_levelselect_t;

// List of all tweaks we allow definitions JSONs to do.
// X_TWEAKS is used as a mask.
typedef enum
{
    HUB_TWEAKS = 0x00,
    TWEAK_HUB_X,
    TWEAK_HUB_Y,

    MAPTHING_TWEAKS = 0x10,
    TWEAK_MAPTHING_X,
    TWEAK_MAPTHING_Y,
    TWEAK_MAPTHING_TYPE,
    TWEAK_MAPTHING_ANGLE,

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

    TWEAK_TYPE_MASK = 0xF0,
} allowed_tweaks_t;

typedef struct
{
    allowed_tweaks_t type;
    int target;
    int value;
    char string[9];
} ap_maptweak_t;

typedef struct {
    const char *input;
    const char *replace_normal;
    const char *replace_skull;
    int key_id;
} ap_hint_autocomplete_t;
// ============================================================================

extern ap_state_t ap_state;
extern int ap_is_in_game; // Don't give items when in menu (Or when dead on the ground).
extern int ap_episode_count;

int apdoom_init(ap_settings_t* settings);
void apdoom_shutdown();
void apdoom_save_state();
void apdoom_check_location(ap_level_index_t idx, int index);
int apdoom_is_location_progression(ap_level_index_t idx, int index);
void apdoom_check_victory();
void apdoom_update();
const char* apdoom_get_seed();
void apdoom_send_message(const char* msg);
void apdoom_complete_level(ap_level_index_t idx);
ap_level_state_t* ap_get_level_state(ap_level_index_t idx); // 1-based
ap_level_info_t* ap_get_level_info(ap_level_index_t idx); // 1-based
const ap_notification_icon_t* ap_get_notification_icons(int* count);
int ap_get_highest_episode();
int ap_validate_doom_location(ap_level_index_t idx, int doom_type, int index);
int ap_get_map_count(int ep);
int ap_total_check_count(ap_level_info_t *level_info);

// Deathlink stuff
void apdoom_on_death();
void apdoom_clear_death();
int apdoom_should_die();

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
const char *ap_get_iwad_name();
const char *ap_get_pwad_name(unsigned int id);
int ap_is_location_type(int doom_type);
// ============================================================================

#ifdef __cplusplus
}
#endif


#endif
