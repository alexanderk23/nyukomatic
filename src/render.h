#pragma once

#include <GL/glew.h>
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#ifdef IMGUI_IMPL_OPENGL_ES2
#include <GLES2/gl2.h>
#endif
#include <GLFW/glfw3.h> // Will drag system OpenGL headers


namespace nm::render {

template <typename T>
union vec4 {
    struct { T x, y, z, w; };
    struct { T r, g, b, a; };
    struct { T left, top, right, bottom; };
};

extern vec4<GLint> borderOffsets;
extern vec4<GLfloat> transform;
extern vec4<GLfloat> bgColor;

void setBorderOffsets(int x1, int y1, int x2, int y2);
void setTransform(vec4<GLfloat> xform);

// Upload pixels into texture
void uploadEmulatorImage(char *image, GLuint width, GLuint height);
void renderEmulatorImage();

} // namespace nm::render
