#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	char *data;
	uint32_t size;
	uint32_t checksum;
} APZipFile;

typedef struct {
	char *name;
	uint32_t offset;

	int is_cached;
	int is_valid;
	APZipFile cache;
} APZipDirEntry;

typedef struct _APZipStruct {
	uint16_t num_entries;
	uint32_t dir_start;
	APZipDirEntry *directory;

	void (*ReadRaw)(struct _APZipStruct *self, char *buf, size_t length);
	uint8_t (*ReadByte)(struct _APZipStruct *self);
	void (*Seek)(struct _APZipStruct *self, size_t offset, int origin);
	size_t (*Tell)(struct _APZipStruct *self);

	// Used for file based I/O
	FILE *handle;

	// Used for memory I/O
	const char *start_p;
	const char *cur_p;
	const char *end_p;
} APZipReader;

// Create a new APZipReader from either a file or an area of memory.
// Returns NULL if opening the zip file was not successful.
APZipReader *APZipReader_FromFile(const char *path);
APZipReader *APZipReader_FromMemory(const char *data, size_t length);

// Close an open APZipReader and removes all data associated with it.
// For simplicity's sake, NULL is accepted.
void APZipReader_Close(APZipReader *zip);

// Returns true if the file exists in the zip file, false if it does not.
bool APZipReader_FileExists(APZipReader *zip, const char *name);

// Get and return a file from the zip file, decompressing it if necessary.
// Returns NULL if the file couldn't be read from the zip file.
APZipFile *APZipReader_GetFile(APZipReader *zip, const char *name);

// Caches an APZipReader by a given short name (e.g. "$ASSETS") so that it may be obtained later.
// Returns true if successful.
bool APZipReader_Cache(APZipReader *zip, const char *cache_short_name);

// Retrieves a previously cached APZipReader by its short name.
// Returns NULL if no such APZipReader exists.
APZipReader *APZipReader_FetchFromCache(const char *cache_short_name);

#ifdef __cplusplus
}
#endif
