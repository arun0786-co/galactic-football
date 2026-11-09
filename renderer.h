// section 7: renderer module
// handling all the opengl draw calls.
// this separates the visual presentation from the physics and game logic.

#ifndef RENDERER_H
#define RENDERER_H

extern int g_camera_mode;

#include "math_utils.h"

void init_renderer();
void render_frame();

// offscreen captures draw at a fixed size without the hud
extern int g_render_w, g_render_h; // 0 = use the window's framebuffer size
extern bool g_draw_hud;

const Matrix4f& get_view_matrix();
const Matrix4f& get_proj_matrix();

#endif // RENDERER_H
