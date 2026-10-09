#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DIR *__wrap_opendir(const char *);
struct dirent *__wrap_readdir(DIR *);
int __wrap_closedir(DIR *);

/* A backend with one physical CDFS handle, 400 entries, and eight levels.
 * The test exercises the actual shim and tinydir's recursive calling pattern.
 */
typedef struct
{
    int depth, cursor;
    struct dirent entry;
} Physical;
static Physical *active;
static unsigned seen;
static int failRead;

DIR *__real_opendir(const char *path)
{
    if (strncmp(path, "cdfs:", 5)) return opendir(path);
    assert(!active);
    active = calloc(1, sizeof *active);
    assert(active);
    for (const char *s = path + 5; *s; s++) if (*s == '/') active->depth++;
    return (DIR *)active;
}

struct dirent *__real_readdir(DIR *dir)
{
    if ((DIR *)active != dir) return readdir(dir);
    if (failRead) { errno = EIO; return NULL; }
    int n = active->cursor++;
    if (n == 400) { errno = 0; return NULL; }
    active->entry.d_type = n == 0 && active->depth < 8 ? DT_DIR : DT_REG;
    snprintf(active->entry.d_name, sizeof active->entry.d_name, "entry%d", n);
    return &active->entry;
}

int __real_closedir(DIR *dir)
{
    if ((DIR *)active != dir) return closedir(dir);
    free(active); active = NULL;
    return 0;
}

#define opendir __wrap_opendir
#define readdir __wrap_readdir
#define closedir __wrap_closedir
#include <tinydir/tinydir.h>
#undef opendir
#undef readdir
#undef closedir

static void Walk(const char *path)
{
    tinydir_dir dir;
    assert(tinydir_open(&dir, path) == 0);
    assert(!active); // SDK handle was closed before the caller sees the DIR.
    unsigned count = 0;
    while (dir.has_next)
    {
        tinydir_file file;
        assert(tinydir_readfile(&dir, &file) == 0);
        count++;
        if (file.is_dir)
        {
            Walk(file.path);
            tinydir_file again;
            assert(tinydir_readfile(&dir, &again) == 0);
            assert(!strcmp(file.name, again.name));
        }
        else { assert(file.is_reg); seen++; }
        assert(tinydir_next(&dir) == 0);
    }
    assert(count == 400);
    tinydir_close(&dir);
}

int main(void)
{
    Walk("cdfs:/graphics");
    assert(seen == 8 * 400 - 7);
    DIR *normal = __wrap_opendir(".");
    assert(normal && __wrap_readdir(normal));
    assert(__wrap_closedir(normal) == 0);
    failRead = 1;
    assert(!__wrap_opendir("cdfs:/bad") && errno == EIO);
    assert(!active);
    puts("CDFS snapshot: 8 levels, 400 entries, stable parents, one physical handle, errors and POSIX OK");
    return 0;
}
