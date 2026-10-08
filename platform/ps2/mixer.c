#include "SDL_mixer.h"
#ifndef CDOGS_PS2_RFAUDS2
int Mix_OpenAudioDevice(int f, Uint16 fmt, int c, int size, const char *d, int changes)
{ (void)f; (void)fmt; (void)c; (void)size; (void)d; (void)changes; return SDL_SetError("PS2 audio disabled at build time"); }
int Mix_QuerySpec(int *f, Uint16 *fmt, int *c) { (void)f; (void)fmt; (void)c; return 0; }
void Mix_CloseAudio(void) {}
int Mix_Init(int flags) { (void)flags; return 0; }
void Mix_Quit(void) {}
int Mix_AllocateChannels(int count) { (void)count; return 0; }
Mix_Chunk *Mix_LoadWAV(const char *p) { (void)p; return NULL; }
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *p, Uint32 size) { (void)p; (void)size; return NULL; }
void Mix_FreeChunk(Mix_Chunk *p) { (void)p; }
int Mix_PlayChannel(int c, Mix_Chunk *p, int loops) { (void)c; (void)p; (void)loops; return -1; }
int Mix_Playing(int c) { (void)c; return 0; }
int Mix_HaltChannel(int c) { (void)c; return 0; }
void Mix_Pause(int c) { (void)c; }
void Mix_Resume(int c) { (void)c; }
void Mix_ChannelFinished(void (*cb)(int)) { (void)cb; }
int Mix_Volume(int c, int v) { (void)c; (void)v; return 0; }
int Mix_SetPosition(int c, Sint16 a, Uint8 d) { (void)c; (void)a; (void)d; return 0; }
int Mix_SetPanning(int c, Uint8 l, Uint8 r) { (void)c; (void)l; (void)r; return 0; }
int Mix_RegisterEffect(int c, Mix_EffectFunc_t e, Mix_EffectDone_t d, void *u)
{ (void)c; (void)e; (void)d; (void)u; return 0; }
Mix_Music *Mix_LoadMUS(const char *p) { (void)p; return NULL; }
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *rw, int freeRW) { if (freeRW && rw) SDL_RWclose(rw); return NULL; }
void Mix_FreeMusic(Mix_Music *p) { (void)p; }
int Mix_PlayMusic(Mix_Music *p, int loops) { (void)p; (void)loops; return -1; }
int Mix_PlayingMusic(void) { return 0; }
int Mix_PausedMusic(void) { return 0; }
int Mix_HaltMusic(void) { return 0; }
void Mix_PauseMusic(void) {}
void Mix_ResumeMusic(void) {}
int Mix_VolumeMusic(int v) { (void)v; return 0; }
void CDogsPS2AudioPump(void) {}
#else
#include "rfa_mixer.inc"
#endif
