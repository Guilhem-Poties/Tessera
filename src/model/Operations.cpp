#include "model/Operations.h"

#include "model/Builtins.h"
#include "model/Declarations.h"
#include "model/ShaderFormat.h"

#include <algorithm>
#include <functional>
#include <set>

namespace tessera::ops {

namespace {

void fail(std::string* why, const std::string& text) {
    if (why) *why = text;
}

void removeLink(Document& doc, const Uid& linkUid) {
    Model& m = doc.model;
    m.params.eraseIf([&](const Parameter& p) { return p.ownerUid == linkUid; });
    m.links.erase(linkUid);
}

void removeLinksTouching(Document& doc, const Uid& paramUid) {
    std::vector<Uid> dead;
    for (const Link& l : doc.model.links)
        if (l.sourceParam == paramUid || l.targetParam == paramUid) dead.push_back(l.uid);
    for (const Uid& u : dead) removeLink(doc, u);
}

void removeParamsOf(Document& doc, const Uid& ownerUid) {
    Model& m = doc.model;
    std::vector<Uid> owned;
    for (const Parameter& p : m.params)
        if (p.ownerUid == ownerUid) owned.push_back(p.uid);
    for (const Uid& p : owned) {
        removeLinksTouching(doc, p);
        for (CuePoint& c : m.cuePoints)
            if (c.parameterUid == p) c.parameterUid = {};
    }
    m.params.eraseIf([&](const Parameter& p) { return p.ownerUid == ownerUid; });
}

Uid ownerNode(const Model& m, const Uid& paramUid) {
    const Parameter* p = m.params.find(paramUid);
    return p && m.kindOf(p->ownerUid) == Kind::Node ? p->ownerUid : Uid{};
}

std::string fileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

} // namespace

Document newProject(const std::string& name) {
    Document doc;
    Model& m = doc.model;
    Uid root = newUid(), timeline = newUid();
    m.projects.add({newUid(), name, root, timeline});
    m.folders.add({root, name, ".", {}});
    m.timelines.add({timeline});
    ensureBuiltins(m);
    addTrack(doc);
    addTrack(doc);
    return doc;
}

void syncParams(Document& doc, const Uid& ownerUid) {
    Model& m = doc.model;
    std::vector<ParamDecl> decls = ownerDecls(doc, ownerUid);
    std::set<std::string> names;
    for (const ParamDecl& d : decls) {
        names.insert(d.name);
        std::string type = toString(d.type);
        std::string direction = d.output ? "out" : "in";
        Parameter* p = m.findParam(ownerUid, d.name);
        if (!p) {
            m.params.add({newUid(), ownerUid, d.name, type, direction, moduleFor(d.type).defaultValue(d), d.linkable});
            continue;
        }
        if (p->type != type || p->direction != direction) {
            Uid puid = p->uid;
            p->type = type;
            p->direction = direction;
            p->value = moduleFor(d.type).defaultValue(d);
            removeLinksTouching(doc, puid);   // may invalidate p
            p = m.params.find(puid);
        }
        p->linkable = d.linkable;
    }
    std::vector<Uid> extra;
    for (const Parameter& p : m.params)
        if (p.ownerUid == ownerUid && !names.count(p.name)) extra.push_back(p.uid);
    for (const Uid& u : extra) {
        removeLinksTouching(doc, u);
        for (CuePoint& c : m.cuePoints)
            if (c.parameterUid == u) c.parameterUid = {};
        m.params.erase(u);
    }
}

// ── Graph ──────────────────────────────────────────────────
Uid addGraph(Document& doc, const std::string& name) {
    Uid uid = newUid();
    doc.model.graphs.add({uid, name, {640, 360}});
    addNode(doc, uid, nameUid("nodetype:builtin:Output"));
    return uid;
}

Uid addNode(Document& doc, const Uid& graphUid, const Uid& nodeTypeUid) {
    Model& m = doc.model;
    if (!m.graphs.contains(graphUid) || !m.nodeTypes.contains(nodeTypeUid)) return {};
    Uid uid = newUid();
    m.nodes.add({uid, graphUid, nodeTypeUid});
    syncParams(doc, uid);
    return uid;
}

Uid nodeTypeForAsset(const Model& m, const Uid& uid) {
    switch (m.kindOf(uid)) {
        case Kind::NodeType: return uid;
        case Kind::Shader:
        case Kind::Media:
        case Kind::Converter: return m.nodeTypeFor(uid);
        default: return {};
    }
}

bool canLink(const Document& doc, Uid from, Uid to, std::string* why) {
    const Model& m = doc.model;
    const Parameter* a = m.params.find(from);
    const Parameter* b = m.params.find(to);
    if (!a || !b) return fail(why, "Unknown parameter"), false;
    if (!a->isOutput() && b->isOutput()) std::swap(a, b), std::swap(from, to);
    if (!a->isOutput() || b->isOutput()) return fail(why, "Links go from an output to an input"), false;
    Uid srcNode = ownerNode(m, from), dstNode = ownerNode(m, to);
    if (srcNode.is_nil() || dstNode.is_nil()) return fail(why, "Only node parameters can be linked"), false;
    if (srcNode == dstNode) return fail(why, "Can't link a node to itself"), false;
    if (m.nodes.find(srcNode)->graphUid != m.nodes.find(dstNode)->graphUid)
        return fail(why, "Both nodes must be in the same graph"), false;
    if (!b->linkable || b->paramType() == ParamType::Label) return fail(why, "This input is not linkable"), false;

    // Refuse loops: is the source node reachable from the target node?
    std::function<bool(const Uid&, std::set<Uid>&)> reaches = [&](const Uid& node, std::set<Uid>& seen) {
        if (node == srcNode) return true;
        if (!seen.insert(node).second) return false;
        for (const Link& l : m.links) {
            if (l.targetParam == to) continue;   // replaced by the new link
            if (ownerNode(m, l.sourceParam) == node && reaches(ownerNode(m, l.targetParam), seen)) return true;
        }
        return false;
    };
    std::set<Uid> seen;
    if (reaches(dstNode, seen)) return fail(why, "This link would create a loop"), false;
    return true;
}

Uid link(Document& doc, Uid from, Uid to, std::string* why) {
    if (!canLink(doc, from, to, why)) return {};
    Model& m = doc.model;
    if (m.params.find(from)->direction != "out") std::swap(from, to);
    if (const Link* existing = m.linkInto(to)) removeLink(doc, existing->uid);

    const Parameter* src = m.params.find(from);
    const Parameter* dst = m.params.find(to);
    ParamType srcType = src->paramType(), dstType = dst->paramType();
    // Every link runs through the converter for the input's type, or for
    // the source's type when the input is `any`.
    ParamType convType = dstType == ParamType::Any ? srcType : dstType;
    const Converter* conv = m.converterFor(convType);
    if (!conv) return fail(why, "No converter to " + std::string(toString(convType))), Uid{};

    Uid uid = newUid();
    Uid graph = m.nodes.find(ownerNode(m, to))->graphUid;
    m.links.add({uid, from, to, conv->uid, graph});
    syncParams(doc, uid);

    // Sensible converter defaults from the declarations at both ends.
    auto srcDecl = declFor(doc, *m.params.find(from));
    auto dstDecl = declFor(doc, *m.params.find(to));
    auto set = [&](const char* name, Value v) {
        if (Parameter* p = m.findParam(uid, name)) p->value = std::move(v);
    };
    if (convType == ParamType::Float && dstType == ParamType::Float && dstDecl) {
        set("outMin", dstDecl->min);
        set("outMax", dstDecl->max);
    }
    if (convType == ParamType::Choice && srcType == ParamType::Choice) set("mode", 1);   // Index
    (void)srcDecl;
    return uid;
}

// ── Delete ─────────────────────────────────────────────────
bool deleteEntity(Document& doc, const Uid& uid, std::string* why) {
    Model& m = doc.model;
    switch (m.kindOf(uid)) {
        case Kind::Node: {
            const NodeType* t = m.nodeTypes.find(m.nodes.find(uid)->typeUid);
            if (t && t->kind == "builtin" && t->label == "Output") {
                int outputs = 0;
                Uid g = m.nodes.find(uid)->graphUid;
                for (const Node& n : m.nodes)
                    if (n.graphUid == g && n.typeUid == t->uid) ++outputs;
                if (outputs <= 1) return fail(why, "A graph keeps its Output node"), false;
            }
            removeParamsOf(doc, uid);
            m.nodes.erase(uid);
            return true;
        }
        case Kind::Link: removeLink(doc, uid); return true;
        case Kind::Graph: {
            std::vector<Uid> dead;
            for (const Node& n : m.nodes)
                if (n.graphUid == uid) dead.push_back(n.uid);
            for (const Uid& n : dead) {
                removeParamsOf(doc, n);
                m.nodes.erase(n);
            }
            std::vector<Uid> clips;
            for (const Clip& c : m.clips)
                if (c.sourceUid == uid) clips.push_back(c.uid);
            for (const Uid& c : clips) deleteEntity(doc, c);
            m.graphs.erase(uid);
            return true;
        }
        case Kind::Clip: {
            std::vector<Uid> fx;
            for (const Effect& e : m.effects)
                if (e.ownerUid == uid) fx.push_back(e.uid);
            for (const Uid& e : fx) deleteEntity(doc, e);
            removeParamsOf(doc, uid);
            m.clips.erase(uid);
            return true;
        }
        case Kind::Effect: {
            removeParamsOf(doc, uid);
            m.effects.erase(uid);
            return true;
        }
        case Kind::Track: {
            std::vector<Uid> clips;
            for (const Clip& c : m.clips)
                if (c.trackUid == uid) clips.push_back(c.uid);
            for (const Uid& c : clips) deleteEntity(doc, c);
            m.tracks.erase(uid);
            return true;
        }
        case Kind::Marker: {
            removeParamsOf(doc, uid);
            m.cuePoints.erase(uid);
            m.holdPoints.erase(uid);
            m.markers.erase(uid);
            return true;
        }
        case Kind::Loop: m.loops.erase(uid); return true;
        case Kind::Shader:
        case Kind::Media: {
            bool shader = m.kindOf(uid) == Kind::Shader;
            std::string path = shader ? m.shaders.find(uid)->path : m.media.find(uid)->path;
            if (isBuiltinPath(path)) return fail(why, "Built-ins are read-only"), false;
            Uid type = m.nodeTypeFor(uid);
            std::vector<Uid> dead;
            for (const Node& n : m.nodes)
                if (n.typeUid == type) dead.push_back(n.uid);
            for (const Effect& e : m.effects)
                if (e.shaderUid == uid) dead.push_back(e.uid);
            for (const Clip& c : m.clips)
                if (c.sourceUid == uid) dead.push_back(c.uid);
            for (const Uid& d : dead) {
                if (m.kindOf(d) == Kind::Node) {
                    removeParamsOf(doc, d);
                    m.nodes.erase(d);
                } else {
                    deleteEntity(doc, d);
                }
            }
            m.nodeTypes.erase(type);
            if (shader) {
                m.shaders.erase(uid);
            } else {
                m.images.erase(uid);
                m.videos.erase(uid);
                m.audios.erase(uid);
                m.media.erase(uid);
            }
            doc.files.erase(path);
            return true;
        }
        case Kind::Folder: {
            const Folder* f = m.folders.find(uid);
            if (f->parentUid.is_nil()) return fail(why, "The root folder can't be deleted"), false;
            std::vector<Uid> contents;
            for (const Folder& c : m.folders)
                if (c.parentUid == uid) contents.push_back(c.uid);
            for (const Shader& s : m.shaders)
                if (s.folderUid == uid) contents.push_back(s.uid);
            for (const Media& md : m.media)
                if (md.folderUid == uid) contents.push_back(md.uid);
            for (const Uid& c : contents) deleteEntity(doc, c);
            m.folders.erase(uid);
            return true;
        }
        case Kind::None: return fail(why, "Nothing selected"), false;
        default: return fail(why, std::string(toString(m.kindOf(uid))) + " rows can't be deleted"), false;
    }
}

// ── Assets ─────────────────────────────────────────────────
std::string folderDir(const Model& m, const Uid& folderUid) {
    const Folder* f = m.folders.find(folderUid);
    if (!f || f->localPath == "." || f->localPath.empty()) return "";
    return f->localPath + "/";
}

std::string uniquePath(const Document& doc, const std::string& path) {
    auto taken = [&](const std::string& p) {
        if (doc.files.count(p)) return true;
        for (const Shader& s : doc.model.shaders)
            if (s.path == p) return true;
        for (const Media& md : doc.model.media)
            if (md.path == p) return true;
        return false;
    };
    if (!taken(path)) return path;
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) dot = path.size();
    for (int i = 2;; ++i) {
        std::string p = path.substr(0, dot) + "-" + std::to_string(i) + path.substr(dot);
        if (!taken(p)) return p;
    }
}

Uid newFolder(Document& doc, const Uid& parentUid, const std::string& label) {
    Model& m = doc.model;
    Uid parent = m.folders.contains(parentUid) ? parentUid : m.project()->rootFolderUid;
    Uid uid = newUid();
    m.folders.add({uid, label, folderDir(m, parent) + label, parent});
    return uid;
}

Uid addShaderFile(Document& doc, const Uid& folderUid, const std::string& name, const std::string& source) {
    Model& m = doc.model;
    Uid folder = m.folders.contains(folderUid) ? folderUid : m.project()->rootFolderUid;
    std::string path = uniquePath(doc, folderDir(m, folder) + name);
    doc.files[path] = std::make_shared<const std::string>(source);
    ShaderInfo info = parseTsh(source);
    Uid uid = newUid();
    m.shaders.add({uid, info.ok ? info.label : fileName(path), path, folder});
    m.nodeTypes.add({newUid(), info.ok ? info.label : fileName(path), info.ok ? info.category : "Generator", "shader", uid});
    return uid;
}

Uid newShader(Document& doc, const Uid& folderUid) {
    return addShaderFile(doc, folderUid, "new-shader.tsh", newShaderTemplate());
}

Uid copyShaderIntoProject(Document& doc, const Uid& shaderUid, const Uid& folderUid) {
    const std::string* source = shaderSource(doc, shaderUid);
    const Shader* s = doc.model.shaders.find(shaderUid);
    if (!source || !s) return {};
    return addShaderFile(doc, folderUid, fileName(s->path), *source);
}

bool setShaderSource(Document& doc, const Uid& shaderUid, const std::string& source, std::string* error) {
    Model& m = doc.model;
    Shader* s = m.shaders.find(shaderUid);
    if (!s) return fail(error, "Unknown shader"), false;
    if (isBuiltinPath(s->path)) return fail(error, "Built-in shaders are read-only: copy it into the project"), false;
    doc.files[s->path] = std::make_shared<const std::string>(source);
    ShaderInfo info = parseTsh(source);
    if (!info.ok) return fail(error, info.error), false;

    s->label = info.label;
    if (NodeType* t = m.nodeTypes.find(m.nodeTypeFor(shaderUid))) {
        t->label = info.label;
        t->category = info.category;
    }
    std::vector<Uid> owners;
    for (const Node& n : m.nodes) {
        const NodeType* t = m.nodeTypes.find(n.typeUid);
        if (t && t->backingUid == shaderUid) owners.push_back(n.uid);
    }
    for (const Effect& e : m.effects)
        if (e.shaderUid == shaderUid) owners.push_back(e.uid);
    for (const Clip& c : m.clips)
        if (c.sourceUid == shaderUid) owners.push_back(c.uid);
    for (const Uid& o : owners) syncParams(doc, o);
    return true;
}

Uid addMedia(Document& doc, const Uid& folderUid, const std::string& path, Kind kind, int width, int height,
             float duration, FileData data) {
    Model& m = doc.model;
    Uid folder = m.folders.contains(folderUid) ? folderUid : m.project()->rootFolderUid;
    Uid uid = newUid();
    m.media.add({uid, path, folder});
    if (kind == Kind::Image) m.images.add({uid, width, height});
    else if (kind == Kind::Video) m.videos.add({uid, duration});
    else m.audios.add({uid, duration});
    if (data) doc.files[path] = std::move(data);
    m.nodeTypes.add({newUid(), fileName(path), "Media", "media", uid});
    return uid;
}

// ── Timeline ───────────────────────────────────────────────
Uid addTrack(Document& doc) {
    Model& m = doc.model;
    int order = 0;
    for (const Track* t : m.tracksBottomToTop()) order = std::max(order, t->order + 1);
    Uid uid = newUid();
    m.tracks.add({uid, order, 1.0f, BlendMode::Normal, m.project()->timelineUid});
    return uid;
}

Uid addClip(Document& doc, const Uid& trackUid, const Uid& sourceUid, float start, float duration) {
    Model& m = doc.model;
    Kind k = m.kindOf(sourceUid);
    if (!m.tracks.contains(trackUid) || (k != Kind::Graph && k != Kind::Media && k != Kind::Shader)) return {};
    if (k == Kind::Shader && !isFilterShader(doc, sourceUid)) return {};
    Clip c;
    c.uid = newUid();
    c.sourceUid = sourceUid;
    c.start = std::max(0.0f, start);
    c.duration = std::max(0.25f, duration);
    c.trackUid = trackUid;
    m.clips.add(c);
    syncParams(doc, c.uid);
    return c.uid;
}

Uid addEffect(Document& doc, const Uid& clipUid, const Uid& shaderUid) {
    Model& m = doc.model;
    if (!m.clips.contains(clipUid) || !isFilterShader(doc, shaderUid)) return {};
    int order = 0;
    for (const Effect& e : m.effects)
        if (e.ownerUid == clipUid) order = std::max(order, e.order + 1);
    Uid uid = newUid();
    m.effects.add({uid, shaderUid, order, clipUid});
    syncParams(doc, uid);
    return uid;
}

void moveEffect(Document& doc, const Uid& effectUid, int delta) {
    Model& m = doc.model;
    const Effect* e = m.effects.find(effectUid);
    if (!e) return;
    std::vector<Effect*> chain;
    for (Effect& x : m.effects)
        if (x.ownerUid == e->ownerUid) chain.push_back(&x);
    std::sort(chain.begin(), chain.end(), [](Effect* a, Effect* b) { return a->order < b->order; });
    auto it = std::find_if(chain.begin(), chain.end(), [&](Effect* x) { return x->uid == effectUid; });
    int i = (int)(it - chain.begin()), j = i + delta;
    if (j < 0 || j >= (int)chain.size()) return;
    std::swap(chain[i], chain[j]);
    for (int k = 0; k < (int)chain.size(); ++k) chain[k]->order = k;
}

Uid addMarker(Document& doc, float position, Kind kind, const std::string& label) {
    Model& m = doc.model;
    Uid uid = newUid();
    m.markers.add({uid, std::max(0.0f, position), m.project()->timelineUid});
    if (kind == Kind::CuePoint) m.cuePoints.add({uid, {}, 0.5f});
    if (kind == Kind::HoldPoint) m.holdPoints.add({uid, 2.0f});
    syncParams(doc, uid);
    if (!label.empty()) m.findParam(uid, "label")->value = label;
    return uid;
}

Uid addLoop(Document& doc, float start, float end) {
    Uid uid = newUid();
    doc.model.loops.add({uid, start, std::max(start + 0.25f, end)});
    return uid;
}

} // namespace tessera::ops
