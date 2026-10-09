/* Close CDFS's physical directory before a caller recurses into children.
 * Compact dirent snapshots also keep entries stable across nested enumerations.
 * host:/mass:/mc0: continue to use the normal libc implementation.
 */
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

typedef struct Snapshot
{
    struct Snapshot *next;
    struct dirent *entries;
    size_t count, cursor;
} Snapshot;
static Snapshot *snapshots;

DIR *__real_opendir(const char *path);
struct dirent *__real_readdir(DIR *dir);
int __real_closedir(DIR *dir);

static Snapshot **Find(DIR *dir)
{
    Snapshot **p = &snapshots;
    while (*p && (DIR *)*p != dir) p = &(*p)->next;
    return p;
}

DIR *__wrap_opendir(const char *path)
{
    if (!path || strncmp(path, "cdfs:", 5)) return __real_opendir(path);
    DIR *physical = __real_opendir(path);
    if (!physical) return NULL;
    Snapshot *s = calloc(1, sizeof *s);
    if (!s) { __real_closedir(physical); errno = ENOMEM; return NULL; }
    size_t capacity = 0;
    struct dirent *entry;
    errno = 0;
    while ((entry = __real_readdir(physical)))
    {
        if (s->count == capacity)
        {
            capacity = capacity ? capacity * 2 : 16;
            struct dirent *p = realloc(s->entries, capacity * sizeof *p);
            if (!p)
            {
                free(s->entries); free(s); __real_closedir(physical);
                errno = ENOMEM; return NULL;
            }
            s->entries = p;
        }
        s->entries[s->count++] = *entry;
        errno = 0;
    }
    int error = errno;
    __real_closedir(physical);
    if (error)
    {
        free(s->entries); free(s); errno = error; return NULL;
    }
    s->next = snapshots;
    snapshots = s;
    return (DIR *)s;
}

struct dirent *__wrap_readdir(DIR *dir)
{
    Snapshot *s = *Find(dir);
    if (!s) return __real_readdir(dir);
    if (s->cursor == s->count) return NULL;
    return &s->entries[s->cursor++];
}

int __wrap_closedir(DIR *dir)
{
    Snapshot **p = Find(dir);
    if (!*p) return __real_closedir(dir);
    Snapshot *s = *p;
    *p = s->next;
    free(s->entries); free(s);
    return 0;
}
