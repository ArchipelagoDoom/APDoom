
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
	std::string servername; // Name of the game used to connect to the server (the AP name)
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
	w.c_world_info.servername = w.servername.c_str();
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
	APZipFile *f = APZipReader_GetFile(world, "apdoom.json");
	if (f)
	{
		std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
		reader->parse(f->data, f->data + f->size, &json, NULL);
	}

	if (!json // Doesn't exist at all
		|| !json.isObject() // Isn't an object type
		// Or if any of the required fields are missing or non-string
		|| !(json.isMember("_definitions") && json["_definitions"].isString())
		|| !(json.isMember("_iwad") && json["_iwad"].isString())
		|| !(json.isMember("_fullname") && json["_fullname"].isString())
		|| !(json.isMember("_shortname") && json["_shortname"].isString())
		|| !(json.isMember("_servername") && json["_servername"].isString())
	)
	{
		return NULL;
	}

	// Check all previously loaded worlds for a matching shortname;
	// if one is found, don't load this one.
	{
		std::string shortname = json["_shortname"].asString();
		for (WorldInfo &other : AllGameInfo)
		{
			if (other.shortname == shortname)
				return NULL;
		}
	}

	WorldInfo w;
	w.definitions = json["_definitions"].asString();
	w.shortname = json["_shortname"].asString();
	w.fullname = json["_fullname"].asString();
	w.servername = json["_servername"].asString();
	w.iwad = json["_iwad"].asString();

	if (json.isMember("included_files") && json["included_files"].isArray())
		json_to_vector(json["included_files"], w.included_wads);
	if (json.isMember("required_files") && json["required_files"].isArray())
		json_to_vector(json["required_files"], w.required_wads);
	if (json.isMember("optional_files") && json["optional_files"].isArray())
		json_to_vector(json["optional_files"], w.optional_wads);

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

			APZipReader *zip = APZipReader_FromFile(entry.path().c_str());
			WorldInfo *w = parse_world(zip);
			if (w)
				w->path = entry.path().string();
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
