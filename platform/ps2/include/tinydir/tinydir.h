#pragma once
/* Keep upstream tinydir unchanged. CDFS's legacy getstat returns 1 on a
 * match, so IOMANX does not convert its old mode bits; missing files also
 * return 0. Dread does convert modes correctly. Use the known directory
 * entry's type on CDFS and leave all other devices on normal stat/lstat.
 */
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

struct tinydir_dir;
static inline int CDogsPS2TinydirStat(
    const struct tinydir_dir *dir, const char *path, struct stat *st);

/* Function-like macros do not rename the struct stat type. Their scope is
 * limited to the upstream header, whose readfile function has a dir argument.
 */
#define stat(path, st) CDogsPS2TinydirStat(dir, path, st)
#define lstat(path, st) CDogsPS2TinydirStat(dir, path, st)
#include_next <tinydir/tinydir.h>
#undef stat
#undef lstat

static inline int CDogsPS2TinydirStat(
    const struct tinydir_dir *dir, const char *path, struct stat *st)
{
    if (!strncmp(path, "cdfs:", 5) && dir && dir->_e &&
        (dir->_e->d_type == DT_DIR || dir->_e->d_type == DT_REG))
    {
        memset(st, 0, sizeof *st);
        st->st_mode = dir->_e->d_type == DT_DIR ? S_IFDIR : S_IFREG;
        return 0;
    }
    return stat(path, st);
}
