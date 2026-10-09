#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ps2_platform.h"
#include "window_context.h"
#include "log.h"
#include "sys_config.h"
#include <tinydir/tinydir.h>

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

static bool androidCwd;
char *__real_getcwd(char *buf, size_t size);
char *__wrap_getcwd(char *buf, size_t size)
{
    if (!androidCwd) return __real_getcwd(buf, size);
    const char *uri = "host:/content:/com.android.externalstorage.documents/tree/primary%3APs2";
    if (strlen(uri) + 1 > size) return NULL;
    return strcpy(buf, uri);
}

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
    const char *uris[] = {
        "content://com.android.externalstorage.documents/tree/primary%3APs2",
        "host:content://com.android.externalstorage.documents/document/primary%3APs2%2Fcdogs-sdl.elf",
        "host:/content:/com.android.externalstorage.documents/document/primary%3APs2",
        "host0:content://provider/tree/id"};
    for (unsigned i = 0; i < sizeof uris / sizeof *uris; ++i)
    {
        char resolved[CDOGS_PATH_MAX];
        CDogsPS2ResolvePath(uris[i], resolved);
        assert(!*resolved); /* Never send a mangled Android URI to hostfs. */
    }
    androidCwd = true;
    char resolved[CDOGS_PATH_MAX];
    CDogsPS2ResolvePath("data/guns.json", resolved);
    assert(!strcmp(resolved, "host:data/guns.json"));
    androidCwd = false;
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

static void BootRoots(void)
{
    char original[CDOGS_PATH_MAX]; assert(getcwd(original, sizeof original));
    char temporary[] = "/tmp/cdogs-ps2-boot-XXXXXX";
    assert(mkdtemp(temporary)); assert(chdir(temporary) == 0);
    const char *directories[] = {"cdfs:", "cdfs:/data", "cdfs:/graphics", "host:data", "host:graphics"};
    for (unsigned i = 0; i < sizeof directories / sizeof *directories; ++i)
        assert(mkdir(directories[i], 0700) == 0);
    const char *files[] = {"cdfs:/data/guns.json", "cdfs:/graphics/font.png",
        "host:data/guns.json", "host:graphics/font.png"};
    for (unsigned i = 0; i < sizeof files / sizeof *files; ++i)
    {
        FILE *f = fopen(files[i], "wb"); assert(f); fputc('x', f); fclose(f);
    }
    unsetenv("CDOGS_DATA_DIR"); unsetenv("CDOGS_CONFIG_DIR");
    char *disc[] = {"cdrom0:\\CDOGS.ELF;1"};
    assert(CDogsPS2InitPaths(1, disc));
    assert(!strcmp(CDogsPS2DataRoot(), "cdfs:/"));
    assert(!strcmp(CDogsPS2ConfigPath("config.json"), "mc0:/CDOGS/config.json"));
    char data[CDOGS_PATH_MAX]; CDogsPS2DataPath("data/guns.json", data);
    assert(!strcmp(data, "cdfs:/data/guns.json"));
    char *saf[] = {"host:content://com.android.externalstorage.documents/document/primary%3APs2%2Fcdogs-sdl.elf"};
    androidCwd = true;
    assert(CDogsPS2InitPaths(1, saf));
    assert(!strcmp(CDogsPS2DataRoot(), "host:"));
    androidCwd = false;
    for (unsigned i = 0; i < sizeof files / sizeof *files; ++i) assert(unlink(files[i]) == 0);
    for (unsigned i = sizeof directories / sizeof *directories; i > 0; --i) assert(rmdir(directories[i - 1]) == 0);
    assert(chdir(original) == 0); assert(rmdir(temporary) == 0);
}

static void DiscDirectoryTypes(void)
{
    tinydir_dir dir = {0};
    struct dirent entry = {0};
    strcpy(dir.path, "cdfs:/graphics");
    strcpy(entry.d_name, "sprites");
    dir._e = &entry;
    tinydir_file file;
    entry.d_type = DT_DIR;
    assert(tinydir_readfile(&dir, &file) == 0 && file.is_dir && !file.is_reg);
    entry.d_type = DT_REG;
    strcpy(entry.d_name, "table_wood_round_terminal_wreck.png");
    assert(tinydir_readfile(&dir, &file) == 0 && file.is_reg && !file.is_dir);
    assert(!strcmp(file.path, "cdfs:/graphics/table_wood_round_terminal_wreck.png"));
    assert(!strcmp(file.extension, "png"));
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
    Paths(); BootRoots(); DiscDirectoryTypes(); Pad(); Render(); SDL_Quit();
    puts("PS2 paths, pad mapping and target-texture composition OK (host SDL)");
    return 0;
}
