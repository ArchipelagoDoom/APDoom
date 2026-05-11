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

#include "deh_main.h"
#include "p_local.h"
#include "s_sound.h"

#include "apdoom.h"
#include "ap_msg.h"
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
    S_StartSound(NULL, sfx_chat);
}

// ============================================================================

// Callback function for "ap_settings.victory_callback"
// Should play some sort of ending screen.

void APC_OnVictory(void)
{
    F_StartFinale();
}

// ============================================================================

// Callback function for "ap_settings.give_item_callback"

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

    // unused in heretic, no skull keys
    //ap_level_info_t* level_info = ap_get_level_info(ap_make_level_index(gameepisode, gamemap));

    if (!APC_CanGiveItem(doom_type))
    {
        int64_t value = APC_EnergyLinkItemCost(doom_type);

        const int *shopitem = APDOOM_EnergyLink_ShopItemList(NULL);
        for (; *shopitem && *shopitem != doom_type; ++shopitem) {}
        if (!(*shopitem))
            value /= 2; // Halve value of items not in the shop.

        APDOOM_EnergyLink_GiveEnergy(value);
        return false;
    }

    switch (doom_type)
    {
        // Level specifics
        case 79:
            if (is_in_level(ep, map))
            {
                if (!player->keys[key_blue])
                {
                    player->keys[key_blue] = true;
                    P_SetMessage(player, DEH_String(TXT_GOTBLUEKEY), false);
                    sound = sfx_keyup;
                }
            }
            break;
        case 80:
            if (is_in_level(ep, map))
            {
                if (!player->keys[key_yellow])
                {
                    player->keys[key_yellow] = true;
                    P_SetMessage(player, DEH_String(TXT_GOTYELLOWKEY), false);
                    sound = sfx_keyup;
                }
            }
            break;
        case 73:
            if (is_in_level(ep, map))
            {
                if (!player->keys[key_green])
                {
                    player->keys[key_green] = true;
	                P_SetMessage(player, DEH_String(TXT_GOTGREENKEY), false);
                    sound = sfx_keyup;
                }
            }
            break;
        case 35: // Map
            if (is_in_level(ep, map))
            {
	            if (P_GivePower(player, pw_allmap))
                {
                    P_SetMessage(player, DEH_String(TXT_ITEMSUPERMAP), false);
                }
            }
            break;

        case 8: // Bag of Holding
            P_SetMessage(player, DEH_String(TXT_ITEMBAGOFHOLDING), false);
            // fall through
        case 65001: // Wand crystal capacity
        case 65002: // Ethereal arrow capacity
        case 65003: // Claw orb capacity
        case 65004: // Rune capacity
        case 65005: // Flame orb capacity
        case 65006: // Mace sphere capacity
            // update max ammo with newly recalced values
            for (int i = 0; i < NUMAMMO; i++)
                player->maxammo[i] = ap_state.player_state.max_ammo[i];
            break;

        // Weapons
        case 2005:
            P_GiveWeapon(player, wp_gauntlets);
	        P_SetMessage(player, DEH_String(TXT_WPNGAUNTLETS), false);
	        sound = sfx_wpnup;	
            break;
        case 2001:
            P_GiveWeapon(player, wp_crossbow);
	        P_SetMessage(player, DEH_String(TXT_WPNCROSSBOW), false);
	        sound = sfx_wpnup;	
            break;
        case 53:
            P_GiveWeapon(player, wp_blaster);
	        P_SetMessage(player, DEH_String(TXT_WPNBLASTER), false);
	        sound = sfx_wpnup;	
            break;
        case 2003:
            P_GiveWeapon(player, wp_phoenixrod);
	        P_SetMessage(player, DEH_String(TXT_WPNPHOENIXROD), false);
	        sound = sfx_wpnup;	
            break;
        case 2002:
            P_GiveWeapon(player, wp_mace);
	        P_SetMessage(player, DEH_String(TXT_WPNMACE), false);
	        sound = sfx_wpnup;	
            break;
        case 2004:
            P_GiveWeapon(player, wp_skullrod);
	        P_SetMessage(player, DEH_String(TXT_WPNSKULLROD), false);
	        sound = sfx_wpnup;	
            break;

        // Powerups
        case 85:
	        P_GiveArmor (player, 1);
            P_SetMessage(player, DEH_String(TXT_ITEMSHIELD1), false);
            break;
        case 31:
	        P_GiveArmor (player, 2);
            P_SetMessage(player, DEH_String(TXT_ITEMSHIELD2), false);
            break;

        // Artifacts
        case 36: // Chaos Device
            P_GiveArtifact(player, arti_teleport, 0);
            P_SetMessage(player, DEH_String(TXT_ARTITELEPORT), false);
            break;
        case 30: // Morph Ovum
            P_GiveArtifact(player, arti_egg, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIEGG), false);
            break;
        case 32: // Mystic Urn
            P_GiveArtifact(player, arti_superhealth, 0);
            P_SetMessage(player, DEH_String(TXT_ARTISUPERHEALTH), false);
            break;
        case 82: // Quartz Flask
            P_GiveArtifact(player, arti_health, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIHEALTH), false);
            break;
        case 84: // Ring of Invincibility
            P_GiveArtifact(player, arti_invulnerability, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIINVULNERABILITY), false);
            break;
        case 75: // Shadowsphere
            P_GiveArtifact(player, arti_invisibility, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIINVISIBILITY), false);
            break;
        case 34: // Timebomb of the Ancients
            P_GiveArtifact(player, arti_firebomb, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIFIREBOMB), false);
            break;
        case 86: // Tome of Power
            P_GiveArtifact(player, arti_tomeofpower, 0);
            P_SetMessage(player, DEH_String(TXT_ARTITOMEOFPOWER), false);
            break;
        case 83: // Wings of Wrath
            P_GiveArtifact(player, arti_fly, 0);
            P_SetMessage(player, DEH_String(TXT_ARTIFLY), false);
            break;
        case 33: // Torch
            P_GiveArtifact(player, arti_torch, 0);
            P_SetMessage(player, DEH_String(TXT_ARTITORCH), false);
            break;

        // Junk
        case 81: // Crystal Vial
            P_GiveBody(player, 10);
            P_SetMessage(player, DEH_String(TXT_ITEMHEALTH), false);
            break;
        case 12: // Crystal Geode
            P_GiveAmmo(player, am_goldwand, AMMO_GWND_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOGOLDWAND2), false);
            break;
        case 55: // Energy Orb
            P_GiveAmmo(player, am_blaster, AMMO_BLSR_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOBLASTER2), false);
            break;
        case 21: // Greater Runes
            P_GiveAmmo(player, am_skullrod, AMMO_SKRD_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOSKULLROD2), false);
            break;
        case 23: // Inferno Orb
            P_GiveAmmo(player, am_phoenixrod, AMMO_PHRD_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOPHOENIXROD2), false);
            break;
        case 16: // Pile of Mace Spheres
            P_GiveAmmo(player, am_mace, AMMO_MACE_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOMACE2), false);
            break;
        case 19: // Quiver of Ethereal Arrows
            P_GiveAmmo(player, am_crossbow, AMMO_CBOW_HEFTY);
            P_SetMessage(player, DEH_String(TXT_AMMOCROSSBOW2), false);
            break;

        // Things not usually present in random pool, but can be !getitem-ed or bought
        case 65000: // Shop ammo refill
            for (int i = 0; i < NUMAMMO; ++i)
                player->ammo[i] = player->maxammo[i];
            break;

    }

	S_StartSound(NULL, sound); // [NS] Fallback to itemup.
    return true;
}

// ============================================================================

// These functions are mostly for handling EnergyLink interactions.

static int large_ammo_count(ammotype_t am)
{
    int value = 0;
    switch (am)
    {
        case am_goldwand:   value = AMMO_GWND_HEFTY; break;
        case am_blaster:    value = AMMO_BLSR_HEFTY; break;
        case am_skullrod:   value = AMMO_SKRD_HEFTY; break;
        case am_phoenixrod: value = AMMO_PHRD_HEFTY; break;
        case am_mace:       value = AMMO_MACE_HEFTY; break;
        case am_crossbow:   value = AMMO_CBOW_HEFTY; break;
        default:            return 0;
    }
    if (ap_state.difficulty == sk_baby || ap_state.difficulty == sk_nightmare || critical->moreammo)
        value += value >> 1;
    return value;
}

static int count_player_artifacts(player_t *player, artitype_t arti)
{
    int i = 0;
    while (player->inventory[i].type != arti && i < player->inventorySlotNum)
        ++i;
    return (i == player->inventorySlotNum) ? 0 : player->inventory[i].count;
}

boolean APC_CanGiveItem(int doom_type)
{
    player_t* player = &players[consoleplayer];

    switch (doom_type)
    {
        default: // Any other item
            return true;

        // Powerups
        case 85: // Silver Shield
            return (player->armorpoints < 100);
        case 31: // Enchanted Shield
            return (player->armorpoints < 200);

        // Artifacts
        case 36: // Chaos Device
            return (count_player_artifacts(player, arti_teleport) < 16);
        case 30: // Morph Ovum
            return (count_player_artifacts(player, arti_egg) < 16);
        case 32: // Mystic Urn
            return (count_player_artifacts(player, arti_superhealth) < 16);
        case 82: // Quartz Flask
            return (count_player_artifacts(player, arti_health) < 16);
        case 84: // Ring of Invincibility
            return (count_player_artifacts(player, arti_invulnerability) < 16);
        case 75: // Shadowsphere
            return (count_player_artifacts(player, arti_invisibility) < 16);
        case 34: // Timebomb of the Ancients
            return (count_player_artifacts(player, arti_firebomb) < 16);
        case 86: // Tome of Power
            return (count_player_artifacts(player, arti_tomeofpower) < 16);
        case 83: // Wings of Wrath
            return (count_player_artifacts(player, arti_fly) < 16);
        case 33: // Torch
            return (count_player_artifacts(player, arti_torch) < 16);

        // Junk
        case 81: // Crystal Vial
            return (player->health < MAXHEALTH);
        case 12: // Crystal Geode
            return (player->ammo[am_goldwand] < player->maxammo[am_goldwand]);
        case 55: // Energy Orb
            return (player->ammo[am_blaster] < player->maxammo[am_blaster]);
        case 21: // Greater Runes
            return (player->ammo[am_skullrod] < player->maxammo[am_skullrod]);
        case 23: // Inferno Orb
            return (player->ammo[am_phoenixrod] < player->maxammo[am_phoenixrod]);
        case 16: // Pile of Mace Spheres
            return (player->ammo[am_mace] < player->maxammo[am_mace]);
        case 19: // Quiver of Ethereal Arrows
            return (player->ammo[am_crossbow] < player->maxammo[am_crossbow]);

        // Other
        case 65000: // Shop ammo refill
            for (int i = 0; i < NUMAMMO; ++i)
            {
                if (player->ammo[i] < player->maxammo[i])
                    return true;
            }
            return false;
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
        case 85: // Silver Shield
            return AP_ENERGYLINK_COST(100);
        case 31: // Enchanted Shield
            return AP_ENERGYLINK_COST(200);

        // Artifacts
        case 36: // Chaos Device
            return AP_ENERGYLINK_COST(125);
        case 30: // Morph Ovum
            return AP_ENERGYLINK_COST(70);
        case 32: // Mystic Urn
            return AP_ENERGYLINK_COST(100);
        case 82: // Quartz Flask
            return AP_ENERGYLINK_COST(25);
        case 84: // Ring of Invincibility
            return AP_ENERGYLINK_COST(500);
        case 75: // Shadowsphere
            return AP_ENERGYLINK_COST(100);
        case 34: // Timebomb of the Ancients
            return AP_ENERGYLINK_COST(30);
        case 86: // Tome of Power
            return AP_ENERGYLINK_COST(150);
        case 83: // Wings of Wrath
            return 0; // This... should not ever happen?
        case 33: // Torch
            return AP_ENERGYLINK_COST(120);

        // Junk
        case 81: // Crystal Vial
            return AP_ENERGYLINK_COST(10);
        case 12: // Crystal Geode
        case 55: // Energy Orb
        case 21: // Greater Runes
        case 23: // Inferno Orb
        case 16: // Pile of Mace Spheres
        case 19: // Quiver of Ethereal Arrows
            return AP_ENERGYLINK_COST(8);

        // Other
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
            return AP_ENERGYLINK_COST(pickups * 2);
        }
    }
}

int APC_ArtifactCount(int doom_type)
{
    player_t* player = &players[consoleplayer];

    switch (doom_type)
    {
        default:
            return -1;
        case 36: // Chaos Device
            return count_player_artifacts(player, arti_teleport);
        case 30: // Morph Ovum
            return count_player_artifacts(player, arti_egg);
        case 32: // Mystic Urn
            return count_player_artifacts(player, arti_superhealth);
        case 82: // Quartz Flask
            return count_player_artifacts(player, arti_health);
        case 84: // Ring of Invincibility
            return count_player_artifacts(player, arti_invulnerability);
        case 75: // Shadowsphere
            return count_player_artifacts(player, arti_invisibility);
        case 34: // Timebomb of the Ancients
            return count_player_artifacts(player, arti_firebomb);
        case 86: // Tome of Power
            return count_player_artifacts(player, arti_tomeofpower);
        case 83: // Wings of Wrath
            return count_player_artifacts(player, arti_fly);
        case 33: // Torch
            return count_player_artifacts(player, arti_torch);
    }
}
