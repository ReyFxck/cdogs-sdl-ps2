/* SDL2main prepares the IOP, filesystem drivers and device readiness. */
#include <SDL.h>
#include <stdio.h>
#include "ps2_platform.h"

int CDogsMain(int argc, char **argv);

int SDL_main(int argc, char **argv)
{
    if (!CDogsPS2InitPaths(argc, argv))
    {
        fprintf(stderr, "PS2: assets missing. Place data/ and graphics/ beside the ELF.\n");
        return 1;
    }
    printf("PS2: C-Dogs SDL; data=%s; network=offline\n", CDogsPS2DataRoot());
    return CDogsMain(argc, argv);
}
