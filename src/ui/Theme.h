#pragma once

// Colours shared by the panels, from the architecture diagrams.

#include "model/Params.h"

#include <imgui.h>

#include <string>

namespace tessera::ui {

inline constexpr ImU32 kGraphBlue = IM_COL32(0x4a, 0x7f, 0xc8, 255);
inline constexpr ImU32 kTimelinePink = IM_COL32(0xc8, 0x5a, 0x86, 255);
inline constexpr ImU32 kParamAmber = IM_COL32(0xd0, 0x9a, 0x2a, 255);
inline constexpr ImU32 kAssetGreen = IM_COL32(0x86, 0xa8, 0x3a, 255);
inline constexpr ImU32 kRuntimeTeal = IM_COL32(0x3a, 0xa8, 0x90, 255);
inline constexpr ImU32 kConverterPurple = IM_COL32(0x8e, 0x6c, 0xc8, 255);
inline constexpr ImU32 kMuted = IM_COL32(0x8a, 0x92, 0xa2, 255);

ImU32 categoryColor(const std::string& category);
ImU32 paramTypeColor(ParamType type);
ImVec4 toVec4(ImU32 color);

} // namespace tessera::ui
