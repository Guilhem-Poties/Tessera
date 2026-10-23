#pragma once

// Where a parameter's declaration comes from (architecture section 3.5):
// resolved through its owner. A Node goes through its NodeType, a Link through
// its Converter, a Clip or an Effect through its Shader, a Marker has a label.

#include "model/Model.h"
#include "model/ShaderFormat.h"

#include <optional>
#include <string>
#include <vector>

namespace tessera {

// Source text of a shader: built-in sources ship with the app, user shaders
// live in the project files. Null when it can't be found.
const std::string* shaderSource(const Document& doc, const Uid& shaderUid);

// Parsed header of a shader, cached by source contents.
const ShaderInfo* shaderInfo(const Document& doc, const Uid& shaderUid);

bool isFilterShader(const Document& doc, const Uid& shaderUid);

std::vector<ParamDecl> nodeTypeDecls(const Document& doc, const Uid& nodeTypeUid);
// Shader inputs except `image`: settings of an Effect or of an effect Clip.
std::vector<ParamDecl> shaderSettingDecls(const Document& doc, const Uid& shaderUid);
std::vector<ParamDecl> markerDecls();

// Declarations of every parameter an owner should have.
std::vector<ParamDecl> ownerDecls(const Document& doc, const Uid& ownerUid);
std::optional<ParamDecl> declFor(const Document& doc, const Parameter& param);

} // namespace tessera
