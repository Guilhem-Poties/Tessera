#include "model/Declarations.h"

#include "model/Builtins.h"

#include <unordered_map>

namespace tessera {

const std::string* shaderSource(const Document& doc, const Uid& shaderUid) {
    const Shader* s = doc.model.shaders.find(shaderUid);
    if (!s) return nullptr;
    if (isBuiltinPath(s->path)) return builtinShaderSource(s->path);
    auto it = doc.files.find(s->path);
    return it == doc.files.end() || !it->second ? nullptr : it->second.get();
}

const ShaderInfo* shaderInfo(const Document& doc, const Uid& shaderUid) {
    const std::string* source = shaderSource(doc, shaderUid);
    if (!source) return nullptr;
    // Keyed by contents. A new entry only appears when a shader is compiled,
    // so the cache stays small, and returned pointers stay valid.
    static std::unordered_map<std::string, ShaderInfo> cache;
    auto it = cache.find(*source);
    if (it != cache.end()) return &it->second;
    return &cache.emplace(*source, parseTsh(*source)).first->second;
}

bool isFilterShader(const Document& doc, const Uid& shaderUid) {
    const ShaderInfo* info = shaderInfo(doc, shaderUid);
    return info && info->ok && info->role == ShaderRole::Filter;
}

std::vector<ParamDecl> nodeTypeDecls(const Document& doc, const Uid& nodeTypeUid) {
    const Model& m = doc.model;
    const NodeType* type = m.nodeTypes.find(nodeTypeUid);
    if (!type) return {};
    std::vector<ParamDecl> decls;
    if (type->kind == "shader") {
        if (const ShaderInfo* info = shaderInfo(doc, type->backingUid); info && info->ok) decls = info->inputs;
        ParamDecl out;
        out.name = "out"; out.type = ParamType::Texture; out.output = true;
        decls.push_back(out);
    } else if (type->kind == "media") {
        ParamDecl out;
        out.output = true;
        if (m.mediaKind(type->backingUid) == Kind::Audio) {
            out.name = "level"; out.type = ParamType::Float;
        } else {
            out.name = "image"; out.type = ParamType::Texture;
        }
        decls.push_back(out);
    } else if (type->kind == "converter") {
        const Converter* c = m.converters.find(type->backingUid);
        ParamType to = c ? paramTypeFromString(c->toType) : ParamType::Float;
        ParamDecl in;
        in.name = "in"; in.type = ParamType::Any;
        decls.push_back(in);
        for (ParamDecl& d : converterSettings(to)) decls.push_back(d);
        ParamDecl out;
        out.name = "out"; out.type = to; out.output = true;
        decls.push_back(out);
    } else {
        decls = builtinNodeDecls(type->label);
    }
    return decls;
}

std::vector<ParamDecl> shaderSettingDecls(const Document& doc, const Uid& shaderUid) {
    std::vector<ParamDecl> decls;
    if (const ShaderInfo* info = shaderInfo(doc, shaderUid); info && info->ok)
        for (const ParamDecl& d : info->inputs)
            if (!(d.type == ParamType::Texture && d.name == "image")) decls.push_back(d);
    return decls;
}

std::vector<ParamDecl> markerDecls() {
    ParamDecl d;
    d.name = "label"; d.type = ParamType::Label; d.def = std::string(); d.linkable = false;
    return {d};
}

std::vector<ParamDecl> ownerDecls(const Document& doc, const Uid& ownerUid) {
    const Model& m = doc.model;
    switch (m.kindOf(ownerUid)) {
        case Kind::Node: return nodeTypeDecls(doc, m.nodes.find(ownerUid)->typeUid);
        case Kind::Link: {
            const Converter* c = m.converters.find(m.links.find(ownerUid)->converterUid);
            if (!c) return {};
            auto decls = converterSettings(paramTypeFromString(c->toType));
            for (ParamDecl& d : decls) d.linkable = false;   // link settings are not linkable yet
            return decls;
        }
        case Kind::Clip: {
            const Clip* clip = m.clips.find(ownerUid);
            if (m.kindOf(clip->sourceUid) != Kind::Shader) return {};
            auto decls = shaderSettingDecls(doc, clip->sourceUid);
            for (ParamDecl& d : decls) d.linkable = false;
            return decls;
        }
        case Kind::Effect: {
            auto decls = shaderSettingDecls(doc, m.effects.find(ownerUid)->shaderUid);
            for (ParamDecl& d : decls) d.linkable = false;
            return decls;
        }
        case Kind::Marker: return markerDecls();
        default: return {};
    }
}

std::optional<ParamDecl> declFor(const Document& doc, const Parameter& param) {
    for (ParamDecl& d : ownerDecls(doc, param.ownerUid))
        if (d.name == param.name) return d;
    return std::nullopt;
}

} // namespace tessera
