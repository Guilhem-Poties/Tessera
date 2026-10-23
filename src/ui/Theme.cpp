#include "ui/Theme.h"

#include "ui/App.h"

namespace tessera::ui {

ImU32 categoryColor(const std::string& c) {
    if (c == "Generator") return kRuntimeTeal;
    if (c == "Filter") return kGraphBlue;
    if (c == "Mixer") return kConverterPurple;
    if (c == "Value") return kParamAmber;
    if (c == "Converter") return IM_COL32(0x6c, 0x5a, 0x9e, 255);
    if (c == "Media") return kAssetGreen;
    if (c == "Output") return IM_COL32(0x5d, 0x66, 0x78, 255);
    return kMuted;
}

ImU32 paramTypeColor(ParamType t) {
    switch (t) {
        case ParamType::Float: return kParamAmber;
        case ParamType::Choice: return IM_COL32(0xe0, 0x7a, 0x3a, 255);
        case ParamType::Bool: return IM_COL32(0xd0, 0x50, 0x50, 255);
        case ParamType::Color: return kTimelinePink;
        case ParamType::ColorList: return IM_COL32(0xc0, 0x60, 0xc8, 255);
        case ParamType::Texture: return kRuntimeTeal;
        default: return kMuted;
    }
}

ImVec4 toVec4(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }

void applyTheme() {
    ImGui::StyleColorsDark();
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 4;
    s.FrameRounding = 3;
    s.GrabRounding = 3;
    s.TabRounding = 3;
    s.WindowPadding = ImVec2(8, 8);
    s.FramePadding = ImVec2(6, 3);
    s.ItemSpacing = ImVec2(6, 5);
    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.115f, 0.135f, 1);
    c[ImGuiCol_ChildBg] = ImVec4(0.09f, 0.10f, 0.12f, 1);
    c[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.13f, 0.16f, 0.98f);
    c[ImGuiCol_FrameBg] = ImVec4(0.17f, 0.19f, 0.23f, 1);
    c[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.25f, 0.30f, 1);
    c[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.29f, 0.36f, 1);
    c[ImGuiCol_TitleBg] = ImVec4(0.09f, 0.10f, 0.12f, 1);
    c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.14f, 0.17f, 1);
    c[ImGuiCol_MenuBarBg] = ImVec4(0.09f, 0.10f, 0.12f, 1);
    c[ImGuiCol_Header] = ImVec4(0.18f, 0.37f, 0.62f, 0.45f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.18f, 0.37f, 0.62f, 0.70f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.18f, 0.37f, 0.62f, 0.90f);
    c[ImGuiCol_Button] = ImVec4(0.20f, 0.23f, 0.28f, 1);
    c[ImGuiCol_ButtonHovered] = ImVec4(0.18f, 0.37f, 0.62f, 0.85f);
    c[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.37f, 0.62f, 1);
    c[ImGuiCol_Tab] = ImVec4(0.13f, 0.15f, 0.18f, 1);
    c[ImGuiCol_TabHovered] = ImVec4(0.18f, 0.37f, 0.62f, 0.8f);
    c[ImGuiCol_TabSelected] = ImVec4(0.18f, 0.27f, 0.40f, 1);
    c[ImGuiCol_CheckMark] = ImVec4(0.30f, 0.82f, 0.77f, 1);
    c[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.82f, 0.77f, 0.8f);
    c[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.82f, 0.77f, 1);
}

} // namespace tessera::ui
