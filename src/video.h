#ifndef VIDEO_H
#define VIDEO_H

#include "common.h"

extern uint32_t g_framebuffer[SCREEN_W * SCREEN_H];
extern BITMAPINFO g_bmi;
extern bool g_hires_mode;

void video_init(void);
void video_render_frame(void);

#endif /* VIDEO_H */
