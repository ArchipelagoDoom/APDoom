// This file is meant to be included in each game's "m_menu.c" or equivalent.
// ============================================================================
// [AP] Goal display "menu"
// ============================================================================

#if defined(AP_INC_DOOM)
#define TextDrawFunc(x, y, tx)  M_WriteText(x, y, tx)
#define TextWidthFunc(tx)       M_StringWidth(tx)
#define MenuRef                 currentMenu
#define HandlerReturnType       void
#elif defined(AP_INC_HERETIC)
#define TextDrawFunc(x, y, tx)  MN_DrTextA(tx, x, y - 2)
#define TextWidthFunc(tx)       MN_TextAWidth(tx)
#define MenuRef                 CurrentMenu
#define HandlerReturnType       int
#endif

static HandlerReturnType ShowGoals_Handler(int key);
static void ShowGoals_Drawer(void);

enum
{
    effect_none,
    effect_center,
    effect_spacer_ng,
    effect_spacer_ok,
    effect_clearcount
};

static int sgm_levelcount = 0;
static int sgm_clearcount = 0;
static int sgm_goalcount = 0;

#if defined(AP_INC_DOOM)

static menuitem_t MIShowGoal[] =
{
    {5,"",ShowGoals_Handler,'\0'}
};

static menu_t ShowGoalDef =
{
    1,
    NULL,
    MIShowGoal,
    ShowGoals_Drawer,
    -9, // used as current draw position
    -666, // inhibits default drawing
    0,
    0
};

// Only Doom needs this function.
static void M_ShowGoals(int choice)
{
	M_SetupNextMenu(&ShowGoalDef);
}

#elif defined(AP_INC_HERETIC)

static MenuItem_t MIShowGoal[] =
{
    {ITT_HIJACK, "", ShowGoals_Handler, 0, MENU_NONE}
};

static Menu_t ShowGoalsMenu = {
    -9, // used as current draw position
    -666, // inhibits default drawing
	ShowGoals_Drawer,
	1,
	MIShowGoal,
	0,
	MENU_NONE
};

#endif

// ----------------------------------------------------------------------------

static void ShowGoals_Init(void)
{
    sgm_levelcount = 0;
    sgm_clearcount = 0;

    if (ap_state.goal >= 2)
    {
        sgm_levelcount = ap_state.goal_level_count;
        for (int i = 0; i < sgm_levelcount; ++i)
        {
            ap_level_state_t *state = ap_get_level_state(ap_state.goal_level_list[i]);
            sgm_clearcount += (state->completed ? 1 : 0);
        }
    }
    else
    {
        for (ap_level_index_t *idx = ap_get_available_levels(); idx->ep != -1; ++idx)
        {
            ap_level_state_t *state = ap_get_level_state(*idx);
            sgm_clearcount += (state->completed ? 1 : 0);

            ++sgm_levelcount;
        }
    }
    sgm_goalcount = (!ap_state.goal) ? sgm_levelcount : ap_state.goal_level_count;
}

void ShowGoals_Drawer(void)
{
    static char buf[80];

    int h = MenuRef->x;

    if (ap_game_info.goal_menu_flat)
    { // Draw background
		pixel_t *dest = I_VideoBuffer;
    	const byte *src = W_CacheLumpName(ap_game_info.goal_menu_flat, PU_CACHE);

		V_FillFlat(0, SCREENHEIGHT, 0, SCREENWIDTH, src, dest);

#ifdef AP_INC_DOOM
		inhelpscreens = true;
#endif
    }

    for (int i = 0; i < 20; ++i, ++h)
    {
        int effect = (h < 0 ? effect_center : effect_none);
        buf[0] = 0;

        if (h == -3)
            M_snprintf(buf, sizeof(buf), "%sTo win, you must complete", crstr[CR_RED]);
        else if (h == -2 && ap_state.goal >= 2)
            M_snprintf(buf, sizeof(buf), "%sthe following levels.", crstr[CR_RED]);
        else if (h == -2 && ap_state.goal == 1)
            M_snprintf(buf, sizeof(buf), "%s%i%s different levels.", crstr[CR_GRAY], sgm_goalcount, crstr[CR_RED]);
        else if (h == -2)
            M_snprintf(buf, sizeof(buf), "%sall levels.", crstr[CR_RED]);
        else if (h >= 0 && h < sgm_levelcount)
        {
            ap_level_index_t *idx = (ap_state.goal >= 2)
                ? &ap_state.goal_level_list[h]
                : &(ap_get_available_levels()[h]);
            ap_level_info_t *info = ap_get_level_info(*idx);

            // Work backwards until we reach the last open parenthesis (the lump name)
            // Based off of the HU code to parse level names.
            const char *src = info->name + strlen(info->name);
            while (src > info->name && *src != '(') --src;

            // If we didn't find one, oh well, dump the entire map name into title text
            if (src == info->name)
                M_snprintf(buf, sizeof(buf), "%s%s", crstr[CR_GOLD], src);
            else
            {
                // Turn "Name (LUMP01)" into "LUMP01: Name"
                const char *paren_ptr = src++;

                M_StringConcat(buf, crstr[CR_GOLD], sizeof(buf));
                M_StringConcat(buf, src, sizeof(buf));
                buf[strlen(buf)-1] = ':';
                M_StringConcat(buf, crstr[CR_GRAY], sizeof(buf));
                M_StringConcat(buf, " ", sizeof(buf));

                src = info->name;
                char *dst = buf + strlen(buf);
                while (dst-buf < 79 && *src && src < paren_ptr)
                    *(dst++) = *(src++);
                *dst = 0;
            }

            ap_level_state_t *state = ap_get_level_state(*idx);
            effect = (state->completed) ? effect_spacer_ok : effect_spacer_ng;
        }
        else if (h == sgm_levelcount + 1)
        {
            M_snprintf(buf, sizeof(buf), "%sLevels completed:", crstr[CR_GOLD]);
            effect = effect_clearcount;
        }
        else
            continue;

        const int x = (effect == effect_center ? (ORIGWIDTH/2 - TextWidthFunc(buf) / 2) : 20);
        const int y = 2 + (i * 10);

        TextDrawFunc(x, y, buf);

        switch (effect)
        {
        default:
            break;
        case effect_spacer_ng:
        case effect_spacer_ok:
            dp_translation = cr[CR_GRAY];

            int x_stop = x + TextWidthFunc(buf);
            for (int dot_x = 280; dot_x > x_stop; dot_x -= 10)
                TextDrawFunc(dot_x, y, ".");

            dp_translation = NULL;
            V_DrawPatch(290, y - 1, W_CacheLumpName((effect == effect_spacer_ok) ? "CHECKMRK" : "REDX", PU_CACHE));
            break;
        case effect_clearcount:
            M_snprintf(buf, sizeof(buf), "%s%i / %i",
                (ap_state.victory ? crstr[CR_GREEN] : crstr[CR_RED]), sgm_clearcount, sgm_goalcount);
            TextDrawFunc((ORIGWIDTH-x) - TextWidthFunc(buf), y, buf);
            break;
        }
        dp_translation = NULL;
    }

#if defined(AP_INC_DOOM)
    // Use the font to draw up and down arrows
    dp_translation = cr[whichSkull ? CR_RED : CR_DARK];
    if (MenuRef->x > -9)
        TextDrawFunc(8,     8, "^");
    if (MenuRef->x < sgm_levelcount - 12)
        TextDrawFunc(8, 192-8, "v");
    dp_translation = NULL;
#elif defined(AP_INC_HERETIC)
    // Use patches instead, the font doesn't work for this.
    if (MenuRef->x > -9)
        V_DrawPatch(4,     8, W_CacheLumpName((MenuTime&8) ? "GEMUP1" : "GEMUP2", PU_CACHE));
    if (MenuRef->x < sgm_levelcount - 12)
        V_DrawPatch(4, 192-8, W_CacheLumpName((MenuTime&8) ? "GEMDN1" : "GEMDN2", PU_CACHE));
#endif
}

HandlerReturnType ShowGoals_Handler(int key)
{
    const int last_x = MenuRef->x;

    if (key == key_menu_activate || key == key_menu_forward || key == key_menu_back)
    {
#if defined(AP_INC_DOOM)
        M_ClearMenus();
        S_StartSoundOptional(NULL, sfx_mnucls, sfx_swtchx);
#elif defined(AP_INC_HERETIC)
        MN_DeactivateMenu();
        S_StartSound(NULL, sfx_dorcls);
        return 0;        
#endif
    }
    else if (key == key_menu_down || key == key_down || key == key_alt_down)
        ++MenuRef->x;
    else if (key == key_menu_up || key == key_up || key == key_alt_up)
        --MenuRef->x;

    if (MenuRef->x > sgm_levelcount - 12)
        MenuRef->x = sgm_levelcount - 12;
    if (MenuRef->x < -9)
        MenuRef->x = -9;

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
