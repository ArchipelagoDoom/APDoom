// This file is meant to be included in each game's "p_setup.c".
// ============================================================================
// [AP] Map Tweaks
// Allow making any change that wouldn't require a nodesbuild.
// ============================================================================

static void P_TweakSector(mapsector_t *sector, ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_SECTOR_SPECIAL:     sector->special = tweak->value;               break;
        case TWEAK_SECTOR_TAG:         sector->tag = tweak->value;                   break;
        case TWEAK_SECTOR_FLOOR:       sector->floorheight = tweak->value;           break;
        case TWEAK_SECTOR_FLOOR_PIC:   memcpy(sector->floorpic, tweak->string, 8);   break;
        case TWEAK_SECTOR_CEILING:     sector->ceilingheight = tweak->value;         break;
        case TWEAK_SECTOR_CEILING_PIC: memcpy(sector->ceilingpic, tweak->string, 8); break;
        default: break;
    }
    printf("P_TweakSector: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}

static void P_TweakMapThing(mapthing_t *mapthing, ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_MAPTHING_X:     mapthing->x = tweak->value;       break;
        case TWEAK_MAPTHING_Y:     mapthing->y = tweak->value;       break;
        case TWEAK_MAPTHING_TYPE:  mapthing->type = tweak->value;    break;
        case TWEAK_MAPTHING_ANGLE: mapthing->angle = tweak->value;   break;
        case TWEAK_MAPTHING_FLAGS: mapthing->options = tweak->value; break;

        case TWEAK_MAPTHING_VOODOO_NOITEMS:  mapthing->options |= ( APMTF_VOODOO_NOITEMS * (!!tweak->value)); break;
        case TWEAK_MAPTHING_VOODOO_NODAMAGE: mapthing->options |= (APMTF_VOODOO_NODAMAGE * (!!tweak->value)); break;
        case TWEAK_MAPTHING_FLYING_ONLY:     mapthing->options |= (    APMTF_FLYING_ONLY * (!!tweak->value)); break;
        case TWEAK_MAPTHING_DONT_RANDOMIZE:  mapthing->options |= ( APMTF_DONT_RANDOMIZE * (!!tweak->value)); break;
        default: break;
    }
    printf("P_TweakMapThing: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}

static void P_TweakHub(mapthing_t *hub, ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_HUB_X:     hub->x = tweak->value;     break;
        case TWEAK_HUB_Y:     hub->y = tweak->value;     break;
        case TWEAK_HUB_ANGLE: hub->angle = tweak->value; break;
        default: break;
    }
    printf("P_TweakHub: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}

static void P_TweakLinedef(maplinedef_t *linedef, ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_LINEDEF_SPECIAL: linedef->special = tweak->value; break;
        case TWEAK_LINEDEF_TAG:     linedef->tag = tweak->value;     break;
        case TWEAK_LINEDEF_FLAGS:   linedef->flags = tweak->value;   break;
        default: break;
    }
    printf("P_TweakLinedef: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}

static void P_TweakSidedef(mapsidedef_t *sidedef, ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_SIDEDEF_LOWER:  memcpy(sidedef->bottomtexture, tweak->string, 8); break;
        case TWEAK_SIDEDEF_MIDDLE: memcpy(sidedef->midtexture, tweak->string, 8);    break;
        case TWEAK_SIDEDEF_UPPER:  memcpy(sidedef->toptexture, tweak->string, 8);    break;
        case TWEAK_SIDEDEF_X:      sidedef->textureoffset = tweak->value;            break;
        case TWEAK_SIDEDEF_Y:      sidedef->rowoffset = tweak->value;                break;
        default: break;
    }
    printf("P_TweakSidedef: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}

static void P_TweakMeta(ap_maptweak_t *tweak)
{
    if (ap_force_disable_behaviors) return;
    switch (tweak->type)
    {
        case TWEAK_META_BEHAVES_AS:
            // Let any arbitrary map have normally hardcoded hacks applied to it
            if (strncmp(tweak->string, "MAP", 3) == 0)
            {
                apmeta.gameepisode = 1;
                apmeta.gamemap = atoi(&tweak->string[3]);
            }
            else if (tweak->string[0] == 'E'
                && tweak->string[1] >= '1' && tweak->string[1] <= '9'
                && tweak->string[2] == 'M')
            {
                apmeta.gameepisode = (tweak->string[1] - '0');
                apmeta.gamemap = atoi(&tweak->string[3]);
            }
            else if (strncmp(tweak->string, "NORMAL", 6) == 0)
            { // Ignore normally present hacks
                apmeta.gameepisode = 1;
                apmeta.gamemap = 1;
            }
            break;

        case TWEAK_META_SECRET_EXIT:
            // Allow secret exit to behave as a normal exit for this map
            apmeta.secretexit = !!tweak->value;
            break;

        default:
            break;
    }
    printf("P_TweakMeta: [%i] %02x: %i / %s\n", tweak->target, tweak->type, tweak->value, tweak->string);
}
