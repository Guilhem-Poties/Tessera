#pragma once

// Tessera's shader format (.tsh, architecture section 6): a JSON header in a
// comment, followed by GLSL that defines `vec4 render(vec2 uv)` returning
// premultiplied RGBA.

#include "model/Params.h"

#include <string>
#include <vector>

namespace tessera {

enum class ShaderRole { Generator, Filter, Mixer };
const char* toString(ShaderRole role);

struct ShaderInfo {
    bool ok = false;
    std::string error;        // header error, with its .tsh line number
    std::string label;
    std::string category;     // from the header, or the role when absent
    std::vector<ParamDecl> inputs;
    std::string body;         // GLSL after the header
    int bodyLine = 1;         // .tsh line where the body starts (for #line)
    ShaderRole role = ShaderRole::Generator;
};

ShaderInfo parseTsh(const std::string& source);

// The GLSL standard library available to every shader.
const std::string& shaderStdlib();

// Full fragment shader: uniforms generated from the inputs, the standard
// library, then the body with a #line directive so that compile errors
// report .tsh line numbers.
std::string buildFragmentSource(const ShaderInfo& info);

// Template used by "New shader".
const char* newShaderTemplate();

} // namespace tessera
