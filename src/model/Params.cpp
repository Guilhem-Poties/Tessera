#include "model/ParamModules.h"

#include <nlohmann/json.hpp>

#include <cstdio>

namespace tessera {

using nlohmann::json;

const char* toString(ParamType type) {
    switch (type) {
        case ParamType::Float: return "float";
        case ParamType::Choice: return "choice";
        case ParamType::Bool: return "bool";
        case ParamType::Color: return "color";
        case ParamType::ColorList: return "colorList";
        case ParamType::Texture: return "texture";
        case ParamType::Label: return "label";
        case ParamType::Any: return "any";
    }
    return "float";
}

ParamType paramTypeFromString(const std::string& text) {
    for (ParamType t : {ParamType::Float, ParamType::Choice, ParamType::Bool, ParamType::Color,
                        ParamType::ColorList, ParamType::Texture, ParamType::Label, ParamType::Any})
        if (text == toString(t)) return t;
    return ParamType::Float;
}

float valueAsFloat(const Value& value) {
    if (auto f = std::get_if<float>(&value)) return *f;
    if (auto i = std::get_if<int>(&value)) return (float)*i;
    if (auto b = std::get_if<bool>(&value)) return *b ? 1.0f : 0.0f;
    if (auto c = std::get_if<Color>(&value)) return luma(*c);
    if (auto l = std::get_if<ColorList>(&value)) return l->empty() ? 0.0f : luma(l->front());
    return 0.0f;
}

Color valueAsColor(const Value& value) {
    if (auto c = std::get_if<Color>(&value)) return *c;
    if (auto l = std::get_if<ColorList>(&value)) return l->empty() ? Color{} : l->front();
    float v = valueAsFloat(value);
    return {v, v, v};
}

static std::string fmtFloat(float v) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.3g", v);
    return buf;
}

// ── float ──────────────────────────────────────────────────
Value FloatModule::defaultValue(const ParamDecl& d) const {
    return std::holds_alternative<float>(d.def) ? d.def : Value(d.min);
}
json FloatModule::save(const Value& v) const { return valueAsFloat(v); }
Value FloatModule::load(const json& j) const { return j.is_number() ? j.get<float>() : 0.0f; }
std::string FloatModule::glslUniform(const std::string& n) const { return "uniform float " + n + ";"; }
std::string FloatModule::describe(const Value& v, const ParamDecl&) const { return fmtFloat(valueAsFloat(v)); }

// ── choice ─────────────────────────────────────────────────
Value ChoiceModule::defaultValue(const ParamDecl& d) const {
    return std::holds_alternative<int>(d.def) ? d.def : Value(0);
}
json ChoiceModule::save(const Value& v) const { return (int)valueAsFloat(v); }
Value ChoiceModule::load(const json& j) const { return j.is_number() ? j.get<int>() : 0; }
std::string ChoiceModule::glslUniform(const std::string& n) const { return "uniform int " + n + ";"; }
std::string ChoiceModule::describe(const Value& v, const ParamDecl& d) const {
    int i = (int)valueAsFloat(v);
    if (i >= 0 && i < (int)d.options.size()) return d.options[i];
    return std::to_string(i);
}

// ── bool ───────────────────────────────────────────────────
Value BoolModule::defaultValue(const ParamDecl& d) const {
    return std::holds_alternative<bool>(d.def) ? d.def : Value(false);
}
json BoolModule::save(const Value& v) const { return valueAsFloat(v) > 0.5f; }
Value BoolModule::load(const json& j) const { return j.is_boolean() ? j.get<bool>() : false; }
std::string BoolModule::glslUniform(const std::string& n) const { return "uniform bool " + n + ";"; }
std::string BoolModule::describe(const Value& v, const ParamDecl&) const { return valueAsFloat(v) > 0.5f ? "on" : "off"; }

// ── color ──────────────────────────────────────────────────
Value ColorModule::defaultValue(const ParamDecl& d) const {
    return std::holds_alternative<Color>(d.def) ? d.def : Value(Color{1, 1, 1});
}
json ColorModule::save(const Value& v) const { return toHex(valueAsColor(v)); }
Value ColorModule::load(const json& j) const { return j.is_string() ? fromHex(j.get<std::string>()) : Color{1, 1, 1}; }
std::string ColorModule::glslUniform(const std::string& n) const { return "uniform vec3 " + n + ";"; }
std::string ColorModule::describe(const Value& v, const ParamDecl&) const { return toHex(valueAsColor(v)); }

// ── colorList ──────────────────────────────────────────────
Value ColorListModule::defaultValue(const ParamDecl& d) const {
    if (std::holds_alternative<ColorList>(d.def)) return d.def;
    return ColorList{Color{1, 0.25f, 0.5f}, Color{0.25f, 0.8f, 1}};
}
json ColorListModule::save(const Value& v) const {
    json arr = json::array();
    if (auto l = std::get_if<ColorList>(&v))
        for (const Color& c : *l) arr.push_back(toHex(c));
    return arr;
}
Value ColorListModule::load(const json& j) const {
    ColorList list;
    if (j.is_array())
        for (const auto& e : j)
            if (e.is_string() && (int)list.size() < kMaxColors) list.push_back(fromHex(e.get<std::string>()));
    return list;
}
std::string ColorListModule::glslUniform(const std::string& n) const {
    return "uniform vec3 " + n + "[4];\nuniform int " + n + "_count;";
}
std::string ColorListModule::describe(const Value& v, const ParamDecl&) const {
    auto l = std::get_if<ColorList>(&v);
    return std::to_string(l ? l->size() : 0) + " colors";
}

// ── texture ────────────────────────────────────────────────
Value TextureModule::defaultValue(const ParamDecl&) const { return std::monostate{}; }
json TextureModule::save(const Value&) const { return nullptr; }
Value TextureModule::load(const json&) const { return std::monostate{}; }
std::string TextureModule::glslUniform(const std::string& n) const { return "uniform sampler2D " + n + ";"; }
std::string TextureModule::describe(const Value&, const ParamDecl&) const { return ""; }

// ── label ──────────────────────────────────────────────────
Value LabelModule::defaultValue(const ParamDecl& d) const {
    return std::holds_alternative<std::string>(d.def) ? d.def : Value(std::string());
}
json LabelModule::save(const Value& v) const {
    auto s = std::get_if<std::string>(&v);
    return s ? *s : std::string();
}
Value LabelModule::load(const json& j) const { return j.is_string() ? j.get<std::string>() : std::string(); }
std::string LabelModule::glslUniform(const std::string&) const { return ""; }
std::string LabelModule::describe(const Value& v, const ParamDecl&) const {
    auto s = std::get_if<std::string>(&v);
    return s ? *s : std::string();
}

// ── any ────────────────────────────────────────────────────
Value AnyModule::defaultValue(const ParamDecl&) const { return std::monostate{}; }
json AnyModule::save(const Value&) const { return nullptr; }
Value AnyModule::load(const json&) const { return std::monostate{}; }
std::string AnyModule::glslUniform(const std::string&) const { return ""; }
std::string AnyModule::describe(const Value&, const ParamDecl&) const { return ""; }

const ParamModule& moduleFor(ParamType type) {
    static const FloatModule f;
    static const ChoiceModule c;
    static const BoolModule b;
    static const ColorModule col;
    static const ColorListModule list;
    static const TextureModule tex;
    static const LabelModule label;
    static const AnyModule any;
    switch (type) {
        case ParamType::Float: return f;
        case ParamType::Choice: return c;
        case ParamType::Bool: return b;
        case ParamType::Color: return col;
        case ParamType::ColorList: return list;
        case ParamType::Texture: return tex;
        case ParamType::Label: return label;
        case ParamType::Any: return any;
    }
    return f;
}

} // namespace tessera
