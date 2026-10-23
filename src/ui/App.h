#pragma once

#include "editor/EditorState.h"
#include "editor/History.h"
#include "model/Model.h"
#include "runtime/Playback.h"
#include "runtime/Runtime.h"
#include "ui/FileDialog.h"

#include <string>

struct GLFWwindow;

namespace tessera {

// Owns the document, the editor state, the history and the runtime, and
// draws the UI. The UI edits the model through ops::, then calls commit().
class App {
public:
    Document doc;
    EditorState ed;
    History history;
    Runtime rt;
    Playback playback;
    FileDialog fileDialog;

    TexRef output;          // composited timeline at the playhead
    unsigned revision = 0;  // bumped on every commit, undo and redo
    bool resetLayout = false;
    bool quitRequested = false;

    void init(GLFWwindow* window);
    void shutdown();

    // One frame: runtime first (playback, composite, previews, cue check),
    // then the UI.
    void frame(float dt);

    // Pushes a snapshot of the model + node positions + files.
    void commit(const std::string& label);
    void undo();
    void redo();
    void afterModelChange();   // runtime GC, selection fix-up

    void select(const Uid& uid) { ed.selection = uid; }
    void deleteSelection();
    void setStatus(const std::string& text);

    void newProject(bool demo);
    void openProject(const std::string& path);
    void saveProject(bool saveAs);

private:
    void runtimeFrame(float dt);
    void drawMenuBar();
    void drawDockspace();
    void handleShortcuts();
    void updateTitle();

    GLFWwindow* window_ = nullptr;
    std::string lastTitle_;
};

namespace ui {
void applyTheme();
void drawAssetsPanel(App& app);
void drawGraphEditor(App& app);
void drawInspector(App& app);
void drawDataModelView(App& app);
void drawOutputMonitor(App& app);
void drawTimeline(App& app);

// Draws the parameters owned by `owner` with their modules; commits edits.
void drawParams(App& app, const Uid& owner);

// Drag and drop of model uids between panels.
inline constexpr const char* kUidPayload = "TESSERA_UID";
void setUidPayload(const Uid& uid, const std::string& label);
Uid acceptUidPayload();   // inside BeginDragDropTarget(); nil if nothing dropped
} // namespace ui

} // namespace tessera
