#pragma once

#include <glad/glad.h>

#include <string>

namespace tessera::gl {

// A colour render target: one texture attached to one framebuffer.
struct Fbo {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
    GLenum format = GL_RGBA8;

    void ensure(int width, int height, GLenum internalFormat = GL_RGBA8);
    void release();
    void bind() const;   // binds the framebuffer and sets the viewport
};

// Compiles and links a program. Returns 0 and fills `log` on failure.
GLuint makeProgram(const std::string& vertex, const std::string& fragment, std::string& log);

// Vertex shader of every pass: a fullscreen triangle with `vUv` in 0..1.
const char* fullscreenVertex();

// Draws the fullscreen triangle with the bound program and framebuffer.
void drawFullscreen();

void init();
void shutdown();

GLuint blackTexture();   // 1x1 transparent black, bound to unlinked texture inputs

} // namespace tessera::gl
