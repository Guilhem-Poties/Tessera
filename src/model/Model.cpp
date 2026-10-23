#include "model/Model.h"

#include <algorithm>

namespace tessera {

const char* toString(BlendMode mode) { return kBlendModeNames[(int)mode]; }

BlendMode blendModeFromString(const std::string& text) {
    for (int i = 0; i < 5; ++i)
        if (text == kBlendModeNames[i]) return (BlendMode)i;
    return BlendMode::Normal;
}

const char* toString(Kind kind) {
    switch (kind) {
        case Kind::None: return "None";
        case Kind::Project: return "Project";
        case Kind::Folder: return "Folder";
        case Kind::Media: return "Media";
        case Kind::Image: return "Image";
        case Kind::Video: return "Video";
        case Kind::Audio: return "Audio";
        case Kind::Shader: return "Shader";
        case Kind::Graph: return "Graph";
        case Kind::NodeType: return "NodeType";
        case Kind::Node: return "Node";
        case Kind::Link: return "Link";
        case Kind::Converter: return "Converter";
        case Kind::Parameter: return "Parameter";
        case Kind::Timeline: return "Timeline";
        case Kind::Track: return "Track";
        case Kind::Clip: return "Clip";
        case Kind::Effect: return "Effect";
        case Kind::Marker: return "Marker";
        case Kind::CuePoint: return "CuePoint";
        case Kind::HoldPoint: return "HoldPoint";
        case Kind::Loop: return "Loop";
    }
    return "None";
}

Kind Model::kindOf(const Uid& uid) const {
    if (uid.is_nil()) return Kind::None;
    Kind found = Kind::None;
    forEachTable(*this, [&](Kind kind, const char*, const auto& table) {
        // Subtype tables share their parent's uid; the parent is listed first.
        if (found == Kind::None && table.contains(uid)) found = kind;
    });
    return found;
}

Kind Model::mediaKind(const Uid& uid) const {
    if (images.contains(uid)) return Kind::Image;
    if (videos.contains(uid)) return Kind::Video;
    if (audios.contains(uid)) return Kind::Audio;
    return Kind::None;
}

Kind Model::markerKind(const Uid& uid) const {
    if (cuePoints.contains(uid)) return Kind::CuePoint;
    if (holdPoints.contains(uid)) return Kind::HoldPoint;
    return Kind::Marker;
}

static std::string fileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

std::string Model::displayName(const Uid& uid) const {
    switch (kindOf(uid)) {
        case Kind::Project: return projects.find(uid)->name;
        case Kind::Folder: return folders.find(uid)->label;
        case Kind::Media: return fileName(media.find(uid)->path);
        case Kind::Shader: return shaders.find(uid)->label;
        case Kind::Graph: return graphs.find(uid)->name;
        case Kind::NodeType: return nodeTypes.find(uid)->label;
        case Kind::Node: {
            const NodeType* t = nodeTypes.find(nodes.find(uid)->typeUid);
            return t ? t->label : "Node";
        }
        case Kind::Link: {
            const Link* l = links.find(uid);
            return displayName(l->sourceParam) + " -> " + displayName(l->targetParam);
        }
        case Kind::Converter: return converters.find(uid)->label;
        case Kind::Parameter: {
            const Parameter* p = params.find(uid);
            return displayName(p->ownerUid) + "." + p->name;
        }
        case Kind::Timeline: return "Timeline";
        case Kind::Track: return "Track " + std::to_string(tracks.find(uid)->order + 1);
        case Kind::Clip: return displayName(clips.find(uid)->sourceUid);
        case Kind::Effect: return displayName(effects.find(uid)->shaderUid);
        case Kind::Marker: {
            const Parameter* label = findParam(uid, "label");
            if (label)
                if (auto s = std::get_if<std::string>(&label->value); s && !s->empty()) return *s;
            Kind k = markerKind(uid);
            return k == Kind::CuePoint ? "Cue" : k == Kind::HoldPoint ? "Hold" : "Marker";
        }
        case Kind::Loop: return "Loop";
        default: return "(missing " + shortUid(uid) + ")";
    }
}

Parameter* Model::findParam(const Uid& ownerUid, const std::string& name) {
    for (Parameter& p : params)
        if (p.ownerUid == ownerUid && p.name == name) return &p;
    return nullptr;
}

const Parameter* Model::findParam(const Uid& ownerUid, const std::string& name) const {
    for (const Parameter& p : params)
        if (p.ownerUid == ownerUid && p.name == name) return &p;
    return nullptr;
}

const Link* Model::linkInto(const Uid& targetParam) const {
    for (const Link& l : links)
        if (l.targetParam == targetParam) return &l;
    return nullptr;
}

Uid Model::nodeTypeFor(const Uid& backingUid) const {
    for (const NodeType& t : nodeTypes)
        if (t.backingUid == backingUid) return t.uid;
    return {};
}

const Converter* Model::converterFor(ParamType toType) const {
    for (const Converter& c : converters)
        if (c.toType == tessera::toString(toType)) return &c;
    return nullptr;
}

std::vector<const Track*> Model::tracksBottomToTop() const {
    std::vector<const Track*> out;
    const Project* p = project();
    for (const Track& t : tracks)
        if (!p || t.timelineUid == p->timelineUid) out.push_back(&t);
    std::stable_sort(out.begin(), out.end(), [](const Track* a, const Track* b) { return a->order < b->order; });
    return out;
}

} // namespace tessera
