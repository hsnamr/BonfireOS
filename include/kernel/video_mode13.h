#ifndef BONFIRE_VIDEO_MODE13_H
#define BONFIRE_VIDEO_MODE13_H

#include <kernel/types.h>

#define MODE13_WIDTH  320
#define MODE13_HEIGHT 200
#define MODE13_FB     0xA0000

void video_mode13_enter(void);
void video_mode13_leave(void);
uint8_t *video_mode13_framebuffer(void);
void video_mode13_set_palette(const uint8_t *rgb768);

#endif /* BONFIRE_VIDEO_MODE13_H */
