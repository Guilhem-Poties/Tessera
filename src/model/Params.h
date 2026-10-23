#pragma once

#include "core/Math.h"

#include <nlohmann/json_fwd.hpp>
#include <string>
#include <variant>
#include <vector>

namespace tessera {

// Parameter types (architecture section 4). Each one is a reusable module.
enum class ParamType { Float, Choice, Bool, Color, ColorList, Texture, Label, Any };

const char* toString(ParamType type);
ParamType paramTypeFromString(const std::string& text);   // Float on unknown input

using ColorList = std::vector<Color>;   // up to 4 colors

// Stored parameter value. Textures are runtime handles and never stored, so
// `texture` and `any` parameters hold std::monostate.
using Value = std::variant<std::monostate, float, int, bool, Color, ColorList, std::string>;

// A parameter declaration: range, options and default. Never stored in the
// model; it is resolved through the parameter's owner (see Declarations.h).
struct ParamDecl {
    std::string name;
    ParamType type = ParamType::Float;
    bool output = false;
    float min = 0.0f, max = 1.0f;
    Value def;
    std::vector<std::string> options;   // for `choice`
    bool linkable = true;
};

// A module knows how to store a value, draw its widget, save and load it and
// declare its shader uniform. drawWidget() lives in the UI layer
// (ui/ParamWidgets.cpp) so that the model stays free of ImGui.
class ParamModule {
public:
    virtual ~ParamModule() = default;
    virtual ParamType type() const = 0;
    virtual Value defaultValue(const ParamDecl& decl) const = 0;
    virtual nlohmann::json save(const Value& value) const = 0;
    virtual Value load(const nlohmann::json& json) const = 0;
    virtual std::string glslUniform(const std::string& name) const = 0;   // "" when it has none
    virtual std::string describe(const Value& value, const ParamDecl& decl) const = 0;

    enum WidgetResult { None = 0, Changed = 1, Commit = 2 };
    virtual int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const = 0;
};

const ParamModule& moduleFor(ParamType type);

// Coercion helpers shared by the runtime and the UI.
float valueAsFloat(const Value& value);
Color valueAsColor(const Value& value);

} // namespace tessera
