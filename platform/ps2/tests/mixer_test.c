#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "SDL_mixer.h"
#include <rfauds2/rfauds2.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

void CDogsPS2AudioPump(void);
const unsigned char rfauds2_irx[] = {0};
const unsigned int rfauds2_irx_size = 1;
static s16 submitted[960 * 2], capture[20000 * 2];
static u32 request, captured, queued;
static int busy, delay, sequence, deviceStarted, finishes, effectsDone;

/* A delayed IOP accepting zero and partial prefixes, never borrowing memory. */
int rfauds2_init(const void *p, u32 n) { assert(p && n); return 0; }
int rfauds2_bind(void) { assert(!busy); return 0; }
int rfauds2_stop(void) { assert(!busy); deviceStarted = 0; queued = 0; return 0; }
int rfauds2_start(void) { assert(!busy); deviceStarted = 1; return 0; }
int rfauds2_set_latency_ms(u32 ms) { assert(!busy && ms == 64); return 0; }
int rfauds2_set_volume(u32 volume) { assert(!busy && volume == RFAUDS2_VOLUME_MAX); return 0; }
int rfauds2_get_cached_stats(rfauds2_stats *stats)
{ memset(stats, 0, sizeof *stats); stats->queued_frames = deviceStarted ? 0 : queued; return 0; }
int rfauds2_submit_s16_async(const s16 *pcm, u32 frames)
{
    assert(!busy && frames && frames <= 960);
    memcpy(submitted, pcm, frames * 4); request = frames; busy = 1; delay = 2;
    return 0;
}
int rfauds2_submit_poll(u32 *accepted)
{
    assert(busy);
    if (delay--) return 0;
    const u32 pattern[] = {0, 137, 96, 0, 960, 351};
    u32 count = pattern[sequence++ % 6];
    if (count > request) count = request;
    assert(captured + count <= 20000);
    memcpy(capture + captured * 2, submitted, count * 4);
    captured += count; queued += count; *accepted = count; busy = 0;
    return 1;
}
static void Finish(int channel) { assert(channel >= 0); ++finishes; }
static void Half(int channel, void *stream, int bytes, void *user)
{
    assert(channel == 0 && user == &effectsDone);
    s16 *p = stream; for (int i = 0; i < bytes / 2; ++i) p[i] /= 2;
}
static void Done(int channel, void *user) { assert(channel == 0 && user == &effectsDone); ++effectsDone; }
static void Open(void)
{
    captured = queued = sequence = deviceStarted = finishes = effectsDone = 0;
    assert(!busy);
    assert(Mix_OpenAudioDevice(44100, AUDIO_S16SYS, 2, 1024, NULL, 0) < 0);
    assert(Mix_OpenAudioDevice(48000, AUDIO_S16SYS, 2, 1024, NULL, 0) == 0);
    assert(Mix_AllocateChannels(4) == 4);
    Mix_Volume(-1, 128); Mix_VolumeMusic(128); Mix_ChannelFinished(Finish);
}
static void Pump(u32 frames)
{
    for (int i = 0; i < 20000 && captured < frames; ++i) CDogsPS2AudioPump();
    assert(captured >= frames && deviceStarted);
    Mix_CloseAudio(); assert(!busy);
}
static void Transport(void)
{
    static s16 ramp[5003 * 2];
    for (int i = 0; i < 5003; ++i) { ramp[i * 2] = i - 2500; ramp[i * 2 + 1] = 2500 - i; }
    Open();
    Mix_Chunk *chunk = Mix_QuickLoad_RAW((Uint8 *)ramp, sizeof ramp); assert(chunk);
    assert(Mix_PlayChannel(0, chunk, 0) == 0); Pump(5760);
    for (int i = 0; i < 5760; ++i)
    {
        assert(capture[i * 2] == (i < 5003 ? ramp[i * 2] : 0));
        assert(capture[i * 2 + 1] == (i < 5003 ? ramp[i * 2 + 1] : 0));
    }
    assert(finishes == 1); Mix_FreeChunk(chunk);
}
static void Mixing(void)
{
    static s16 tone[960 * 2]; for (int i = 0; i < 1920; ++i) tone[i] = 30000;
    Open(); Mix_Chunk *chunk = Mix_QuickLoad_RAW((Uint8 *)tone, sizeof tone); assert(chunk);
    assert(Mix_PlayChannel(0, chunk, 1) == 0);
    assert(Mix_PlayChannel(1, chunk, 0) == 1);
    assert(Mix_RegisterEffect(0, Half, Done, &effectsDone));
    assert(Mix_SetPanning(0, 255, 0));
    Mix_Volume(1, 64); Pump(2880);
    for (int i = 0; i < 2880; ++i)
    {
        int left = i < 960 ? 30000 : (i < 1920 ? 15000 : 0);
        int right = i < 960 ? 15000 : 0;
        assert(capture[i * 2] == left && capture[i * 2 + 1] == right);
    }
    assert(finishes == 2 && effectsDone == 1); Mix_FreeChunk(chunk);
    Open(); chunk = Mix_QuickLoad_RAW((Uint8 *)tone, sizeof tone);
    assert(Mix_PlayChannel(0, chunk, 0) == 0 && Mix_PlayChannel(1, chunk, 0) == 1);
    Pump(1920); assert(capture[0] == 32767 && capture[1] == 32767); Mix_FreeChunk(chunk);
}
static void Music(const char *directory)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/music.ogg", directory);
    Open(); Mix_Music *song = Mix_LoadMUS(path); assert(song);
    Mix_VolumeMusic(64); assert(Mix_PlayMusic(song, 0) == 0); Pump(2880);
    for (int i = 0; i < 2880; ++i)
        assert(capture[i * 2] == (i < 2107 ? -1500 : 0) &&
               capture[i * 2 + 1] == (i < 2107 ? 3500 : 0));
    assert(!Mix_PlayingMusic()); Mix_FreeMusic(song);
    snprintf(path, sizeof path, "%s/truncated.wav", directory); assert(!Mix_LoadWAV(path));
    snprintf(path, sizeof path, "%s/wrong-rate.wav", directory); assert(!Mix_LoadWAV(path));
    snprintf(path, sizeof path, "%s/oversize.wav", directory);
    Open(); Mix_Chunk *chunk = Mix_LoadWAV(path); assert(chunk);
    assert(Mix_PlayChannel(0, chunk, 0) == -1); /* Exceeds the 4 MiB cache. */
    Mix_FreeChunk(chunk); Mix_CloseAudio();
}
int main(int argc, char **argv)
{
    assert(argc == 2); assert(SDL_Init(SDL_INIT_TIMER) == 0);
    Transport(); Mixing(); Music(argv[1]); SDL_Quit();
    puts("RFAuds2 frontend: partial/zero acceptance, mixing, loops/effects, streaming and PCM validation OK");
    return 0;
}
