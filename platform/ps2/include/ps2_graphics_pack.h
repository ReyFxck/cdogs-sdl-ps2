#pragma once

#include <SDL_surface.h>
#include <stdbool.h>

/* Returns the image count, -1 if absent (loose-asset fallback), -2 if invalid. */
int CDogsPS2LoadGraphicsPack(
    const char *path,
    void (*add)(void *context, const char *name, bool is_hd, SDL_Surface *image),
    void *context);
