#include "ui/App.h"

#include "editor/ProjectIO.h"
#include "model/Operations.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>

namespace tessera {

void App::init(GLFWwindow* window) {
    window_ = window;
    rt.init();
    ui::applyTheme();
    newProject(true);
}

void App::shutdown() { rt.shutdown(); }

void App::setStatus(const std::string& text) {
    ed.status = text;
    ed.statusTime = (float)ImGui::GetTime();
}

// ── Model changes ──────────────────────────────────────────
void App::afterModelChange() {
    ++revision;
    rt.collect(doc);
    const Model& m = doc.model;
    if (m.kindOf(ed.selection) == Kind::None) ed.selection = {};
    std::erase_if(ed.openGraphs, [&](const Uid& g) { return !m.graphs.contains(g); });
    if (!m.folders.contains(ed.selectedFolder) && m.project()) ed.selectedFolder = m.project()->rootFolderUid;
    for (auto it = ed.shaderBuffers.begin(); it != ed.shaderBuffers.end();)
        it = m.shaders.contains(it->first) ? std::next(it) : ed.shaderBuffers.erase(it);
}

void App::commit(const std::string& label) {
    history.commit(doc, ed, label);
    afterModelChange();
}

void App::undo() {
    std::string label = history.undoLabel();
    if (history.undo(doc, ed)) {
        afterModelChange();
        setStatus("Undo " + label);
    }
}

void App::redo() {
    if (history.redo(doc, ed)) {
        afterModelChange();
        setStatus("Redo " + history.undoLabel());
    }
}

void App::deleteSelection() {
    if (ed.selection.is_nil()) return;
    std::string name = doc.model.displayName(ed.selection);
    std::string why;
    if (ops::deleteEntity(doc, ed.selection, &why)) {
        ed.selection = {};
        commit("Delete " + name);
    } else {
        setStatus(why);
    }
}

// ── Projects ───────────────────────────────────────────────
void App::newProject(bool demo) {
    rt.collect(Document{});   // release everything keyed by the old model
    ed = EditorState{};
    if (demo) {
        doc = makeDemoProject(ed);
    } else {
        doc = ops::newProject("Untitled");
        ed.selectedFolder = doc.model.project()->rootFolderUid;
    }
    history.reset(doc, ed);
    afterModelChange();
}

void App::openProject(const std::string& path) {
    Document loaded;
    EditorState loadedEd;
    std::string error;
    if (!loadProject(path, loaded, loadedEd, error)) {
        setStatus(error);
        return;
    }
    rt.collect(Document{});
    doc = std::move(loaded);
    ed = std::move(loadedEd);
    history.reset(doc, ed);
    afterModelChange();
    setStatus(error.empty() ? "Opened " + ed.projectPath : error);
}

void App::saveProject(bool saveAs) {
    auto save = [this](const std::string& dir) {
        std::string error;
        if (tessera::saveProject(doc, ed, dir, error)) {
            ed.projectPath = dir;
            history.markSaved();
            setStatus("Saved " + dir);
        } else {
            setStatus(error);
        }
    };
    if (!saveAs && !ed.projectPath.empty()) {
        save(ed.projectPath);
        return;
    }
    std::string name = doc.model.project() ? doc.model.project()->name : "Untitled";
    fileDialog.open("Save project as", FileDialog::Mode::ChooseFolder, {}, save, name);
}

// ── Frame ──────────────────────────────────────────────────
void App::runtimeFrame(float dt) {
    rt.beginFrame();
    playback.update(doc, ed, dt);
    output = rt.composite(doc, ed.playhead, ed.outputWidth, ed.outputHeight);
    // Evaluate the open graph for node previews, at its clip time when the
    // timeline is playing it.
    if (doc.model.graphs.contains(ed.activeGraph)) {
        auto it = rt.activeGraphTimes.find(ed.activeGraph);
        rt.evaluateGraph(doc, ed.activeGraph, it != rt.activeGraphTimes.end() ? it->second : ed.playhead, false);
    }
    Uid cue = playback.cueCheck(doc, ed, rt);
    if (!cue.is_nil()) setStatus("Cue fired: " + doc.model.displayName(cue));
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void App::frame(float dt) {
    runtimeFrame(dt);

    drawMenuBar();
    drawDockspace();
    ui::drawAssetsPanel(*this);
    ui::drawGraphEditor(*this);
    ui::drawOutputMonitor(*this);
    ui::drawInspector(*this);
    ui::drawDataModelView(*this);
    ui::drawTimeline(*this);
    fileDialog.draw();
    handleShortcuts();
    updateTitle();
}

void App::handleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    bool ctrl = io.KeyCtrl || io.KeySuper;
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) saveProject(io.KeyShift);
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false))
        fileDialog.open("Open project", FileDialog::Mode::OpenFile, {".tessera"}, [this](const std::string& p) { openProject(p); });
    if (io.WantTextInput || ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) return;
    if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) {
        if (ed.playState == PlayState::Paused) playback.play(ed);
        else playback.pause(ed);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) deleteSelection();
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (io.KeyShift) redo();
        else undo();
    }
    if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) redo();
}

void App::updateTitle() {
    std::string title = "Tessera - " + (doc.model.project() ? doc.model.project()->name : std::string("Untitled"));
    if (history.dirty()) title += " *";
    if (title != lastTitle_ && window_) {
        glfwSetWindowTitle(window_, title.c_str());
        lastTitle_ = title;
    }
}

void App::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;
    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("New project")) newProject(false);
        if (ImGui::MenuItem("New demo project")) newProject(true);
        if (ImGui::MenuItem("Open...", "Ctrl+O"))
            fileDialog.open("Open project", FileDialog::Mode::OpenFile, {".tessera"}, [this](const std::string& p) { openProject(p); });
        if (ImGui::MenuItem("Save", "Ctrl+S")) saveProject(false);
        if (ImGui::MenuItem("Save as...", "Ctrl+Shift+S")) saveProject(true);
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) quitRequested = true;
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
        std::string undoText = "Undo " + history.undoLabel(), redoText = "Redo " + history.redoLabel();
        if (ImGui::MenuItem(undoText.c_str(), "Ctrl+Z", false, history.canUndo())) undo();
        if (ImGui::MenuItem(redoText.c_str(), "Ctrl+Shift+Z", false, history.canRedo())) redo();
        ImGui::Separator();
        if (ImGui::MenuItem("Delete selection", "Del", false, !ed.selection.is_nil())) deleteSelection();
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View")) {
        if (ImGui::MenuItem("Reset layout")) resetLayout = true;
        ImGui::EndMenu();
    }
    // Status on the right of the menu bar.
    bool fresh = !ed.status.empty() && ImGui::GetTime() - ed.statusTime < 6.0;
    std::string right = fresh ? ed.status + "    " : std::string();
    char fps[32];
    std::snprintf(fps, sizeof fps, "%.0f fps", ImGui::GetIO().Framerate);
    right += fps;
    float w = ImGui::CalcTextSize(right.c_str()).x;
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - w - 16);
    ImGui::TextDisabled("%s", right.c_str());
    ImGui::EndMainMenuBar();
}

void App::drawDockspace() {
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGuiID dock = ImGui::GetID("TesseraDockspace");
    if (!ImGui::DockBuilderGetNode(dock) || resetLayout) {
        resetLayout = false;
        ImGui::DockBuilderRemoveNode(dock);
        ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dock, vp->WorkSize);
        ImGuiID left, right, bottom, center, centerRight;
        ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.18f, &left, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, &right, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.34f, &bottom, &center);
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.42f, &centerRight, &center);
        ImGui::DockBuilderDockWindow("Assets", left);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Data model", right);
        ImGui::DockBuilderDockWindow("Timeline", bottom);
        ImGui::DockBuilderDockWindow("Graph editor", center);
        ImGui::DockBuilderDockWindow("Output", centerRight);
        ImGui::DockBuilderFinish(dock);
    }
    ImGui::DockSpaceOverViewport(dock, vp);
}

// ── Drag and drop ──────────────────────────────────────────
namespace ui {

void setUidPayload(const Uid& uid, const std::string& label) {
    std::string text = toString(uid);
    ImGui::SetDragDropPayload(kUidPayload, text.c_str(), text.size() + 1);
    ImGui::TextUnformatted(label.c_str());
}

Uid acceptUidPayload() {
    if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload(kUidPayload))
        return uidFromString(std::string((const char*)p->Data));
    return {};
}

} // namespace ui

} // namespace tessera
