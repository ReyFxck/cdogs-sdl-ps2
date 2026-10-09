/* Link this with PS2SDK's cdfs_iop.c, not a reimplementation of the parser. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libcdvd-common.h>
#include "cdfs_iop.h"

static FILE *disc;
static unsigned files, directories, reads;
static int interfere;
static const char *package;
int sceCdInit(int unused) { (void)unused; return 1; }
int sceCdRead(u32 lba, u32 count, void *buffer, sceCdRMode *mode)
{
    (void)mode;
    assert(fseek(disc, (long)lba * 2048, SEEK_SET) == 0);
    int ok = count == 0 || fread(buffer, 2048, count, disc) == count;
    reads++;
    if (interfere)
    {
        // Model another IOP module using shared ROM strtok during CD I/O.
        static char other[] = "other,module";
        memcpy(other, "other,module", sizeof other);
        strtok(other, ",");
    }
    return ok;
}
int sceCdReadDVDV(u32 a, u32 b, void *c, sceCdRMode *d)
{ (void)a; (void)b; (void)c; (void)d; abort(); }
int sceCdSync(int unused) { (void)unused; return 0; }
int sceCdGetError(void) { return 0; }
int sceCdGetDiskType(void) { return SCECdPS2DVD; }
int sceCdDiskReady(int unused) { (void)unused; return 1; }
int sceCdTrayReq(int unused, u32 *changed)
{ (void)unused; *changed = 0; return 1; }
void *AllocSysMemory(int unused, int size, void *address)
{ (void)unused; (void)address; return malloc(size); }
int FreeSysMemory(void *p) { free(p); return 0; }

static void found(const char *name)
{
    struct TocEntry e;
    if (!cdfs_findfile(name, &e))
    {
        fprintf(stderr, "SDK CDFS cannot find %s\n", name);
        exit(1);
    }
    if (package)
    {
        char path[2048];
        const char *relative = !strcmp(name, "/CDOGS.ELF") ? "cdogs-sdl.elf" : name + 1;
        if (!strcmp(name, "/SYSTEM.CNF")) return;
        assert(snprintf(path, sizeof path, "%s/%s", package, relative) < (int)sizeof path);
        FILE *source = fopen(path, "rb");
        assert(source);
        u8 actual[2048], expected[2048];
        for (u32 offset = 0; offset < e.fileSize; offset += 2048)
        {
            size_t n = e.fileSize - offset;
            if (n > 2048) n = 2048;
            assert(cdfs_readSect(e.fileLBA + offset / 2048, 1, actual));
            assert(fread(expected, 1, n, source) == n);
            assert(!memcmp(actual, expected, n));
        }
        assert(fgetc(source) == EOF);
        fclose(source);
    }
}

static void walk(const char *path)
{
    struct TocEntry entries[512];
    int n = cdfs_getDir(path, entries, 512);
    if (n < 0)
    {
        fprintf(stderr, "SDK CDFS cannot list %s\n", path);
        exit(1);
    }
    assert(n < 512);
    directories++;
    for (int i = 0; i < n; i++)
    {
        if (!strcmp(entries[i].filename, "..")) continue;
        char child[1024];
        assert(snprintf(child, sizeof child, "%s/%s", path,
                        entries[i].filename) < (int)sizeof child);
        if (entries[i].fileProperties & CDFS_FILEPROPERTY_DIR) walk(child);
        else { found(child); files++; }
    }
}

int main(int argc, char **argv)
{
    assert(argc >= 2 && argc <= 5);
    interfere = argc >= 3;
    package = argc == 5 ? argv[4] : NULL;
    disc = fopen(argv[1], "rb");
    assert(disc);
    cdfs_prepare();
    found("/data/guns.json");
    found("/graphics/font.png");
    walk("");
    found("/graphics/font.png");
    found("/graphics/table_wood_round_terminal_wreck.png");
    found("/data/guns.json");
    if (argc >= 4) assert(files == (unsigned)atoi(argv[3]));
    cdfs_finish();
    fclose(disc);
    printf("Real SDK CDFS: %u files and %u directories readable (%u reads; interference=%d)\n",
           files, directories, reads, interfere);
    return 0;
}
