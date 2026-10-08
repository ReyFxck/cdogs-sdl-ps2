#include "ps2_platform.h"
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include "sys_config.h"

static char dataRoot[CDOGS_PATH_MAX];
static char configRoot[CDOGS_PATH_MAX];
static char configPath[CDOGS_PATH_MAX];

static bool JoinPath(char *out, const char *base, const char *name)
{
    size_t a = strlen(base), b = strlen(name);
    bool separator = a && base[a - 1] != '/' && base[a - 1] != ':';
    if (a + separator + b >= CDOGS_PATH_MAX) { out[0] = '\0'; return false; }
    memcpy(out, base, a);
    if (separator) out[a++] = '/';
    memcpy(out + a, name, b + 1);
    return true;
}

bool CDogsPS2IsAbsolutePath(const char *path)
{
    if (!path || !*path) return false;
    const char *colon = strchr(path, ':');
    const char *slash = strchr(path, '/');
    return path[0] == '/' || (colon && (!slash || colon < slash));
}

void CDogsPS2ResolvePath(const char *path, char *out)
{
    char absolute[CDOGS_PATH_MAX];
    if (!path || strlen(path) >= sizeof absolute) { out[0] = '\0'; return; }
    if (CDogsPS2IsAbsolutePath(path))
        strcpy(absolute, path);
    else
    {
        char cwd[CDOGS_PATH_MAX];
        if (!getcwd(cwd, sizeof cwd)) strcpy(cwd, "host:");
        if (!JoinPath(absolute, cwd, path)) { out[0] = '\0'; return; }
    }
    for (char *p = absolute; *p; ++p) if (*p == '\\') *p = '/';
    char *begin = absolute;
    size_t used = 0;
    char *colon = strchr(absolute, ':');
    char *slash = strchr(absolute, '/');
    if (colon && slash && slash < colon) colon = NULL;
    if (colon)
    {
        used = (size_t)(colon - absolute + 1);
        memcpy(out, absolute, used);
        begin = colon + 1;
    }
    /* host: paths are relative to the loader's host root. A leading slash
     * has different meanings across hostfs servers; do not introduce one.
     */
    bool host = colon && !strncmp(absolute, "host", 4);
    if (host)
        for (char *p = absolute + 4; p < colon; ++p)
            if (*p < '0' || *p > '9') host = false;
    if (!host) out[used++] = '/';
    const size_t rootEnd = used;
    char *save = NULL;
    for (char *part = strtok_r(begin, "/", &save); part; part = strtok_r(NULL, "/", &save))
    {
        if (!strcmp(part, ".")) continue;
        if (!strcmp(part, ".."))
        {
            if (used > rootEnd)
            {
                --used;
                while (used > rootEnd && out[used - 1] != '/') --used;
            }
            continue;
        }
        const size_t len = strlen(part);
        if (used + len + 1 >= CDOGS_PATH_MAX) { out[0] = '\0'; return; }
        memcpy(out + used, part, len);
        used += len;
        out[used++] = '/';
    }
    if (used > rootEnd) --used;
    out[used] = '\0';
}

static bool TryRoot(const char *candidate)
{
    char root[CDOGS_PATH_MAX], path[CDOGS_PATH_MAX];
    CDogsPS2ResolvePath(candidate, root);
    if (!*root) return false;
    struct stat st;
    if (!JoinPath(path, root, "data/guns.json")) return false;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    if (!JoinPath(path, root, "graphics/font.png")) return false;
    if (stat(path, &st) != 0 || !S_ISREG(st.st_mode)) return false;
    return JoinPath(dataRoot, root, "");
}

bool CDogsPS2InitPaths(int argc, char **argv)
{
    const char *override = getenv("CDOGS_DATA_DIR");
    bool found = override && TryRoot(override);
    if (!found && argc > 0 && argv && argv[0])
    {
        char exe[CDOGS_PATH_MAX];
        snprintf(exe, sizeof exe, "%s", argv[0]);
        char *slash = strrchr(exe, '/');
        if (slash) { slash[1] = '\0'; found = TryRoot(exe); }
        else
        {
            char *colon = strchr(exe, ':');
            if (colon) { colon[1] = '\0'; found = TryRoot(exe); }
        }
    }
    if (!found) found = TryRoot(".");
    const char *fallback[] = {"host:", "mass:/cdogs-sdl", "mass0:/cdogs-sdl", NULL};
    for (int i = 0; !found && fallback[i]; ++i) found = TryRoot(fallback[i]);
    if (!found) return false;
    override = getenv("CDOGS_CONFIG_DIR");
    if (override)
    {
        char resolved[CDOGS_PATH_MAX];
        CDogsPS2ResolvePath(override, resolved);
        if (!*resolved || !JoinPath(configRoot, resolved, "")) return false;
    }
    else if (!strncmp(dataRoot, "cdfs:", 5))
        strcpy(configRoot, "mc0:/CDOGS/");
    else
        if (!JoinPath(configRoot, dataRoot, "config/")) return false;
    return true;
}
const char *CDogsPS2DataRoot(void) { return dataRoot; }
const char *CDogsPS2ConfigPath(const char *name)
{
    JoinPath(configPath, configRoot, name);
    return configPath;
}
void CDogsPS2DataPath(const char *path, char *out)
{
    char joined[CDOGS_PATH_MAX];
    if (!JoinPath(joined, CDogsPS2IsAbsolutePath(path) ? "" : dataRoot, path))
    { out[0] = '\0'; return; }
    CDogsPS2ResolvePath(joined, out);
}

static void FixedInt(Config *c, const char *name, int value)
{
    Config *item = ConfigGet(c, name);
    item->u.Int.Min = item->u.Int.Max = value;
    item->u.Int.Value = item->u.Int.Last = item->u.Int.Default = value;
}
void CDogsPS2ApplyConfig(Config *c)
{
    /* Fixed 320x240 composition keeps RAM and CPU cost bounded. */
    FixedInt(c, "Graphics.WindowWidth", 640);
    FixedInt(c, "Graphics.WindowHeight", 480);
    FixedInt(c, "Graphics.ScaleFactor", 2);
    ConfigGet(c, "Graphics.Fullscreen")->u.Bool.Value = false;
    ConfigGet(c, "Graphics.SecondWindow")->u.Bool.Value = false;
    ConfigGet(c, "Graphics.DOSPAR")->u.Bool.Value = false;
#ifndef CDOGS_PS2_RFAUDS2
    FixedInt(c, "Sound.SoundVolume", 0);
    FixedInt(c, "Sound.MusicVolume", 0);
#endif
}

void CDogsPS2AddControllerMappings(void)
{
    for (int i = 0; i < SDL_NumJoysticks(); ++i)
    {
        char guid[33], mapping[512];
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(i), guid, sizeof guid);
        /* Button indices are the PAD_* bit positions in SDL's PS2 driver. */
        snprintf(mapping, sizeof mapping,
            "%s,PS2 Controller,a:b14,b:b13,x:b15,y:b12,back:b0,start:b3,"
            "leftstick:b1,rightstick:b2,leftshoulder:b10,rightshoulder:b11,"
            "lefttrigger:b8,righttrigger:b9,dpup:b4,dpright:b5,dpdown:b6,dpleft:b7,"
            "leftx:a0,lefty:a1,rightx:a2,righty:a3,", guid);
        if (SDL_GameControllerAddMapping(mapping) < 0)
            fprintf(stderr, "PS2: controller mapping: %s\n", SDL_GetError());
    }
}
