#pragma once

#include <cinttypes>
#include <vector>
#include <onut/Color.h>
#include <onut/Vector2.h>


struct map_thing_t
{
    int16_t x;
    int16_t y;
    int16_t direction;
    int16_t type;
    int16_t flags;
};


struct map_linedefs_t
{
    int16_t start_vertex;
    int16_t end_vertex;
    int16_t flags;
    int16_t special_type;
    int16_t sector_tag;
    int16_t front_sidedef;
    int16_t back_sidedef;
};


struct map_sidedefs_t
{
    int16_t x_offset;
    int16_t y_offset;
    char    upper_texture[8];
    char    lower_texture[8];
    char    middle_texture[8];
    int16_t sector;
};


struct map_vertex_t
{
    int16_t x;
    int16_t y;
};


struct map_sectors_t
{
    int16_t floor_height;
    int16_t ceiling_height;
    char    floor_texture[8];
    char    ceiling_texture[8];
    int16_t light_level;
    int16_t type;
    int16_t tag;
};


struct map_subsector_t
{
    uint16_t numsegs;
    uint16_t firstseg;
};


struct map_node_t
{
    // Partition line from (x,y) to x+dx,y+dy)
    int16_t x;
    int16_t y;
    int16_t dx;
    int16_t dy;

    // Bounding box for each child,
    // clip against view frustum.
    int16_t bbox[2][4];

    // If NF_SUBSECTOR its a subsector,
    // else it's a node of another subtree.
    uint16_t children[2];
};


struct map_seg_t
{
    uint16_t v1;
    uint16_t v2;
    int16_t angle;
    uint16_t linedef;
    int16_t side;
    int16_t offset;
};


struct seg_t
{
    int sidedef;
    int front_sector;
};


struct sector_t
{
    std::vector<int> vertices;
    std::vector<int> walls;
};


struct node_t
{
    int x;
    int y;
    int dx;
    int dy;
    int bbox[2][4];
    int children[2];
};


struct subsector_t
{
    int sector;
};


struct arrow_t
{
    Vector2 from;
    Vector2 to;
    Color color;
};


struct map_t
{
    std::vector<map_thing_t>        things;
    std::vector<map_linedefs_t>     linedefs;
    std::vector<map_sidedefs_t>     sidedefs;
    std::vector<map_vertex_t>       vertexes;
    std::vector<map_sectors_t>      map_sectors;
    std::vector<map_subsector_t>    map_subsectors;
    std::vector<map_node_t>         map_nodes;
    std::vector<map_seg_t>          map_segs;
    std::vector<seg_t>              segs;
    std::vector<subsector_t>        subsectors;
    std::vector<node_t>             nodes;
    std::vector<sector_t>           sectors;
    int16_t bb[4];
    std::vector<arrow_t>            arrows;
    int check_count;
};


struct game_t;

bool init_maps(game_t& game);
int sector_at(int x, int y, map_t* map);
subsector_t* point_in_subsector(int x, int y, map_t* map);
