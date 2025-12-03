#ifndef RENDERER_H
#define RENDERER_H

#include "xil_types.h"

#define RENDERER_MAX_FRAME (1920*1080*3)
#define RENDERER_STRIDE (1920*3)

/*
 * Expose only the necessary API in the header
 */

//draws pixel to the current frame
void renderer_draw_pixel(u32 x, u32 y, u8 r, u8 g, u8 b);
//renders the frame to display and move to the next frame
void renderer_render();

void renderer_initialize();
void renderer_oscillate_test();
void renderer_moving_box_test();

#endif /* RENDERER_H */
