#include "maps.h"

#include <onut/onut.h>
#include <onut/Dialogs.h>
#include <onut/Point.h>

#include "earcut.hpp"

#include <stdio.h>
#include <algorithm>

#include "data.h"
#include "defs.h"


// For earcut to work
namespace mapbox {
namespace util {
template <>
struct nth<0, Point> {
    inline static auto get(const Point &t) {
        return t.x;
    };
};
template <>
struct nth<1, Point> {
    inline static auto get(const Point &t) {
        return t.y;
    };
};
} // namespace util
} // namespace mapbox


#define	NF_SUBSECTOR_VANILLA	0x8000
#define	NF_SUBSECTOR	0x80000000 // [crispy] extended nodes
#define	NO_INDEX	((unsigned short)-1) // [crispy] extended nodes


struct map_header_t
{
    char identification[4];
    int32_t num_lumps;
    int32_t directory_offset;
};


struct map_directory_t
{
    int32_t offset;
    int32_t size;
    char name[8];
};


struct patch_header_t
{
    uint16_t width;
    uint16_t height;
    int16_t leftoffset;
    int16_t topoffset;
};


struct post_t
{
    uint8_t topdelta;
    uint8_t length;
    uint8_t unused;
};


template<typename T>
static bool try_load_lump(const char *lump_name, 
                          FILE *f, 
                          const map_directory_t &dir_entry, 
                          std::vector<T> &elements)
{
    if (strncmp(dir_entry.name, lump_name, 8) == 0)
    {
        auto count = dir_entry.size / sizeof(T);
        elements.resize(count);
        fseek(f, dir_entry.offset, SEEK_SET);
        fread(elements.data(), sizeof(T), count, f);
        return true;
    }
    return false;
}


int FixedMul(int a, int b)
{
    return ((int64_t) a * (int64_t) b) >> 16;
}


int point_on_side(int x, int y, node_t* node)
{
    int	dx;
    int	dy;
    int	left;
    int	right;
	
    if (!node->dx)
    {
        if (x <= node->x)
            return node->dy > 0;
	
        return node->dy < 0;
    }
    if (!node->dy)
    {
        if (y <= node->y)
            return node->dx < 0;
	
        return node->dx > 0;
    }
	
    dx = (x - node->x);
    dy = (y - node->y);
	
    // Try to quickly decide by looking at sign bits.
    if ( (node->dy ^ node->dx ^ dx ^ dy)&0x80000000 )
    {
        if  ( (node->dy ^ dx) & 0x80000000 )
        {
            // (left is negative)
            return 1;
        }
        return 0;
    }

    left = FixedMul(node->dy >> 16, dx);
    right = FixedMul(dy, node->dx >> 16);
	
    if (right < left)
    {
        // front side
        return 0;
    }

    // back side
    return 1;			
}


subsector_t* point_in_subsector(int x, int y, map_t* map)
{
    node_t*	node;
    int side;
    int nodenum;

    // single subsector is a special case
    if (map->nodes.empty())
        return map->subsectors.data();
		
    nodenum = (int)map->nodes.size() - 1;

    while (!(nodenum & NF_SUBSECTOR))
    {
        node = &map->nodes[nodenum];
        side = point_on_side(x, y, node);
        nodenum = node->children[side];
    }
	
    return &map->subsectors[nodenum & ~NF_SUBSECTOR];
}


struct wall_t
{
    int v1, v2;
    int sector;
};


static void create_wall(std::vector<wall_t>& map_walls, map_t* map, int16_t linedef_idx, int16_t sidedef_idx)
{
    const auto &linedef = map->linedefs[linedef_idx];
    const auto &sidedef = map->sidedefs[sidedef_idx];
    const auto &sectordef = map->sectors[sidedef.sector];

    auto &sector = map->sectors[sidedef.sector];

    wall_t wall;
    wall.v1 = linedef.start_vertex;
    wall.v2 = linedef.end_vertex;
    
    // When moving a sector we will need to refer to the sector behind to invalidate it
    if (linedef.front_sidedef != sidedef_idx)
    {
        // Invert direction of wall if we are behind the linedef
        std::swap(wall.v1, wall.v2);
    }

    wall.sector = sidedef.sector;

    sector.walls.push_back((int)map_walls.size());
    map_walls.push_back(wall);
}


static void triangulate_sector(const std::vector<wall_t>& map_walls, map_t* map, int sectori)
{
    auto& sector = map->sectors[sectori];

    std::vector<int> wall_idx_remaining(sector.walls.size());
    for (int i = 0, len = (int)sector.walls.size(); i < len; ++i)
        wall_idx_remaining[i] = i;

    // Build loops
    std::vector<std::vector<int>> loops;
    while (wall_idx_remaining.size() >= 3)
    {
        // Pick first line, and try to build a loop
        std::vector<int> loop = { wall_idx_remaining[0] };
        wall_idx_remaining.erase(wall_idx_remaining.begin());
        while (true)
        {
            auto previous = loop[(int)loop.size() - 1];
            bool found = false;
            for (int i = 0, len = (int)wall_idx_remaining.size(); i < len; ++i)
            {
                auto idx = wall_idx_remaining[i];
                const auto &wall = map_walls[sector.walls[idx]];
                if (map_walls[sector.walls[previous]].v1 == wall.v2)
                {
                    loop.push_back(idx);
                    wall_idx_remaining.erase(wall_idx_remaining.begin() + i);
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }

        if (loop.size() >= 3)
        {
            loops.push_back(loop);
        }
    }

    // Calculate biggest loop, and put it in front. This will be our outside loop. This is what earcut expects.
    double biggest_area = 0;
    int biggest_loop = 0;
    for (int i = 0, len = (int)loops.size(); i < len; ++i)
    {
        const auto &loop = loops[i];
        auto first_wall = loop[0];
        const auto &vertex = map->vertexes[map_walls[sector.walls[first_wall]].v1];
        double mins[2] = { (double)vertex.x, (double)vertex.y };
        double maxs[2] = { (double)vertex.x, (double)vertex.y };
        for (auto wall_idx : loop)
        {
            const auto &vert = map->vertexes[map_walls[sector.walls[wall_idx]].v1];
            mins[0] = std::min(mins[0], (double)vert.x);
            mins[1] = std::min(mins[1], (double)vert.y);
            maxs[0] = std::max(maxs[0], (double)vert.x);
            maxs[1] = std::max(maxs[1], (double)vert.y);
        }
        auto area = (maxs[0] - mins[0]) * (maxs[1] - mins[1]);
        if (area > biggest_area)
        {
            biggest_area = area;
            biggest_loop = i;
        }
    }
    if (biggest_loop != 0)
        std::swap(loops[0], loops[biggest_loop]);

    // Construct points that earcut will understand
    std::vector<std::vector<Point>> point_loops(loops.size());
    for (int j = 0, len = (int)loops.size(); j < len; ++j)
    {
        const auto &loop = loops[j];
        auto &point_loop = point_loops[j];
        point_loop.resize(loop.size());
        for (int i = 0, len = (int)loop.size(); i < len; ++i)
        {
            const auto &vertex = map->vertexes[map_walls[sector.walls[loop[i]]].v1];
            auto &point = point_loop[i];
            point.x = (int)vertex.x;
            point.y = (int)vertex.y;
        }
    }

    // Triangulate
    auto indices = mapbox::earcut<int>(point_loops);

    // Flatten loop so we can know which indice match which loop
    std::vector<int> flatten_loops;
    for (int j = 0, len = (int)loops.size(); j < len; ++j)
    {
        const auto &loop = loops[j];
        flatten_loops.insert(flatten_loops.end(), loop.begin(), loop.end());
    }

    // Translate this into indices to vertexes from mapdef
    sector.vertices.resize(indices.size());
    for (int i = 0, len = (int)indices.size(); i < len; ++i)
    {
        sector.vertices[i] = map_walls[sector.walls[flatten_loops[indices[i]]]].v1;
    }
}


// Used by below two functions if file not present in PWAD
static std::string fallback_iwad_name;

std::vector<uint8_t> load_lump(const std::vector<map_directory_t>& directory, const char* lump_name, FILE* f)
{
    for (const auto& dir_entry : directory)
    {
        if (strncmp(dir_entry.name, lump_name, 8) == 0)
        {
            std::vector<uint8_t> ret;
            ret.resize(dir_entry.size);
            fseek(f, dir_entry.offset, SEEK_SET);
            fread(ret.data(), 1, dir_entry.size, f);
            return ret;
        }
    }

    // Presume it's a PWAD and load the IWAD
    printf("Falling back to IWAD (%s) for %s\n", fallback_iwad_name.c_str(), lump_name);
    FILE* iwad_f = fopen(fallback_iwad_name.c_str(), "rb");
    if (!iwad_f)
    {
        onut::showMessageBox("Error", std::string("Cannot open file: ") + fallback_iwad_name);
        exit(1); // Hard kill
    }
    
    // Read header
    map_header_t iwad_header;
    fread(&iwad_header, sizeof(iwad_header), 1, iwad_f);
    
    // Read directory
    std::vector<map_directory_t> iwad_directory(iwad_header.num_lumps);
    fseek(iwad_f, iwad_header.directory_offset, SEEK_SET);
    fread(iwad_directory.data(), sizeof(map_directory_t), iwad_header.num_lumps, iwad_f);

    for (const auto& dir_entry : iwad_directory)
    {
        if (strncmp(dir_entry.name, lump_name, 8) == 0)
        {
            std::vector<uint8_t> ret;
            ret.resize(dir_entry.size);
            fseek(iwad_f, dir_entry.offset, SEEK_SET);
            fread(ret.data(), 1, dir_entry.size, iwad_f);
            return ret;
        }
    }
    return {};
}


OTextureRef load_sprite(const std::vector<map_directory_t>& directory, const char* lump_name, FILE* f, const uint8_t* pal)
{
    auto raw_data = load_lump(directory, lump_name, f);
    if (raw_data.empty()) return nullptr;

    patch_header_t header;
    memcpy(&header, raw_data.data(), sizeof(patch_header_t));
    uint32_t* columnofs = new uint32_t[header.width * sizeof(uint32_t)];
    memcpy(columnofs, raw_data.data() + sizeof(patch_header_t), header.width * sizeof(uint32_t));

    std::vector<uint8_t> img_data;
    img_data.resize(header.width * header.height * 4);

    for (int x = 0; x < header.width; ++x)
    {
        int offset = columnofs[x];
        while (raw_data[offset] != 0xFF)
        {
            post_t post;
            memcpy(&post, &raw_data[offset], sizeof(post_t));
            offset += 3;
            for (int j = 0; j < post.length; ++j, ++offset)
            {
                int y = post.topdelta + j;
                int idx = raw_data[offset] * 3;
                int k = y * header.width * 4 + x * 4;
                img_data[k + 0] = pal[idx + 0];
                img_data[k + 1] = pal[idx + 1];
                img_data[k + 2] = pal[idx + 2];
                img_data[k + 3] = 255;
            }
            ++offset;
        }
    }

    delete[] columnofs;
    return OTexture::createFromData(img_data.data(), {header.width, header.height}, false);
}


Color get_color_for_line_type(int special)
{
    switch (special)
    {
        case LT_DR_DOOR_OPEN_WAIT_CLOSE_ALSO_MONSTERS:
        case LT_DR_DOOR_OPEN_WAIT_CLOSE_FAST:
        case LT_SR_DOOR_OPEN_WAIT_CLOSE:
        case LT_SR_DOOR_OPEN_WAIT_CLOSE_FAST:
        case LT_S1_DOOR_OPEN_WAIT_CLOSE:
        case LT_S1_DOOR_OPEN_WAIT_CLOSE_FAST:
        case LT_SR_DOOR_OPEN_STAY:
        case LT_SR_DOOR_OPEN_STAY_FAST:
        case LT_S1_DOOR_OPEN_STAY:
        case LT_S1_DOOR_OPEN_STAY_FAST:
        case LT_SR_DOOR_CLOSE_STAY:
        case LT_SR_DOOR_CLOSE_STAY_FAST:
        case LT_S1_DOOR_CLOSE_STAY:
        case LT_S1_DOOR_CLOSE_STAY_FAST:
        case LT_GR_DOOR_OPEN_STAY:
        case LT_D1_DOOR_OPEN_STAY:
        case LT_D1_DOOR_OPEN_STAY_FAST:
        case LT_DR_DOOR_BLUE_OPEN_WAIT_CLOSE:
        case LT_DR_DOOR_RED_OPEN_WAIT_CLOSE:
        case LT_DR_DOOR_YELLOW_OPEN_WAIT_CLOSE:
        case LT_D1_DOOR_BLUE_OPEN_STAY:
        case LT_D1_DOOR_RED_OPEN_STAY:
        case LT_D1_DOOR_YELLOW_OPEN_STAY:
        case LT_SR_DOOR_BLUE_OPEN_STAY_FAST:
        case LT_SR_DOOR_RED_OPEN_STAY_FAST:
        case LT_SR_DOOR_YELLOW_OPEN_STAY_FAST:
        case LT_S1_DOOR_BLUE_OPEN_STAY_FAST:
        case LT_S1_DOOR_RED_OPEN_STAY_FAST:
        case LT_S1_DOOR_YELLOW_OPEN_STAY_FAST:
            return Color(1, 0, 1);

        case LT_WR_DOOR_OPEN_WAIT_CLOSE:
        case LT_WR_DOOR_OPEN_WAIT_CLOSE_FAST:
        case LT_W1_DOOR_OPEN_WAIT_CLOSE_ALSO_MONSTERS:
        case LT_W1_DOOR_OPEN_WAIT_CLOSE_FAST:
        case LT_WR_DOOR_OPEN_STAY:
        case LT_WR_DOOR_OPEN_STAY_FAST:
        case LT_W1_DOOR_OPEN_STAY:
        case LT_W1_DOOR_OPEN_STAY_FAST:
        case LT_WR_DOOR_CLOSE_STAY:
        case LT_WR_DOOR_CLOSE_STAY_FAST:
        case LT_W1_DOOR_CLOSE_STAY:
        case LT_W1_DOOR_CLOSE_FAST:
        case LT_WR_DOOR_CLOSE_STAY_OPEN:
        case LT_W1_DOOR_CLOSE_WAIT_OPEN:
            return Color(1, 0.5f, 1);

        case LT_SR_FLOOR_LOWER_TO_LOWEST_FLOOR:
        case LT_S1_FLOOR_LOWER_TO_LOWEST_FLOOR:
        case LT_SR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR:
        case LT_S1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR:
        case LT_S1_FLOOR_RAISE_BY_512:
        case LT_SR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_FAST:
        case LT_S1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_FAST:
        case LT_SR_FLOOR_RAISE_TO_LOWEST_CEILING:
        case LT_S1_FLOOR_RAISE_TO_LOWEST_CEILING:
        case LT_SR_FLOOR_LOWER_TO_8_ABOVE_HIGHEST_FLOOR:
        case LT_S1_FLOOR_LOWER_TO_8_ABOVE_HIGHEST_FLOOR:
        case LT_G1_FLOOR_RAISE_TO_LOWEST_CEILING:
        case LT_SR_FLOOR_RAISE_TO_8_BELLOW_LOWEST_CEILING_CRUSHES:
        case LT_S1_FLOOR_RAISE_TO_8_BELLOW_LOWEST_CEILING_CRUSHES:
        case LT_SR_FLOOR_LOWER_TO_HIGHEST_FLOOR:
        case LT_S1_FLOOR_LOWER_TO_HIGHEST_FLOOR:
        case LT_SR_CEILING_LOWER_TO_FLOOR:
        case LT_S1_CEILING_LOWER_TO_FLOOR:

        case LT_SR_FLOOR_RAISE_BY_24_CHANGES_TEXTURE:
        case LT_S1_FLOOR_RAISE_BY_24_CHANGES_TEXTURE:
        case LT_SR_FLOOR_RAISE_BY_32_CHANGES_TEXTURE:
        case LT_S1_FLOOR_RAISE_BY_32_CHANGES_TEXTURE:
        case LT_SR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_CHANGES_TEXTURE:
        case LT_S1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_CHANGES_TEXTURE:
        case LT_SR_LIFT_LOWER_WAIT_RAISE_FAST:
        case LT_S1_LIFT_LOWER_WAIT_RAISE_FAST:
        case LT_G1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_CHANGES_TEXTURE:
        case LT_SR_LIFT_LOWER_WAIT_RAISE:
        case LT_S1_LIFT_LOWER_WAIT_RAISE:
            return Color(0, 1, 1);

        case LT_WR_FLOOR_RAISE_TO_8_BELLOW_LOWEST_CEILING_CRUSHES:
        case LT_W1_FLOOR_RAISE_TO_8_BELLOW_LOWEST_CEILING_CRUSHES:
        case LT_WR_FLOOR_LOWER_TO_HIGHEST_FLOOR:
        case LT_W1_FLOOR_LOWER_TO_HIGHEST_FLOOR:
        case LT_WR_FLOOR_LOWER_TO_8_ABOVE_HIGHEST_FLOOR:
        case LT_W1_FLOOR_LOWER_BY_8_ABOVE_HIGHEST_FLOOR:
        case LT_WR_FLOOR_RAISE_BY_24:
        case LT_W1_FLOOR_RAISE_BY_24:
        case LT_WR_FLOOR_RAISE_BY_24_CHANGES_TEXTURE:
        case LT_W1_FLOOR_RAISE_BY_24_CHANGES_TEXTURE:
        case LT_WR_FLOOR_RAISE_BY_SHORTEST_LOWER_TEXTURE:
        case LT_W1_FLOOR_RAISE_BY_SHORTEST_LOWER_TEXTURE:
        case LT_WR_FLOOR_LOWER_TO_LOWEST_FLOOR:
        case LT_W1_FLOOR_LOWER_TO_LOWEST_FLOOR:
        case LT_WR_FLOOR_LOWER_TO_LOWEST_FLOOR_CHANGES_TEXTURE:
        case LT_W1_FLOOR_LOWER_BY_LOWEST_FLOOR_CHANGES_TEXTURE:
        case LT_WR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR:
        case LT_W1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR:
        case LT_WR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_FAST:
        case LT_W1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_FAST:
        case LT_WR_FLOOR_RAISE_TO_LOWEST_CEILING:
        case LT_W1_FLOOR_RAISE_TO_LOWEST_CEILING:
        case LT_W1_CEILING_RAISE_TO_HIGHEST_CEILING:
        case LT_WR_CEILING_LOWER_TO_8_ABOVE_FLOOR:
        case LT_W1_CEILING_LOWER_TO_8_ABOVE_FLOOR:

        case LT_WR_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_CHANGES_TEXTURE:
        case LT_W1_FLOOR_RAISE_TO_NEXT_HIGHER_FLOOR_CHANGES_TEXTURE:
        case LT_WR_FLOOR_START_MOVING_UP_AND_DOWN:
        case LT_W1_FLOOR_START_MOVING_UP_AND_DOWN:
        case LT_WR_FLOOR_STOP_MOVING:
        case LT_W1_FLOOR_STOP_MOVING:
        case LT_WR_LIFT_LOWER_WAIT_RAISE_ALSO_MONSTERS:
        case LT_W1_LIFT_LOWER_WAIT_RAISE:
        case LT_WR_LIFT_LOWER_WAIT_RAISE_FAST:
        case LT_W1_LIFT_LOWER_WAIT_RAISE_FAST:
            return Color(0.5f, 1, 1);

        // Crusher types
        case LT_S1_CEILING_LOWER_TO_8_ABOVE_FLOOR_PERPETUAL_SLOW_CHRUSHER_DAMAGE:
        case LT_WR_CRUSHER_START_WITH_SLOW_DAMAGE:
        case LT_W1_CRUSHER_START_WITH_SLOW_DAMAGE:
        case LT_WR_CRUSHER_START_WITH_FAST_DAMAGE:
        case LT_W1_CRUSHER_START_WITH_FAST_DAMAGE:
        case LT_W1_CRUSHER_START_WITH_SLOW_DAMAGE_SILENT:
        case LT_WR_CRUSHER_STOP:
        case LT_W1_CRUSHER_STOP:
            return Color(1, 0, 0);

        // Stairs types
        case LT_S1_STAIRS_RAISE_BY_8:
        case LT_S1_STAIRS_RAISE_BY_16_FAST:
        case LT_W1_STAIRS_RAISE_BY_8:
        case LT_W1_STAIRS_RAISE_BY_16_FAST:
            return Color(0, 0, 1);

        // Teleporter types
        case LT_WR_TELEPORT_ALSO_MONSTERS:
        case LT_W1_TELEPORT_ALSO_MONSTERS:
        case LT_WR_TELEPORT_MONSTERS_ONLY:
        case LT_W1_TELEPORT_MONSTERS_ONLY:
            return Color(1, 1, 0.5f);
    }

    return Color(1, 1, 1);
}


void init_wad(const char* filename, game_t& game)
{
    fallback_iwad_name = game.iwad_name;

    // Load DOOM.WAD
    FILE* f = fopen(filename, "rb");
    if (!f)
    {
        onut::showMessageBox("Error", std::string("Cannot open file: ") + filename);
        exit(1); // Hard kill
    }
    
    // Read header
    map_header_t header;
    fread(&header, sizeof(header), 1, f);
    if (strncmp(header.identification, "PWAD", 4) != 0 && strncmp(header.identification, "IWAD", 4) != 0)
    {
        onut::showMessageBox("Error", std::string("Invalid IWAD or PWAD: ") + filename);
        exit(1); // Hard kill
    }
    
    // Read directory
    std::vector<map_directory_t> directory(header.num_lumps);
    fseek(f, header.directory_offset, SEEK_SET);
    fread(directory.data(), sizeof(map_directory_t), header.num_lumps, f);

#if 0
    bool is_doom2 = (game.codename == "doom2" || game.codename == "tnt" || game.codename == "plutonia");
#endif

    // loop directory and find levels, then load them all. YOLO
    for (int i = 0, len = (int)directory.size(); i < len; ++i)
    {
        const auto &dir_entry = directory[i];
        {
            map_t* map = nullptr;

#if 0
            if (is_doom2)
            {
                // DOOM 2 style
                if (strlen(dir_entry.name) == 5 && strncmp(dir_entry.name, "MAP", 3) == 0)
                {
                    auto lvl = std::atoi(dir_entry.name + 3) - 1;
                    if (lvl >= 0)
                    {
                        for (auto& episode : game.episodes)
                        {
                            if (lvl < (int)episode.size())
                            {
                                map = &episode[lvl].map;
                                break;
                            }
                            lvl -= (int)episode.size();
                        }
                    }
                }
            }
            else
            {
                // DOOM 1 style
                if (strlen(dir_entry.name) == 4 && dir_entry.name[0] == 'E' && dir_entry.name[2] == 'M')
                {
                    // That's a level!
                    auto ep = dir_entry.name[1] - '1';
                    auto lvl = dir_entry.name[3] - '1';

                    if (ep >= 0 && ep < game.ep_count)
                    {
                        if (lvl >= 0 && lvl < (int)game.episodes[ep].size())
                        {
                            map = &game.episodes[ep][lvl].map;
                        }
                    }
                }
            }
#else
            // Possible future support for EnMnn, MAPnnM, MAPnnN, ?
            if (!(strlen(dir_entry.name) >= 5 && strncmp(dir_entry.name, "MAP", 3) == 0) &&
                !(strlen(dir_entry.name) >= 4 && dir_entry.name[0] == 'E' && dir_entry.name[2] == 'M'))
            {
                continue;
            }
            for (auto& episode : game.episodes)
            {
                for (auto& level : episode)
                {
                    if (!level.lump_name.compare(dir_entry.name))
                    {
                        map = &level.map;
                        break;
                    }
                }

                if (map)
                    break;
            }
#endif

            if (!map) continue;

            ++i;
            for (; i < len; ++i)
            {
                const auto &dir_entry = directory[i];
                try_load_lump("THINGS", f, dir_entry, map->things);
                try_load_lump("LINEDEFS", f, dir_entry, map->linedefs);
                try_load_lump("SIDEDEFS", f, dir_entry, map->sidedefs);
                try_load_lump("VERTEXES", f, dir_entry, map->vertexes);
                try_load_lump("SECTORS", f, dir_entry, map->map_sectors);
                try_load_lump("SSECTORS", f, dir_entry, map->map_subsectors);
                try_load_lump("NODES", f, dir_entry, map->map_nodes);
                try_load_lump("SEGS", f, dir_entry, map->map_segs);
                if (strncmp(dir_entry.name, "BLOCKMAP", 8) == 0)
                {
                    break;
                }
            }

            // Adjust things based on map tweaks. Only things matter enough to do this for
            const Json::Value &tweaks = game.json_map_tweaks.get(dir_entry.name, {});
            const auto& tweak_thing_ids = tweaks.get("things", {}).getMemberNames();
            for (const auto& tweak_id : tweak_thing_ids)
            {
                map_thing_t &mt = map->things[std::stoi(tweak_id)];
                const Json::Value tweak = tweaks["things"][tweak_id];

                mt.x = tweak.get("x", mt.x).asInt();
                mt.y = tweak.get("y", mt.y).asInt();
                mt.type = tweak.get("type", mt.type).asInt();
                mt.direction = tweak.get("angle", mt.direction).asInt();
                mt.flags = tweak.get("flags", mt.flags).asInt();
            }

            map->sectors.resize(map->map_sectors.size());
            map->subsectors.resize(map->map_subsectors.size());
            map->nodes.resize(map->map_nodes.size());
            for (int j = 0, lenj = (int)map->map_nodes.size(); j < lenj; ++j)
            {
                map->nodes[j].x = (int16_t)map->map_nodes[j].x << 16;
                map->nodes[j].y = (int16_t)map->map_nodes[j].y << 16;
                map->nodes[j].dx = (int16_t)map->map_nodes[j].dx << 16;
                map->nodes[j].dy = (int16_t)map->map_nodes[j].dy << 16;
                for (int jj = 0; jj < 2; ++jj)
                {
                    map->nodes[j].children[jj] = (uint16_t)(int16_t)map->map_nodes[j].children[jj];
                    if (map->nodes[j].children[jj] == NO_INDEX)
                        map->nodes[j].children[jj] = -1;
                    else if (map->nodes[j].children[jj] & NF_SUBSECTOR_VANILLA)
                    {
                        map->nodes[j].children[jj] &= ~NF_SUBSECTOR_VANILLA;
                        if (map->nodes[j].children[jj] >= (int)map->map_subsectors.size())
                            map->nodes[j].children[jj] = 0;
                        map->nodes[j].children[jj] |= NF_SUBSECTOR;
                    }
                    for (int k = 0; k < 4; ++k)
                        map->nodes[j].bbox[jj][k] = (int16_t)map->map_nodes[j].bbox[jj][k] << 16;
                }
            }

            map->segs.resize(map->map_segs.size());
            for (int j = 0, lenj = (int)map->map_segs.size(); j < lenj; ++j)
            {
                const auto& map_seg = map->map_segs[j];
                auto& seg = map->segs[j];
                int side = map_seg.side;
                seg.sidedef = (&(map->linedefs[map_seg.linedef].front_sidedef))[side];
                seg.front_sector = map->sidedefs[seg.sidedef].sector;
            }

            // Assign sector to subsector
            for (int j = 0, lenj = (int)map->map_subsectors.size(); j < lenj; ++j)
            {
                const auto& seg = map->segs[map->map_subsectors[j].firstseg];
                //const auto& map_sidedef = map->sidedefs[seg.sidedef];
                map->subsectors[j].sector = seg.front_sector;
            }

            map->bb[0] = map->vertexes[0].x;
            map->bb[1] = map->vertexes[0].y;
            map->bb[2] = map->vertexes[0].x;
            map->bb[3] = map->vertexes[0].y;
            for (int v = 1, vlen = (int)map->vertexes.size(); v < vlen; ++v)
            {
                map->bb[0] = std::min(map->bb[0], map->vertexes[v].x);
                map->bb[1] = std::min(map->bb[1], map->vertexes[v].y);
                map->bb[2] = std::max(map->bb[2], map->vertexes[v].x);
                map->bb[3] = std::max(map->bb[3], map->vertexes[v].y);
            }

            // Create "walls" used in triangulation step
            std::vector<wall_t> map_walls;
            for (int j = 0; j < (int)map->linedefs.size(); ++j)
            {
                const auto &linedef = map->linedefs[j];

                if (linedef.front_sidedef != -1)
                    create_wall(map_walls, map, j, linedef.front_sidedef);
                if (linedef.back_sidedef != -1)
                    create_wall(map_walls, map, j, linedef.back_sidedef);
            }

            // Triangulate
            for (int j = 0; j < (int)map->sectors.size(); ++j)
            {
                triangulate_sector(map_walls, map, j);
            }

            // Create arrows
            for (int j = 0; j < (int)map->linedefs.size(); ++j)
            {
                const auto& line_def = map->linedefs[j];
                if (line_def.special_type != 0 && line_def.sector_tag != 0)
                {
                    arrow_t arrow;
                    arrow.color = get_color_for_line_type(line_def.special_type);
                    const auto& v1 = map->vertexes[line_def.start_vertex];
                    const auto& v2 = map->vertexes[line_def.end_vertex];
                    arrow.from = {
                        (float)(v1.x + v2.x) * 0.5f,
                        -(float)(v1.y + v2.y) * 0.5f
                    };
                    for (int k = 0; k < (int)map->map_sectors.size(); ++k)
                    {
                        const auto& map_sector = map->map_sectors[k];
                        if (map_sector.tag == line_def.sector_tag)
                        {
                            Vector2 bbmin, bbmax;
                            const auto& sector = map->sectors[k];
                            if (sector.vertices.empty()) continue;
                            bbmin = {
                                (float)map->vertexes[sector.vertices[0]].x,
                                -(float)map->vertexes[sector.vertices[0]].y
                            };
                            bbmax = bbmin;
                            for (int l = 1; l < (int)sector.vertices.size(); ++l)
                            {
                                Vector2 pt = {
                                    (float)map->vertexes[sector.vertices[l]].x,
                                    -(float)map->vertexes[sector.vertices[l]].y
                                };
                                bbmin = onut::min(bbmin, pt);
                                bbmax = onut::max(bbmax, pt);
                            }
                            arrow.to = (bbmin + bbmax) * 0.5f;
                            map->arrows.push_back(arrow);
                        }
                    }
                }
            }

            // Count checks
            map->check_count = 0;
            for (int j = 0, len = (int)map->things.size(); j < len; ++j)
            {
                const auto& thing = map->things[j];

                // Count total thing count (Consider UV difficulty)
                if (thing.flags & 0x0004)
                    game.total_doom_types[thing.type]++;

                if (thing.flags & 0x0010) continue; // Thing is not in single player
                auto it = game.location_doom_types.find(thing.type);
                if (it == game.location_doom_types.end()) continue;
                map->check_count++;
            }
        }
    }

    // Load palette
    auto pal = load_lump(directory, "PLAYPAL", f);

    // Load sprites for item requirements
    for (auto& item_requirement : game.item_requirements)
    {
        if (item_requirement.sprite != "")
        {
            item_requirement.icon = load_sprite(directory, item_requirement.sprite.c_str(), f, pal.data());
        }
    }

    // close file
    fclose(f);
}


void init_maps(game_t& game)
{
    init_wad(game.wad_name.c_str(), game);
}


int sector_at(int x, int y, map_t* map)
{
    x = (int)((int16_t)x << 16);
    y = (int)((int16_t)y << 16);

    auto subsector = point_in_subsector(x, y, map);
    return subsector->sector;
}
