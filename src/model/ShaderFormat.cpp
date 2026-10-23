#include "model/ShaderFormat.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

namespace tessera {

using nlohmann::json;

const char* toString(ShaderRole role) {
    switch (role) {
        case ShaderRole::Generator: return "Generator";
        case ShaderRole::Filter: return "Filter";
        case ShaderRole::Mixer: return "Mixer";
    }
    return "Generator";
}

static int lineAt(const std::string& s, size_t pos) {
    return 1 + (int)std::count(s.begin(), s.begin() + (std::ptrdiff_t)std::min(pos, s.size()), '\n');
}

static bool isIdentifier(const std::string& s) {
    if (s.empty() || !(std::isalpha((unsigned char)s[0]) || s[0] == '_')) return false;
    return std::all_of(s.begin(), s.end(), [](char c) { return std::isalnum((unsigned char)c) || c == '_'; });
}

ShaderInfo parseTsh(const std::string& src) {
    ShaderInfo info;
    size_t open = src.find("/*");
    size_t tag = src.find("@tessera", open == std::string::npos ? 0 : open);
    if (open == std::string::npos || tag == std::string::npos) {
        info.error = "line 1: missing the /* @tessera { ... } */ header";
        return info;
    }
    size_t close = src.find("*/", tag);
    if (close == std::string::npos) {
        info.error = "line " + std::to_string(lineAt(src, tag)) + ": the header comment is not closed";
        return info;
    }
    size_t jsonStart = tag + std::string("@tessera").size();
    std::string headerText = src.substr(jsonStart, close - jsonStart);

    json header;
    try {
        header = json::parse(headerText);
    } catch (const json::parse_error& e) {
        // byte offset → .tsh line
        int line = lineAt(src, jsonStart + (e.byte > 0 ? e.byte - 1 : 0));
        info.error = "line " + std::to_string(line) + ": header JSON: " + e.what();
        return info;
    }
    if (!header.is_object()) {
        info.error = "line " + std::to_string(lineAt(src, jsonStart)) + ": header must be a JSON object";
        return info;
    }

    info.label = header.value("label", std::string("Untitled shader"));
    int textures = 0;
    bool hasImage = false;
    if (header.contains("inputs")) {
        for (const json& in : header["inputs"]) {
            ParamDecl d;
            d.name = in.value("name", std::string());
            if (!isIdentifier(d.name)) {
                info.error = "header: input name '" + d.name + "' is not a valid GLSL identifier";
                return info;
            }
            std::string type = in.value("type", std::string("float"));
            d.type = paramTypeFromString(type);
            if (d.type == ParamType::Any || toString(d.type) != type) {
                info.error = "header: input '" + d.name + "' has unsupported type '" + type + "'";
                return info;
            }
            d.min = in.value("min", 0.0f);
            d.max = in.value("max", 1.0f);
            if (in.contains("options"))
                for (const json& o : in["options"]) d.options.push_back(o.get<std::string>());
            if (in.contains("default")) {
                const json& def = in["default"];
                if (d.type == ParamType::Choice && def.is_string()) {
                    auto it = std::find(d.options.begin(), d.options.end(), def.get<std::string>());
                    d.def = it == d.options.end() ? 0 : (int)(it - d.options.begin());
                } else {
                    d.def = moduleFor(d.type).load(def);
                }
            } else {
                d.def = moduleFor(d.type).defaultValue(d);
            }
            if (d.type == ParamType::Label) d.linkable = false;
            if (d.type == ParamType::Texture) {
                ++textures;
                if (d.name == "image") hasImage = true;
            }
            info.inputs.push_back(std::move(d));
        }
    }

    if (textures == 0) info.role = ShaderRole::Generator;
    else if (textures == 1 && hasImage) info.role = ShaderRole::Filter;
    else info.role = ShaderRole::Mixer;
    info.category = header.value("category", std::string(toString(info.role)));

    size_t bodyStart = close + 2;
    info.body = src.substr(bodyStart);
    info.bodyLine = lineAt(src, bodyStart);
    if (info.body.find("render") == std::string::npos) {
        info.error = "line " + std::to_string(info.bodyLine) + ": the body must define vec4 render(vec2 uv)";
        return info;
    }
    info.ok = true;
    return info;
}

const std::string& shaderStdlib() {
    static const std::string lib = R"GLSL(
float hash21(vec2 p){ p = fract(p * vec2(123.34, 456.21)); p += dot(p, p + 45.32); return fract(p.x * p.y); }
vec2 hash22(vec2 p){ float n = hash21(p); return vec2(n, hash21(p + n)); }
float vnoise(vec2 p){
  vec2 i = floor(p), f = fract(p); vec2 u = f * f * (3.0 - 2.0 * f);
  float a = hash21(i), b = hash21(i + vec2(1, 0)), c = hash21(i + vec2(0, 1)), d = hash21(i + vec2(1, 1));
  return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}
float fbm(vec2 p){
  float v = 0.0, a = 0.5;
  for (int i = 0; i < 5; i++){ v += a * vnoise(p); p = p * 2.03 + vec2(1.7, 9.2); a *= 0.5; }
  return v / 0.96875;
}
vec3 hsv2rgb(vec3 c){
  vec3 p = abs(fract(c.xxx + vec3(0.0, 2.0 / 3.0, 1.0 / 3.0)) * 6.0 - 3.0);
  return c.z * mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), c.y);
}
vec3 rgb2hsv(vec3 c){
  vec4 K = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
  vec4 p = mix(vec4(c.bg, K.wz), vec4(c.gb, K.xy), step(c.b, c.g));
  vec4 q = mix(vec4(p.xyw, c.r), vec4(c.r, p.yzx), step(p.x, c.r));
  float d = q.x - min(q.w, q.y); float e = 1.0e-10;
  return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
}
)GLSL";
    return lib;
}

std::string buildFragmentSource(const ShaderInfo& info) {
    std::string s = "#version 330 core\nin vec2 vUv;\nout vec4 tessera_FragColor;\n"
                    "uniform float TIME;\nuniform vec2 RES;\n";
    for (const ParamDecl& d : info.inputs) {
        std::string u = moduleFor(d.type).glslUniform(d.name);
        if (!u.empty()) s += u + "\n";
    }
    s += shaderStdlib();
    s += "#line " + std::to_string(info.bodyLine) + "\n";
    s += info.body;
    s += "\nvoid main(){ tessera_FragColor = render(vUv); }\n";
    return s;
}

const char* newShaderTemplate() {
    return R"(/* @tessera {
  "label": "New shader", "category": "Generator",
  "inputs": [
    {"name": "speed", "type": "float", "default": 0.5, "min": 0, "max": 4},
    {"name": "tint", "type": "color", "default": "#4fd1c5"}
  ]
} */
vec4 render(vec2 uv){
  float w = 0.5 + 0.5 * sin(uv.x * 12.0 + TIME * speed * 6.0);
  return vec4(tint * w, 1.0);
}
)";
}

} // namespace tessera
