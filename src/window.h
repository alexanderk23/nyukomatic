#pragma once

#include <GL/glew.h>
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#define GL_SILENCE_DEPRECATION
#ifdef IMGUI_IMPL_OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers

namespace nm::window {

extern GLFWwindow *window;
extern const char *glsl_version;
extern int display_w, display_h;
extern float highDPIscaleFactor;

int initWindow(int width, int height, const char *title);
void deinitWindow();
void limitFPS(double fps);

} // namespace nm::window
