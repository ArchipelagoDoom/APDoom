#include <vector>
#include <sstream>
#include <ostream>
#include <filesystem>

#include <sys/stat.h>
#include <zlib.h>
#include <onut/Json.h>

class GroupedOutput
{
public:
	bool include_manifest_version = false;

	virtual bool Finalize(void) = 0;

	virtual bool AddSStream(const std::string& group_path, std::stringstream& stream) = 0;
	virtual bool AddFile(const std::string& group_path, const std::string& file_path) = 0;

	bool AddJson(const std::string &group_path, const Json::Value &json, bool fast = true)
	{
		Json::StreamWriterBuilder builder;
		builder["commentStyle"] = "None";
		builder["enableYAMLCompatibility"] = true;
		if (fast)
			builder["indentation"] = "";
		else
			builder["indentation"] = "\t";

		std::stringstream stream;
		builder.newStreamWriter()->write(json, &stream);
		return AddSStream(group_path, stream);
	}

	virtual const std::string& GetOutputPathName(void) = 0;

	virtual ~GroupedOutput(void) {};
};

struct ZipEntry
{
	std::string path;
	std::vector<char> buffer;

	uint16_t compression = 0;
	uint32_t checksum;
	uint32_t uncomp_size;
	uint32_t offset = 0;
	uint16_t is_text = 0;

	uint16_t moddate = 0;
	uint16_t modtime = 0;

private:
	void DoCompress(Bytef *origbuf, std::size_t origsize)
	{
		uncomp_size = origsize;
		checksum = crc32(0, origbuf, origsize);

		z_stream stream;
		stream.zalloc = Z_NULL;
		stream.zfree = Z_NULL;
		stream.opaque = Z_NULL;

		int result;
		result = deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
		if (result != Z_OK)
			goto uncompressed_fallback;

		// Make the compression output buffer big enough to guarantee inflate returns Z_STREAM_END
		buffer.resize(deflateBound(&stream, origsize));
		stream.next_in = origbuf;
		stream.avail_in = origsize;
		stream.next_out = reinterpret_cast<Bytef*>(buffer.data());
		stream.avail_out = buffer.size();
		result = deflate(&stream, Z_FINISH);
		deflateEnd(&stream);
		if (result != Z_STREAM_END)
			goto uncompressed_fallback;

		buffer.resize(stream.total_out);
		compression = 8;
		return;

	uncompressed_fallback:
		printf("Falling back to uncompressed (%s)\n", path.c_str());
		buffer.resize(origsize);
		memcpy(buffer.data(), origbuf, origsize);
		compression = 0;
	}

public:
	void SetDateTime(time_t tt = time(NULL))
	{
		struct tm* datetime = localtime(&tt);
		moddate = 0;
		moddate |= ((datetime->tm_year - 80) << 9);
		moddate |= ((datetime->tm_mon + 1) << 5);
		moddate |= (datetime->tm_mday);
		modtime = 0;
		modtime |= (datetime->tm_hour << 11);
		modtime |= (datetime->tm_min << 5);
		modtime |= (datetime->tm_sec >> 1);
	}

	ZipEntry(const std::string& relative_path, std::vector<char> &buf) : path(relative_path)
	{
		DoCompress(reinterpret_cast<Bytef*>(buf.data()), buf.size());
		SetDateTime();
	}

	ZipEntry(const std::string& relative_path, char *buf, std::size_t size) : path(relative_path)
	{
		DoCompress(reinterpret_cast<Bytef*>(buf), size);
		SetDateTime();
	}
};

class ZipFile : public GroupedOutput
{
	std::string base_path;
	std::vector<ZipEntry> entries;

	const std::string eocd_comment = "apworld created with ap_gen_tool";

	FILE *handle;

	void Write(const char *bytes, int len)
	{
		fwrite(bytes, len, 1, handle);
	}

	void WriteShort(uint16_t i)
	{
		static char buf[2];
		buf[0] = (uint8_t)(i);
		buf[1] = (uint8_t)(i >> 8);
		Write(buf, 2);
	}

	void WriteLong(uint32_t i)
	{
		static char buf[4];
		buf[0] = (uint8_t)(i);
		buf[1] = (uint8_t)(i >> 8);
		buf[2] = (uint8_t)(i >> 16);
		buf[3] = (uint8_t)(i >> 24);
		Write(buf, 4);
	}

	void OutputData(ZipEntry &entry)
	{
		entry.offset = ftell(handle);
		Write("PK\x03\x04", 4); // Local file header
		WriteShort(20); // Version needed for extract (2.0)
		WriteShort(entry.compression ? 0x0002 : 0x0000); // Flags
		WriteShort(entry.compression); // Compression type
		WriteShort(entry.modtime); // MS-DOS format time
		WriteShort(entry.moddate); // MS-DOS format date
		WriteLong(entry.checksum); // CRC32
		WriteLong(entry.buffer.size()); // Compressed size
		WriteLong(entry.uncomp_size); // Uncompressed size
		WriteShort(entry.path.length()); // Filename length
		WriteShort(0); // Extra data field length (we don't use them)
		Write(entry.path.c_str(), entry.path.length()); // Filename
		Write(entry.buffer.data(), entry.buffer.size()); // Actual data
	}

	void OutputCDFH(ZipEntry &entry)
	{
		Write("PK\x01\x02", 4); // CDFH header
		WriteShort(20); // Version made by
		WriteShort(20); // Version needed for extract (2.0)
		WriteShort(entry.compression ? 0x0002 : 0x0000); // Flags
		WriteShort(entry.compression); // Compression type
		WriteShort(entry.modtime); // MS-DOS format time
		WriteShort(entry.moddate); // MS-DOS format date
		WriteLong(entry.checksum); // CRC32
		WriteLong(entry.buffer.size()); // Compressed size
		WriteLong(entry.uncomp_size); // Uncompressed size
		WriteShort(entry.path.length()); // Filename length
		WriteShort(0); // Extra data field length (we don't use them)
		WriteShort(0); // Comment length (no comment)
		WriteShort(0); // Multipart zip stuff (ignored)
		WriteShort(entry.is_text); // Internal attributes
		WriteLong(0); // External attributes (system dependent, who cares?)
		WriteLong(entry.offset); // Location of local file header
		Write(entry.path.c_str(), entry.path.length()); // Filename
	}

	void OutputEOCD(uint32_t cd_offset, uint32_t cd_size)
	{
		Write("PK\x05\x06", 4); // EOCD Header
		WriteShort(0); // Multipart zip stuff (ignored)
		WriteShort(0); // Multipart zip stuff (ignored)
		WriteShort((uint16_t)entries.size()); // Entries on disk
		WriteShort((uint16_t)entries.size()); // Entries total
		WriteLong(cd_size); // Size of central directory
		WriteLong(cd_offset); // Central directory offset
		WriteShort(eocd_comment.length()); // Comment size
		Write(eocd_comment.c_str(), eocd_comment.length()); // Comment
	}

public:
	ZipFile(const std::string& base_path) : base_path(base_path)
	{
		include_manifest_version = true;
	}

	bool Finalize(void) override
	{
		handle = fopen(base_path.c_str(), "wb");
		if (!handle)
			return false;

		for (ZipEntry &entry : entries)
			OutputData(entry);
		uint32_t cd_start = ftell(handle);

		for (ZipEntry &entry : entries)
			OutputCDFH(entry);
		uint32_t cd_end = ftell(handle);

		OutputEOCD(cd_start, cd_end - cd_start);
		fclose(handle);
		return true;
	}

	bool AddSStream(const std::string &group_path, std::stringstream &stream) override
	{
		ZipEntry e(group_path, stream.str().data(), stream.str().length());
		e.is_text = 1;
		entries.push_back(e);
		return true;
	}

	bool AddFile(const std::string &group_path, const std::string &file_path) override
	{
		FILE *f = fopen(file_path.c_str(), "rb");
		if (!f)
			return false;

		std::vector<char> file_data;
		fseek(f, 0, SEEK_END);
		file_data.resize(ftell(f));
		fseek(f, 0, SEEK_SET);
		fread(file_data.data(), file_data.size(), 1, f);
		fclose(f);

		ZipEntry e(group_path, file_data);

		// Get datetime from file
		struct stat filestat;
		if (!stat(file_path.c_str(), &filestat))
			e.SetDateTime(filestat.st_mtime);

		entries.push_back(e);
		return true;
	}

	const std::string& GetOutputPathName(void) override
	{
		return base_path;
	}
};

class OutputToFolder : public GroupedOutput
{
	std::string output_world_name;

	std::filesystem::path base_path;
	std::filesystem::path next_path;

	bool GetNextPath(const std::filesystem::path group_path)
	{
		if (group_path.is_absolute())
			return false;

		next_path = base_path / group_path;
		std::filesystem::path full_dir = next_path;
		full_dir.remove_filename();
		try
		{
			std::filesystem::create_directories(full_dir);
		}
		catch (std::filesystem::filesystem_error &)
		{
			return false;
		}

		return true;
	}

public:
	OutputToFolder(const std::string &path, const std::string &world) : base_path(path)
	{
		base_path.remove_filename().make_preferred();

		if (!std::filesystem::exists(base_path)
			|| !std::filesystem::exists(base_path / "__init__.py")
			|| !std::filesystem::exists(base_path / "AutoWorld.py"))
		{
			throw std::runtime_error("Folder does not appear to be a valid worlds folder.");
		}

		output_world_name = std::string(base_path) + world;
	}

	bool Finalize(void) override { return true; } // No operation.

	bool AddSStream(const std::string &group_path, std::stringstream &stream) override
	{
		if (!GetNextPath(group_path))
			return false;

		std::ofstream file(next_path);
		if (!file.is_open())
			return false;

		file << stream.rdbuf();			
		return true;
	}

	bool AddFile(const std::string &group_path, const std::string &file_path) override
	{
		if (!GetNextPath(group_path))
			return false;

		std::filesystem::path source_path = file_path;
		try
		{
			std::filesystem::copy_file(source_path, next_path, std::filesystem::copy_options::update_existing);
			return true; // If the file is already there, copy_file returns false; that's not an error for us
		}
		catch (std::filesystem::filesystem_error &)
		{
			return false;
		}
	}

	const std::string& GetOutputPathName(void) override
	{
		return output_world_name;
	}
};
