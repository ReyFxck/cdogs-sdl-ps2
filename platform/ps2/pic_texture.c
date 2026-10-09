/* The pinned SDL software backend keeps streaming pixels in a stable,
 * tightly-packed SDL_Surface. Lock returns that buffer; unlock is a no-op.
 * Pic.Data borrows it so the EE does not retain a second copy of every sprite.
 * This contract is NOT valid for accelerated SDL textures or arbitrary SDL
 * backends; reject them and test pointer stability/pitch in native regressions.
 */
#include "ps2_pic.h"
#include "grafx.h"
#include <stdlib.h>
#include <string.h>

bool CDogsPS2PicMakeTex(Pic *p)
{
    SDL_Renderer *renderer = gGraphicsDevice.gameWindow.renderer;
    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0 || strcmp(info.name, "software"))
    {
        SDL_SetError("PS2 Pic requires the pinned SDL software backend");
        return false;
    }
    const int w = p->size.x * (p->isHD ? 2 : 1);
    const int h = p->size.y * (p->isHD ? 2 : 1);
    SDL_Texture *t = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING, w, h);
    if (!t) return false;
    void *pixels;
    int pitch;
    if (SDL_LockTexture(t, NULL, &pixels, &pitch) != 0)
    {
        SDL_DestroyTexture(t);
        return false;
    }
    if (pitch != w * (int)sizeof *p->Data)
    {
        SDL_UnlockTexture(t); SDL_DestroyTexture(t);
        SDL_SetError("PS2 software texture pitch is not packed");
        return false;
    }
    memcpy(pixels, p->Data, (size_t)pitch * h);
    SDL_UnlockTexture(t);
    if (SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND) != 0)
    {
        SDL_DestroyTexture(t);
        return false;
    }
    // Copy before destroying the previous texture: it may own the source.
    SDL_DestroyTexture(p->Tex);
    if (!p->DataFromTexture) free(p->Data);
    p->Tex = t;
    p->Data = pixels;
    p->DataFromTexture = true;
    return true;
}
