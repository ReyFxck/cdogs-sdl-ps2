/* PCM frontend for the C-Dogs Mix_* call sites. Output is exclusively RFAuds2.
 * All operations run on the game thread; RPC never borrows a game buffer.
 */
#include <rfauds2/rfauds2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>

#define RF_CHANNELS 128
#define RF_BLOCK RFAUDS2_ASYNC_MAX_FRAMES
#define RF_CACHE_BYTES (4u * 1024u * 1024u)
extern const unsigned char rfauds2_irx[];
extern const unsigned int rfauds2_irx_size;

typedef struct {
    Mix_Chunk *chunk;
    Uint32 cursor;
    int loops, volume, paused;
    Uint8 left, right, distance;
    Mix_EffectFunc_t effect;
    Mix_EffectDone_t done;
    void *user;
} RFVoice;
struct Mix_Music { char *path; long start; Uint32 bytes; };
static RFVoice voices[RF_CHANNELS];
static int voiceCount, musicVolume = MIX_MAX_VOLUME, opened, started, moduleLoaded;
static Mix_Chunk *chunks;
static Uint32 cacheBytes;
static void (*finished)(int);
static Mix_Music *song;
static FILE *songFile;
static Uint32 songCursor;
static int songLoops, songPaused;
static s16 pending[RF_BLOCK * 2], scratch[RF_BLOCK * 2];
static s32 accumulator[RF_BLOCK * 2];
static u32 pendingFrames, pendingOffset, inflightFrames;
static int inflight;
static Uint32 snapshotTick;

static Uint16 RFRead16(const Uint8 *p) { return (Uint16)(p[0] | (p[1] << 8)); }
static Uint32 RFRead32(const Uint8 *p)
{ return (Uint32)p[0] | ((Uint32)p[1] << 8) | ((Uint32)p[2] << 16) | ((Uint32)p[3] << 24); }

/* The staging tool decodes/resamples WAV/OGG/MP3 and tracker music on the host.
 * Validate the wire format here, so arbitrary compressed bytes never reach SPU2.
 */
static FILE *RFWave(const char *path, long *start, Uint32 *bytes)
{
    FILE *f = fopen(path, "rb");
    Uint8 header[16];
    int formatOK = 0;
    if (!f) return NULL;
    if (fread(header, 1, 12, f) != 12 || memcmp(header, "RIFF", 4) || memcmp(header + 8, "WAVE", 4))
        goto fail;
    for (int i = 0; i < 128; ++i)
    {
        if (fread(header, 1, 8, f) != 8) break;
        Uint32 size = RFRead32(header + 4);
        long pos = ftell(f);
        if (pos < 0 || size > (Uint32)(LONG_MAX - pos - 1)) break;
        if (!memcmp(header, "fmt ", 4))
        {
            if (size < 16 || fread(header, 1, 16, f) != 16) break;
            formatOK = RFRead16(header) == 1 && RFRead16(header + 2) == 2 &&
                RFRead32(header + 4) == RFAUDS2_OUTPUT_RATE &&
                RFRead16(header + 12) == 4 && RFRead16(header + 14) == 16;
        }
        else if (!memcmp(header, "data", 4) && formatOK && size && !(size % 4))
        {
            if (fseek(f, 0, SEEK_END) != 0 || ftell(f) < pos + (long)size) break;
            *start = pos; *bytes = size;
            if (fseek(f, pos, SEEK_SET) != 0) break;
            return f;
        }
        if (fseek(f, pos + (long)size + (size & 1u), SEEK_SET) != 0) break;
    }
fail:
    fclose(f);
    return NULL;
}
static char *RFPath(const char *path, long *start, Uint32 *bytes)
{
    FILE *f = RFWave(path, start, bytes);
    if (f) { fclose(f); return SDL_strdup(path); }
    size_t len = strlen(path);
    char *sidecar = SDL_malloc(len + 5);
    if (!sidecar) return NULL;
    memcpy(sidecar, path, len);
    memcpy(sidecar + len, ".pcm", 5);
    f = RFWave(sidecar, start, bytes);
    if (f) { fclose(f); return sidecar; }
    SDL_free(sidecar);
    SDL_SetError("PS2 audio needs 48000 Hz stereo S16 RIFF PCM: %s (run stage.py --audio)", path);
    return NULL;
}
static int RFUsed(const Mix_Chunk *chunk)
{
    for (int i = 0; i < voiceCount; ++i) if (voices[i].chunk == chunk) return 1;
    return 0;
}
static int RFCache(Mix_Chunk *chunk)
{
    if (chunk->abuf) return 1;
    if (!chunk->path || chunk->alen > RF_CACHE_BYTES) return 0;
    while (cacheBytes + chunk->alen > RF_CACHE_BYTES)
    {
        Mix_Chunk *oldest = NULL;
        Uint32 now = SDL_GetTicks();
        for (Mix_Chunk *p = chunks; p; p = p->next)
            if (p->abuf && p->allocated && !RFUsed(p) &&
                (!oldest || now - p->lastUse > now - oldest->lastUse)) oldest = p;
        if (!oldest) return 0;
        SDL_free(oldest->abuf); oldest->abuf = NULL;
        cacheBytes -= oldest->alen;
    }
    long start; Uint32 bytes;
    FILE *f = RFWave(chunk->path, &start, &bytes);
    if (!f || bytes != chunk->alen) { if (f) fclose(f); return 0; }
    chunk->abuf = SDL_malloc(bytes);
    if (!chunk->abuf) { fclose(f); return 0; }
    size_t read = fread(chunk->abuf, 1, bytes, f);
    fclose(f);
    if (read != bytes) { SDL_free(chunk->abuf); chunk->abuf = NULL; return 0; }
    cacheBytes += bytes;
    return 1;
}
static void RFFinish(int i)
{
    RFVoice *v = &voices[i];
    Mix_EffectDone_t done = v->done;
    void *user = v->user;
    int hadVoice = v->chunk != NULL;
    v->chunk = NULL; v->cursor = 0; v->paused = 0;
    v->effect = NULL; v->done = NULL; v->user = NULL;
    if (done) done(i, user);
    if (hadVoice && finished) finished(i);
}
static int RFVolume(int value) { return value < 0 ? 0 : (value > MIX_MAX_VOLUME ? MIX_MAX_VOLUME : value); }

int Mix_OpenAudioDevice(int frequency, Uint16 format, int channels, int size, const char *device, int changes)
{
    (void)size; (void)device; (void)changes;
    if (frequency != (int)RFAUDS2_OUTPUT_RATE || format != AUDIO_S16SYS || channels != 2)
        return SDL_SetError("RFAuds2 frontend requires 48000 Hz stereo S16");
    if (opened) Mix_CloseAudio();
    if (inflight) return SDL_SetError("RFAuds2 RPC has not completed");
    int result = moduleLoaded ? rfauds2_bind() : rfauds2_init(rfauds2_irx, rfauds2_irx_size);
    if (result < 0) return SDL_SetError("rfauds2_init failed: %d", result);
    moduleLoaded = 1;
    if (rfauds2_stop() < 0 || rfauds2_set_latency_ms(64) < 0 ||
        rfauds2_set_volume(RFAUDS2_VOLUME_MAX) < 0)
        return SDL_SetError("RFAuds2 device configuration failed");
    opened = 1; started = 0; pendingFrames = pendingOffset = 0;
    snapshotTick = SDL_GetTicks();
    printf("PS2: RFAuds2 PCM output enabled (no audsrv/SDL audio)\n");
    return 0;
}
int Mix_QuerySpec(int *frequency, Uint16 *format, int *channels)
{
    if (!opened) return 0;
    if (frequency) *frequency = RFAUDS2_OUTPUT_RATE;
    if (format) *format = AUDIO_S16SYS;
    if (channels) *channels = 2;
    return 1;
}
int Mix_Init(int flags) { (void)flags; return 0; }
void Mix_Quit(void) {}
int Mix_AllocateChannels(int count)
{
    if (count < 0) return voiceCount;
    if (count > RF_CHANNELS) count = RF_CHANNELS;
    for (int i = count; i < voiceCount; ++i) RFFinish(i);
    for (int i = voiceCount; i < count; ++i)
    {
        memset(&voices[i], 0, sizeof voices[i]);
        voices[i].volume = MIX_MAX_VOLUME;
        voices[i].left = voices[i].right = 255;
    }
    voiceCount = count;
    return count;
}
Mix_Chunk *Mix_LoadWAV(const char *path)
{
    long start; Uint32 bytes;
    char *resolved = RFPath(path, &start, &bytes);
    if (!resolved) return NULL;
    Mix_Chunk *chunk = SDL_calloc(1, sizeof *chunk);
    if (!chunk) { SDL_free(resolved); return NULL; }
    chunk->allocated = 1; chunk->path = resolved; chunk->alen = bytes;
    chunk->volume = MIX_MAX_VOLUME; chunk->next = chunks; chunks = chunk;
    return chunk;
}
Mix_Chunk *Mix_QuickLoad_RAW(Uint8 *data, Uint32 len)
{
    if (!data || !len || len % 4) return NULL;
    Mix_Chunk *chunk = SDL_calloc(1, sizeof *chunk);
    if (!chunk) return NULL;
    chunk->abuf = data; chunk->alen = len; chunk->volume = MIX_MAX_VOLUME;
    return chunk;
}
void Mix_FreeChunk(Mix_Chunk *chunk)
{
    if (!chunk) return;
    for (int i = 0; i < voiceCount; ++i) if (voices[i].chunk == chunk) RFFinish(i);
    if (chunk->allocated)
    {
        Mix_Chunk **p = &chunks;
        while (*p && *p != chunk) p = &(*p)->next;
        if (*p) *p = chunk->next;
        if (chunk->abuf) { cacheBytes -= chunk->alen; SDL_free(chunk->abuf); }
    }
    SDL_free(chunk->path); SDL_free(chunk);
}
int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
    if (!opened || !chunk || !RFCache(chunk)) return -1;
    if (channel == -1)
        for (int i = 0; i < voiceCount; ++i) if (!voices[i].chunk) { channel = i; break; }
    if (channel < 0 || channel >= voiceCount) return -1;
    RFFinish(channel);
    RFVoice *v = &voices[channel];
    v->chunk = chunk; v->loops = loops; v->cursor = 0;
    v->left = v->right = 255; v->distance = 0;
    chunk->lastUse = SDL_GetTicks();
    return channel;
}
int Mix_Playing(int channel)
{
    if (channel >= 0 && channel < voiceCount) return voices[channel].chunk != NULL;
    if (channel != -1) return 0;
    int count = 0;
    for (int i = 0; i < voiceCount; ++i) count += voices[i].chunk != NULL;
    return count;
}
int Mix_HaltChannel(int channel)
{
    if (channel == -1) { for (int i = 0; i < voiceCount; ++i) RFFinish(i); }
    else if (channel >= 0 && channel < voiceCount) RFFinish(channel);
    else return -1;
    return 0;
}
void Mix_Pause(int channel)
{ for (int i = 0; i < voiceCount; ++i) if (channel == -1 || i == channel) voices[i].paused = 1; }
void Mix_Resume(int channel)
{ for (int i = 0; i < voiceCount; ++i) if (channel == -1 || i == channel) voices[i].paused = 0; }
void Mix_ChannelFinished(void (*callback)(int)) { finished = callback; }
int Mix_Volume(int channel, int volume)
{
    int previous = 0, count = 0;
    for (int i = 0; i < voiceCount; ++i)
        if (channel == -1 || channel == i)
        {
            previous += voices[i].volume; ++count;
            if (volume >= 0) voices[i].volume = RFVolume(volume);
        }
    return count ? previous / count : 0;
}
int Mix_SetPanning(int channel, Uint8 left, Uint8 right)
{
    if (channel < 0 || channel >= voiceCount) return 0;
    voices[channel].left = left; voices[channel].right = right;
    return 1;
}
int Mix_SetPosition(int channel, Sint16 angle, Uint8 distance)
{
    if (channel < 0 || channel >= voiceCount) return 0;
    float pan = sinf((float)angle * (3.14159265f / 180.0f));
    voices[channel].left = (Uint8)(255.0f * (pan > 0 ? 1.0f - pan : 1.0f));
    voices[channel].right = (Uint8)(255.0f * (pan < 0 ? 1.0f + pan : 1.0f));
    voices[channel].distance = distance;
    return 1;
}
int Mix_RegisterEffect(int channel, Mix_EffectFunc_t effect, Mix_EffectDone_t done, void *user)
{
    if (channel < 0 || channel >= voiceCount) return 0;
    voices[channel].effect = effect; voices[channel].done = done; voices[channel].user = user;
    return 1;
}
Mix_Music *Mix_LoadMUS(const char *path)
{
    long start; Uint32 bytes;
    char *resolved = RFPath(path, &start, &bytes);
    if (!resolved) return NULL;
    Mix_Music *m = SDL_malloc(sizeof *m);
    if (!m) { SDL_free(resolved); return NULL; }
    m->path = resolved; m->start = start; m->bytes = bytes;
    return m;
}
Mix_Music *Mix_LoadMUS_RW(SDL_RWops *rw, int freeRW)
{
    if (freeRW && rw) SDL_RWclose(rw);
    SDL_SetError("PS2 in-memory compressed/AdLib music is not implemented");
    return NULL;
}
int Mix_HaltMusic(void)
{
    if (songFile) fclose(songFile);
    songFile = NULL; song = NULL; songPaused = 0; songCursor = 0;
    return 0;
}
void Mix_FreeMusic(Mix_Music *m)
{ if (m) { if (song == m) Mix_HaltMusic(); SDL_free(m->path); SDL_free(m); } }
int Mix_PlayMusic(Mix_Music *m, int loops)
{
    if (!opened || !m) return -1;
    Mix_HaltMusic();
    songFile = fopen(m->path, "rb");
    if (!songFile) return -1;
    if (fseek(songFile, m->start, SEEK_SET)) { Mix_HaltMusic(); return -1; }
    song = m; songLoops = loops; songCursor = 0; songPaused = 0;
    return 0;
}
int Mix_PlayingMusic(void) { return song != NULL; }
int Mix_PausedMusic(void) { return song != NULL && songPaused; }
void Mix_PauseMusic(void) { songPaused = 1; }
void Mix_ResumeMusic(void) { songPaused = 0; }
int Mix_VolumeMusic(int volume)
{ int previous = musicVolume; if (volume >= 0) musicVolume = RFVolume(volume); return previous; }

static void RFMixBlock(void)
{
    memset(accumulator, 0, sizeof accumulator);
    for (int i = 0; i < voiceCount; ++i)
    {
        RFVoice *v = &voices[i];
        if (!v->chunk || v->paused) continue;
        memset(scratch, 0, sizeof scratch);
        const s16 *source = (const s16 *)v->chunk->abuf;
        Uint32 total = v->chunk->alen / 4;
        int ended = 0;
        for (u32 frame = 0; frame < RF_BLOCK; ++frame)
        {
            if (v->cursor >= total)
            {
                if (!v->loops) { ended = 1; break; }
                if (v->loops > 0) --v->loops;
                v->cursor = 0;
            }
            scratch[frame * 2] = source[v->cursor * 2];
            scratch[frame * 2 + 1] = source[v->cursor * 2 + 1];
            ++v->cursor;
        }
        if (v->cursor == total && !v->loops) ended = 1;
        if (v->effect) v->effect(i, scratch, sizeof scratch, v->user);
        s32 gain = (s32)(((s64)v->volume * v->chunk->volume * (255 - v->distance) * 32768) /
            (MIX_MAX_VOLUME * MIX_MAX_VOLUME * 255));
        s32 left = gain * v->left / 255, right = gain * v->right / 255;
        for (u32 frame = 0; frame < RF_BLOCK; ++frame)
        {
            accumulator[frame * 2] += (scratch[frame * 2] * left) >> 15;
            accumulator[frame * 2 + 1] += (scratch[frame * 2 + 1] * right) >> 15;
        }
        if (ended) RFFinish(i);
    }
    for (u32 i = 0; i < RF_BLOCK * 2; ++i)
        pending[i] = (s16)(accumulator[i] > 32767 ? 32767 : (accumulator[i] < -32768 ? -32768 : accumulator[i]));
    memset(scratch, 0, sizeof scratch);
    u32 offset = 0;
    while (song && !songPaused && offset < RF_BLOCK)
    {
        Uint32 remaining = (song->bytes - songCursor) / 4;
        if (!remaining)
        {
            if (!songLoops) { Mix_HaltMusic(); break; }
            if (songLoops > 0) --songLoops;
            songCursor = 0;
            if (fseek(songFile, song->start, SEEK_SET)) { Mix_HaltMusic(); break; }
            continue;
        }
        u32 count = RF_BLOCK - offset;
        if (count > remaining) count = remaining;
        size_t read = fread(scratch + offset * 2, 4, count, songFile);
        songCursor += (Uint32)read * 4; offset += (u32)read;
        if (read != count) { Mix_HaltMusic(); break; }
    }
    rfauds2_mix_s16(pending, scratch, RF_BLOCK * 2, musicVolume * 32768 / MIX_MAX_VOLUME);
    pendingFrames = RF_BLOCK; pendingOffset = 0;
}

void CDogsPS2AudioPump(void)
{
    if (!opened) return;
    if (inflight)
    {
        u32 accepted = 0;
        int result = rfauds2_submit_poll(&accepted);
        if (!result) return;
        inflight = 0;
        if (result < 0 || accepted > inflightFrames)
        {
            fprintf(stderr, "PS2: RFAuds2 submit failed (%d), disabling audio\n", result);
            opened = 0;
            rfauds2_stop();
            return;
        }
        /* Never advance by the requested size: zero/partial acceptance is legal. */
        pendingOffset += accepted;
        snapshotTick = SDL_GetTicks();
    }
    rfauds2_stats stats;
    memset(&stats, 0, sizeof stats);
    rfauds2_get_cached_stats(&stats);
    if (!started && stats.queued_frames >= RF_BLOCK * 2)
    {
        if (rfauds2_start() < 0) { opened = 0; rfauds2_stop(); return; }
        started = 1; snapshotTick = SDL_GetTicks();
    }
    Uint32 elapsed = started ? SDL_GetTicks() - snapshotTick : 0;
    Uint32 drained = elapsed > 1000 ? stats.queued_frames : elapsed * 48;
    Uint32 estimated = stats.queued_frames > drained ? stats.queued_frames - drained : 0;
    if (estimated >= RF_BLOCK * 3) return;
    if (pendingOffset == pendingFrames) RFMixBlock();
    inflightFrames = pendingFrames - pendingOffset;
    int result = rfauds2_submit_s16_async(pending + pendingOffset * 2, inflightFrames);
    if (result == 0) inflight = 1;
    else if (result != RFAUDS2_ERROR_BUSY)
    { fprintf(stderr, "PS2: RFAuds2 RPC launch failed (%d)\n", result); opened = 0; rfauds2_stop(); }
}
void Mix_CloseAudio(void)
{
    Uint32 deadline = SDL_GetTicks() + 1000;
    while (inflight)
    {
        u32 accepted;
        if (rfauds2_submit_poll(&accepted) != 0) { inflight = 0; break; }
        if ((Sint32)(SDL_GetTicks() - deadline) >= 0) break;
        SDL_Delay(1);
    }
    Mix_HaltChannel(-1); Mix_HaltMusic();
    if (!inflight) rfauds2_stop();
    opened = started = 0;
    pendingFrames = pendingOffset = 0;
}
