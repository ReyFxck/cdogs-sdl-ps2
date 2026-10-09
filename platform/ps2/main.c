/* SDL2main prepares the IOP, filesystem drivers and device readiness. */
#include <SDL.h>
#include <stdio.h>
#include <unistd.h>
#include "sys_config.h"
#include "ps2_platform.h"
#ifdef _EE
#include <loadfile.h>
extern const unsigned char cdogs_cdfs_irx[];
extern const unsigned int cdogs_cdfs_irx_size;
#endif

int CDogsMain(int argc, char **argv);

int SDL_main(int argc, char **argv)
{
#ifdef _EE
    int result = -1;
    int module = SifExecModuleBuffer((void *)cdogs_cdfs_irx,
        cdogs_cdfs_irx_size, 0, NULL, &result);
    if (module < 0 || result != 0)
    {
        fprintf(stderr, "PS2: CDFS replacement failed: id=%d result=%d\n", module, result);
        return 1;
    }
    printf("PS2: game-local CDFS ready\n");
#endif
    if (!CDogsPS2InitPaths(argc, argv))
    {
        char cwd[CDOGS_PATH_MAX];
        if (!getcwd(cwd, sizeof cwd)) snprintf(cwd, sizeof cwd, "(unavailable)");
        fprintf(stderr, "PS2: assets missing; boot=%s; cwd=%s\n",
            argc > 0 && argv && argv[0] ? argv[0] : "(none)", cwd);
        fprintf(stderr, "PS2: use the complete ISO on Android; ELF alone has no assets.\n"
            "PS2: ELF loaders need data/ and graphics/ beside the ELF or on mass:/cdogs-sdl.\n");
        return 1;
    }
    printf("PS2: C-Dogs SDL; data=%s; network=offline\n", CDogsPS2DataRoot());
    return CDogsMain(argc, argv);
}
