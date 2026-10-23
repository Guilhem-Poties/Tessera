// Tessera — node-based VJing interface.
//
//   tessera [--open <project dir or project.tessera>] [--time <seconds>] [--play]
//           [--screenshot <file.png> [--frames <n>]] [--size <w>x<h>]

#include "ui/App.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <stb_image_write.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

struct Options {
    std::string open;
    std::string screenshot;
    int frames = 30;
    int width = 1600, height = 900;
    float time = -1;
    bool play = false;
};

Options parse(int argc, char** argv) {
    Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&]() -> std::string { return i + 1 < argc ? argv[++i] : ""; };
        if (a == "--open") o.open = next();
        else if (a == "--screenshot") o.screenshot = next();
        else if (a == "--frames") o.frames = std::atoi(next().c_str());
        else if (a == "--time") o.time = (float)std::atof(next().c_str());
        else if (a == "--play") o.play = true;
        else if (a == "--size") std::sscanf(next().c_str(), "%dx%d", &o.width, &o.height);
    }
    return o;
}

void saveScreenshot(GLFWwindow* win, const std::string& path) {
    int w, h;
    glfwGetFramebufferSize(win, &w, &h);
    std::vector<unsigned char> px((size_t)w * h * 4);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    stbi_flip_vertically_on_write(1);
    if (stbi_write_png(path.c_str(), w, h, 4, px.data(), w * 4)) std::printf("Saved %s (%dx%d)\n", path.c_str(), w, h);
    else std::fprintf(stderr, "Can't write %s\n", path.c_str());
}

} // namespace

int main(int argc, char** argv) {
    Options opt = parse(argc, argv);

    glfwSetErrorCallback([](int code, const char* msg) { std::fprintf(stderr, "GLFW %d: %s\n", code, msg); });
    if (!glfwInit()) return 1;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif
    GLFWwindow* win = glfwCreateWindow(opt.width, opt.height, "Tessera", nullptr, nullptr);
    if (!win) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(win);
    glfwSwapInterval(1);
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::fprintf(stderr, "Failed to load OpenGL\n");
        return 1;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    if (!opt.screenshot.empty()) io.IniFilename = nullptr;   // reproducible layout
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    tessera::App app;
    app.init(win);
    if (!opt.open.empty()) app.openProject(opt.open);
    if (opt.time >= 0) app.ed.playhead = opt.time;
    if (opt.play) app.playback.play(app.ed);

    int frame = 0;
    double last = glfwGetTime();
    while (!glfwWindowShouldClose(win) && !app.quitRequested) {
        glfwPollEvents();
        double now = glfwGetTime();
        float dt = (float)(now - last);
        last = now;
        if (!opt.screenshot.empty()) dt = 1.0f / 60.0f;   // deterministic captures

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        app.frame(dt);
        ImGui::Render();

        int fw, fh;
        glfwGetFramebufferSize(win, &fw, &fh);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, fw, fh);
        glClearColor(0.08f, 0.09f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (!opt.screenshot.empty() && ++frame >= opt.frames) {
            saveScreenshot(win, opt.screenshot);
            break;
        }
        glfwSwapBuffers(win);
    }

    app.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}
