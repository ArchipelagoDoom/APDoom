#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "apzip.h"

#ifdef HAVE_LIBZ
#include <zlib.h>
#else
#warning "zlib is not present, compression will not be available"
#endif

static uint16_t ReadShort(APZipReader *zip) {
	uint16_t v = zip->ReadByte(zip);
	v |= zip->ReadByte(zip) << 8;
	return v;
}

static uint32_t ReadLong(APZipReader *zip) {
	uint32_t v = zip->ReadByte(zip);
	v |= zip->ReadByte(zip) << 8;
	v |= zip->ReadByte(zip) << 16;
	v |= zip->ReadByte(zip) << 24;
	return v;
}

static bool CheckHeader(APZipReader *zip, const char *wanted_header)
{
	char buf[4];
	zip->ReadRaw(zip, buf, 4);
	return !memcmp(buf, wanted_header, 4);
}

// ---

#define MAX_CACHED_READERS 8
typedef struct {
	char name[16];
	APZipReader *reader;
} ReaderCache;

ReaderCache cached_readers[MAX_CACHED_READERS];

bool APZipReader_Cache(APZipReader *zip, const char *name)
{
	if (!zip || strlen(name) > 15)
		return false;

	int first_empty = -1;
	for (int i = MAX_CACHED_READERS - 1; i >= 0; --i)
	{
		ReaderCache *cache = &cached_readers[i];
		if (!cache->reader)
			first_empty = i; // Empty slot, keep track
		else if (!strncmp(cache->name, name, 15))
			return false; // Name conflict detected
	}
	if (first_empty >= 0 && first_empty < MAX_CACHED_READERS)
	{
		ReaderCache *cache = &cached_readers[first_empty];
		strncpy(cache->name, name, 15);
		cache->name[15] = '\0';
		cache->reader = zip;
		return true;
	}
	return false; // No empty slots.
}

APZipReader *APZipReader_FetchFromCache(const char *name)
{
	if (strlen(name) > 15)
		return NULL;

	for (int i = 0; i < MAX_CACHED_READERS; ++i)
	{
		ReaderCache *cache = &cached_readers[i];
		if (cache->reader && !strncmp(cache->name, name, 15))
			return cache->reader;
	}
	return NULL;
}

// ---

// Initializes a partial zip reader structure,
// e.g. one made from the APZipReader_From* functions.
static APZipReader *ZipReaderInit(APZipReader *zip)
{
	zip->directory = NULL;

	// 22 is the minimum possible size of the EOCD, so we start there.
	// The comment can be up to 64k bytes in length.
	zip->Seek(zip, -22, SEEK_END);

	long start_pos = zip->Tell(zip);
	long end_pos = (start_pos > 0xFFFF ? start_pos - 0xFFFF : 0);

	bool found_header = false;
	for (long i = start_pos; !found_header && i >= end_pos; --i)
	{
		zip->Seek(zip, i, SEEK_SET);
		found_header = CheckHeader(zip, "PK\x05\x06");
	}
	if (!found_header)
	{
		printf("ZipReaderInit: Unsupported file (not a zip file)\n");
		goto error;
	}

	zip->Seek(zip, 4, SEEK_CUR);
	zip->num_entries = ReadShort(zip);
	if (zip->num_entries != ReadShort(zip))
	{
		printf("ZipReaderInit: Unsupported file (multipart zip file)\n");
		goto error;
	}
	zip->Seek(zip, 4, SEEK_CUR);
	zip->dir_start = ReadLong(zip);

	//printf("Entry count: %d\n", zip->num_entries);
	//printf("Start: 0x%05x\n", zip->dir_start);
	zip->directory = (APZipDirEntry *)malloc(sizeof(APZipDirEntry) * zip->num_entries);
	memset(zip->directory, 0, sizeof(APZipDirEntry) * zip->num_entries);

	zip->Seek(zip, zip->dir_start, SEEK_SET);
	for (int i = 0; i < zip->num_entries; ++i)
	{
		APZipDirEntry *entry = &zip->directory[i];
		if (!CheckHeader(zip, "PK\x01\x02"))
		{
			printf("ZipReaderInit: Unsupported file (not a zip file)\n");
			goto error;
		}

		// Skip version, flags, last modified date, CRC32, compressed/uncompressed size
		// We get the canonical info for most of this from the local headers
		zip->Seek(zip, 24, SEEK_CUR);

		uint16_t filename_len = ReadShort(zip);
		uint16_t extra_len = ReadShort(zip);
		uint16_t comment_len = ReadShort(zip);
		zip->Seek(zip, 8, SEEK_CUR); // Internal and external attributes

		entry->offset = ReadLong(zip);
		entry->name = (char *)malloc(filename_len + 1);
		zip->ReadRaw(zip, entry->name, filename_len);
		entry->name[filename_len] = 0;

		// Seek past extra data and comment
		zip->Seek(zip, extra_len + comment_len, SEEK_CUR);
	}

	return zip;

error:
	APZipReader_Close(zip);
	return NULL;
}

bool APZipReader_FileExists(APZipReader *zip, const char *filename)
{
	if (!zip)
		return false;

	for (int i = 0; i < zip->num_entries; ++i)
	{
		APZipDirEntry *entry = &zip->directory[i];
		if (!strcmp(filename, entry->name))
			return true;
	}
	return false;
}

APZipFile *APZipReader_GetFile(APZipReader *zip, const char *filename)
{
	if (!zip)
		return NULL;

	for (int i = 0; i < zip->num_entries; ++i)
	{
		APZipDirEntry *entry = &zip->directory[i];
		if (strcmp(filename, entry->name))
			continue;

		if (entry->is_cached) // If cached, return previous result.
			return (entry->is_valid) ? &entry->cache : NULL;

		entry->is_cached = true; // We're attempting to cache it now.
		entry->is_valid = false; // But we don't know if it's valid yet.
		entry->cache.data = NULL;

		zip->Seek(zip, entry->offset, SEEK_SET);
		if (!CheckHeader(zip, "PK\x03\x04"))
			return NULL;
		zip->Seek(zip, 2, SEEK_CUR); // Skip version

		uint16_t flags = ReadShort(zip);
		uint16_t compression = ReadShort(zip);

#ifdef HAVE_LIBZ
		// We only allow flags values of 0 or 2, and compression values of 0 or 8.
		if ((flags & ~0x0002) || (compression & ~0x0008))
#else
		// We can't allow any compression
		if (flags || compression)
#endif
		{
			printf("%s: Can not read: unsupported compression\n", entry->name);
			return NULL;
		}

		APZipFile *cache = &entry->cache;
		zip->Seek(zip, 4, SEEK_CUR); // Skip modification date
		cache->checksum = ReadLong(zip);

		uint32_t compressed_size = ReadLong(zip);
		cache->size = ReadLong(zip);

		// Empty file, probably a directory.
		// This is fully valid, so we return non-NULL, but NULL data.
		if (cache->size == 0)
		{
			entry->is_valid = true;
			return cache;
		}

		// Skip filename and extra sections
		uint16_t filename_len = ReadShort(zip);
		uint16_t extra_len = ReadShort(zip);
		zip->Seek(zip, filename_len + extra_len, SEEK_CUR);

#ifdef HAVE_LIBZ
		if (!compression) // Just get the data and go
		{
			cache->data = (char *)malloc(cache->size);
			zip->ReadRaw(zip, cache->data, cache->size);
			entry->is_valid = (crc32(0, (Bytef *)cache->data, cache->size) == cache->checksum);
		}
		else // Deflate compressed, get zlib to inflate it
		{
			cache->data = (char *)malloc(cache->size);
			char *compressed_data = (char *)malloc(compressed_size);
			zip->ReadRaw(zip, compressed_data, compressed_size);

			z_stream stream;
			stream.zalloc = Z_NULL;
			stream.zfree = Z_NULL;
			stream.opaque = Z_NULL;
			stream.next_in = (Bytef *)compressed_data;
			stream.avail_in = compressed_size;
			stream.next_out = (Bytef *)cache->data;
			stream.avail_out = cache->size;

			if (inflateInit2(&stream, -15) == Z_OK
				&& inflate(&stream, Z_FINISH) == Z_STREAM_END
				&& inflateEnd(&stream) == Z_OK)
			{
				entry->is_valid = (crc32(0, (Bytef *)cache->data, cache->size) == cache->checksum);
			}

			free(compressed_data);
		}
#else
		(void)compressed_size;
		cache->data = (char *)malloc(cache->size);
		zip->ReadRaw(zip, cache->data, cache->size);
		// We don't feel like implementing crc32 ourselves, so we just pretend everything's good.
		entry->is_valid = true;
#endif

		// If the result wasn't valid, don't bother keeping any data around from it.
		if (!entry->is_valid && cache->data)
		{
			free(cache->data);
			cache->data = NULL;
		}
		return (entry->is_valid) ? &entry->cache : NULL;
	}
	return NULL;
}

void APZipReader_Close(APZipReader *zip)
{
	if (!zip)
		return;

	for (int i = 0; i < MAX_CACHED_READERS; ++i)
	{
		ReaderCache *cache = &cached_readers[i];
		if (cache->reader == zip)
			cache->reader = NULL;
	}

	if (zip->handle)
		fclose(zip->handle);
	if (zip->directory)
	{
		for (int i = 0; i < zip->num_entries; ++i)
		{
			APZipDirEntry *entry = &zip->directory[i];
			if (entry->name)
				free(entry->name);
			if (entry->is_cached && entry->cache.data)
				free(entry->cache.data);
		}
		free(zip->directory);
	}
	free(zip);
}

// ---

static void _File_ReadRaw(APZipReader *self, char *buf, size_t length)
{
	fread(buf, 1, length, self->handle);
}

static uint8_t _File_ReadByte(APZipReader *self)
{
	uint8_t v;
	fread(&v, 1, 1, self->handle);
	return v;
}

static void _File_Seek(APZipReader *self, size_t offset, int origin)
{
	fseek(self->handle, offset, origin);
}

static size_t _File_Tell(APZipReader *self)
{
	return ftell(self->handle);
}

APZipReader *APZipReader_FromFile(const char *path)
{
	FILE *file = fopen(path, "rb");
	if (!file)
		return NULL;

	APZipReader *zip = malloc(sizeof(APZipReader));
	memset(zip, 0, sizeof(APZipReader));
	zip->ReadRaw = _File_ReadRaw;
	zip->ReadByte = _File_ReadByte;
	zip->Seek = _File_Seek;
	zip->Tell = _File_Tell;
	zip->handle = file;
	return ZipReaderInit(zip);
}

// ---

static void _Memory_ReadRaw(APZipReader *self, char *buf, size_t length)
{
	memset(buf, 0, length);

	const char *copy_p = self->cur_p + length;
	if (copy_p > self->end_p) copy_p = self->end_p;
	memcpy(buf, self->cur_p, copy_p - self->cur_p);
	self->cur_p = copy_p;
}

static uint8_t _Memory_ReadByte(APZipReader *self)
{
	return (self->cur_p >= self->end_p) ? 0 : *(self->cur_p++);
}

static void _Memory_Seek(APZipReader *self, size_t offset, int origin)
{
	switch (origin)
	{
		default:
		case SEEK_CUR: self->cur_p += offset;                break;
		case SEEK_SET: self->cur_p = self->start_p + offset; break;
		case SEEK_END: self->cur_p = self->end_p + offset;   break;
	}
	if (self->cur_p < self->start_p)    self->cur_p = self->start_p;
	else if (self->cur_p > self->end_p) self->cur_p = self->end_p;
}

static size_t _Memory_Tell(APZipReader *self)
{
	return (size_t)(self->cur_p - self->start_p);
}

APZipReader *APZipReader_FromMemory(const char *data, size_t length)
{
	APZipReader *zip = malloc(sizeof(APZipReader));
	memset(zip, 0, sizeof(APZipReader));
	zip->ReadRaw = _Memory_ReadRaw;
	zip->ReadByte = _Memory_ReadByte;
	zip->Seek = _Memory_Seek;
	zip->Tell = _Memory_Tell;
	zip->start_p = zip->cur_p = data;
	zip->end_p = data + length;
	return ZipReaderInit(zip);
}
