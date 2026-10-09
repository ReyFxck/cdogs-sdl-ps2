/* SDL2main prepares the IOP, filesystem drivers and device readiness. */
#include <SDL.h>
#include <stdio.h>
#include <unistd.h>
#include "sys_config.h"
#include "ps2_platform.h"

int CDogsMain(int argc, char **argv);

int SDL_main(int argc, char **argv)
{
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
