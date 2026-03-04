/**
 * Red Alert audio host (SOS replacement).
 * Stub implementation: no sound driver in BonfireOS yet.
 * Port can call these; they no-op so linking succeeds.
 * Replace with real PCM/PC speaker or driver when available.
 */

#include <kernel/redalert_host.h>
#include <kernel/types.h>

static bool audio_inited;

int redalert_audio_init_impl(void)
{
    audio_inited = true;
    return 0; /* success */
}

void redalert_audio_shutdown_impl(void)
{
    audio_inited = false;
}

int redalert_audio_play_impl(int voice, const uint8_t *data, size_t len, uint32_t sample_rate)
{
    (void)voice;
    (void)data;
    (void)len;
    (void)sample_rate;
    if (voice < 0 || voice >= REDALERT_AUDIO_MAX_VOICES) return -1;
    return 0; /* accepted, no-op */
}

void redalert_audio_stop_impl(int voice)
{
    (void)voice;
}

void redalert_audio_stop_all_impl(void)
{
}
