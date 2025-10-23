
#include <algorithm>
#include <filesystem>
#include <json/json.h>

#include "apdoom.h"
#include "apzip.h"
#include "embedded_files.h"

#include <iostream>

struct WorldInfo {
	std::string path; // Path to external file containing world data
	const embedded_file_t *embedded; // Embedded file version of the above

	// Required fields
	std::string shortname; // -game arguments: e.g. "doom", "doom2" ...
	std::string fullname; // Displayed in launcher
	std::string apname; // Name of the game in Archipelago, used to connect to slot
	std::string iwad; // Base IWAD this game uses (can infer specific behavior)
	std::string definitions; // Location of game defs inside apworld

	// Optional fields
	std::vector<std::string> included_wads; // PWADs included in apworld
	std::vector<std::string> required_wads; // PWADs reqiured for play
	std::vector<std::string> optional_wads; // PWADs auto-loaded if available, but optional

	std::vector<const char *> c_included_wads; // Pointed to by C structure below, NULL terminated
	std::vector<const char *> c_required_wads; // As above
	std::vector<const char *> c_optional_wads; // As above
	ap_worldinfo_t c_world_info;

	bool operator<(const WorldInfo& other)
	{
		return strcmp(fullname.c_str(), other.fullname.c_str()) < 0;
	}
};

std::vector<WorldInfo> AllGameInfo;
std::vector<ap_worldinfo_t *> CGameInfo;

static void to_cstring_vector(std::vector<const char *> &cvector, std::vector<std::string> &strvector)
{
	cvector.reserve(strvector.size() + 1);
	for (std::string &str : strvector)
		cvector.push_back(str.c_str());
	cvector.push_back(NULL);
}

static void json_to_vector(Json::Value &json, std::vector<std::string> &vec)
{
	for (Json::Value &value : json)
	{
		if (value.isString())
			vec.push_back(value.asString());
	}
	vec.shrink_to_fit();
}

static ap_worldinfo_t *make_c_world(WorldInfo &w)
{
	to_cstring_vector(w.c_included_wads, w.included_wads);
	to_cstring_vector(w.c_required_wads, w.required_wads);
	to_cstring_vector(w.c_optional_wads, w.optional_wads);
	w.c_world_info.shortname = w.shortname.c_str();
	w.c_world_info.fullname = w.fullname.c_str();
	w.c_world_info.apname = w.apname.c_str();
	w.c_world_info.definitions = w.definitions.c_str();
	w.c_world_info.iwad = w.iwad.c_str();
	w.c_world_info.required_wads = w.c_required_wads.data();
	w.c_world_info.optional_wads = w.c_optional_wads.data();
	w.c_world_info.included_wads = w.c_included_wads.data();
	return &w.c_world_info;
}

static WorldInfo *parse_world(APZipReader *world)
{
	static Json::CharReaderBuilder builder;
	Json::Value json;

	// Note that it's entirely possible that NULL is passed into this function.
	// APZipReader unconditionally returns NULL back in that case.
	APZipFile *f = APZipReader_FindFile(world, "archipelago.json");
	if (f)
	{
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		reader->parse(f->data, f->data + f->size, &json, NULL);
	}

	if (!json // Doesn't exist at all
		|| !json.isObject() // Isn't an object type
		|| json.get("compatible_version", 0).asInt() < 7 // Incompatible manifest version
		|| !(json.isMember("game") && json["game"].isString()) // Missing game name
		|| !(json.isMember("__apdoom") && json["__apdoom"].isObject()) // Missing APDoom submanifest
	)
	{
		return NULL;
	}

	Json::Value &apdoom_json = json["__apdoom"];
	if (
		// Definitions, iwad, and short name are required; everything else is optional.
		!(apdoom_json.isMember("definitions") && apdoom_json["definitions"].isString())
		|| !(apdoom_json.isMember("iwad") && apdoom_json["iwad"].isString())
		|| !(apdoom_json.isMember("short_name") && apdoom_json["short_name"].isString())
	)
	{
		return NULL;
	}

	// Check all previously loaded worlds for a matching shortname;
	// if one is found, don't load this one.
	{
		std::string shortname = apdoom_json["short_name"].asString();
		for (WorldInfo &other : AllGameInfo)
		{
			if (other.shortname == shortname)
				return NULL;
		}
	}

	WorldInfo w;
	w.apname = json["game"].asString();
	w.shortname = apdoom_json["short_name"].asString();
	w.iwad = apdoom_json["iwad"].asString();
	w.definitions = apdoom_json["definitions"].asString();

	w.fullname = apdoom_json.get("full_name", w.apname).asString();

	if (apdoom_json.isMember("wads_required") && apdoom_json["wads_required"].isArray())
		json_to_vector(apdoom_json["wads_required"], w.required_wads);
	if (apdoom_json.isMember("wads_optional") && apdoom_json["wads_optional"].isArray())
		json_to_vector(apdoom_json["wads_optional"], w.optional_wads);
	if (apdoom_json.isMember("wads_included") && apdoom_json["wads_included"].isArray())
		json_to_vector(apdoom_json["wads_included"], w.included_wads);

	// Will be set later. For now, initialize them.
	w.embedded = NULL;
	memset(&w.c_world_info, 0, sizeof(ap_worldinfo_t));

	AllGameInfo.push_back(w);
	return &AllGameInfo.back();
}

void populate_worlds(void)
{
	// Populate worlds placed in the games folder of the cwd.
	// This folder is allowed to be missing.
	try {
		const std::filesystem::path cwd_dir("./games");
		for (auto const &entry : std::filesystem::recursive_directory_iterator(cwd_dir))
		{
			if (!entry.is_regular_file() || entry.path().extension() != ".apworld")
				continue;

			std::string path_str = entry.path().string();
			APZipReader *zip = APZipReader_FromFile(path_str.c_str());
			WorldInfo *w = parse_world(zip);
			if (w)
				w->path = path_str;
			APZipReader_Close(zip);
		}			
	}
	catch (std::filesystem::filesystem_error &) {}

	// Populate embedded worlds after.
	// This is so files can override embedded worlds (beta versions, etc.)
	for (int i = 0; i < NUM_EMBEDDED_FILES; ++i)
	{
		const embedded_file_t *embed = &embedded_files[i];
		APZipReader *zip = APZipReader_FromMemory(embed->data, embed->size);
		WorldInfo *w = parse_world(zip);
		if (w)
			w->embedded = embed;
		APZipReader_Close(zip);
	}

	std::sort(AllGameInfo.begin(), AllGameInfo.end());

	// Create the C version of game info.
	CGameInfo.reserve(AllGameInfo.size() + 1);
	for (WorldInfo &w : AllGameInfo)
		CGameInfo.push_back(make_c_world(w));
	CGameInfo.push_back(NULL);
}

extern "C" const ap_worldinfo_t **ap_list_worlds(void)
{
	if (AllGameInfo.empty())
		populate_worlds();
	return (const ap_worldinfo_t **)CGameInfo.data();
}

extern "C" const ap_worldinfo_t *ap_get_world(const char *shortname)
{
	if (AllGameInfo.empty())
		populate_worlds();
	for (WorldInfo &world : AllGameInfo)
	{
		if (world.shortname == shortname)
			return &world.c_world_info;
	}
	return NULL;
}

extern "C" int ap_load_world(const char *shortname)
{
	if (AllGameInfo.empty())
		populate_worlds();

	for (WorldInfo &world : AllGameInfo)
	{
		if (world.shortname == shortname)
		{
			APZipReader *f = NULL;
			if (world.path.empty())
				f = APZipReader_FromMemory(world.embedded->data, world.embedded->size);
			else
				f = APZipReader_FromFile(world.path.c_str());

			if (!f || !APZipReader_Cache(f, ":world:"))
			{
				std::cerr << "Couldn't cache world data for world " << world.shortname << std::endl;
				return false;
			}
			return true;
		}
	}
	std::cerr << "Couldn't find a world that matched " << shortname << std::endl;
	return false;
}
