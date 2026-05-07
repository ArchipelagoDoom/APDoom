// This file is meant to be included in each game's "m_menu.c" or equivalent.
// ============================================================================
// [AP] EnergyLink Item Obtain Menu
// ============================================================================

#if defined(AP_INC_DOOM)
#define NumDrawFunc(x, y, num)  ST_RightAlignedShortNum(x, y, num)
#define TextDrawFunc(x, y, tx)  M_WriteText(x, y, tx)
#define TextWidthFunc(tx)       M_StringWidth(tx)
#define MenuRef                 currentMenu
#define HandlerReturnType       void
#elif defined(AP_INC_HERETIC)
#define NumDrawFunc(x, y, num)  SB_RightAlignedSmallNum(x, y, num)
#define TextDrawFunc(x, y, tx)  MN_DrTextA(tx, x, y - 2)
#define TextWidthFunc(tx)       MN_TextAWidth(tx)
#define MenuRef                 CurrentMenu
#define HandlerReturnType       int
#endif

static HandlerReturnType EnergyLink_Handler(int key);
static void EnergyLink_Drawer(void);

#if defined(AP_INC_DOOM)

static menuitem_t MIEnergyLink[] =
{
    {5,"",EnergyLink_Handler,'\0'}
};

static menu_t EnergyLinkDef =
{
    1,
    NULL,
    MIEnergyLink,
    EnergyLink_Drawer,
    0, // used to track cursor
    -666, // inhibits default drawing
    0,
    0
};

// Only Doom needs this function.
static void M_EnergyLink(int choice)
{
	M_SetupNextMenu(&EnergyLinkDef);
}

// We need a prototype for the num function because it's not public.
void ST_RightAlignedShortNum(int x, int y, int digit);

#elif defined(AP_INC_HERETIC)

static MenuItem_t MIEnergyLink[] =
{
    {ITT_HIJACK, "", EnergyLink_Handler, 0, MENU_NONE}
};

static Menu_t EnergyLinkMenu = {
    0, // used to track cursor
    -666, // inhibits default drawing
	EnergyLink_Drawer,
	1,
	MIEnergyLink,
	0,
	MENU_NONE
};

// We need a prototype for the num function because it's not public.
void SB_RightAlignedSmallNum(int x, int y, int digit);

#endif

// ----------------------------------------------------------------------------

void EnergyLink_Drawer(void)
{
    int cursor = MenuRef->x;
    int menumax = 0;
    const int* elshop_items = APDOOM_EnergyLink_ShopItemList(&menumax);

#ifdef AP_INC_DOOM
    // Always refresh the status bar.
    inhelpscreens = true;
#endif

    if (ap_game_info.goal_menu_flat)
    { // Draw background
        pixel_t *dest = I_VideoBuffer;
        const byte *src = W_CacheLumpName(ap_game_info.goal_menu_flat, PU_CACHE);

        V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);
    }

    // Draw an EnergyLink header.
#if defined(AP_INC_DOOM)
    patch_t *header = W_CacheLumpName("M_ENERGY", PU_CACHE);
    V_DrawPatchDirect(ORIGWIDTH/2 - header->width/2, 15, header);
#elif defined(AP_INC_HERETIC)
    MN_DrTextB("ENERGY LINK", ORIGWIDTH/2 - MN_TextBWidth("ENERGY LINK")/2, 15);
#endif

    patch_t *icon_sel = W_CacheLumpName("NOTIFSEL", PU_CACHE);
    char text_buffer[80];

    for (int i = 0; elshop_items[i]; ++i)
    {
        const ap_item_t *item = ap_get_item(elshop_items[i]);
        if (!item)
            continue;

        const int x = (ORIGWIDTH/2) + ((i-cursor) * (AP_NOTIF_SIZE*4/3));
        const int y = 110;
        if (x + AP_NOTIF_SIZE/2 < 0 - WIDESCREENDELTA)
            continue; // Off left edge
        if (x - AP_NOTIF_SIZE/2 >= ORIGWIDTH + WIDESCREENDELTA)
            break; // Off right edge (no point in drawing anything further either)

        // Darken bg and item if the item has no use.
        const boolean usable = APC_CanGiveItem(item->doom_type);

        APC_DrawNotifBox(x, y, ap_get_sprite(item->doom_type), !usable);
        NumDrawFunc(
            x + (AP_NOTIF_SIZE / 2) - 2,
            y + (AP_NOTIF_SIZE / 2) - 8,
            APC_EnergyLinkItemCost(elshop_items[i]) / AP_ENERGYLINK_RATIO);

        if (i == cursor)
        {
            V_DrawPatch(
                x - (AP_NOTIF_SIZE / 2),
                y - (AP_NOTIF_SIZE / 2),
                icon_sel);
            TextDrawFunc((ORIGWIDTH / 2) - TextWidthFunc(item->name) / 2, 70, item->name);

#ifdef AP_INC_HERETIC
            // For Heretic's artifacts, show artifact count
            int articount = APC_ArtifactCount(item->doom_type);
            if (articount != -1)
            {
                const int base_w = TextWidthFunc("(Have: 16)");
                const int base_x = (ORIGWIDTH / 2) - (base_w / 2);

                M_snprintf(text_buffer, sizeof(text_buffer), "%s%d%s)",
                    crstr[CR_GOLD], articount, crstr[CR_NONE]);
                TextDrawFunc(base_x, 80, "(Have:");
                TextDrawFunc(base_x + base_w - TextWidthFunc(text_buffer), 80, text_buffer);
            }
            else
#endif
            if (!usable)
                TextDrawFunc((ORIGWIDTH / 2) - TextWidthFunc("(No effect)") / 2, 80, "(No effect)");
        }
    }

    const char *reminder_text = (gamestate == GS_LEVEL
        ? "Items take effect after unpausing."
        : "Items take effect when entering a level.");
    M_snprintf(text_buffer, sizeof(text_buffer), "EnergyLink credit: %s%d",
        crstr[CR_GOLD], APDOOM_EnergyLink_DisplayEnergy());

#if defined(AP_INC_DOOM)
    // Doom's status bar is helpful enough that it's a good idea to display it here.
    void ST_RenderStatusBarAnywhere(void);
    ST_RenderStatusBarAnywhere();

    // We now need to compensate for the status bar's presence.
    const int font_height = SHORT(hu_font[0]->height);
    TextDrawFunc(0 - WIDESCREENDELTA, 167 - font_height, reminder_text);
    TextDrawFunc(0 - WIDESCREENDELTA, 167 - (font_height*2) - 1, text_buffer);

    // Use the font to draw arrows
    dp_translation = cr[whichSkull ? CR_RED : CR_DARK];
    if (MenuRef->x > 0)
        TextDrawFunc(8, 70, "<");
    if (MenuRef->x < menumax - 1)
        TextDrawFunc((ORIGWIDTH - 8) - hu_font['>'-HU_FONTSTART]->width, 70, ">");
    dp_translation = NULL;
#elif defined(AP_INC_HERETIC)
    // Heretic's status bar is substantially less helpful for this, so we ignore it.
    TextDrawFunc((ORIGWIDTH / 2) - TextWidthFunc(reminder_text) / 2, 190, reminder_text);
    TextDrawFunc((ORIGWIDTH / 2) - TextWidthFunc(text_buffer) / 2,   180, text_buffer);

    // Use patches instead, the font doesn't work as well for this.
    if (MenuRef->x > 0)
        V_DrawPatch(8, 70, W_CacheLumpName((MenuTime&8) ? "INVGEML1" : "INVGEML2", PU_CACHE));
    if (MenuRef->x < menumax - 1)
        V_DrawPatch((ORIGWIDTH - 8) - 8, 70, W_CacheLumpName((MenuTime&8) ? "INVGEMR1" : "INVGEMR2", PU_CACHE));
#endif
}

HandlerReturnType EnergyLink_Handler(int key)
{
    int menumax = 0;
    const int* elshop_items = APDOOM_EnergyLink_ShopItemList(&menumax);
    const int last_x = MenuRef->x;

#ifdef AP_INC_HERETIC
    const boolean mousextobutton = false;
#endif

    if (key == key_menu_activate || key == key_menu_back)
    {
#if defined(AP_INC_DOOM)
        M_ClearMenus();
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
#elif defined(AP_INC_HERETIC)
        MN_DeactivateMenu();
        //S_StartSound(NULL, sfx_dorcls);
        return 0;        
#endif
    }
    if (key == key_menu_forward || key == key_use)
    {
        const int item = elshop_items[last_x];
        boolean allowed = APC_CanGiveItem(item);
        if (allowed)
            allowed = APDOOM_EnergyLink_TakeEnergyForItem(APC_EnergyLinkItemCost(item), item);

        if (allowed)
#if defined(AP_INC_DOOM)
            S_StartSoundOptional(NULL, sfx_mnuact, sfx_pistol); // [NS] Optional menu sounds.
#elif defined(AP_INC_HERETIC)
            S_StartSound(NULL, sfx_dorcls);
#endif
        else
#if defined(AP_INC_DOOM)
            S_StartSound(NULL, sfx_noway);
#elif defined(AP_INC_HERETIC)
            S_StartSound(NULL, sfx_artiuse);
#endif
    }
    else if ((key == key_menu_right || key == key_right) && !mousextobutton)
        ++MenuRef->x;
    else if ((key == key_menu_left || key == key_left) && !mousextobutton)
        --MenuRef->x;

    if (MenuRef->x > menumax - 1)
        MenuRef->x = menumax - 1;
    if (MenuRef->x < 0)
        MenuRef->x = 0;

    if (MenuRef->x != last_x)
#if defined(AP_INC_DOOM)
        S_StartSoundOptional(NULL, sfx_mnumov, sfx_pstop);
#elif defined(AP_INC_HERETIC)
        S_StartSound(NULL, sfx_switch);
#endif

#ifdef AP_INC_HERETIC
    return 0;
#endif
}

#undef TextDrawWidth
#undef TextDrawFunc
#undef MenuRef
#undef HandlerReturnType
#undef NumDrawFunc
