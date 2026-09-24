#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <cstdio>

// Fullscreen triangle, no vertex buffer needed
static const char* kVert = R"(#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
})";

// Your shader goes here (Shadertoy/ISF-like uniforms)
static const char* kFrag = R"(#version 330 core
in vec2 uv;
out vec4 fragColor;
uniform float iTime;
uniform vec2  iResolution;
void main() {
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));
    fragColor = vec4(col, 1.0);
})";

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[1024]; glGetShaderInfoLog(s, 1024, nullptr, log); fprintf(stderr, "%s\n", log); }
    return s;
}

static GLuint makeProgram(const char* vs, const char* fs) {
    GLuint p = glCreateProgram();
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glDeleteShader(v); glDeleteShader(f);
    return p;
}

struct RenderTarget {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
    void resize(int nw, int nh) {
        if (nw == w && nh == h) return;
        w = nw; h = nh;
        if (!fbo) { glGenFramebuffers(1, &fbo); glGenTextures(1, &tex); }
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(1600, 900, "Tessera", nullptr, nullptr);
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    GLuint prog = makeProgram(kVert, kFrag);
    GLuint vao; glGenVertexArrays(1, &vao);   // core profile requires a bound VAO
    RenderTarget rt;

    while (!glfwWindowShouldClose(win)) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::DockSpaceOverViewport();

        ImGui::Begin("Preview");
        ImVec2 size = ImGui::GetContentRegionAvail();
        if (size.x > 0 && size.y > 0) {
            rt.resize((int)size.x, (int)size.y);

            // Render the shader into the FBO
            glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
            glViewport(0, 0, rt.w, rt.h);
            glUseProgram(prog);
            glUniform1f(glGetUniformLocation(prog, "iTime"), (float)glfwGetTime());
            glUniform2f(glGetUniformLocation(prog, "iResolution"), (float)rt.w, (float)rt.h);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // Show it; UVs flipped because GL textures are bottom-up
            ImGui::Image((ImTextureID)(intptr_t)rt.tex, size, ImVec2(0, 1), ImVec2(1, 0));
        }
        ImGui::End();

        ImGui::Begin("Info");
        ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
        ImGui::End();

        ImGui::Render();
        int fw, fh; glfwGetFramebufferSize(win, &fw, &fh);
        glViewport(0, 0, fw, fh);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(win);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwTerminate();
}