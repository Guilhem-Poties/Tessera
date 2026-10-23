// Graph evaluation (architecture section 7.2): nodes in topological order;
// each input is either a linked source output run through the link's
// converter, or the parameter's value; then the node renders or computes.

#include "runtime/Runtime.h"

#include "model/Declarations.h"

#include <cmath>
#include <functional>

namespace tessera {

namespace {

Uid ownerOf(const Model& m, const Uid& paramUid) {
    const Parameter* p = m.params.find(paramUid);
    return p ? p->ownerUid : Uid{};
}

float lfo(int shape, float phase) {
    phase -= std::floor(phase);
    switch (shape) {
        case 1: return 1.0f - std::fabs(2.0f * phase - 1.0f);   // triangle
        case 2: return phase;                                     // saw
        case 3: return phase < 0.5f ? 1.0f : 0.0f;                // square
        default: return 0.5f + 0.5f * std::sin(phase * 6.2831853f);
    }
}

} // namespace

TexRef Runtime::evaluateGraph(const Document& doc, const Uid& graphUid, float time, bool outputOnly) {
    const Model& m = doc.model;
    const Graph* g = m.graphs.find(graphUid);
    if (!g) return {};
    int w = std::max(16, (int)g->resolution.x), h = std::max(16, (int)g->resolution.y);
    readbacks_.clear();

    std::unordered_map<Uid, const Link*> incoming;              // target param -> link
    std::unordered_map<Uid, std::vector<Uid>> dependencies;     // node -> source nodes
    for (const Link& l : m.links) {
        if (l.graphUid != graphUid) continue;
        incoming[l.targetParam] = &l;
        dependencies[ownerOf(m, l.targetParam)].push_back(ownerOf(m, l.sourceParam));
    }

    // Topological sort by links. Loops are refused when a link is created, so
    // the "visiting" guard only protects against corrupt files.
    std::vector<Uid> order;
    std::unordered_map<Uid, int> state;
    std::function<void(const Uid&)> visit = [&](const Uid& node) {
        if (state[node]) return;
        state[node] = 1;
        for (const Uid& dep : dependencies[node]) visit(dep);
        state[node] = 2;
        order.push_back(node);
    };
    const Uid outputType = nameUid("nodetype:builtin:Output");
    for (const Node& n : m.nodes)
        if (n.graphUid == graphUid && (!outputOnly || n.typeUid == outputType)) visit(n.uid);

    TexRef result;
    for (const Uid& node : order) evaluateNode(doc, node, time, w, h, incoming, result);
    return result;
}

void Runtime::evaluateNode(const Document& doc, const Uid& nodeUid, float time, int w, int h,
                           const std::unordered_map<Uid, const Link*>& incoming, TexRef& result) {
    const Model& m = doc.model;
    const Node* node = m.nodes.find(nodeUid);
    if (!node) return;
    const NodeType* type = m.nodeTypes.find(node->typeUid);
    if (!type) return;

    std::vector<ParamDecl> decls = nodeTypeDecls(doc, type->uid);
    Settings in;
    const Parameter* outParam = nullptr;
    const ParamDecl* outDecl = nullptr;
    for (const ParamDecl& d : decls) {
        const Parameter* p = m.findParam(nodeUid, d.name);
        if (!p) continue;
        if (d.output) {
            outParam = p;
            outDecl = &d;
            continue;
        }
        auto it = incoming.find(p->uid);
        if (it == incoming.end()) {
            live.erase(p->uid);
            in[d.name] = toLive(p->value);
            continue;
        }
        const Link* link = it->second;
        const Converter* conv = m.converters.find(link->converterUid);
        Settings settings;
        for (const Parameter& s : m.params)
            if (s.ownerUid == link->uid) settings[s.name] = toLive(s.value);
        const LiveValue* src = liveValue(link->sourceParam);
        LiveValue v = convert(conv ? paramTypeFromString(conv->toType) : d.type, settings, src ? *src : LiveValue{}, &d,
                              "l:" + toString(link->uid), w, h);
        live[p->uid] = v;
        in[d.name] = std::move(v);
    }

    auto output = [&](LiveValue v) {
        if (outParam) live[outParam->uid] = std::move(v);
    };

    if (type->kind == "shader") {
        gl::Fbo& target = fbo("n:" + toString(nodeUid), w, h);
        runShader(doc, type->backingUid, target, time, [&](const ParamDecl& d) {
            auto it = in.find(d.name);
            return it == in.end() ? LiveValue{} : it->second;
        });
        output(TexRef{target.tex, target.w, target.h});
    } else if (type->kind == "media") {
        if (m.mediaKind(type->backingUid) == Kind::Audio) {
            output(0.0f);   // no audio analyser in the prototype
        } else {
            gl::Fbo& target = fbo("n:" + toString(nodeUid), w, h);
            output(fit(mediaTexture(doc, type->backingUid), target));
        }
    } else if (type->kind == "converter") {
        const Converter* conv = m.converters.find(type->backingUid);
        ParamType to = conv ? paramTypeFromString(conv->toType) : ParamType::Float;
        output(convert(to, in, in["in"], outDecl, "cn:" + toString(nodeUid), w, h));
    } else if (type->label == "LFO") {
        float rate = toNumber(in["rate"]), phase = toNumber(in["phase"]);
        int shape = (int)toNumber(in["shape"]);
        float lo = toNumber(in["min"]), hi = toNumber(in["max"]);
        output(lo + (hi - lo) * lfo(shape, time * rate + phase));
    } else if (type->label == "Value") {
        output(toNumber(in["value"]));
    } else if (type->label == "Color") {
        output(toColor(in["color"]));
    } else if (type->label == "Output") {
        if (!result.tex)
            if (auto t = std::get_if<TexRef>(&in["image"])) result = *t;
    }
}

} // namespace tessera
