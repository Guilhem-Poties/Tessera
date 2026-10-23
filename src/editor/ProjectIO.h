#pragma once

// Saving, loading and importing files.
//
// A project is a directory holding `project.tessera` (the model and the node
// positions, as JSON) next to the project files (user shaders and copied
// images) at their project-relative paths.

#include "editor/EditorState.h"
#include "model/Model.h"

#include <string>

namespace tessera {

inline constexpr const char* kProjectFileName = "project.tessera";

bool saveProject(const Document& doc, const EditorState& ed, const std::string& dir, std::string& error);
// `path` is the project directory or its project.tessera file.
bool loadProject(const std::string& path, Document& doc, EditorState& ed, std::string& error);

// Images under 1.5 MB are copied into the project; larger images, video and
// audio stay external references.
Uid importMedia(Document& doc, const Uid& folderUid, const std::string& filePath, std::string& error);
Uid importShader(Document& doc, const Uid& folderUid, const std::string& filePath, std::string& error);

bool isMediaFile(const std::string& path);
bool isShaderFile(const std::string& path);

// A project with two graphs, clips, an effect clip, markers and a loop.
Document makeDemoProject(EditorState& ed);

} // namespace tessera
