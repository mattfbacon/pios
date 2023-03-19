// The driver always requests a 1920x1080 buffer.
// It might vary, though.
// All the drawing functions check bounds so there won't be UB if the screen is smaller than expected.

#pragma once

// `0xRRGGBBAA`.
typedef u32 framebuffer_color_t;

void framebuffer_init(void);
void framebuffer_draw_pixel(unsigned int x, unsigned int y, framebuffer_color_t color);
framebuffer_color_t framebuffer_current(unsigned int x, unsigned int y);
void framebuffer_fill(framebuffer_color_t color);
