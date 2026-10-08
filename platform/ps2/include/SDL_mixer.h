/* Small C-Dogs mixer frontend; this never links SDL_mixer or SDL audio. */
#pragma once
#include <SDL.h>
typedef struct Mix_Chunk {
    int allocated;
    Uint8 *abuf;
    Uint32 alen;
    Uint8 volume;
    /* Lazy PCM cache, owned only by this frontend. */
    char *path;
    Uint32 lastUse;
    struct Mix_Chunk *next;
} Mix_Chunk;
typedef struct Mix_Music Mix_Music;
typedef void (*Mix_EffectFunc_t)(int, void *, int, void *);
typedef void (*Mix_EffectDone_t)(int, void *);
#define MIX_MAX_VOLUME 128
#define Mix_GetError SDL_GetError
int Mix_OpenAudioDevice(int frequency, Uint16 format, int channels, int size, const char *device, int changes);
int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels);
void Mix_CloseAudio(void);
int Mix_Init(int flags);
void Mix_Quit(void);
int Mix_AllocateChannels(int count);
Mix_Chunk *Mix_LoadWAV(const char *path);
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *data, Uint32 len);
void Mix_FreeChunk(Mix_Chunk *chunk);
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int Mix_Playing(int channel);
int Mix_HaltChannel(int channel);
void Mix_Pause(int channel);
void Mix_Resume(int channel);
void Mix_ChannelFinished(void (*callback)(int));
int Mix_Volume(int channel, int volume);
int Mix_SetPosition(int channel, Sint16 angle, Uint8 distance);
int Mix_SetPanning(int channel, Uint8 left, Uint8 right);
int Mix_RegisterEffect(int channel, Mix_EffectFunc_t effect, Mix_EffectDone_t done, void *user);
Mix_Music *Mix_LoadMUS(const char *path);
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *rw, int freeRW);
void Mix_FreeMusic(Mix_Music *music);
int Mix_PlayMusic(Mix_Music *music, int loops);
int Mix_PlayingMusic(void);
int Mix_PausedMusic(void);
int Mix_HaltMusic(void);
void Mix_PauseMusic(void);
void Mix_ResumeMusic(void);
int Mix_VolumeMusic(int volume);
