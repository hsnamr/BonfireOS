/**
 * Red Alert host API for BonfireOS
 *
 * Interface for porting Command & Conquer: Red Alert
 * (https://github.com/electronicarts/CnC_Red_Alert) to BonfireOS. The original
 * uses DirectX 5, DirectX Media 5.1, Greenleaf Communications Library (GCL/IPX),
 * and Human Machine Interface "Sound Operating System" (SOS). This host provides
 * equivalent abstractions so the port uses only these APIs and standard C.
 *
 * When the user runs the REDALERT command, the kernel switches to graphics
 * mode, calls redalert_main(), then returns to text mode on exit.
 */

#ifndef BONFIRE_REDALERT_HOST_H
#define BONFIRE_REDALERT_HOST_H

#include <kernel/types.h>

/* ---------------------------------------------------------------------------
 * Video (VGA mode 13h: 320x200, 256 colors — same as DOOM; sufficient for
 * classic Red Alert. Higher resolutions can be added later via another mode.)
 * --------------------------------------------------------------------------- */
#define REDALERT_SCREEN_WIDTH  320
#define REDALERT_SCREEN_HEIGHT 200
#define REDALERT_SCREEN_BPP    8

void redalert_video_enter(void);
uint8_t *redalert_video_framebuffer(void);
void redalert_video_set_palette(const uint8_t *rgb768);
void redalert_video_leave(void);

/* ---------------------------------------------------------------------------
 * Input (keyboard + mouse)
 * --------------------------------------------------------------------------- */
int redalert_input_get_key(uint8_t *key, int *down);
void redalert_input_mouse(int *dx, int *dy, int *buttons);
void redalert_input_clear(void);

/* ---------------------------------------------------------------------------
 * Time
 * --------------------------------------------------------------------------- */
uint32_t redalert_time_ms(void);
void redalert_time_delay_ms(uint32_t ms);

/* ---------------------------------------------------------------------------
 * Memory (dedicated heap for Red Alert; MIX loading, etc.)
 * --------------------------------------------------------------------------- */
void *redalert_malloc(size_t size);
void redalert_free(void *ptr);
void *redalert_realloc(void *ptr, size_t new_size);

/* ---------------------------------------------------------------------------
 * File I/O (POSIX-style; for MIX files, INI, save games)
 * --------------------------------------------------------------------------- */
int redalert_open(const char *path, int flags);
int redalert_read(int fd, void *buf, size_t count);
int redalert_write(int fd, const void *buf, size_t count);
int redalert_close(int fd);
int64_t redalert_lseek(int fd, int64_t offset, int whence);
/* Flags: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT; SEEK_SET, SEEK_CUR, SEEK_END */

/* ---------------------------------------------------------------------------
 * Audio (SOS replacement — stub implementation; no sound until driver exists)
 * --------------------------------------------------------------------------- */
#define REDALERT_AUDIO_MAX_VOICES  8
#define REDALERT_AUDIO_SAMPLE_RATE 22050

int redalert_audio_init(void);
void redalert_audio_shutdown(void);
/* Play PCM: 8-bit mono, sample_rate (e.g. 22050). voice 0..REDALERT_AUDIO_MAX_VOICES-1. */
int redalert_audio_play(int voice, const uint8_t *data, size_t len, uint32_t sample_rate);
void redalert_audio_stop(int voice);
void redalert_audio_stop_all(void);

/* ---------------------------------------------------------------------------
 * Network (GCL / IPX replacement — stub implementation; no network until
 * driver exists. Red Alert used IPX for multiplayer.)
 * --------------------------------------------------------------------------- */
#define REDALERT_NET_MAX_PEERS  8
#define REDALERT_NET_PACKET_MAX 512

int redalert_net_init(void);
void redalert_net_shutdown(void);
int redalert_net_broadcast(const void *data, size_t len);
int redalert_net_send(uint8_t peer_id, const void *data, size_t len);
int redalert_net_receive(void *buf, size_t max_len, uint8_t *from_peer_id);
int redalert_net_peer_count(void);
void redalert_net_get_peer_address(uint8_t peer_id, char *out, size_t out_size);

/* ---------------------------------------------------------------------------
 * Entry point (Red Alert port provides redalert_main; kernel calls it from
 * REDALERT command)
 * --------------------------------------------------------------------------- */
int redalert_main(int argc, char **argv);

#endif /* BONFIRE_REDALERT_HOST_H */
