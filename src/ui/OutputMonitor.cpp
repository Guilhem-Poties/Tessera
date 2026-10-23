// Output monitor (architecture section 8): the composited timeline at the
// playhead.

#include "ui/App.h"

#include <imgui.h>

namespace tessera::ui {

void drawOutputMonitor(App& app) {
    if (!ImGui::Begin("Output")) {
        ImGui::End();
        return;
    }
    static const int sizes[][2] = {{640, 360}, {1280, 720}, {1920, 1080}};
    char current[32];
    std::snprintf(current, sizeof current, "%d × %d", app.ed.outputWidth, app.ed.outputHeight);
    ImGui::TextUnformatted("Output");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(130);
    if (ImGui::BeginCombo("##res", current)) {
        for (const auto& s : sizes) {
            char label[32];
            std::snprintf(label, sizeof label, "%d × %d", s[0], s[1]);
            if (ImGui::Selectable(label, s[0] == app.ed.outputWidth)) {
                app.ed.outputWidth = s[0];
                app.ed.outputHeight = s[1];
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("%d passes", app.rt.passes);

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float aspect = (float)app.ed.outputWidth / (float)app.ed.outputHeight;
    ImVec2 size(avail.x, avail.x / aspect);
    if (size.y > avail.y) size = ImVec2(avail.y * aspect, avail.y);
    if (size.x > 1 && size.y > 1 && app.output.tex) {
        ImVec2 p = ImGui::GetCursorScreenPos();
        p.x += (avail.x - size.x) * 0.5f;
        ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), IM_COL32(0, 0, 0, 255));
        ImGui::SetCursorScreenPos(p);
        ImGui::Image((ImTextureID)(intptr_t)app.output.tex, size, ImVec2(0, 1), ImVec2(1, 0));
    }
    ImGui::End();
}

} // namespace tessera::ui
