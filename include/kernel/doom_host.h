/**
 * DOOM host API for BonfireOS
 *
 * This is the interface a DOOM port (e.g. from id-Software/DOOM linuxdoom-1.10)
 * must use to run on BonfireOS. Implementations are in the kernel; the port
 * links against these and does not contain OS-specific code.
 *
 * BonfireOS remains a CLI OS; when the user runs the DOOM command, the kernel
 * switches to graphics mode for the game then back to text mode on exit.
 *
 * Reference: https://github.com/id-Software/DOOM
 */

#ifndef BONFIRE_DOOM_HOST_H
#define BONFIRE_DOOM_HOST_H

#include <kernel/types.h>

/* ---------------------------------------------------------------------------
 * Video (VGA mode 13h: 320x200, 256 colors, linear framebuffer at 0xA0000)
 * --------------------------------------------------------------------------- */
#define DOOM_SCREEN_WIDTH  320
#define DOOM_SCREEN_HEIGHT 200
#define DOOM_SCREEN_BPP    8

/* Enter graphics mode; framebuffer is DOOM_SCREEN_WIDTH * DOOM_SCREEN_HEIGHT bytes. */
void doom_video_enter(void);
/* Return linear framebuffer (write pixels here). */
uint8_t *doom_video_framebuffer(void);
/* Set palette: 256 entries, each 3 bytes R,G,B (0-63). */
void doom_video_set_palette(const uint8_t *rgb768);
/* Leave graphics mode and return to VGA text mode. */
void doom_video_leave(void);

/* ---------------------------------------------------------------------------
 * Input (keyboard + mouse)
 * --------------------------------------------------------------------------- */
/* Non-blocking: get next key event. Returns 0 if none. key=scancode, down=1/0. */
int doom_input_get_key(uint8_t *key, int *down);
/* Mouse state: dx/dy since last read, buttons (bit0=left, bit1=right, bit2=middle). */
void doom_input_mouse(int *dx, int *dy, int *buttons);
/* Clear key and mouse queues (e.g. after mode switch). */
void doom_input_clear(void);

/* ---------------------------------------------------------------------------
 * Time (for game tick ~35 Hz and timing)
 * --------------------------------------------------------------------------- */
/* Milliseconds since boot (from PIT). */
uint32_t doom_time_ms(void);
/* Block for approximately ms milliseconds. */
void doom_time_delay_ms(uint32_t ms);

/* ---------------------------------------------------------------------------
 * Memory (malloc/free for game and WAD loading)
 * --------------------------------------------------------------------------- */
void *doom_malloc(size_t size);
void  doom_free(void *ptr);
void *doom_realloc(void *ptr, size_t new_size);

/* ---------------------------------------------------------------------------
 * File I/O (POSIX-style; for WAD and config)
 * --------------------------------------------------------------------------- */
int   doom_open(const char *path, int flags);
int   doom_read(int fd, void *buf, size_t count);
int   doom_write(int fd, const void *buf, size_t count);
int   doom_close(int fd);
int64_t doom_lseek(int fd, int64_t offset, int whence);
/* Same flags as posix.h: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT; SEEK_SET, SEEK_CUR, SEEK_END */

/* ---------------------------------------------------------------------------
 * Entry point (DOOM port provides doom_main; kernel calls it from DOOM command)
 * --------------------------------------------------------------------------- */
/* Called when user types "DOOM" in the shell. argc/argv like C (argv[0] = "DOOM"). */
int doom_main(int argc, char **argv);

#endif /* BONFIRE_DOOM_HOST_H */
