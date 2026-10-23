// The ImGui half of the parameter modules (architecture section 4): each
// module draws its own widget. The same widgets are used in the inspector for
// Node, Link, Effect, effect-Clip and Marker parameters.

#include "model/ParamModules.h"

#include "model/Declarations.h"
#include "model/Operations.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace tessera {

static int finish(bool changed) {
    int r = changed ? ParamModule::Changed : 0;
    if (ImGui::IsItemDeactivatedAfterEdit()) r |= ParamModule::Commit;
    return r;
}

int FloatModule::drawWidget(const char* label, const ParamDecl& d, Value& v, bool readOnly) const {
    float f = valueAsFloat(v);
    ImGui::BeginDisabled(readOnly);
    bool changed = ImGui::SliderFloat(label, &f, d.min, d.max, "%.3f");
    int r = finish(changed);
    ImGui::EndDisabled();
    if (changed) v = f;
    return r;
}

int ChoiceModule::drawWidget(const char* label, const ParamDecl& d, Value& v, bool readOnly) const {
    int i = (int)valueAsFloat(v);
    std::string preview = describe(v, d);
    int r = None;
    ImGui::BeginDisabled(readOnly);
    if (ImGui::BeginCombo(label, preview.c_str())) {
        for (int k = 0; k < (int)d.options.size(); ++k)
            if (ImGui::Selectable(d.options[(size_t)k].c_str(), k == i) && k != i) {
                v = k;
                r = Changed | Commit;
            }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    return r;
}

int BoolModule::drawWidget(const char* label, const ParamDecl&, Value& v, bool readOnly) const {
    bool b = valueAsFloat(v) > 0.5f;
    ImGui::BeginDisabled(readOnly);
    bool changed = ImGui::Checkbox(label, &b);
    ImGui::EndDisabled();
    if (changed) v = b;
    return changed ? Changed | Commit : None;
}

int ColorModule::drawWidget(const char* label, const ParamDecl&, Value& v, bool readOnly) const {
    Color c = valueAsColor(v);
    float rgb[3] = {c.r, c.g, c.b};
    ImGui::BeginDisabled(readOnly);
    bool changed = ImGui::ColorEdit3(label, rgb, ImGuiColorEditFlags_DisplayHex);
    int r = finish(changed);
    ImGui::EndDisabled();
    if (changed) v = Color{rgb[0], rgb[1], rgb[2]};
    return r;
}

int ColorListModule::drawWidget(const char* label, const ParamDecl&, Value& v, bool readOnly) const {
    ColorList list = std::holds_alternative<ColorList>(v) ? std::get<ColorList>(v) : ColorList{};
    int r = None;
    ImGui::BeginDisabled(readOnly);
    ImGui::PushID(label);
    for (size_t i = 0; i < list.size(); ++i) {
        ImGui::PushID((int)i);
        float rgb[3] = {list[i].r, list[i].g, list[i].b};
        if (ImGui::ColorEdit3("##c", rgb, ImGuiColorEditFlags_NoInputs)) {
            list[i] = {rgb[0], rgb[1], rgb[2]};
            r |= Changed;
        }
        if (ImGui::IsItemDeactivatedAfterEdit()) r |= Commit;
        ImGui::SameLine();
        ImGui::PopID();
    }
    if (list.size() < (size_t)kMaxColors) {
        if (ImGui::SmallButton("+")) {
            list.push_back(list.empty() ? Color{1, 1, 1} : list.back());
            r |= Changed | Commit;
        }
        ImGui::SameLine();
    }
    if (list.size() > 1) {
        if (ImGui::SmallButton("-")) {
            list.pop_back();
            r |= Changed | Commit;
        }
        ImGui::SameLine();
    }
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextUnformatted(label, end);
    ImGui::PopID();
    ImGui::EndDisabled();
    if (r) v = list;
    return r;
}

int TextureModule::drawWidget(const char* label, const ParamDecl&, Value&, bool) const {
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextDisabled("%.*s  (texture port)", (int)(end - label), label);
    return None;
}

int LabelModule::drawWidget(const char* label, const ParamDecl&, Value& v, bool readOnly) const {
    std::string s = std::holds_alternative<std::string>(v) ? std::get<std::string>(v) : std::string();
    if (readOnly) {
        ImGui::Text("%s", s.c_str());
        return None;
    }
    bool changed = ImGui::InputText(label, &s);
    int r = finish(changed);
    if (changed) v = s;
    return r;
}

int AnyModule::drawWidget(const char* label, const ParamDecl&, Value&, bool) const {
    const char* end = label;
    while (*end && !(end[0] == '#' && end[1] == '#')) ++end;
    ImGui::TextDisabled("%.*s  (any: port only)", (int)(end - label), label);
    return None;
}

namespace ui {

void drawParams(App& app, const Uid& owner) {
    Model& m = app.doc.model;
    std::vector<ParamDecl> decls = ownerDecls(app.doc, owner);
    if (decls.empty()) {
        ImGui::TextDisabled("No parameters");
        return;
    }
    for (const ParamDecl& d : decls) {
        Parameter* p = m.findParam(owner, d.name);
        if (!p) continue;
        ImGui::PushID(toString(p->uid).c_str());
        ImGui::PushStyleColor(ImGuiCol_Text, toVec4(paramTypeColor(d.type)));
        ImGui::Bullet();
        ImGui::PopStyleColor();
        ImGui::SameLine();
        const LiveValue* live = app.rt.liveValue(p->uid);
        const Link* link = m.linkInto(p->uid);
        if (d.output) {
            ImGui::Text("%s", d.name.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("out: %s", live ? describeLive(*live, &d).c_str() : "-");
        } else if (link) {
            ImGui::Text("%s", d.name.c_str());
            ImGui::SameLine();
            std::string from = m.displayName(link->sourceParam);
            ImGui::TextColored(toVec4(kConverterPurple), "<- %s", from.c_str());
            if (live) {
                ImGui::SameLine();
                ImGui::TextDisabled("= %s", describeLive(*live, &d).c_str());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Link")) app.select(link->uid);
            ImGui::SameLine();
            if (ImGui::SmallButton("Unlink")) {
                ops::deleteEntity(app.doc, link->uid);
                app.commit("Unlink " + d.name);
                ImGui::PopID();
                break;   // parameters changed under us
            }
        } else {
            std::string label = d.name + "##v";
            ImGui::SetNextItemWidth(-110);
            int r = moduleFor(d.type).drawWidget(label.c_str(), d, p->value, false);
            if (r & ParamModule::Commit) app.commit("Edit " + d.name);
        }
        ImGui::PopID();
    }
}

} // namespace ui

} // namespace tessera
