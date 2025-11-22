// This file is meant to be included in each game's "p_setup.c".
// ============================================================================
// [AP] MapThing Rando
// Monster and Pickup rando functions.
// ============================================================================

#ifndef AP_INC_DOOM
// This helper function only exists for Doom, we need to do something for other games.
static int P_GetNumForMap (int episode, int map, boolean critical)
{
    char lumpname[9];

#ifdef AP_INC_HERETIC
    lumpname[0] = 'E';
    lumpname[1] = '0' + episode;
    lumpname[2] = 'M';
    lumpname[3] = '0' + map;
    lumpname[4] = 0;
#endif

    return critical ? W_GetNumForName(lumpname) : W_CheckNumForName(lumpname);
}
#endif

// ----------------------------------------------------------------------------

// Bounding box testing for monsters, tests for impassible lines and also
// height tests openings. Not doing this results in breaking D2 MAP09 sometimes

extern fixed_t tmbbox[4];
extern fixed_t tmx;
extern fixed_t tmy;

fixed_t tfheight;
fixed_t tfceil;
fixed_t tffloor;

static boolean PIT_TestFit(line_t* ld)
{
    if (tmbbox[BOXRIGHT] <= ld->bbox[BOXLEFT]
     || tmbbox[BOXLEFT] >= ld->bbox[BOXRIGHT]
     || tmbbox[BOXTOP] <= ld->bbox[BOXBOTTOM]
     || tmbbox[BOXBOTTOM] >= ld->bbox[BOXTOP])
        return true;

    if (P_BoxOnLineSide (tmbbox, ld) != -1)
        return true;
        
    // Line hit, this object is touching this line.
    if (!ld->backsector)
        return false; // one sided line
    if (ld->flags & (ML_BLOCKING|ML_BLOCKMONSTERS))
        return false; // blocked by line flags

    // set opentop/openbottom/lowfloor
    P_LineOpening(ld);

    if (tfceil > opentop) // opening ceiling lower than ours
        tfceil = opentop;
    if (tffloor < openbottom) // opening floor higher than ours
        tffloor = openbottom;

    if (tfheight > tfceil - tffloor)
        return false; // can't fit in opening

    // We could potentially check lowfloor too, but I feel it's not worth bothering;
    // most monsters can escape a little overhang and it doesn't cause things to break.
    return true;
}

int P_TestFit(mapthing_t *mt, mobjinfo_t *minfo)
{
    fixed_t x = mt->x << FRACBITS;
    fixed_t y = mt->y << FRACBITS;

    // Trivial rejection if doesn't meet height of sector it's in
    subsector_t *ss = R_PointInSubsector(x, y);

    tffloor = ss->sector->floorheight;
    tfceil = ss->sector->ceilingheight;
    tfheight = minfo->height;
    if (tfheight > tfceil - tffloor)
        return false;

    // Now check lines to see if any intersections create problems.
    tmx = x;
    tmy = y;
    tmbbox[BOXTOP] = y + minfo->radius;
    tmbbox[BOXBOTTOM] = y - minfo->radius;
    tmbbox[BOXRIGHT] = x + minfo->radius;
    tmbbox[BOXLEFT] = x - minfo->radius;
    ++validcount;

    const int xl = (tmbbox[BOXLEFT] - bmaporgx)>>MAPBLOCKSHIFT;
    const int xh = (tmbbox[BOXRIGHT] - bmaporgx)>>MAPBLOCKSHIFT;
    const int yl = (tmbbox[BOXBOTTOM] - bmaporgy)>>MAPBLOCKSHIFT;
    const int yh = (tmbbox[BOXTOP] - bmaporgy)>>MAPBLOCKSHIFT;

    for (int bx = xl; bx <= xh; ++bx)
        for (int by = yl; by <= yh; ++by)
            if (!P_BlockLinesIterator(bx, by, PIT_TestFit))
                return false;
    return true;
}

// ----------------------------------------------------------------------------

typedef struct
{
    int doom_type;
    rando_group_t group;

    int frequency;
    mobjinfo_t *info;

    // ----- Modified at runtime -----

    int _forbidden; // If non-zero, excluded from active rando
} randoitem_t;

typedef struct
{
    // Callback that should return nonzero if an object with that mobjinfo
    // is allowed to be placed at that mapthing. Can be NULL.
    int (*placement_callback)(mapthing_t *mt, mobjinfo_t *info);

    int group_start[NUM_RGROUPS];
    int group_length[NUM_RGROUPS];

    int item_count;
    randoitem_t *items;

    // ----- Modified at runtime -----

    // Total frequency excluding forbidden, used for some rando modes.
    int _freq_per_group[NUM_RGROUPS];
    int _freq_total;

    // Level of randomness for the current run. (see ap_state.random_monsters / random_items)
    int _rando_level;
} randodef_t;

static randodef_t monster_rando = {P_TestFit};
static randodef_t pickup_rando = {NULL};

static void RDef_Init(randodef_t *rdef, ap_itemrando_t *apinfo)
{
    for (int i = 0; i < NUM_RGROUPS; ++i)
        rdef->group_start[i] = rdef->group_length[i] = 0;

    rdef->item_count = 0;
    for (; apinfo[rdef->item_count].group < NUM_RGROUPS; ++rdef->item_count)
    {
        const rando_group_t group = apinfo[rdef->item_count].group;

        if (!rdef->group_length[group])
            rdef->group_start[group] = rdef->item_count;
        rdef->group_length[group] = (rdef->item_count - rdef->group_start[group]) + 1;
    }

    if (!rdef->item_count)
        return;

    rdef->items = calloc(sizeof(randoitem_t), rdef->item_count);
    for (int i = 0; i < rdef->item_count; ++i)
    {
        rdef->items[i].doom_type = apinfo[i].doom_type;
        rdef->items[i].group = apinfo[i].group;
        rdef->items[i].info = NULL;
        rdef->items[i].frequency = 0;
        rdef->items[i]._forbidden = false;

        for (int mobj_i = 0; mobj_i < NUMMOBJTYPES; ++mobj_i)
        {
            if (apinfo[i].doom_type == mobjinfo[mobj_i].doomednum)
            {
                rdef->items[i].info = &mobjinfo[mobj_i];
                break;
            }
        }
        if (!rdef->items[i].info)
            fprintf(stderr, "RDef_Init: Unknown type %i referenced\n", apinfo[i].doom_type);
    }
}

static void RDef_SetFrequencyTotal(randodef_t* rdef)
{
    rdef->_freq_total = 0;
    for (int i = 0; i < NUM_RGROUPS; ++i)
        rdef->_freq_per_group[i] = 0;

    for (int i = 0; i < rdef->item_count; ++i)
    {
        if (!rdef->items[i]._forbidden)
        {
            rdef->_freq_per_group[rdef->items[i].group] += rdef->items[i].frequency;
            rdef->_freq_total += rdef->items[i].frequency;
        }
    }
}

static randoitem_t *RDef_GetItem(randodef_t *rdef, int doom_type)
{
    for (int i = 0; i < rdef->item_count; ++i)
    {
        if (doom_type == rdef->items[i].doom_type)
            return (!rdef->items[i]._forbidden ? &rdef->items[i] : NULL);
    }
    return NULL;
}

static randoitem_t *RDef_ReplaceLikeItem(randodef_t *rdef, randoitem_t *item)
{
    const int rand_max = rdef->_freq_per_group[item->group];
    if (rand_max == 0 || item->_forbidden)
        return item;

    int rand_val = ap_rand() % rand_max;
    for (int i = rdef->group_start[item->group]; i < rdef->item_count; ++i)
    {
        if (rdef->items[i]._forbidden)
            continue;
        rand_val -= rdef->items[i].frequency;
        if (rand_val < 0)
            return &rdef->items[i];
    }

    printf("warning: RDef_ReplaceLikeItem: went out of bounds\n");
    return item;
}

static randoitem_t *RDef_ReplaceAny(randodef_t *rdef)
{
    const int rand_max = rdef->_freq_total;
    if (rand_max == 0)
        return &rdef->items[0];

    int rand_val = ap_rand() % rand_max;
    for (int i = 0; i < rdef->item_count; ++i)
    {
        if (rdef->items[i]._forbidden)
            continue;
        rand_val -= rdef->items[i].frequency;
        if (rand_val < 0)
            return &rdef->items[i];
    }

    printf("warning: RDef_ReplaceAny: went out of bounds\n");
    return &rdef->items[0];
}

// ----------------------------------------------------------------------------

static randodef_t *active_rdef;

//
// [AP]
// P_PrepareMapThingRandos
// Sets up monster and pickup rando for the current game and settings.
//
void P_PrepareMapThingRandos(void)
{
    printf("P_PrepareMapThingRandos: Setting up monster / pickup rando behavior.\n");
    RDef_Init(&monster_rando, ap_game_info.rand_monster_types);
    RDef_Init(&pickup_rando, ap_game_info.rand_pickup_types);

    const int bit = 1 << (MIN(2, MAX(0, ap_state.difficulty - 1)));
    const int max_item_count = MAX(monster_rando.item_count, pickup_rando.item_count);

    // Load all maps, get mapthing frequency
    for (ap_level_index_t *idx = ap_get_available_levels(); idx->ep != -1; ++idx)
    {
        int lump = P_GetNumForMap(ap_index_to_ep(*idx), ap_index_to_map(*idx), false);
        if (lump < 0)
            continue;
        lump += ML_THINGS;

        byte *data = W_CacheLumpNum(lump, PU_STATIC);
        mapthing_t *mt = (mapthing_t *)data;
        int numthings = W_LumpLength(lump) / sizeof(mapthing_t);

        for (int mt_i = 0; mt_i < numthings; ++mt_i, ++mt)
        {
            if ((mt->options & 16) || !(mt->options & bit))
                continue;

            for (int i = 0; i < max_item_count; ++i)
            {
                if (i < monster_rando.item_count && mt->type == monster_rando.items[i].doom_type)
                    ++monster_rando.items[i].frequency;
                else if (i < pickup_rando.item_count && mt->type == pickup_rando.items[i].doom_type)
                    ++pickup_rando.items[i].frequency;
                else
                    continue;
                break;
            }
        }
        W_ReleaseLumpNum(lump);
    }

#ifdef MTRAND_DEBUG
    printf("Monster count:\n");
    for (int i = 0; i < monster_rando.item_count; ++i)
        printf("  %-5i: %i\n", monster_rando.items[i].doom_type, monster_rando.items[i].frequency);
    printf("Pickup count:\n");
    for (int i = 0; i < pickup_rando.item_count; ++i)
        printf("  %-5i: %i\n", pickup_rando.items[i].doom_type, pickup_rando.items[i].frequency);
#endif
}


//
// [AP]
// P_MTRando_Setup
// Starts setting up a MapThing rando with the given options.
//
void P_MTRando_Setup(randodef_t *rdef, int rando_level)
{
    rdef->_rando_level = rando_level;

    // Reset forbidden status. Unforbid all, except bosses.
    for (int i = 0; i < rdef->item_count; ++i)
        rdef->items[i]._forbidden = (rdef->items[i].group == RGROUP_BOSS);

    active_rdef = rdef;
}


//
// [AP]
// P_MTRando_ForbidItem
// Forbids an item that would normally be allowed to be randomized.
// Intended to block boss monsters from being randomized when they're important to map functionality.
//
void P_MTRando_ForbidItem(short doom_type)
{
    if (doom_type <= 0)
        return;
    randoitem_t *item = RDef_GetItem(active_rdef, doom_type);
    if (item)
        item->_forbidden = true;
}


//
// [AP]
// P_MTRando_Run
// Runs a MapThing rando that was previously set up.
// Modifies the entries in out_list to what the new doomednums should be for each mapthing.
//
void P_MTRando_Run(mapthing_t *mts, int numthings, short *out_list)
{
    const int bit = 1 << (MIN(2, MAX(0, gameskill)));

    int *index_list = calloc(numthings, sizeof(int));
    randoitem_t **ritem_list = calloc(numthings, sizeof(randoitem_t *));
    int item_count = 0;

    RDef_SetFrequencyTotal(active_rdef);

#ifdef MTRAND_DEBUG
    if (active_rdef == &monster_rando)
        printf("--------------- Running MapThing Rando. Type: Monster. Level: %i. ---------------\n", active_rdef->_rando_level);
    else if (active_rdef == &pickup_rando)
        printf("--------------- Running MapThing Rando. Type: Pickup.  Level: %i. ---------------\n", active_rdef->_rando_level);
    else
        printf("--------------- Running MapThing Rando. Type: Other.   Level: %i. ---------------\n", active_rdef->_rando_level);
#endif

    // Collect all items that we're going to randomize.
    for (int i = 0; i < numthings; ++i)
    {
        mapthing_t *mt = &mts[i];
        if ((mt->options & 16) || !(mt->options & bit))
            continue; // Item that wouldn't be spawned (multiplayer, or wrong difficulty)

        // If the item exists, then add it to the rando pool.
        randoitem_t *item = RDef_GetItem(active_rdef, mt->type);
        if (item)
        {
            // Except if the vanilla item placement would fail placement tests.
            // If so, don't waste time shuffling. This is usually for trapped, inaccessible monsters
            if (active_rdef->placement_callback && !active_rdef->placement_callback(mt, item->info))
            {
#ifdef MTRAND_DEBUG
                printf("Vanilla mt[%i] fails placement tests! Type %i, location (%i, %i)\n",
                    i,
                    mts[i].type,
                    mts[i].x,
                    mts[i].y);
#endif
                continue;
            }
            ritem_list[item_count] = item;
            index_list[item_count] = i;
            ++item_count;
        }
    }

    if (item_count)
    {
        int shuffle = false;

        switch (active_rdef->_rando_level)
        {
        default: // Unknown / Unsupported
            break;

        case RLEVEL_SHUFFLE:
            // Don't touch items but enable shuffling.
            shuffle = true;
            break;

        case RLEVEL_BALANCED:
            shuffle = true;
            // Fall through

        case RLEVEL_SAMETYPE:
            // Replace items with other items in the same group based on frequency.
            for (int i = 0; i < item_count; ++i)
                ritem_list[i] = RDef_ReplaceLikeItem(active_rdef, ritem_list[i]);
            break;

        case RLEVEL_CHAOTIC:
            shuffle = true;
            for (int i = 0; i < item_count; ++i)
                ritem_list[i] = RDef_ReplaceAny(active_rdef);
            break;            
        }

        // Shuffle which index goes to which item.
        if (shuffle)
            ap_shuffle(index_list, item_count);

        // If this rando has a placement callback, check placements now.
        // If any fail, find something else that fits, with a place we fit into, and swap.
        if (active_rdef->placement_callback)
        {
            for (int i = 0; i < item_count; ++i)
            {
                if (active_rdef->placement_callback(&mts[index_list[i]], ritem_list[i]->info))
                    continue; // Test passed

#ifdef MTRAND_DEBUG
                printf("Problematic placement found. Type %i, location (%i, %i)\n",
                    ritem_list[i]->doom_type,
                    mts[index_list[i]].x,
                    mts[index_list[i]].y);
#endif

                if (shuffle)
                {
                    // Attempt to find another placement that both can accept our item, and has one we can accept.
                    // Then swap indexes with it.
                    int other_i = ap_rand() % item_count;
                    for (int j = 0; j < item_count; ++j)
                    {
                        if (i != other_i  // Don't try a self-swap
                            && ritem_list[i] != ritem_list[other_i]  // Don't try swapping identical monsters
                            && active_rdef->placement_callback(&mts[index_list[i]], ritem_list[other_i]->info)
                            && active_rdef->placement_callback(&mts[index_list[other_i]], ritem_list[i]->info))
                        {
                            int temp = index_list[other_i];
                            index_list[other_i] = index_list[i];
                            index_list[i] = temp;

#ifdef MTRAND_DEBUG
                            printf(" -> Swap candidate found. Type %i, location (%i, %i)\n",
                                ritem_list[other_i]->doom_type,
                                mts[index_list[i]].x,
                                mts[index_list[i]].y);
#endif

                            other_i = -9999;
                            break;
                        }

                        if (++other_i >= item_count)
                            other_i = 0;
                    }
                    if (other_i < 0)
                        continue;
                }

                // Reroll until either success or we give up.
                int tries = 0;
                for (; tries < 64; ++tries)
                {
                    ritem_list[i] = RDef_ReplaceLikeItem(active_rdef, ritem_list[i]);
                    if (active_rdef->placement_callback(&mts[index_list[i]], ritem_list[i]->info))
                    {
#ifdef MTRAND_DEBUG
                        printf(" -> Rerolled to new type %i.\n",
                            ritem_list[i]->doom_type);
#endif
                        break; // Test passed
                    }
                }

#ifdef MTRAND_DEBUG
                if (tries == 64)
                    printf(" -> Failed to resolve.\n");
#endif
            }
        }

        for (int i = 0; i < item_count; ++i)
            out_list[index_list[i]] = ritem_list[i]->doom_type;
    }

#ifdef MTRAND_DEBUG
    printf("--------------- MapThing Rando complete. %5i items randomized. ---------------\n\n", item_count);
#endif


    free(index_list);
    free(ritem_list);
}
