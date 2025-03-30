#include "render.h"
#include "logger.h"

namespace nm::render {

const char *vertexShaderSource = R"##(#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
uniform vec4 transform;
out vec2 uv;
void main() {
    gl_Position = transform * vec4(aPos, 0.0, 1.0);
    uv = aTexCoord;
})##";

const char *fragmentShaderSource = R"##(#version 330 core
in vec2 uv;
uniform sampler2D textureSampler;
uniform ivec4 borderOffsets;
out vec4 fragColor;
void main() {
    ivec2 texSize = textureSize(textureSampler, 0);
    vec2 pixel = (uv * (texSize - borderOffsets.xy - borderOffsets.zw)) + borderOffsets.xy;
    vec2 seam = floor(pixel + 0.5);
    vec2 dudv = fwidth(pixel);
    pixel = seam + clamp((pixel - seam) / dudv, -0.5, 0.5);
    fragColor = texture(textureSampler, pixel / texSize);
})##";

// clang-format off
const float quadVertices[] = {
    // pos         // tex
    -1.0f, -1.0f,  0.0f, 1.0f,
    -1.0f,  1.0f,  0.0f, 0.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
    -1.0f, -1.0f,  0.0f, 1.0f,
     1.0f,  1.0f,  1.0f, 0.0f,
     1.0f, -1.0f,  1.0f, 1.0f
};
// clang-format on

GLuint shaderProgram = GL_NONE;
GLuint imageTexture = GL_NONE;
GLuint vao = GL_NONE, vbo = GL_NONE;
GLint hBorderOffsets;
GLint hTransform;

vec4<GLint> borderOffsets{0, 0, 0, 0};
vec4<GLfloat> transform{1.0f, 1.0f, 1.0f, 1.0f};
vec4<GLfloat> bgColor{0.125f, 0.125f, 0.125f, 1.0f};

GLuint compileShader(const GLenum type, const char *shaderSource) {
    const GLuint shader = glCreateShader(type);
    const GLint len = strlen(shaderSource);
    glShaderSource(shader, 1, &shaderSource, &len);
    glCompileShader(shader);

    // check for shader compile errors
    int success;
    char infoLog[512];
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(shader, 512, NULL, infoLog);
        LOG_E << "shader compilation failed: " << infoLog << std::endl;
        glDeleteShader(shader);

        return GL_NONE;
    }

    return shader;
}

GLuint buildShaderProgram(const char *vertexShaderSource,
                          const char *fragmentShaderSource) {
    GLuint vertexShader, fragmentShader;
    GLuint shaderProgram = glCreateProgram();

    // vertex shader
    if (vertexShaderSource != nullptr) {
        vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSource);
        glAttachShader(shaderProgram, vertexShader);
    }

    // fragment shader
    if (fragmentShaderSource != nullptr) {
        fragmentShader =
            compileShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
        glAttachShader(shaderProgram, fragmentShader);
    }

    glLinkProgram(shaderProgram);

    // check for linking errors
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        LOG_E << "shader linking failed: " << infoLog << std::endl;
    }

    glDetachShader(shaderProgram, fragmentShader);
    glDetachShader(shaderProgram, vertexShader);

    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);

    return shaderProgram;
}

void setBorderOffsets(int x1, int y1, int x2, int y2) {
    borderOffsets = {x1, y1, x2, y2};
}

void setTransform(vec4<GLfloat> xform) {
    transform = xform;
}

void renderEmulatorImage() {
    glClearColor(bgColor.r, bgColor.g, bgColor.b, bgColor.a);
    glClear(GL_COLOR_BUFFER_BIT);

    if (imageTexture == 0)
        return;

    if (vao == 0) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices,
                     GL_STATIC_DRAW);

        if (shaderProgram == GL_NONE)
            shaderProgram =
                buildShaderProgram(vertexShaderSource, fragmentShaderSource);

        hBorderOffsets = glGetUniformLocation(shaderProgram, "borderOffsets");
        hTransform = glGetUniformLocation(shaderProgram, "transform");
    } else {
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
    }

    glUseProgram(shaderProgram);
    glUniform4iv(hBorderOffsets, 1, (const GLint *)&borderOffsets);
    glUniform4fv(hTransform, 1, (const GLfloat *)&transform);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          (void *)(2 * sizeof(float)));

    glBindTexture(GL_TEXTURE_2D, imageTexture);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindTexture(GL_TEXTURE_2D, GL_NONE);

    glUseProgram(GL_NONE);
    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glBindBuffer(GL_ARRAY_BUFFER, GL_NONE);
    glBindVertexArray(GL_NONE);
}

// Upload pixels into texture
void uploadEmulatorImage(char *image, GLuint width, GLuint height) {
    if (imageTexture == 0) {
        glGenTextures(1, &imageTexture);
        glBindTexture(GL_TEXTURE_2D, imageTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    } else {
        glBindTexture(GL_TEXTURE_2D, imageTexture);
    }

#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_BGRA,
                 GL_UNSIGNED_BYTE, image);

    // bgColor.r = *(uint8_t *)(image + 2) / 255.0f * 0.25f;
    // bgColor.g = *(uint8_t *)(image + 1) / 255.0f * 0.25f;
    // bgColor.b = *(uint8_t *)(image + 0) / 255.0f * 0.25f;
}

} // namespace nm::render
