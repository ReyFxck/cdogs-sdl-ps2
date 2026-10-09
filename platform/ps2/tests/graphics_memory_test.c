/* Load the real game art with real SDL under a finite heap budget.
 * This is a native allocation/lifetime regression, not a PS2 emulator.
 */
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "font.h"
#include "grafx.h"
#include "log.h"
#include "pic_manager.h"
#include "ps2_platform.h"
#include "utils.h"

#define BUDGET (24u * 1024u * 1024u)
#define MAGIC UINT64_C(0x43444f47534d454d)
typedef union
{
    long double alignment;
    struct { size_t size; uint64_t magic; } allocation;
} Header;
static size_t live, peak, rejected;
static unsigned images, errors;

void *__real_malloc(size_t);
void *__real_calloc(size_t, size_t);
void *__real_realloc(void *, size_t);
void __real_free(void *);

void *__wrap_malloc(size_t size)
{
    if (size > BUDGET - live)
    {
        ++rejected; errno = ENOMEM; return NULL;
    }
    Header *h = __real_malloc(sizeof *h + size);
    if (!h) return NULL;
    h->allocation.size = size; h->allocation.magic = MAGIC;
    live += size; if (live > peak) peak = live;
    return h + 1;
}
void __wrap_free(void *p)
{
    if (!p) return;
    Header *h = (Header *)p - 1;
    assert(h->allocation.magic == MAGIC);
    live -= h->allocation.size; h->allocation.magic = 0;
    __real_free(h);
}
void *__wrap_calloc(size_t n, size_t size)
{
    if (size && n > SIZE_MAX / size) return NULL;
    void *p = __wrap_malloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}
void *__wrap_realloc(void *p, size_t size)
{
    if (!p) return __wrap_malloc(size);
    if (!size) { __wrap_free(p); return NULL; }
    Header *h = (Header *)p - 1;
    assert(h->allocation.magic == MAGIC);
    const size_t old = h->allocation.size;
    if (size > BUDGET - live + old)
    {
        ++rejected; errno = ENOMEM; return NULL;
    }
    h = __real_realloc(h, sizeof *h + size);
    if (!h) return NULL;
    h->allocation.size = size;
    live = live - old + size; if (live > peak) peak = live;
    return h + 1;
}

FILE *gLogFile;
GraphicsDevice gGraphicsDevice;
extern map_t textureDebugger;
LogLevel LogModuleGetLevel(LogModule m) { (void)m; return LL_ERROR; }
void LogLine(FILE *s, LogModule m, LogLevel l, const char *file, int line,
    const char *fn, const char *fmt, ...)
{
    (void)m; (void)l; (void)file; (void)line; (void)fn;
    if (!s) return;
    ++errors;
    va_list args; va_start(args, fmt); vfprintf(s, fmt, args); va_end(args);
    fputc('\n', s);
}
SDL_Surface *__real_LoadImgToSurface(const char *);
SDL_Surface *__wrap_LoadImgToSurface(const char *path)
{
    SDL_Surface *s = __real_LoadImgToSurface(path);
    assert(s != NULL);
    ++images;
    return s;
}

static unsigned picCount;
static size_t pixelBytes;
static void CheckPic(Pic *p)
{
    assert(!PicIsNone(p) && p->Tex != NULL);
    int w, h; Uint32 format;
    assert(SDL_QueryTexture(p->Tex, &format, NULL, &w, &h) == 0);
    assert(w == p->size.x && h == p->size.y && !p->isHD);
    pixelBytes += (size_t)w * h * sizeof *p->Data;
    ++picCount;
}
static int CheckNamedPic(any_t data, any_t item)
{
    (void)data; CheckPic(&((NamedPic *)item)->pic); return MAP_OK;
}
static int CheckSprites(any_t data, any_t item)
{
    (void)data;
    NamedSprites *ns = item;
    for (size_t i = 0; i < ns->pics.size; ++i) CheckPic(CArrayGet(&ns->pics, i));
    return MAP_OK;
}

int main(int argc, char **argv)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    assert(argc == 3); /* asset root, PNG count supplied by the test runner */
    assert(SDL_SetMemoryFunctions(__wrap_malloc, __wrap_calloc,
        __wrap_realloc, __wrap_free) == 0);
    assert(SDL_Init(SDL_INIT_VIDEO) == 0);
    setenv("CDOGS_DATA_DIR", argv[1], 1);
    char *boot[] = {"host:cdogs-sdl.elf"};
    assert(CDogsPS2InitPaths(1, boot));
    SDL_Surface *target = SDL_CreateRGBSurfaceWithFormat(0, 320, 240, 32,
        SDL_PIXELFORMAT_ARGB8888);
    assert(target);
    gGraphicsDevice.gameWindow.renderer = SDL_CreateSoftwareRenderer(target);
    gGraphicsDevice.Format = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    assert(gGraphicsDevice.gameWindow.renderer && gGraphicsDevice.Format);
    const size_t baseline = live;
    size_t steady = 0;
    for (int round = 0; round < 3; ++round)
    {
        images = picCount = errors = 0; pixelBytes = 0;
        memset(&gFont, 0, sizeof gFont);
        gFont.Size = svec2i(7, 8); gFont.Stride = 32;
        FontLoad(&gFont, "graphics/font.png", true, svec2i(4, 5));
        assert(gFont.Chars.size == 256);
        PicManagerInit(&gPicManager);
        PicManagerLoad(&gPicManager);
        assert(images == (unsigned)atoi(argv[2]) + 1 && errors == 0);
        assert(hashmap_iterate(gPicManager.pics, CheckNamedPic, NULL) == MAP_OK);
        assert(hashmap_iterate(gPicManager.sprites, CheckSprites, NULL) == MAP_OK);
        assert(picCount > 3000);
        if (!round) steady = live;
        else assert(live == steady); /* No accumulation across reloads. */
        printf("Art round %d: %u PNGs, %u sprites, pixels=%zu, live=%zu, peak=%zu / %u bytes\n",
            round + 1, images, picCount, pixelBytes, live, peak, BUDGET);
        FontTerminate(&gFont);
        PicManagerTerminate(&gPicManager);
        printf("After cleanup: live=%zu, baseline=%zu\n", live, baseline);
        /* Pic's disabled texture debugger is allocated once on first load. */
        assert(live - baseline < 16384);
    }
    assert(rejected == 0);
    SDL_DestroyRenderer(gGraphicsDevice.gameWindow.renderer);
    SDL_FreeFormat(gGraphicsDevice.Format); SDL_FreeSurface(target);
    SDL_Quit();
    SDL_TLSCleanup();
    hashmap_free(textureDebugger);
    printf("After SDL shutdown: live=%zu\n", live);
    assert(live == 0);
    puts("Real graphics fit the 24 MiB allocation budget; load/unload lifetime OK");
    return 0;
}
