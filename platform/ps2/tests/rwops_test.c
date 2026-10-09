#define _LARGEFILE64_SOURCE
#define SDL_MAIN_HANDLED
#define SDL_STBIMAGE_IMPLEMENTATION
#include <stb/SDL_stbimage.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int legacyMode;
/* Executable interposition reaches the real native shared SDL, not a mock
 * RWops implementation. Cover both Linux large-file configurations.
 */
int fstat(int fd, struct stat *st)
{
    int (*real)(int, struct stat *) = dlsym(RTLD_NEXT, "fstat");
    assert(real);
    int ret = real(fd, st);
    if (!ret && legacyMode) st->st_mode = 0; /* SDK CDFS's unconverted mode. */
    return ret;
}
int fstat64(int fd, struct stat64 *st)
{
    int (*real)(int, struct stat64 *) = dlsym(RTLD_NEXT, "fstat64");
    assert(real);
    int ret = real(fd, st);
    if (!ret && legacyMode) st->st_mode = 0;
    return ret;
}
SDL_RWops *__real_SDL_RWFromFile(const char *file, const char *mode);

int main(int argc, char **argv)
{
    assert(argc == 2);
    assert(SDL_Init(0) == 0);
    SDL_Surface *reference = STBIMG_Load(argv[1]);
    assert(reference);
    char cwd[4096]; assert(getcwd(cwd, sizeof cwd));
    char temporary[] = "/tmp/cdogs-ps2-rwops-XXXXXX";
    assert(mkdtemp(temporary)); assert(chdir(temporary) == 0);
    assert(mkdir("cdfs:", 0700) == 0);
    const char *file = "cdfs:/font.png";
    FILE *src = fopen(argv[1], "rb"), *dest = fopen(file, "wb");
    assert(src && dest);
    unsigned char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, src))) assert(fwrite(buf, 1, n, dest) == n);
    assert(!ferror(src)); fclose(src); fclose(dest);
    legacyMode = 1;
    SDL_RWops *rejected = __real_SDL_RWFromFile(file, "rb");
    assert(!rejected && strstr(SDL_GetError(), "not a regular file"));
    /* The CDFS-only adapter must preserve all seek/read/close semantics. */
    for (int i = 0; i < 64; ++i)
    {
        SDL_RWops *rw = SDL_RWFromFile(file, "rb"); assert(rw);
        assert(SDL_RWsize(rw) > 8);
        assert(SDL_RWseek(rw, 0, RW_SEEK_SET) == 0);
        SDL_Surface *image = STBIMG_Load_RW(rw, 1); assert(image);
        assert(image->w == reference->w && image->h == reference->h);
        assert(image->pitch == reference->pitch);
        assert(!memcmp(image->pixels, reference->pixels, image->pitch * image->h));
        SDL_FreeSurface(image);
    }
    assert(!SDL_RWFromFile(argv[1], "rb")); /* Not CDFS: original checks remain. */
    assert(!SDL_RWFromFile("cdfs:/missing.png", "rb"));
    assert(strstr(SDL_GetError(), "CDFS: cannot open"));
    legacyMode = 0;
    SDL_RWops *normal = SDL_RWFromFile(argv[1], "rb"); assert(normal); SDL_RWclose(normal);
    SDL_FreeSurface(reference); SDL_Quit();
    assert(unlink(file) == 0); assert(rmdir("cdfs:") == 0);
    assert(chdir(cwd) == 0); assert(rmdir(temporary) == 0);
    puts("Real SDL fstat rejection reproduced; CDFS-only RWops adapter decodes font pixels correctly");
    return 0;
}
