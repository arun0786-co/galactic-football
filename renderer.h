// section 7: renderer module
// handling all the opengl draw calls.
// this separates the visual presentation from the physics and game logic.

#ifndef RENDERER_H
#define RENDERER_H

extern int g_camera_mode;

#include "math_utils.h"

void init_renderer();
void render_frame();

const Matrix4f& get_view_matrix();
const Matrix4f& get_proj_matrix();

#endif // RENDERER_H
