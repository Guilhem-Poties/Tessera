#include "runtime/Gl.h"

#include <vector>

namespace tessera::gl {

static GLuint g_vao = 0;
static GLuint g_black = 0;

void Fbo::ensure(int width, int height, GLenum internalFormat) {
    width = width < 1 ? 1 : width;
    height = height < 1 ? 1 : height;
    if (fbo && width == w && height == h && internalFormat == format) return;
    if (!fbo) {
        glGenFramebuffers(1, &fbo);
        glGenTextures(1, &tex);
    }
    w = width;
    h = height;
    format = internalFormat;
    glBindTexture(GL_TEXTURE_2D, tex);
    GLenum type = internalFormat == GL_RGBA8 ? GL_UNSIGNED_BYTE : GL_FLOAT;
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalFormat, w, h, 0, GL_RGBA, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Fbo::release() {
    if (fbo) glDeleteFramebuffers(1, &fbo);
    if (tex) glDeleteTextures(1, &tex);
    fbo = tex = 0;
    w = h = 0;
}

void Fbo::bind() const {
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
}

static GLuint compile(GLenum type, const std::string& src, std::string& log) {
    GLuint s = glCreateShader(type);
    const char* p = src.c_str();
    glShaderSource(s, 1, &p, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf((size_t)len + 1, 0);
        glGetShaderInfoLog(s, len, nullptr, buf.data());
        log = buf.data();
        glDeleteShader(s);
        return 0;
    }
    return s;
}

GLuint makeProgram(const std::string& vertex, const std::string& fragment, std::string& log) {
    GLuint v = compile(GL_VERTEX_SHADER, vertex, log);
    if (!v) return 0;
    GLuint f = compile(GL_FRAGMENT_SHADER, fragment, log);
    if (!f) {
        glDeleteShader(v);
        return 0;
    }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        glGetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> buf((size_t)len + 1, 0);
        glGetProgramInfoLog(p, len, nullptr, buf.data());
        log = buf.data();
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

const char* fullscreenVertex() {
    return R"(#version 330 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vUv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";
}

void drawFullscreen() {
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(g_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
}

void init() {
    glGenVertexArrays(1, &g_vao);   // core profile requires a bound VAO
    unsigned char zero[4] = {0, 0, 0, 0};
    glGenTextures(1, &g_black);
    glBindTexture(GL_TEXTURE_2D, g_black);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, zero);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void shutdown() {
    glDeleteVertexArrays(1, &g_vao);
    glDeleteTextures(1, &g_black);
}

GLuint blackTexture() { return g_black; }

} // namespace tessera::gl
