/* Decode every packed game PNG via the same parser as the PS2 build. */
#define SDL_MAIN_HANDLED
#define SDL_STBIMAGE_IMPLEMENTATION
#include <stb/SDL_stbimage.h>
#include <SDL.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ps2_graphics_pack.h"

typedef struct { unsigned count; bool logo; } Images;
static void Add(void *context, const char *name, bool is_hd, SDL_Surface *image)
{
    Images *images = context;
    assert(name[0] && image->w > 0 && image->h > 0);
    if (!strcmp(name, "logo")) { images->logo = true; assert(!is_hd); }
    images->count++;
}

int main(int argc, char **argv)
{
    assert(argc == 3 && SDL_Init(0) == 0);
    Images images = {0};
    int count = CDogsPS2LoadGraphicsPack(argv[1], Add, &images);
    assert(count == atoi(argv[2]) && images.count == (unsigned)count && images.logo);
    assert(CDogsPS2LoadGraphicsPack("absent-cdogs-graphics.ps2pack", Add, &images) == -1);
    char path[] = "/tmp/cdogs-graphics-pack-XXXXXX";
    int fd = mkstemp(path); assert(fd >= 0);
    FILE *broken = fdopen(fd, "wb"); assert(broken);
    assert(fwrite("CDGSPNG1\x01\x00\x00\x00", 1, 12, broken) == 12);
    /* A truncated record must fail before the callback is invoked. */
    fclose(broken);
    assert(CDogsPS2LoadGraphicsPack(path, Add, &images) == -2);
    assert(images.count == (unsigned)count);
    unlink(path);
    SDL_Quit();
    printf("Decoded %d packed game PNGs, including logo\n", count);
    return 0;
}
