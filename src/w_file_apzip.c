// A quick and dirty wad file class to handle WAD files in loaded Zip files.

#include <string.h>

#include "config.h"
#include "doomtype.h"

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

#include "apzip.h"

static wad_file_t *W_APZip_OpenFile(const char *path)
{
    wad_file_t *result;

    char shortname[16];
    const char *sep = strchr(path, '/');
    if (!sep || sep - path >= 16 || !*(sep + 1))
        return NULL;

    strncpy(shortname, path, sep - path);
    shortname[sep - path] = '\0';

    APZipReader *zip = APZipReader_FetchFromCache(shortname);
    if (!zip) // No cached zip with that shortname
        return NULL;

    APZipFile *file = APZipReader_GetFile(zip, sep + 1);
    if (!file) // File doesn't exist in zip or couldn't be read
        return NULL;

    result = Z_Malloc(sizeof(wad_file_t), PU_STATIC, 0);
    result->file_class = &apzip_wad_file;
    result->mapped = (byte*)file->data;
    result->length = file->size;
    result->path = M_StringDuplicate(path);
    return result;
}

static void W_APZip_CloseFile(wad_file_t *wad)
{
    // DO NOT close the cached Zip file here!
    Z_Free(wad);
}

size_t W_APZip_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    if (offset >= wad->length)
        return 0;

    size_t real_len = buffer_len;
    if (real_len + offset > wad->length)
        real_len = wad->length - offset;

    memcpy(buffer, wad->mapped + offset, real_len);
    return real_len;
}

wad_file_class_t apzip_wad_file = 
{
    W_APZip_OpenFile,
    W_APZip_CloseFile,
    W_APZip_Read,
};
