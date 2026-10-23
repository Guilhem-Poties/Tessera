// Data model view (architecture section 8): the raw tables, the selected
// record, what it points to and what references it, and the project as JSON.

#include "model/Serialize.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

namespace tessera::ui {

namespace {

std::string cellText(const json& v) {
    if (v.is_string()) return v.get<std::string>();
    if (v.is_null()) return "null";
    return v.dump();
}

// Shortens uids in table cells; the full value shows in the tooltip.
std::string shortCell(const json& v) {
    std::string s = cellText(v);
    if (v.is_string() && s.size() == 36 && s[8] == '-' && s[13] == '-') return s.substr(0, 8);
    return s;
}

template <class T>
void drawTable(App& app, const Table<T>& table) {
    std::vector<std::string> columns;
    const T sample{};
    T::fields(sample, [&](const char* name, const auto&) { columns.push_back(name); });
    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX |
                            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit;
    if (!ImGui::BeginTable("rows", (int)columns.size(), flags)) return;
    ImGui::TableSetupScrollFreeze(1, 1);
    for (const std::string& c : columns) ImGui::TableSetupColumn(c.c_str());
    ImGui::TableHeadersRow();
    ImGuiListClipper clipper;
    clipper.Begin((int)table.size());
    while (clipper.Step()) {
        for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
            const T& row = table.rows[(size_t)i];
            json j = recordToJson(row);
            ImGui::TableNextRow();
            int col = 0;
            for (const std::string& c : columns) {
                ImGui::TableSetColumnIndex(col++);
                std::string text = shortCell(j[c]);
                if (c == "uid") {
                    std::string label = text + "##" + std::to_string(i);
                    if (ImGui::Selectable(label.c_str(), app.ed.selection == row.uid, ImGuiSelectableFlags_SpanAllColumns))
                        app.select(row.uid);
                } else {
                    ImGui::TextUnformatted(text.c_str());
                    if (ImGui::IsItemHovered() && text != cellText(j[c])) ImGui::SetTooltip("%s", cellText(j[c]).c_str());
                }
            }
        }
    }
    ImGui::EndTable();
}

void drawRecord(App& app) {
    const Model& m = app.doc.model;
    Uid sel = app.ed.selection;
    if (m.kindOf(sel) == Kind::None) {
        ImGui::TextDisabled("Nothing selected.");
        return;
    }
    // Every table holding this uid: "is a" subtypes share the parent's uid.
    Model::forEachTable(m, [&](Kind, const char* name, const auto& table) {
        if (const auto* r = table.find(sel)) {
            ImGui::SeparatorText(name);
            std::string text = recordToJson(*r).dump(2);
            ImGui::TextUnformatted(text.c_str());
        }
    });
    auto refList = [&](const char* title, const std::vector<Reference>& refs, bool outgoing) {
        ImGui::SeparatorText(title);
        if (refs.empty()) ImGui::TextDisabled("nothing");
        int i = 0;
        for (const Reference& r : refs) {
            Uid other = outgoing ? r.target : r.fromUid;
            Kind kind = outgoing ? m.kindOf(other) : r.fromKind;
            std::string label = std::string(toString(kind)) + "  " + m.displayName(other) + "   (" + r.field + ")##" +
                                std::to_string(i++);
            if (ImGui::Selectable(label.c_str())) app.select(other);
        }
    };
    refList("Points to", referencesFrom(m, sel), true);
    refList("Referenced by", referencesTo(m, sel), false);
}

} // namespace

void drawDataModelView(App& app) {
    if (!ImGui::Begin("Data model")) {
        ImGui::End();
        return;
    }
    const Model& m = app.doc.model;
    if (ImGui::BeginTabBar("dm")) {
        if (ImGui::BeginTabItem("Tables")) {
            static std::string current = "Node";
            if (ImGui::BeginCombo("Table", current.c_str())) {
                Model::forEachTable(m, [&](Kind, const char* name, const auto& table) {
                    std::string label = std::string(name) + "  (" + std::to_string(table.size()) + ")";
                    if (ImGui::Selectable(label.c_str(), current == name)) current = name;
                });
                ImGui::EndCombo();
            }
            Model::forEachTable(m, [&](Kind, const char* name, const auto& table) {
                if (current == name) drawTable(app, table);
            });
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Record")) {
            ImGui::BeginChild("record");
            drawRecord(app);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("JSON")) {
            static unsigned builtFor = ~0u;
            static std::string text;
            if (builtFor != app.revision) {
                text = modelToJson(m).dump(2);
                builtFor = app.revision;
            }
            if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(text.c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("%zu bytes · refreshed on every commit", text.size());
            ImGui::InputTextMultiline("##json", &text, ImVec2(-1, -1), ImGuiInputTextFlags_ReadOnly);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace tessera::ui
