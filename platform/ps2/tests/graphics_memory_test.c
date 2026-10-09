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

#define BUDGET (16u * 1024u * 1024u)
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
    void *pixels; int pitch;
    assert(p->DataFromTexture);
    assert(SDL_LockTexture(p->Tex, NULL, &pixels, &pitch) == 0);
    assert(pixels == p->Data && pitch == w * (int)sizeof *p->Data);
    SDL_UnlockTexture(p->Tex);
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

static void TextureLifetime(void)
{
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 2, 2, 32, SDL_PIXELFORMAT_ARGB8888);
    assert(s);
    Uint32 original[] = {0xffff0000, 0xff00ff00, 0xff0000ff, 0xffffffff};
    memcpy(s->pixels, original, sizeof original);
    Pic p;
    PicLoad(&p, svec2i(2, 2), svec2i_zero(), s, false);
    assert(p.DataFromTexture && !memcmp(p.Data, original, sizeof original));
    Pic copy = PicCopy(&p);
    assert(!copy.DataFromTexture && !copy.Tex && copy.Data != p.Data);
    copy.Data[0] = 0xff888888;
    assert(PicTryMakeTex(&copy) && copy.DataFromTexture);
    assert(p.Data[0] == original[0] && copy.Data[0] == 0xff888888);
    for (int i = 0; i < 10; i++)
    {
        assert(PicTryMakeTex(&copy));
        assert(copy.Data[0] == 0xff888888);
    }
    // Queue rendering before resize: destruction must flush using live pixels.
    SDL_Rect dst = {0, 0, 2, 2};
    assert(SDL_RenderCopy(gGraphicsDevice.gameWindow.renderer, p.Tex, NULL, &dst) == 0);
    PicShrink(&p, svec2i(1, 1), svec2i(1, 1));
    assert(p.DataFromTexture && p.Data[0] == original[3]);
    Uint32 rendered[4];
    SDL_Rect area = {0, 0, 2, 2};
    assert(SDL_RenderReadPixels(gGraphicsDevice.gameWindow.renderer, &area,
        SDL_PIXELFORMAT_ARGB8888, rendered, 8) == 0);
    assert(!memcmp(rendered, original, sizeof original));
    PicFree(&copy); PicFree(&p); SDL_FreeSurface(s);
    assert(!copy.Tex && !p.Tex && !copy.Data && !p.Data);
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
    TextureLifetime();
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
    puts("Real graphics fit the 16 MiB allocation budget; shared-pixel lifetime OK");
    return 0;
}
