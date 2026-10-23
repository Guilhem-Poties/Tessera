#pragma once

// What ships with the app: built-in shaders and media under tessera://,
// the converters (one per parameter type) and the built-in node types
// (LFO, Value, Color, Output).

#include "model/Model.h"

#include <string>
#include <vector>

namespace tessera {

struct BuiltinShader {
    const char* path;
    const std::string* source;
};
const std::vector<BuiltinShader>& builtinShaders();
const std::string* builtinShaderSource(const std::string& path);   // null if unknown

struct BuiltinMedia {
    const char* path;
    int width, height;
};
const std::vector<BuiltinMedia>& builtinMedia();

// Parameter types that have a converter, in palette order.
const std::vector<ParamType>& convertibleTypes();
std::string converterLabel(ParamType toType);
// The converter's settings (owned by each Link, or inputs on a converter node).
std::vector<ParamDecl> converterSettings(ParamType toType);

// Inputs and outputs of the built-in node types.
std::vector<ParamDecl> builtinNodeDecls(const std::string& label);
inline constexpr const char* kBuiltinNodeLabels[] = {"LFO", "Value", "Color", "Output"};

// Adds every built-in row (shaders, media, converters, node types) that is
// missing, with deterministic uids. Safe to call on a loaded project.
void ensureBuiltins(Model& model);

} // namespace tessera
