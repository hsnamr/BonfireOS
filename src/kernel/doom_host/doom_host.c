/**
 * DOOM host API implementation.
 * Wires doom_* to kernel video, input, time, memory, and POSIX file I/O.
 */

#include <kernel/doom_host.h>
#include <kernel/video_mode13.h>
#include <kernel/posix.h>
#include <kernel/types.h>

/* Video */
void doom_video_enter(void)
{
    video_mode13_enter();
}

uint8_t *doom_video_framebuffer(void)
{
    return video_mode13_framebuffer();
}

void doom_video_set_palette(const uint8_t *rgb768)
{
    video_mode13_set_palette(rgb768);
}

void doom_video_leave(void)
{
    video_mode13_leave();
}

/* Input: implemented in doom_input.c (keyboard queue + mouse) */
extern void doom_input_get_key_impl(uint8_t *key, int *down);
extern void doom_input_mouse_impl(int *dx, int *dy, int *buttons);
extern void doom_input_clear_impl(void);

void doom_input_get_key(uint8_t *key, int *down) { doom_input_get_key_impl(key, down); }
void doom_input_mouse(int *dx, int *dy, int *buttons) { doom_input_mouse_impl(dx, dy, buttons); }
void doom_input_clear(void) { doom_input_clear_impl(); }

/* Time: implemented in doom_time.c */
extern uint32_t doom_time_ms_impl(void);
extern void doom_time_delay_ms_impl(uint32_t ms);

uint32_t doom_time_ms(void) { return doom_time_ms_impl(); }
void doom_time_delay_ms(uint32_t ms) { return doom_time_delay_ms_impl(ms); }

/* Memory: doom_malloc/free/realloc in doom_malloc.c */
extern void *doom_malloc_impl(size_t size);
extern void doom_free_impl(void *ptr);
extern void *doom_realloc_impl(void *ptr, size_t new_size);

void *doom_malloc(size_t size) { return doom_malloc_impl(size); }
void doom_free(void *ptr) { doom_free_impl(ptr); }
void *doom_realloc(void *ptr, size_t new_size) { return doom_realloc_impl(ptr, new_size); }

/* File I/O: map to POSIX */
int doom_open(const char *path, int flags) { return open(path, flags); }
int doom_read(int fd, void *buf, size_t count) { return (int)read(fd, buf, count); }
int doom_write(int fd, const void *buf, size_t count) { return (int)write(fd, buf, count); }
int doom_close(int fd) { return close(fd); }
int64_t doom_lseek(int fd, int64_t offset, int whence) { return (int64_t)lseek(fd, (off_t)offset, whence); }

/* Weak stub: DOOM port overrides this. Return 0 = ok, non-zero = error. */
__attribute__((weak)) int doom_main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    return -1; /* not linked: DOOM not available */
}
