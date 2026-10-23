// Inspector (architecture section 8): edits the selected entity with the
// parameter modules. Shaders open in a code editor with compile errors.

#include "model/Declarations.h"
#include "model/Operations.h"
#include "model/ShaderFormat.h"
#include "runtime/Playback.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace tessera::ui {

namespace {

// Field editors that commit when the edit is finished.
void commitIfDone(App& app, const char* what) {
    if (ImGui::IsItemDeactivatedAfterEdit()) app.commit(std::string("Edit ") + what);
}

void editFloat(App& app, const char* label, float& v, float speed, float mn, float mx, const char* fmt = "%.2f") {
    ImGui::DragFloat(label, &v, speed, mn, mx, fmt);
    commitIfDone(app, label);
}

void editText(App& app, const char* label, std::string& s) {
    ImGui::InputText(label, &s);
    commitIfDone(app, label);
}

bool editBlend(App& app, const char* label, BlendMode& mode) {
    int i = (int)mode;
    if (ImGui::Combo(label, &i, kBlendModeNames, 5)) {
        mode = (BlendMode)i;
        app.commit(std::string("Edit ") + label);
        return true;
    }
    return false;
}

void refButton(App& app, const char* label, const Uid& target) {
    ImGui::TextDisabled("%s", label);
    ImGui::SameLine();
    if (target.is_nil()) {
        ImGui::TextDisabled("null");
        return;
    }
    std::string name = app.doc.model.displayName(target) + "##" + label;
    if (ImGui::SmallButton(name.c_str())) app.select(target);
}

void header(const char* kind, const std::string& name, ImU32 color) {
    ImGui::TextColored(toVec4(color), "%s", kind);
    ImGui::SameLine();
    ImGui::TextUnformatted(name.c_str());
    ImGui::Separator();
}

void shaderEditor(App& app, const Uid& shaderUid) {
    Model& m = app.doc.model;
    Shader* s = m.shaders.find(shaderUid);
    const std::string* source = shaderSource(app.doc, shaderUid);
    bool builtin = isBuiltinPath(s->path);
    const ShaderInfo* info = shaderInfo(app.doc, shaderUid);

    ImGui::TextDisabled("Path");
    ImGui::SameLine();
    ImGui::TextUnformatted(s->path.c_str());
    if (info && info->ok) {
        ImGui::TextDisabled("Role");
        ImGui::SameLine();
        ImGui::Text("%s  ·  category %s  ·  %d inputs", toString(info->role), info->category.c_str(), (int)info->inputs.size());
    }
    if (builtin) {
        ImGui::TextDisabled("Built-in shaders are read-only.");
        if (ImGui::Button("Copy into project")) {
            Uid copy = ops::copyShaderIntoProject(app.doc, shaderUid, app.ed.selectedFolder);
            app.select(copy);
            app.commit("Copy shader into project");
            return;
        }
    }

    std::string& buffer = app.ed.shaderBuffers[shaderUid];
    if (buffer.empty() && source) buffer = *source;
    bool modified = source && buffer != *source;

    bool compile = false;
    if (!builtin) {
        compile = ImGui::Button("Compile");
        ImGui::SameLine();
        ImGui::TextDisabled(modified ? "Ctrl+Enter to compile · modified" : "Ctrl+Enter to compile");
    }

    std::string error = app.rt.shaderError(app.doc, shaderUid);
    float wrap = ImGui::GetContentRegionAvail().x - 16;
    float errorHeight = error.empty() ? 0 : std::min(160.0f, ImGui::CalcTextSize(error.c_str(), nullptr, false, wrap).y + 16);
    ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | (builtin ? ImGuiInputTextFlags_ReadOnly : 0);
    ImGui::InputTextMultiline("##source", &buffer, ImVec2(-1, std::max(160.0f, ImGui::GetContentRegionAvail().y - errorHeight - 8)), flags);
    bool editorFocused = ImGui::IsItemActive() || ImGui::IsItemFocused();
    ImGuiIO& io = ImGui::GetIO();
    if (!builtin && (io.KeyCtrl || io.KeySuper) && ImGui::IsKeyPressed(ImGuiKey_Enter, false) &&
        (editorFocused || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)))
        compile = true;

    if (compile) {
        std::string why;
        bool ok = ops::setShaderSource(app.doc, shaderUid, buffer, &why);
        app.commit("Compile " + s->label);
        app.setStatus(ok ? "Compiled " + app.doc.model.shaders.find(shaderUid)->label : why);
        error = app.rt.shaderError(app.doc, shaderUid);
    }

    if (!error.empty()) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.25f, 0.08f, 0.08f, 1));
        ImGui::BeginChild("errors", ImVec2(-1, errorHeight), ImGuiChildFlags_Borders);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 0.6f, 0.6f, 1));
        ImGui::TextWrapped("%s", error.c_str());
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
}

void inspectNode(App& app, const Uid& uid) {
    Model& m = app.doc.model;
    const Node* n = m.nodes.find(uid);
    const NodeType* t = m.nodeTypes.find(n->typeUid);
    header("Node", t ? t->label : "?", kGraphBlue);
    refButton(app, "Graph", n->graphUid);
    refButton(app, "Type", n->typeUid);
    if (t && !t->backingUid.is_nil()) refButton(app, "Backed by", t->backingUid);
    ImGui::Spacing();
    ImGui::SeparatorText("Parameters");
    drawParams(app, uid);
    if (t && t->kind == "shader") {
        std::string err = app.rt.shaderError(app.doc, t->backingUid);
        if (!err.empty()) ImGui::TextColored(ImVec4(1, 0.5f, 0.5f, 1), "Shader error - select the shader to edit it.");
    }
}

void inspectLink(App& app, const Uid& uid) {
    Model& m = app.doc.model;
    const Link* l = m.links.find(uid);
    const Converter* c = m.converters.find(l->converterUid);
    header("Link", c ? c->label : "?", kConverterPurple);
    refButton(app, "From", l->sourceParam);
    refButton(app, "To", l->targetParam);
    refButton(app, "Converter", l->converterUid);
    ImGui::SeparatorText("Converter settings");
    drawParams(app, uid);
}

void inspectClip(App& app, const Uid& uid) {
    Model& m = app.doc.model;
    Clip* c = m.clips.find(uid);
    Kind kind = m.kindOf(c->sourceUid);
    header(kind == Kind::Shader ? "Effect clip" : "Clip", m.displayName(c->sourceUid), kTimelinePink);
    refButton(app, "Source", c->sourceUid);
    refButton(app, "Track", c->trackUid);
    editFloat(app, "start", c->start, 0.05f, 0, 3600, "%.2f s");
    editFloat(app, "duration", c->duration, 0.05f, 0.25f, 3600, "%.2f s");
    int mode = c->startMode == kStartModeContinuous ? 1 : 0;
    const char* modes[] = {kStartModeFromClip, kStartModeContinuous};
    if (ImGui::Combo("startMode", &mode, modes, 2)) {
        c->startMode = modes[mode];
        app.commit("Edit startMode");
        c = m.clips.find(uid);
    }
    editBlend(app, "blendMode", c->blendMode);
    editFloat(app, "opacity", c->opacity, 0.01f, 0, 1);
    editFloat(app, "speed", c->speed, 0.01f, -8, 8);
    editFloat(app, "scale", c->scale, 0.01f, 0.05f, 8);
    editFloat(app, "rotation", c->rotation, 0.5f, -360, 360, "%.1f deg");

    if (kind == Kind::Shader) {
        ImGui::SeparatorText("Effect clip settings");
        drawParams(app, uid);
        return;
    }
    ImGui::SeparatorText("Effects");
    std::vector<const Effect*> chain;
    for (const Effect& e : m.effects)
        if (e.ownerUid == uid) chain.push_back(&e);
    std::sort(chain.begin(), chain.end(), [](const Effect* a, const Effect* b) { return a->order < b->order; });
    std::vector<Uid> chainUids;
    for (const Effect* e : chain) chainUids.push_back(e->uid);
    for (const Uid& e : chainUids) {
        ImGui::PushID(toString(e).c_str());
        bool open = ImGui::TreeNodeEx("fx", ImGuiTreeNodeFlags_DefaultOpen, "%s", m.displayName(e).c_str());
        ImGui::SameLine(ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX() - 70);
        bool changed = false;
        if (ImGui::SmallButton("^")) { ops::moveEffect(app.doc, e, -1); app.commit("Reorder effects"); changed = true; }
        ImGui::SameLine();
        if (!changed && ImGui::SmallButton("v")) { ops::moveEffect(app.doc, e, 1); app.commit("Reorder effects"); changed = true; }
        ImGui::SameLine();
        if (!changed && ImGui::SmallButton("x")) { ops::deleteEntity(app.doc, e); app.commit("Remove effect"); changed = true; }
        if (open) {
            if (!changed) drawParams(app, e);
            ImGui::TreePop();
        }
        ImGui::PopID();
        if (changed) break;
    }
    if (ImGui::BeginCombo("##addfx", "Add effect...")) {
        for (const Shader& s : m.shaders)
            if (isFilterShader(app.doc, s.uid) && ImGui::Selectable(s.label.c_str())) {
                Uid shader = s.uid;
                ops::addEffect(app.doc, uid, shader);
                app.commit("Add effect");
                break;
            }
        ImGui::EndCombo();
    }
    ImGui::TextDisabled("Or drop a filter shader on the clip.");
}

void inspectMarker(App& app, const Uid& uid) {
    Model& m = app.doc.model;
    Marker* mk = m.markers.find(uid);
    Kind sub = m.markerKind(uid);
    header(sub == Kind::CuePoint ? "Cue point" : sub == Kind::HoldPoint ? "Hold point" : "Marker", m.displayName(uid), kParamAmber);
    editFloat(app, "position", mk->position, 0.05f, 0, 3600, "%.2f s");
    drawParams(app, uid);
    if (CuePoint* cue = m.cuePoints.find(uid)) {
        ImGui::SeparatorText("Cue");
        std::string current = cue->parameterUid.is_nil() ? "(none)" : m.displayName(cue->parameterUid);
        if (ImGui::BeginCombo("watches", current.c_str())) {
            for (const Parameter& p : m.params) {
                if (m.kindOf(p.ownerUid) != Kind::Node) continue;
                ParamType t = p.paramType();
                if (t == ParamType::Texture || t == ParamType::Any || t == ParamType::Label) continue;
                std::string label = m.displayName(p.uid) + (p.isOutput() ? "  (out)" : "") + "##" + toString(p.uid);
                if (ImGui::Selectable(label.c_str(), p.uid == cue->parameterUid)) {
                    cue->parameterUid = p.uid;
                    app.commit("Watch parameter");
                    break;
                }
            }
            ImGui::EndCombo();
        }
        cue = m.cuePoints.find(uid);
        if (cue) editFloat(app, "condition", cue->condition, 0.01f, -10, 10);
        ImGui::TextDisabled("Jumps the playhead here when the value rises\nabove the condition, even while paused.");
    }
    if (HoldPoint* hold = m.holdPoints.find(uid)) {
        ImGui::SeparatorText("Hold");
        editFloat(app, "duration", hold->duration, 0.05f, 0, 600, "%.2f s");
    }
}

} // namespace

void drawInspector(App& app) {
    if (!ImGui::Begin("Inspector")) {
        ImGui::End();
        return;
    }
    Model& m = app.doc.model;
    Uid sel = app.ed.selection;
    switch (m.kindOf(sel)) {
        case Kind::None:
            ImGui::TextDisabled("Select a node, link, clip, marker or asset.");
            break;
        case Kind::Node: inspectNode(app, sel); break;
        case Kind::Link: inspectLink(app, sel); break;
        case Kind::Clip: inspectClip(app, sel); break;
        case Kind::Marker: inspectMarker(app, sel); break;
        case Kind::Graph: {
            Graph* g = m.graphs.find(sel);
            header("Graph", g->name, kGraphBlue);
            editText(app, "name", g->name);
            g = m.graphs.find(sel);
            float res[2] = {g->resolution.x, g->resolution.y};
            if (ImGui::DragFloat2("resolution", res, 1, 16, 4096, "%.0f")) g->resolution = {res[0], res[1]};
            commitIfDone(app, "resolution");
            if (ImGui::Button("Open in graph editor")) app.ed.openGraph(sel);
            break;
        }
        case Kind::Shader: {
            Shader* s = m.shaders.find(sel);
            header("Shader", s->label, kRuntimeTeal);
            shaderEditor(app, sel);
            break;
        }
        case Kind::Media: {
            Media* md = m.media.find(sel);
            header("Media", m.displayName(sel), kAssetGreen);
            ImGui::TextDisabled("Path");
            ImGui::SameLine();
            ImGui::TextWrapped("%s", md->path.c_str());
            Kind sub = m.mediaKind(sel);
            ImGui::Text("Kind: %s", toString(sub));
            if (const Image* img = m.images.find(sel)) ImGui::Text("Size: %d × %d", img->width, img->height);
            if (Video* v = m.videos.find(sel)) editFloat(app, "duration", v->duration, 0.1f, 0, 36000, "%.1f s");
            if (Audio* a = m.audios.find(sel)) editFloat(app, "duration", a->duration, 0.1f, 0, 36000, "%.1f s");
            if (sub != Kind::Image) ImGui::TextDisabled("No decoder in the prototype: shows a placeholder.");
            else if (!app.doc.files.count(md->path) && !isBuiltinPath(md->path))
                ImGui::TextDisabled("External file (over 1.5 MB, not copied into the project).");
            TexRef t = app.rt.mediaTexture(app.doc, sel);
            if (t.tex) {
                float w = ImGui::GetContentRegionAvail().x;
                ImGui::Image((ImTextureID)(intptr_t)t.tex, ImVec2(w, w * (float)t.h / (float)t.w), ImVec2(0, 1), ImVec2(1, 0));
            }
            break;
        }
        case Kind::Track: {
            Track* t = m.tracks.find(sel);
            header("Track", m.displayName(sel), kTimelinePink);
            editFloat(app, "opacity", t->opacity, 0.01f, 0, 1);
            editBlend(app, "blendMode", m.tracks.find(sel)->blendMode);
            ImGui::TextDisabled("Order %d (a higher order is drawn on top)", m.tracks.find(sel)->order);
            break;
        }
        case Kind::Loop: {
            Loop* l = m.loops.find(sel);
            header("Loop", "", kTimelinePink);
            editFloat(app, "start", l->start, 0.05f, 0, 3600, "%.2f s");
            editFloat(app, "end", m.loops.find(sel)->end, 0.05f, 0, 3600, "%.2f s");
            ImGui::Checkbox("Loop playback", &app.ed.loopEnabled);
            break;
        }
        case Kind::Folder: {
            Folder* f = m.folders.find(sel);
            header("Folder", f->label, kAssetGreen);
            editText(app, "label", f->label);
            ImGui::TextDisabled("localPath: %s", m.folders.find(sel)->localPath.c_str());
            ImGui::TextDisabled("New files land in the selected folder.");
            break;
        }
        case Kind::Effect: {
            const Effect* e = m.effects.find(sel);
            header("Effect", m.displayName(sel), kTimelinePink);
            refButton(app, "Clip", e->ownerUid);
            refButton(app, "Shader", e->shaderUid);
            drawParams(app, sel);
            break;
        }
        case Kind::Parameter: {
            const Parameter* p = m.params.find(sel);
            header("Parameter", p->name, kParamAmber);
            refButton(app, "Owner", p->ownerUid);
            ImGui::Text("type %s · %s · %s", p->type.c_str(), p->direction.c_str(), p->linkable ? "linkable" : "not linkable");
            if (const LiveValue* v = app.rt.liveValue(sel)) ImGui::Text("Live value: %s", describeLive(*v, nullptr).c_str());
            break;
        }
        case Kind::Project: {
            Project* p = m.projects.find(sel);
            header("Project", p->name, kAssetGreen);
            editText(app, "name", p->name);
            break;
        }
        default: {
            Kind k = m.kindOf(sel);
            header(toString(k), m.displayName(sel), kMuted);
            ImGui::TextDisabled("See the Data model panel for its fields.");
            break;
        }
    }
    ImGui::End();
}

} // namespace tessera::ui
