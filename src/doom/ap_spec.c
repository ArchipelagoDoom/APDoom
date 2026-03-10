//
// Copyright(C) 2023 David St-Louis
// Copyright(C) 2026 Kay "Kaito" Sinclaire
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
// Game specific AP related functions
//

#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "dstrings.h"
#include "sounds.h"

#include "deh_main.h"
#include "deh_misc.h"

#include "f_finale.h"
#include "hu_stuff.h"
#include "p_local.h"
#include "s_sound.h"

#include "apdoom.h"
#include "ap_spec.h"

// Run every gametic, should call a function to update message state.

void tick_sticky_msgs(void)
{
    HU_TickAPMessages();
}

// ============================================================================

// Callback function for "ap_settings.message_callback"
// Should add the message to the HUD's message queue.

void APC_OnMessage(const char* text)
{
    HU_AddAPMessage(text); // This string is cached for several seconds
    S_StartSound(NULL, sfx_tink);
}

// ============================================================================

// Callback function for "ap_settings.victory_callback"
// Should play some sort of ending screen.

void APC_OnVictory(void)
{
    extern const char* finaletext;
    extern const char* finaleflat;
    //extern boolean finalfullscreenbg;
    //finalfullscreenbg = true;
    if (gamemode == commercial)
    {
        finaletext = "YOU DID IT!\n"
                     "BY TURNING THE EVIL OF THE HORRORS OF RNG IN UPON ITSELF YOU HAVE DESTROYED THE LOGIC OF THE MULTIWORLD.\n"
                     "THEIR DREADFUL CHECKS HAVE BEEN FOUND ONCE MORE!\n"
                     "NOW YOU CAN RETIRE TO A LIFETIME OF FRIVOLITY.\n"
                     "CONGRATULATIONS!";
        finaleflat = "CEIL5_1";
    }
    //else
    //{
    //    finaletext = "You've done it, you've saved the multiworld. Completely out of logic, the mighty RNG has bestowed you the gift of bringing Daisy back to life in this reality and in the next. Clearing the remaining evil forces on Mars can wait for now, you must make up the lost time with your pet rabbit.";
    //    finaleflat = "PFUB1";
    //}
    F_StartFinale();
}

// ============================================================================

// Callback function for "ap_settings.give_item_callback"

// Functions elsewhere in the source for giving the player items
boolean P_GiveArmor (player_t* player, int armortype);
boolean P_GiveBody  (player_t* player, int num);
boolean P_GivePower (player_t* player, int power);
boolean P_GiveWeapon(player_t* player, weapontype_t weapon, boolean dropped);
boolean P_GiveAmmo  (player_t* player, ammotype_t ammo, int num, boolean dropped);

static boolean is_in_level(int ep, int map)
{
    ap_level_index_t idx = { ep - 1, map - 1 };
    return gameepisode == ap_index_to_ep(idx) && gamemap == ap_index_to_map(idx);
}

// Kind of a copy of P_TouchSpecialThing
int APC_OnGiveItem(int doom_type, int ep, int map)
{
    player_t* player = &players[consoleplayer];
    int sound = sfx_itemup;
    ap_level_info_t* level_info = ap_get_level_info(ap_make_level_index(gameepisode, gamemap));

    if (!APC_CanGiveItem(doom_type))
    {
        int64_t value = APC_EnergyLinkItemCost(doom_type);

        const int *shopitem = APDOOM_EnergyLink_ShopItemList(NULL);
        for (; *shopitem && *shopitem != doom_type; ++shopitem) {}
        if (!(*shopitem))
            value /= 2; // Halve value of items not in the shop.

        APDOOM_EnergyLink_GiveEnergy(value);
        printf("Sent %ld to the EnergyLink pool.\n", value);
        return false;
    }

    switch (doom_type)
    {
        // Level specifics
        case 5: // Blue keycard
        case 40: // Blue skull key
            if (is_in_level(ep, map))
            {
                if (!player->cards[it_bluecard] && !player->cards[it_blueskull])
                {
                    player->cards[it_bluecard] = !level_info->use_skull[0];
                    player->cards[it_blueskull] = level_info->use_skull[0];
                    player->message = DEH_String((level_info->use_skull[0]) ? GOTBLUESKUL : GOTBLUECARD);
                    sound = sfx_keyup;
                }
            }
            break;
        case 6: // Yellow keycard
        case 39: // Yellow skull key
            if (is_in_level(ep, map))
            {
                if (!player->cards[it_yellowcard] && !player->cards[it_yellowskull])
                {
                    player->cards[it_yellowcard] = !level_info->use_skull[1];
                    player->cards[it_yellowskull] = level_info->use_skull[1];
                    player->message = DEH_String((level_info->use_skull[1]) ? GOTYELWSKUL : GOTYELWCARD);
                    sound = sfx_keyup;
                }
            }
            break;
        case 13: // Red keycard
        case 38: // Red skull key
            if (is_in_level(ep, map))
            {
                if (!player->cards[it_redcard] && !player->cards[it_redskull])
                {
                    player->cards[it_redcard] = !level_info->use_skull[2];
                    player->cards[it_redskull] = level_info->use_skull[2];
                    player->message = DEH_String((level_info->use_skull[2]) ? GOTREDSKULL : GOTREDCARD);
                    sound = sfx_keyup;
                }
            }
            break;
        case 2026: // Map
            if (is_in_level(ep, map))
            {
                P_GivePower(player, pw_allmap);
                player->message = DEH_String(GOTMAP);
                sound = sfx_getpow;
            }
            break;

        case 8: // Backpack
            player->message = DEH_String(GOTBACKPACK);
            // fall through
        case 65001: // Bullet capacity
        case 65002: // Shell capacity
        case 65003: // Energy cell capacity
        case 65004: // Rocket capacity
            // update max ammo with newly recalced values
            for (int i = 0; i < NUMAMMO; i++)
                player->maxammo[i] = ap_state.player_state.max_ammo[i];
            break;

        // Weapons
        case 2001:
            P_GiveWeapon(player, wp_shotgun, false);
            player->message = DEH_String(GOTSHOTGUN);
            sound = sfx_wpnup;  
            break;
        case 2002:
            P_GiveWeapon(player, wp_chaingun, false);
            player->message = DEH_String(GOTCHAINGUN);
            sound = sfx_wpnup;  
            break;
        case 2003:
            P_GiveWeapon(player, wp_missile, false);
            player->message = DEH_String(GOTLAUNCHER);
            sound = sfx_wpnup;  
            break;
        case 2004:
            P_GiveWeapon(player, wp_plasma, false);
            player->message = DEH_String(GOTPLASMA);
            sound = sfx_wpnup;  
            break;
        case 2005:
            P_GiveWeapon(player, wp_chainsaw, false);
            player->message = DEH_String(GOTCHAINSAW);
            sound = sfx_wpnup;  
            break;
        case 2006:
            P_GiveWeapon(player, wp_bfg, false);
            player->message = DEH_String(GOTBFG9000);
            sound = sfx_wpnup;  
            break;
        case 82:
            P_GiveWeapon(player, wp_supershotgun, false);
            player->message = DEH_String(GOTSHOTGUN2);
            sound = sfx_wpnup;  
            break;

        // Powerups
        case 2018:
            P_GiveArmor (player, deh_green_armor_class);
            player->message = DEH_String(GOTARMOR);
            break;
        case 2019:
            P_GiveArmor (player, deh_blue_armor_class);
            player->message = DEH_String(GOTMEGA);
            break;
        case 2023: // Berserk
            P_GivePower(player, pw_strength);
            player->message = DEH_String(GOTBERSERK);
            if (player->readyweapon != wp_fist)
                player->pendingweapon = wp_fist;
            sound = sfx_getpow;
            break;
        case 2013: // Supercharge
            player->health += deh_soulsphere_health;
            if (player->health > deh_max_soulsphere)
                player->health = deh_max_soulsphere;
            player->mo->health = player->health;
            player->message = DEH_String(GOTSUPER);
            sound = sfx_getpow;
            break;
        case 2022: // Invulnerability
            P_GivePower (player, pw_invulnerability);
            player->message = DEH_String(GOTINVUL);
            sound = sfx_getpow;
            break;
        case 2024: // Partial invisibility
            P_GivePower (player, pw_invisibility);
            player->message = DEH_String(GOTINVIS);
            sound = sfx_getpow;
            break;
        case 83: // Megasphere
            if (gamemode != commercial)
                return false;
            player->health = deh_megasphere_health;
            player->mo->health = player->health;

            // We always give armor type 2 for the megasphere; dehacked only 
            // affects the MegaArmor.
            P_GiveArmor (player, 2);
            player->message = DEH_String(GOTMSPHERE);
            sound = sfx_getpow;
            break;

        // Junk
        case 2012: // Medikit
            P_GiveBody(player, 25);
            // [crispy] show "Picked up a Medikit that you really need" message as intended
            if (player->health < 50)
                player->message = DEH_String(GOTMEDINEED);
            else
                player->message = DEH_String(GOTMEDIKIT);
            break;
        case 2048: // Box of bullets
            P_GiveAmmo(player, am_clip, 5, false);
            player->message = DEH_String(GOTCLIPBOX);
            break;
        case 2046: // Box of rockets
            P_GiveAmmo(player, am_misl, 5, false);
            player->message = DEH_String(GOTROCKBOX);
            break;
        case 2049: // Box of shotgun shells
            P_GiveAmmo (player, am_shell,5,false);
            player->message = DEH_String(GOTSHELLBOX);
            break;
        case 17: // Energy cell pack
            P_GiveAmmo (player, am_cell,5,false);
            player->message = DEH_String(GOTCELLBOX);
            break;

        // Things not usually present in random pool, but can be !getitem-ed or bought
        case 65000: // Shop ammo refill
            for (int i = 0; i < NUMAMMO; ++i)
                player->ammo[i] = player->maxammo[i];
            break;
        case 2025: // Radiation shielding suit
            P_GivePower (player, pw_ironfeet);
            player->message = DEH_String(GOTSUIT);
            sound = sfx_getpow;
            break;
        case 2045: // Light amplification visor
            P_GivePower (player, pw_infrared);
            player->message = DEH_String(GOTVISOR);
            sound = sfx_getpow;
            break;
    }

    if (gameversion <= exe_doom_1_2 && sound == sfx_getpow)
        sound = sfx_itemup;
    S_StartSoundOptional (NULL, sound, sfx_itemup); // [NS] Fallback to itemup.
    return true;
}

// ============================================================================

// These functions are mostly for handling EnergyLink interactions.

static int large_ammo_count(ammotype_t am)
{
    return (ap_state.difficulty == sk_baby || ap_state.difficulty == sk_nightmare || critical->moreammo)
        ? (clipammo[am] * 10) : (clipammo[am] * 5);
}

boolean APC_CanGiveItem(int doom_type)
{
    player_t* player = &players[consoleplayer];

    switch (doom_type)
    {
        default: // Any other item
            return 1;

        // Powerups
        case 2018: // Armor
            return (player->armorpoints < deh_green_armor_class * 100);
        case 2019: // Mega Armor
            return (player->armorpoints < deh_blue_armor_class * 100);
        case 2023: // Berserk
            return (player->health < MAXHEALTH || player->powers[pw_strength] == 0);
        case 2013: // Supercharge
            return (player->health < deh_max_soulsphere);
        case 2022: // Invulnerability
            return (player->powers[pw_invulnerability] < INVULNTICS - 2*TICRATE);
        case 2024: // Partial invisibility
            return (player->powers[pw_invisibility] < INVISTICS - 2*TICRATE);
        case 2025: // Radiation shielding suit
            return (player->powers[pw_ironfeet] < IRONTICS - 2*TICRATE);
        case 2045: // Light amplification visor
            return (player->powers[pw_infrared] < INFRATICS - 2*TICRATE);
        case 83: // Megasphere
            return (player->health < deh_megasphere_health || player->armorpoints < 2 * 100);

        // Ammo / Other Junk
        case 2012: // Medikit
            return (player->health < MAXHEALTH);
        case 2048: // Box of bullets
            return (player->ammo[am_clip] < player->maxammo[am_clip]);
        case 2046: // Box of rockets
            return (player->ammo[am_misl] < player->maxammo[am_misl]);
        case 2049: // Box of shotgun shells
            return (player->ammo[am_shell] < player->maxammo[am_shell]);
        case 17: // Energy cell pack
            return (player->ammo[am_cell] < player->maxammo[am_cell]);
        case 65000: // Shop ammo refill
            for (int i = 0; i < NUMAMMO; ++i)
            {
                if (player->ammo[i] < player->maxammo[i])
                    return 1;
            }
            return 0;
    }
}

int64_t APC_EnergyLinkItemCost(int doom_type)
{
    player_t* player = &players[consoleplayer];

    switch (doom_type)
    {
        default: // Any other item
            return 0;

        // Powerups
        case 2018: // Armor
            return AP_ENERGYLINK_COST(deh_green_armor_class * 100);
        case 2019: // Mega Armor
            return AP_ENERGYLINK_COST(deh_blue_armor_class * 100);
        case 2023: // Berserk
            return AP_ENERGYLINK_COST(125);
        case 2013: // Supercharge
            return AP_ENERGYLINK_COST(deh_soulsphere_health);
        case 2022: // Invulnerability
            return AP_ENERGYLINK_COST(400);
        case 2024: // Partial invisibility
            return AP_ENERGYLINK_COST(80);
        case 2025: // Radiation shielding suit
            return AP_ENERGYLINK_COST(80);
        case 2045: // Light amplification visor
            return AP_ENERGYLINK_COST(120);
        case 83: // Megasphere
            return AP_ENERGYLINK_COST(100 + deh_megasphere_health);

        // Ammo / Other Junk
        case 2012: // Medikit
            return AP_ENERGYLINK_COST(25);
        case 2048: // Box of bullets
        case 2046: // Box of rockets
        case 2049: // Box of shotgun shells
        case 17: // Energy cell pack
            return AP_ENERGYLINK_COST(10);
        case 65000: // Shop ammo refill
        {
            int64_t pickups = 0; // Simulate giving large pickups from empty to full.
            for (int i = 0; i < NUMAMMO; ++i)
            {
                const int largeammo = large_ammo_count(i);
                if (largeammo <= 0) continue;
                const int numammo = player->maxammo[i] + largeammo - 1;
                pickups += (numammo / largeammo);
            }
            return AP_ENERGYLINK_COST(pickups * 4);
        }
    }
}
