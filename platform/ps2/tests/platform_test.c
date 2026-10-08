#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ps2_platform.h"
#include "window_context.h"
#include "log.h"
#include "sys_config.h"

/* Only logging is stubbed; the compositor, textures, paths and SDL are real. */
FILE *gLogFile;
LogLevel LogModuleGetLevel(LogModule m) { (void)m; return LL_WARN; }
void LogLine(FILE *s, LogModule m, LogLevel l, const char *f, int n,
    const char *fn, const char *fmt, ...)
{
    (void)m; (void)l; (void)f; (void)n; (void)fn;
    if (!s) return;
    va_list args; va_start(args, fmt); vfprintf(s, fmt, args); va_end(args);
    fputc('\n', s);
}
char *PS2Basename(char *path);
char *PS2Dirname(char *path);

static void Paths(void)
{
    const char *input[] = {"host:games/../data/./guns.json", "mass0:/a//b/../../c",
        "mc0:/../../CDOGS/config.json", "cdfs:/cdogs/graphics/", "/a/b/../c"};
    const char *expected[] = {"host:data/guns.json", "mass0:/c", "mc0:/CDOGS/config.json",
        "cdfs:/cdogs/graphics", "/a/c"};
    for (unsigned i = 0; i < sizeof input / sizeof *input; ++i)
    {
        char resolved[CDOGS_PATH_MAX];
        assert(CDogsPS2IsAbsolutePath(input[i]));
        CDogsPS2ResolvePath(input[i], resolved);
        assert(!strcmp(resolved, expected[i]));
    }
    assert(!CDogsPS2IsAbsolutePath("missions/a.cdogscpn"));
    const char *names[] = {"file", "/", "/a///b/", "host:file", "mass:/dir/file", "mass:/", ""};
    const char *bases[] = {"file", "/", "b", "file", "file", ".", "."};
    const char *dirs[] = {".", "/", "/a", "host:", "mass:/dir", "mass:/", "."};
    for (unsigned i = 0; i < sizeof names / sizeof *names; ++i)
    {
        char copy[128]; strcpy(copy, names[i]); assert(!strcmp(PS2Basename(copy), bases[i]));
        strcpy(copy, names[i]); assert(!strcmp(PS2Dirname(copy), dirs[i]));
    }
    unsetenv("CDOGS_DATA_DIR");
    setenv("CDOGS_CONFIG_DIR", "mc0:/save/../CDOGS", 1);
    char *argv[] = {"/not-present/cdogs-sdl.elf"};
    assert(CDogsPS2InitPaths(1, argv)); /* The runner's cwd is the asset checkout. */
    assert(!strcmp(CDogsPS2ConfigPath("config.json"), "mc0:/CDOGS/config.json"));
    char data[CDOGS_PATH_MAX]; CDogsPS2DataPath("data/guns.json", data);
    FILE *f = fopen(data, "rb"); assert(f); fclose(f);
}

static void Pad(void)
{
    int index = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN, 4, 16, 0);
    assert(index >= 0);
    CDogsPS2AddControllerMappings();
    assert(SDL_IsGameController(index));
    SDL_GameController *pad = SDL_GameControllerOpen(index); assert(pad);
    SDL_Joystick *joy = SDL_GameControllerGetJoystick(pad);
    assert(SDL_JoystickSetVirtualButton(joy, 14, 1) == 0);
    assert(SDL_JoystickSetVirtualButton(joy, 4, 1) == 0);
    assert(SDL_JoystickSetVirtualAxis(joy, 0, 12000) == 0);
    SDL_GameControllerUpdate();
    assert(SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A));
    assert(SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP));
    assert(!SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B));
    assert(SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX) == 12000);
    SDL_GameControllerClose(pad);
    assert(SDL_JoystickDetachVirtual(index) == 0);
}

static void Render(void)
{
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software"); /* Host presenter; actual PS2 compositor unchanged. */
    for (int repeat = 0; repeat < 2; ++repeat)
    {
        WindowContext wc;
        Rect2i dim = {{0, 0}, {640, 480}};
        struct vec2i logical = {320, 240};
        assert(WindowContextCreate(&wc, dim, SDL_WINDOW_HIDDEN, "PS2 host test", NULL, logical));
        assert(SDL_RenderTargetSupported(wc.renderer));
        WindowContextPreRender(&wc);
        SDL_SetRenderDrawColor(wc.renderer, 230, 30, 80, 255);
        SDL_Rect rect = {10, 20, 5, 5};
        assert(SDL_RenderFillRect(wc.renderer, &rect) == 0);
        WindowContextPostRender(&wc);
        Uint32 pixel = *(Uint32 *)((Uint8 *)wc.framebuffer->pixels + 20 * wc.framebuffer->pitch + 10 * 4);
        Uint8 r, g, b, a; SDL_GetRGBA(pixel, wc.framebuffer->format, &r, &g, &b, &a);
        assert(r == 230 && g == 30 && b == 80 && a == 255);
        WindowContextDestroy(&wc);
        assert(!wc.window && !wc.renderer && !wc.framebuffer);
    }
}

int main(void)
{
    assert(SDL_Init(SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0);
    Paths(); Pad(); Render(); SDL_Quit();
    puts("PS2 paths, pad mapping and target-texture composition OK (host SDL)");
    return 0;
}
