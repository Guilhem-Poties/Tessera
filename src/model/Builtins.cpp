#include "model/Builtins.h"

#include "model/ShaderFormat.h"

namespace tessera {

// ── Built-in shaders (architecture section 6) ──────────────
namespace src {

const std::string noise = R"(/* @tessera {
  "label": "Noise", "category": "Generator",
  "inputs": [
    {"name": "scale", "type": "float", "default": 4, "min": 0.5, "max": 20},
    {"name": "speed", "type": "float", "default": 0.3, "min": 0, "max": 3},
    {"name": "contrast", "type": "float", "default": 1.2, "min": 0.2, "max": 4},
    {"name": "tint", "type": "color", "default": "#ffffff"}
  ]
} */
vec4 render(vec2 uv){
  vec2 p = (uv - 0.5) * vec2(RES.x / RES.y, 1.0) * scale;
  float n = fbm(p + vec2(TIME * speed, TIME * speed * 0.7));
  n = clamp((n - 0.5) * contrast + 0.5, 0.0, 1.0);
  return vec4(tint * n, 1.0);
}
)";

const std::string gradient = R"(/* @tessera {
  "label": "Gradient", "category": "Generator",
  "inputs": [
    {"name": "colors", "type": "colorList", "default": ["#ff3d7f", "#3dd6ff", "#ffd23d"]},
    {"name": "angle", "type": "float", "default": 0, "min": 0, "max": 360},
    {"name": "speed", "type": "float", "default": 0.1, "min": -2, "max": 2}
  ]
} */
vec4 render(vec2 uv){
  float a = radians(angle);
  vec2 d = vec2(cos(a), sin(a));
  float t = fract(dot((uv - 0.5) * vec2(RES.x / RES.y, 1.0), d) + 0.5 + TIME * speed);
  int n = max(colors_count, 1);
  float x = t * float(n);
  int i = int(floor(x));
  vec3 c0 = colors[i % n];
  vec3 c1 = colors[(i + 1) % n];
  return vec4(mix(c0, c1, smoothstep(0.0, 1.0, fract(x))), 1.0);
}
)";

const std::string shapes = R"(/* @tessera {
  "label": "Shapes", "category": "Generator",
  "inputs": [
    {"name": "shape", "type": "choice", "options": ["Circle", "Square", "Triangle", "Ring"], "default": "Circle"},
    {"name": "size", "type": "float", "default": 0.5, "min": 0.05, "max": 1},
    {"name": "count", "type": "float", "default": 3, "min": 1, "max": 12},
    {"name": "spin", "type": "float", "default": 0.3, "min": -3, "max": 3},
    {"name": "softness", "type": "float", "default": 0.01, "min": 0.001, "max": 0.2},
    {"name": "color", "type": "color", "default": "#ffd23d"}
  ]
} */
vec4 render(vec2 uv){
  vec2 p = (uv - 0.5) * vec2(RES.x / RES.y, 1.0);
  float c = floor(count + 0.5);
  p = fract(p * c + 0.5) - 0.5;
  float a = TIME * spin;
  p = mat2(cos(a), -sin(a), sin(a), cos(a)) * p;
  float r = size * 0.5;
  float d;
  if (shape == 0) d = length(p) - r;
  else if (shape == 1) { vec2 q = abs(p) - vec2(r * 0.85); d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0); }
  else if (shape == 2) {
    const float k = 1.7320508;
    vec2 q = p / r; q.x = abs(q.x) - 1.0; q.y = q.y + 1.0 / k;
    if (q.x + k * q.y > 0.0) q = vec2(q.x - k * q.y, -k * q.x - q.y) / 2.0;
    q.x -= clamp(q.x, -2.0, 0.0);
    d = -length(q) * sign(q.y) * r;
  }
  else d = abs(length(p) - r * 0.8) - r * 0.15;
  float m = 1.0 - smoothstep(-softness, softness, d);
  return vec4(color * m, m);
}
)";

const std::string plasma = R"(/* @tessera {
  "label": "Plasma", "category": "Generator",
  "inputs": [
    {"name": "speed", "type": "float", "default": 0.6, "min": 0, "max": 3},
    {"name": "scale", "type": "float", "default": 3, "min": 0.5, "max": 10},
    {"name": "hue", "type": "float", "default": 0.6, "min": 0, "max": 1},
    {"name": "saturation", "type": "float", "default": 0.8, "min": 0, "max": 1}
  ]
} */
vec4 render(vec2 uv){
  vec2 p = (uv - 0.5) * vec2(RES.x / RES.y, 1.0) * scale;
  float t = TIME * speed;
  float v = sin(p.x + t) + sin((p.y + t) * 0.5) + sin((p.x + p.y + t) * 0.5);
  vec2 c = p + vec2(sin(t / 3.0) * 2.0, cos(t / 2.0) * 2.0);
  v += sin(sqrt(c.x * c.x + c.y * c.y + 1.0) + t);
  v *= 0.5;
  vec3 col = hsv2rgb(vec3(fract(hue + v * 0.25), saturation, 0.55 + 0.45 * sin(v * 3.14159)));
  return vec4(col, 1.0);
}
)";

const std::string tessellate = R"(/* @tessera {
  "label": "Tessellate", "category": "Filter",
  "inputs": [
    {"name": "image", "type": "texture"},
    {"name": "cells", "type": "float", "default": 14, "min": 2, "max": 60},
    {"name": "edge", "type": "float", "default": 0.08, "min": 0, "max": 0.5},
    {"name": "shape", "type": "choice", "options": ["Hexagon", "Square"], "default": "Hexagon"}
  ]
} */
vec4 render(vec2 uv){
  vec2 asp = vec2(RES.x / RES.y, 1.0);
  vec2 p = uv * asp * cells;
  vec2 center;
  float inside;
  if (shape == 0) {
    vec2 s = vec2(1.0, 1.7320508);
    vec2 h = s * 0.5;
    vec2 a = mod(p, s) - h;
    vec2 b = mod(p - h, s) - h;
    vec2 g = dot(a, a) < dot(b, b) ? a : b;
    center = p - g;
    vec2 ag = abs(g);
    inside = 0.5 - max(dot(ag, normalize(s)), ag.x);
  } else {
    vec2 g = fract(p) - 0.5;
    center = floor(p) + 0.5;
    inside = 0.5 - max(abs(g.x), abs(g.y));
  }
  vec4 c = texture(image, center / cells / asp);
  float m = smoothstep(edge * 0.5, edge * 0.5 + 0.04, inside);
  return c * m;
}
)";

const std::string kaleidoscope = R"(/* @tessera {
  "label": "Kaleidoscope", "category": "Filter",
  "inputs": [
    {"name": "image", "type": "texture"},
    {"name": "segments", "type": "float", "default": 6, "min": 2, "max": 16},
    {"name": "spin", "type": "float", "default": 0.1, "min": -2, "max": 2},
    {"name": "zoom", "type": "float", "default": 1, "min": 0.25, "max": 4}
  ]
} */
vec4 render(vec2 uv){
  vec2 asp = vec2(RES.x / RES.y, 1.0);
  vec2 p = (uv - 0.5) * asp;
  float r = length(p);
  float a = atan(p.y, p.x) + TIME * spin;
  float seg = 6.2831853 / floor(max(segments, 2.0) + 0.5);
  a = mod(a, seg);
  a = abs(a - seg * 0.5);
  vec2 q = vec2(cos(a), sin(a)) * r / zoom;
  return texture(image, q / asp + 0.5);
}
)";

const std::string hueShift = R"(/* @tessera {
  "label": "Hue shift", "category": "Filter",
  "inputs": [
    {"name": "image", "type": "texture"},
    {"name": "hue", "type": "float", "default": 0, "min": 0, "max": 1}
  ]
} */
vec4 render(vec2 uv){
  vec4 c = texture(image, uv);
  vec3 hsv = rgb2hsv(c.a > 0. ? c.rgb / c.a : vec3(0.));
  hsv.x = fract(hsv.x + hue);
  return vec4(hsv2rgb(hsv) * c.a, c.a);
}
)";

const std::string pixelate = R"(/* @tessera {
  "label": "Pixelate", "category": "Filter",
  "inputs": [
    {"name": "image", "type": "texture"},
    {"name": "size", "type": "float", "default": 12, "min": 1, "max": 64}
  ]
} */
vec4 render(vec2 uv){
  vec2 px = max(size, 1.0) / RES;
  return texture(image, (floor(uv / px) + 0.5) * px);
}
)";

const std::string mixer = R"(/* @tessera {
  "label": "Mix", "category": "Mixer",
  "inputs": [
    {"name": "a", "type": "texture"},
    {"name": "b", "type": "texture"},
    {"name": "amount", "type": "float", "default": 0.5, "min": 0, "max": 1},
    {"name": "mode", "type": "choice", "options": ["Crossfade", "Add", "Multiply", "Screen"], "default": "Crossfade"}
  ]
} */
vec4 render(vec2 uv){
  vec4 A = texture(a, uv), B = texture(b, uv);
  if (mode == 0) return mix(A, B, amount);
  if (mode == 1) return A + B * amount;
  if (mode == 2) return mix(A, A * B, amount);
  return mix(A, A + B - A * B, amount);
}
)";

} // namespace src

const std::vector<BuiltinShader>& builtinShaders() {
    static const std::vector<BuiltinShader> list = {
        {"tessera://shaders/noise.tsh", &src::noise},
        {"tessera://shaders/gradient.tsh", &src::gradient},
        {"tessera://shaders/shapes.tsh", &src::shapes},
        {"tessera://shaders/plasma.tsh", &src::plasma},
        {"tessera://shaders/tessellate.tsh", &src::tessellate},
        {"tessera://shaders/kaleidoscope.tsh", &src::kaleidoscope},
        {"tessera://shaders/hue-shift.tsh", &src::hueShift},
        {"tessera://shaders/pixelate.tsh", &src::pixelate},
        {"tessera://shaders/mix.tsh", &src::mixer},
    };
    return list;
}

const std::string* builtinShaderSource(const std::string& path) {
    for (const BuiltinShader& s : builtinShaders())
        if (path == s.path) return s.source;
    return nullptr;
}

const std::vector<BuiltinMedia>& builtinMedia() {
    // Generated by the runtime (runtime/Media.cpp), so nothing ships on disk.
    static const std::vector<BuiltinMedia> list = {
        {"tessera://media/test-card.png", 640, 360},
        {"tessera://media/checker.png", 512, 512},
    };
    return list;
}

// ── Converters (architecture section 5) ────────────────────
const std::vector<ParamType>& convertibleTypes() {
    static const std::vector<ParamType> types = {ParamType::Float, ParamType::Bool, ParamType::Choice,
                                                 ParamType::Color, ParamType::ColorList, ParamType::Texture};
    return types;
}

std::string converterLabel(ParamType t) {
    switch (t) {
        case ParamType::Float: return "To float";
        case ParamType::Bool: return "To bool";
        case ParamType::Choice: return "To choice";
        case ParamType::Color: return "To color";
        case ParamType::ColorList: return "To color list";
        case ParamType::Texture: return "To texture";
        default: return "To ?";
    }
}

static ParamDecl floatDecl(const char* name, float def, float mn, float mx) {
    ParamDecl d;
    d.name = name; d.type = ParamType::Float; d.def = def; d.min = mn; d.max = mx;
    return d;
}
static ParamDecl choiceDecl(const char* name, std::vector<std::string> options, int def = 0) {
    ParamDecl d;
    d.name = name; d.type = ParamType::Choice; d.options = std::move(options); d.def = def;
    return d;
}
static ParamDecl boolDecl(const char* name, bool def) {
    ParamDecl d;
    d.name = name; d.type = ParamType::Bool; d.def = def;
    return d;
}
static ParamDecl colorDecl(const char* name, Color def) {
    ParamDecl d;
    d.name = name; d.type = ParamType::Color; d.def = def;
    return d;
}
static ParamDecl typedDecl(const char* name, ParamType type, bool output) {
    ParamDecl d;
    d.name = name; d.type = type; d.output = output;
    d.def = moduleFor(type).defaultValue(d);
    return d;
}

std::vector<ParamDecl> converterSettings(ParamType t) {
    switch (t) {
        case ParamType::Float:
            return {floatDecl("inMin", 0, -10, 10), floatDecl("inMax", 1, -10, 10), floatDecl("outMin", 0, -10, 10), floatDecl("outMax", 1, -10, 10)};
        case ParamType::Bool:
            return {floatDecl("threshold", 0.5f, 0, 1), boolDecl("invert", false)};
        case ParamType::Choice:
            return {choiceDecl("mode", {"Spread", "Index"}), floatDecl("offset", 0, -16, 16)};
        case ParamType::Color:
            return {choiceDecl("mode", {"Hue", "Brightness"}), floatDecl("hueShift", 0, 0, 1), floatDecl("saturation", 1, 0, 1),
                    colorDecl("tint", {1, 1, 1})};
        case ParamType::ColorList:
            return {floatDecl("count", 3, 1, 4), floatDecl("spread", 0.33f, 0, 1), floatDecl("hueShift", 0, 0, 1)};
        case ParamType::Texture:
            return {floatDecl("gain", 1, 0, 4)};
        default:
            return {};
    }
}

std::vector<ParamDecl> builtinNodeDecls(const std::string& label) {
    if (label == "LFO")
        return {floatDecl("rate", 0.5f, 0, 10), choiceDecl("shape", {"Sine", "Triangle", "Saw", "Square"}),
                floatDecl("min", 0, -10, 10), floatDecl("max", 1, -10, 10), floatDecl("phase", 0, 0, 1),
                typedDecl("value", ParamType::Float, true)};
    if (label == "Value")
        return {floatDecl("value", 0.5f, 0, 1), typedDecl("out", ParamType::Float, true)};
    if (label == "Color")
        return {colorDecl("color", {1.0f, 0.5f, 0.25f}), typedDecl("out", ParamType::Color, true)};
    if (label == "Output")
        return {typedDecl("image", ParamType::Texture, false)};
    return {};
}

void ensureBuiltins(Model& m) {
    for (const BuiltinShader& s : builtinShaders()) {
        Uid uid = nameUid(std::string("shader:") + s.path);
        ShaderInfo info = parseTsh(*s.source);
        if (!m.shaders.contains(uid)) m.shaders.add({uid, info.label, s.path, {}});
        Uid typeUid = nameUid(std::string("nodetype:") + s.path);
        if (!m.nodeTypes.contains(typeUid)) m.nodeTypes.add({typeUid, info.label, info.category, "shader", uid});
    }
    for (const BuiltinMedia& md : builtinMedia()) {
        Uid uid = nameUid(std::string("media:") + md.path);
        if (!m.media.contains(uid)) {
            m.media.add({uid, md.path, {}});
            m.images.add({uid, md.width, md.height});
        }
        Uid typeUid = nameUid(std::string("nodetype:") + md.path);
        if (!m.nodeTypes.contains(typeUid)) m.nodeTypes.add({typeUid, m.displayName(uid), "Media", "media", uid});
    }
    for (ParamType t : convertibleTypes()) {
        std::string key = toString(t);
        Uid uid = nameUid("converter:" + key);
        if (!m.converters.contains(uid)) m.converters.add({uid, converterLabel(t), "any", key});
        Uid typeUid = nameUid("nodetype:converter:" + key);
        if (!m.nodeTypes.contains(typeUid)) m.nodeTypes.add({typeUid, converterLabel(t), "Converter", "converter", uid});
    }
    for (const char* label : kBuiltinNodeLabels) {
        Uid typeUid = nameUid(std::string("nodetype:builtin:") + label);
        std::string category = std::string(label) == "Output" ? "Output" : "Value";
        if (!m.nodeTypes.contains(typeUid)) m.nodeTypes.add({typeUid, label, category, "builtin", {}});
    }
}

} // namespace tessera
