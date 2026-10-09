/* SDL's RWFromFile fstat gate rejects the legacy CDFS mode. Do not alter
 * SDL/SDK or relax checks on writable/host devices: bypass that gate only
 * for read-only CDFS files that fopen has actually opened successfully.
 */
#include <SDL.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

SDL_RWops *__real_SDL_RWFromFile(const char *file, const char *mode);

SDL_RWops *__wrap_SDL_RWFromFile(const char *file, const char *mode)
{
    if (!file || !mode || strncmp(file, "cdfs:", 5) ||
        (strcmp(mode, "rb") && strcmp(mode, "r")))
        return __real_SDL_RWFromFile(file, mode);

    FILE *fp = fopen(file, mode);
    if (!fp)
    {
        SDL_SetError("CDFS: cannot open %s: %s", file, strerror(errno));
        return NULL;
    }
    SDL_RWops *rw = SDL_RWFromFP(fp, SDL_TRUE);
    if (!rw) fclose(fp);
    return rw;
}
