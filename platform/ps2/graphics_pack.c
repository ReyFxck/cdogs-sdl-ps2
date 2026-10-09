/* A bounded, sequential PNG stream for the PS2 optical-drive filesystem. */
#include "ps2_graphics_pack.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <stb/SDL_stbimage.h>

#define MAX_PACK_IMAGES 10000u
#define MAX_PACK_IMAGE_BYTES (2u * 1024u * 1024u)

static unsigned int Read16(const unsigned char *p) { return p[0] | (p[1] << 8); }
static unsigned int Read32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
        ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int CDogsPS2LoadGraphicsPack(
    const char *path,
    void (*add)(void *context, const char *name, bool is_hd, SDL_Surface *image),
    void *context)
{
    FILE *pack = fopen(path, "rb");
    if (!pack) return errno == ENOENT ? -1 : -2;
    unsigned char header[12], entry[7];
    unsigned char *pixels = NULL;
    size_t capacity = 0;
    int result = -2;
    if (fread(header, 1, sizeof header, pack) != sizeof header ||
        memcmp(header, "CDGSPNG1", 8) != 0) goto done;
    const unsigned int count = Read32(header + 8);
    if (!count || count > MAX_PACK_IMAGES) goto done;
    for (unsigned int i = 0; i < count; ++i)
    {
        if (fread(entry, 1, sizeof entry, pack) != sizeof entry) goto done;
        const unsigned int name_len = Read16(entry);
        const unsigned int bytes = Read32(entry + 3);
        if (!name_len || name_len > 255 || entry[2] > 1 ||
            !bytes || bytes > MAX_PACK_IMAGE_BYTES) goto done;
        char name[256];
        if (fread(name, 1, name_len, pack) != name_len) goto done;
        name[name_len] = '\0';
        if (name[0] == '/' || strchr(name, '\\') || strstr(name, "../") ||
            strchr(name, '\0') != name + name_len) goto done;
        if (bytes > capacity)
        {
            unsigned char *new_pixels = realloc(pixels, bytes);
            if (!new_pixels) goto done;
            pixels = new_pixels;
            capacity = bytes;
        }
        if (fread(pixels, 1, bytes, pack) != bytes) goto done;
        SDL_Surface *image = STBIMG_LoadFromMemory(pixels, (int)bytes);
        if (!image) goto done;
        add(context, name, entry[2] != 0, image);
        SDL_FreeSurface(image);
    }
    if (fgetc(pack) != EOF || ferror(pack)) goto done;
    result = (int)count;
done:
    free(pixels);
    fclose(pack);
    return result;
}
