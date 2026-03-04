/**
 * Red Alert host API implementation.
 * Wires redalert_* to kernel video, input, time, memory, and POSIX file I/O.
 * Audio and network are in redalert_audio.c and redalert_net.c (stubs).
 */

#include <kernel/redalert_host.h>
#include <kernel/video_mode13.h>
#include <kernel/doom_host.h>
#include <kernel/posix.h>
#include <kernel/types.h>

/* Video: same as DOOM (VGA mode 13h) */
void redalert_video_enter(void)
{
    video_mode13_enter();
}

uint8_t *redalert_video_framebuffer(void)
{
    return video_mode13_framebuffer();
}

void redalert_video_set_palette(const uint8_t *rgb768)
{
    video_mode13_set_palette(rgb768);
}

void redalert_video_leave(void)
{
    video_mode13_leave();
}

/* Input: reuse DOOM input (keyboard + mouse) */
int redalert_input_get_key(uint8_t *key, int *down)
{
    return doom_input_get_key(key, down);
}

void redalert_input_mouse(int *dx, int *dy, int *buttons)
{
    doom_input_mouse(dx, dy, buttons);
}

void redalert_input_clear(void)
{
    doom_input_clear();
}

/* Time: reuse DOOM time (PIT) */
uint32_t redalert_time_ms(void)
{
    return doom_time_ms();
}

void redalert_time_delay_ms(uint32_t ms)
{
    doom_time_delay_ms(ms);
}

/* Memory: dedicated heap in redalert_malloc.c */
extern void *redalert_malloc_impl(size_t size);
extern void redalert_free_impl(void *ptr);
extern void *redalert_realloc_impl(void *ptr, size_t new_size);

void *redalert_malloc(size_t size) { return redalert_malloc_impl(size); }
void redalert_free(void *ptr) { redalert_free_impl(ptr); }
void *redalert_realloc(void *ptr, size_t new_size) { return redalert_realloc_impl(ptr, new_size); }

/* File I/O: POSIX */
int redalert_open(const char *path, int flags) { return open(path, flags); }
int redalert_read(int fd, void *buf, size_t count) { return (int)read(fd, buf, count); }
int redalert_write(int fd, const void *buf, size_t count) { return (int)write(fd, buf, count); }
int redalert_close(int fd) { return close(fd); }
int64_t redalert_lseek(int fd, int64_t offset, int whence) { return (int64_t)lseek(fd, (off_t)offset, whence); }

/* Audio: implemented in redalert_audio.c (stub) */
extern int redalert_audio_init_impl(void);
extern void redalert_audio_shutdown_impl(void);
extern int redalert_audio_play_impl(int voice, const uint8_t *data, size_t len, uint32_t sample_rate);
extern void redalert_audio_stop_impl(int voice);
extern void redalert_audio_stop_all_impl(void);

int redalert_audio_init(void) { return redalert_audio_init_impl(); }
void redalert_audio_shutdown(void) { redalert_audio_shutdown_impl(); }
int redalert_audio_play(int voice, const uint8_t *data, size_t len, uint32_t sample_rate) {
    return redalert_audio_play_impl(voice, data, len, sample_rate);
}
void redalert_audio_stop(int voice) { redalert_audio_stop_impl(voice); }
void redalert_audio_stop_all(void) { redalert_audio_stop_all_impl(); }

/* Network: implemented in redalert_net.c (stub) */
extern int redalert_net_init_impl(void);
extern void redalert_net_shutdown_impl(void);
extern int redalert_net_broadcast_impl(const void *data, size_t len);
extern int redalert_net_send_impl(uint8_t peer_id, const void *data, size_t len);
extern int redalert_net_receive_impl(void *buf, size_t max_len, uint8_t *from_peer_id);
extern int redalert_net_peer_count_impl(void);
extern void redalert_net_get_peer_address_impl(uint8_t peer_id, char *out, size_t out_size);

int redalert_net_init(void) { return redalert_net_init_impl(); }
void redalert_net_shutdown(void) { redalert_net_shutdown_impl(); }
int redalert_net_broadcast(const void *data, size_t len) { return redalert_net_broadcast_impl(data, len); }
int redalert_net_send(uint8_t peer_id, const void *data, size_t len) { return redalert_net_send_impl(peer_id, data, len); }
int redalert_net_receive(void *buf, size_t max_len, uint8_t *from_peer_id) {
    return redalert_net_receive_impl(buf, max_len, from_peer_id);
}
int redalert_net_peer_count(void) { return redalert_net_peer_count_impl(); }
void redalert_net_get_peer_address(uint8_t peer_id, char *out, size_t out_size) {
    redalert_net_get_peer_address_impl(peer_id, out, out_size);
}

/* Weak stub: Red Alert port overrides this */
__attribute__((weak)) int redalert_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1; /* not linked: Red Alert not available */
}
