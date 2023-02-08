#pragma once

typedef u32 framebuffer_color_t;

void framebuffer_init(void);
void framebuffer_draw_pixel(unsigned int x, unsigned int y, framebuffer_color_t color);
framebuffer_color_t framebuffer_current(unsigned int x, unsigned int y);
void framebuffer_fill(framebuffer_color_t color);
