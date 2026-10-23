// Assets panel (architecture section 8): project files, built-ins, graphs and
// the node palette. Items can be dragged into a graph or onto the timeline.

#include "editor/ProjectIO.h"
#include "model/Builtins.h"
#include "model/Declarations.h"
#include "model/Operations.h"
#include "runtime/Playback.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace tessera::ui {

namespace {

const char* kCategoryOrder[] = {"Generator", "Filter", "Mixer", "Value", "Converter", "Media", "Output"};

// A selectable row that can be dragged elsewhere. Returns true on double-click.
bool assetRow(App& app, const Uid& uid, const std::string& label, ImU32 dot) {
    ImGui::PushID(toString(uid).c_str());
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + 5, p.y + h * 0.5f + 1), 3.5f, dot);
    ImGui::SetCursorScreenPos(ImVec2(p.x + 14, p.y));
    bool dbl = false;
    if (ImGui::Selectable(label.c_str(), app.ed.selection == uid, ImGuiSelectableFlags_AllowDoubleClick)) {
        app.select(uid);
        dbl = ImGui::IsMouseDoubleClicked(0);
    }
    if (ImGui::BeginDragDropSource()) {
        setUidPayload(uid, label);
        ImGui::EndDragDropSource();
    }
    ImGui::PopID();
    return dbl;
}

void addNodeToActiveGraph(App& app, const Uid& asset) {
    Uid type = ops::nodeTypeForAsset(app.doc.model, asset);
    if (type.is_nil() || !app.doc.model.graphs.contains(app.ed.activeGraph)) {
        app.setStatus("Open a graph to add nodes");
        return;
    }
    Uid node = ops::addNode(app.doc, app.ed.activeGraph, type);
    const GraphView& view = app.ed.graphViews[app.ed.activeGraph];
    app.ed.nodePositions[node] = {(-view.pan.x + 60) / view.zoom, (-view.pan.y + 60) / view.zoom};
    app.select(node);
    app.commit("Add " + app.doc.model.displayName(node));
}

void folderTree(App& app, const Uid& folderUid) {
    Model& m = app.doc.model;
    const Folder* f = m.folders.find(folderUid);
    if (!f) return;
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (app.ed.selectedFolder == folderUid) flags |= ImGuiTreeNodeFlags_Selected;
    std::string label = f->label + "##" + toString(folderUid);
    bool open = ImGui::TreeNodeEx(label.c_str(), flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        app.ed.selectedFolder = folderUid;
        app.select(folderUid);
    }
    if (!open) return;
    std::vector<Uid> children;
    for (const Folder& c : m.folders)
        if (c.parentUid == folderUid) children.push_back(c.uid);
    for (const Uid& c : children) folderTree(app, c);
    for (const Shader& s : m.shaders)
        if (s.folderUid == folderUid) {
            std::string err = app.rt.shaderError(app.doc, s.uid);
            std::string label = s.label + (err.empty() ? "" : "  (!)");
            if (assetRow(app, s.uid, label, err.empty() ? kRuntimeTeal : IM_COL32(220, 80, 80, 255))) addNodeToActiveGraph(app, s.uid);
        }
    for (const Media& md : m.media)
        if (md.folderUid == folderUid)
            if (assetRow(app, md.uid, m.displayName(md.uid), kAssetGreen)) addNodeToActiveGraph(app, md.uid);
    ImGui::TreePop();
}

} // namespace

void drawAssetsPanel(App& app) {
    if (!ImGui::Begin("Assets")) {
        ImGui::End();
        return;
    }
    Model& m = app.doc.model;
    const Project* project = m.project();

    // ── Project files ──
    if (ImGui::CollapsingHeader("Project files", ImGuiTreeNodeFlags_DefaultOpen)) {
        Uid folder = app.ed.selectedFolder;
        if (ImGui::SmallButton("Import media")) {
            app.fileDialog.open("Import media", FileDialog::Mode::OpenFile,
                                {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".mp4", ".mov", ".webm", ".wav", ".mp3", ".ogg"},
                                [&app, folder](const std::string& path) {
                                    std::string error;
                                    Uid uid = importMedia(app.doc, folder, path, error);
                                    if (uid.is_nil()) return app.setStatus(error);
                                    app.select(uid);
                                    app.commit("Import " + app.doc.model.displayName(uid));
                                });
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Import shader")) {
            app.fileDialog.open("Import shader", FileDialog::Mode::OpenFile, {".tsh", ".glsl", ".frag"},
                                [&app, folder](const std::string& path) {
                                    std::string error;
                                    Uid uid = importShader(app.doc, folder, path, error);
                                    if (uid.is_nil()) return app.setStatus(error);
                                    app.select(uid);
                                    app.commit("Import " + app.doc.model.displayName(uid));
                                });
        }
        if (ImGui::SmallButton("New shader")) {
            Uid uid = ops::newShader(app.doc, folder);
            app.select(uid);
            app.commit("New shader");
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("New folder")) {
            Uid uid = ops::newFolder(app.doc, folder, "folder");
            app.ed.selectedFolder = uid;
            app.select(uid);
            app.commit("New folder");
        }
        if (project) folderTree(app, project->rootFolderUid);
    }

    // ── Built in ──
    if (ImGui::CollapsingHeader("Built in")) {
        for (const Shader& s : m.shaders)
            if (isBuiltinPath(s.path)) {
                const ShaderInfo* info = shaderInfo(app.doc, s.uid);
                std::string label = s.label + "  ·  " + (info ? info->category : "");
                if (assetRow(app, s.uid, label, kRuntimeTeal)) addNodeToActiveGraph(app, s.uid);
            }
        for (const Media& md : m.media)
            if (isBuiltinPath(md.path))
                if (assetRow(app, md.uid, m.displayName(md.uid), kAssetGreen)) addNodeToActiveGraph(app, md.uid);
    }

    // ── Graphs ──
    if (ImGui::CollapsingHeader("Graphs", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (ImGui::SmallButton("+ New graph")) {
            Uid g = ops::addGraph(app.doc, "Graph " + std::to_string(m.graphs.size() + 1));
            app.ed.openGraph(g);
            app.select(g);
            app.commit("New graph");
        }
        std::vector<Uid> graphs;
        for (const Graph& g : m.graphs) graphs.push_back(g.uid);
        for (const Uid& g : graphs) {
            if (assetRow(app, g, m.displayName(g), kGraphBlue)) app.ed.openGraph(g);
            ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 92);
            ImGui::PushID(toString(g).c_str());
            if (ImGui::SmallButton("To timeline")) {
                // Onto the lowest track that is free at the playhead, or a new track.
                Uid track;
                float t = app.ed.playhead;
                for (const Track* tr : m.tracksBottomToTop()) {
                    bool busy = false;
                    for (const Clip& c : m.clips)
                        busy |= c.trackUid == tr->uid && t < c.start + c.duration && t + 8 > c.start;
                    if (!busy) {
                        track = tr->uid;
                        break;
                    }
                }
                if (track.is_nil()) track = ops::addTrack(app.doc);
                Uid clip = ops::addClip(app.doc, track, g, t, 8);
                app.select(clip);
                app.commit("Add clip");
            }
            ImGui::PopID();
        }
    }

    // ── Node palette ──
    if (ImGui::CollapsingHeader("Node palette", ImGuiTreeNodeFlags_DefaultOpen)) {
        static std::string filter;
        ImGui::SetNextItemWidth(-1);
        ImGui::InputTextWithHint("##filter", "Filter...", &filter);
        auto matches = [&](const std::string& label) {
            if (filter.empty()) return true;
            auto lower = [](std::string s) {
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                return s;
            };
            return lower(label).find(lower(filter)) != std::string::npos;
        };
        std::vector<std::string> categories(std::begin(kCategoryOrder), std::end(kCategoryOrder));
        for (const NodeType& t : m.nodeTypes)
            if (std::find(categories.begin(), categories.end(), t.category) == categories.end()) categories.push_back(t.category);
        for (const std::string& cat : categories) {
            std::vector<const NodeType*> types;
            for (const NodeType& t : m.nodeTypes)
                if (t.category == cat && matches(t.label)) types.push_back(&t);
            if (types.empty()) continue;
            ImGui::TextColored(toVec4(categoryColor(cat)), "%s", cat.c_str());
            std::vector<Uid> uids;
            std::vector<std::string> labels;
            for (const NodeType* t : types) {
                uids.push_back(t->uid);
                labels.push_back(t->label);
            }
            for (size_t i = 0; i < uids.size(); ++i)
                if (assetRow(app, uids[i], labels[i], categoryColor(cat))) addNodeToActiveGraph(app, uids[i]);
        }
    }
    ImGui::End();
}

} // namespace tessera::ui
