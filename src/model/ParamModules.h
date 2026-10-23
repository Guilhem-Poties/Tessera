#pragma once

#include "model/Params.h"

// Concrete parameter modules, one per type. Declared here so that the UI layer
// can define each module's drawWidget().

namespace tessera {

class FloatModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Float; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class ChoiceModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Choice; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class BoolModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Bool; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class ColorModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Color; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class ColorListModule final : public ParamModule {
public:
    static constexpr int kMaxColors = 4;
    ParamType type() const override { return ParamType::ColorList; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class TextureModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Texture; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class LabelModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Label; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

class AnyModule final : public ParamModule {
public:
    ParamType type() const override { return ParamType::Any; }
    Value defaultValue(const ParamDecl& decl) const override;
    nlohmann::json save(const Value& value) const override;
    Value load(const nlohmann::json& json) const override;
    std::string glslUniform(const std::string& name) const override;
    std::string describe(const Value& value, const ParamDecl& decl) const override;
    int drawWidget(const char* label, const ParamDecl& decl, Value& value, bool readOnly) const override;
};

} // namespace tessera
