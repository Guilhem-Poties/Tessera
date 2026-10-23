#pragma once

// The Tessera data model (architecture section 3).
//
// Pure data: flat tables of records keyed by uid, no GPU objects and no
// pointers between records. The whole model is a value: it can be copied for
// undo snapshots and serialised as is.

#include "core/Math.h"
#include "core/Uid.h"
#include "model/Params.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace tessera {

enum class BlendMode { Normal, Add, Multiply, Screen, Difference };
const char* toString(BlendMode mode);
BlendMode blendModeFromString(const std::string& text);
inline constexpr const char* kBlendModeNames[] = {"Normal", "Add", "Multiply", "Screen", "Difference"};

// Each record lists its fields through `fields(self, f)`, which calls
// f("name", self.name) for every field. Serialisation, the data model view and
// the reference finder are written once against this.
#define T_FIELD(name) f(#name, self.name)

// ── Project and Assets ─────────────────────────────────────
struct Project {
    Uid uid;
    std::string name;
    Uid rootFolderUid;   // -> Folder
    Uid timelineUid;     // -> Timeline
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(name); T_FIELD(rootFolderUid); T_FIELD(timelineUid);
    }
};

struct Folder {
    Uid uid;
    std::string label;
    std::string localPath;   // relative to the project, "." for the root
    Uid parentUid;           // -> Folder, null for the root
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(label); T_FIELD(localPath); T_FIELD(parentUid);
    }
};

struct Media {
    Uid uid;
    std::string path;   // tessera://media/... for built-ins
    Uid folderUid;      // -> Folder, null for built-ins
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(path); T_FIELD(folderUid);
    }
};

// "Is a" subtypes of Media: they share the uid of their Media row.
struct Image {
    Uid uid;
    int width = 0, height = 0;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(width); T_FIELD(height); }
};
struct Video {
    Uid uid;
    float duration = 0;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(duration); }
};
struct Audio {
    Uid uid;
    float duration = 0;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(duration); }
};

struct Shader {
    Uid uid;
    std::string label;   // read from the shader header
    std::string path;    // tessera://shaders/... for built-ins
    Uid folderUid;       // -> Folder, null for built-ins
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(label); T_FIELD(path); T_FIELD(folderUid);
    }
};

// ── Graph ──────────────────────────────────────────────────
struct Graph {
    Uid uid;
    std::string name;
    Vec2 resolution{640, 360};   // size of its node FBOs
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(name); T_FIELD(resolution); }
};

struct NodeType {
    Uid uid;
    std::string label;
    std::string category;   // Generator, Filter, Mixer, Value, Converter, Media, Output
    std::string kind;       // shader, media, converter or builtin
    Uid backingUid;         // -> Shader, Media or Converter; null for built-ins
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(label); T_FIELD(category); T_FIELD(kind); T_FIELD(backingUid);
    }
};

struct Node {
    Uid uid;
    Uid graphUid;   // -> Graph
    Uid typeUid;    // -> NodeType
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(graphUid); T_FIELD(typeUid); }
};

struct Link {
    Uid uid;
    Uid sourceParam;    // -> Parameter (an output)
    Uid targetParam;    // -> Parameter (an input)
    Uid converterUid;   // -> Converter, always set
    Uid graphUid;       // -> Graph
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(sourceParam); T_FIELD(targetParam); T_FIELD(converterUid); T_FIELD(graphUid);
    }
};

struct Converter {
    Uid uid;
    std::string label;      // "To float", "To color"...
    std::string fromType;   // always "any"
    std::string toType;     // one per parameter type
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(label); T_FIELD(fromType); T_FIELD(toType);
    }
};

// ── Parameters ─────────────────────────────────────────────
struct Parameter {
    Uid uid;
    Uid ownerUid;            // -> Node, Link, Clip, Effect or Marker
    std::string name;
    std::string type;        // see ParamType
    std::string direction;   // "in" or "out"
    Value value;
    bool linkable = true;
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(ownerUid); T_FIELD(name); T_FIELD(type); T_FIELD(direction); T_FIELD(value); T_FIELD(linkable);
    }
    ParamType paramType() const { return paramTypeFromString(type); }
    bool isOutput() const { return direction == "out"; }
};

// ── Timeline ───────────────────────────────────────────────
struct Timeline {
    Uid uid;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); }
};

struct Track {
    Uid uid;
    int order = 0;   // a higher order is drawn on top
    float opacity = 1.0f;
    BlendMode blendMode = BlendMode::Normal;
    Uid timelineUid;   // -> Timeline
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(order); T_FIELD(opacity); T_FIELD(blendMode); T_FIELD(timelineUid);
    }
};

inline constexpr const char* kStartModeFromClip = "From clip start";
inline constexpr const char* kStartModeContinuous = "Continuous";

struct Clip {
    Uid uid;
    Uid sourceUid;   // -> Graph, Media or Shader (effect clip)
    float start = 0, duration = 8;
    std::string startMode = kStartModeFromClip;
    BlendMode blendMode = BlendMode::Normal;   // overrides the track's mode when it isn't Normal
    float opacity = 1, speed = 1, scale = 1, rotation = 0;
    Uid trackUid;   // -> Track
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(sourceUid); T_FIELD(start); T_FIELD(duration); T_FIELD(startMode); T_FIELD(blendMode);
        T_FIELD(opacity); T_FIELD(speed); T_FIELD(scale); T_FIELD(rotation); T_FIELD(trackUid);
    }
};

struct Effect {
    Uid uid;
    Uid shaderUid;   // -> Shader with a single `image` texture input
    int order = 0;
    Uid ownerUid;    // -> Clip
    template <class S, class F> static void fields(S& self, F&& f) {
        T_FIELD(uid); T_FIELD(shaderUid); T_FIELD(order); T_FIELD(ownerUid);
    }
};

struct Marker {
    Uid uid;
    float position = 0;   // seconds
    Uid timelineUid;      // -> Timeline
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(position); T_FIELD(timelineUid); }
};

// "Is a" subtypes of Marker: they share the uid of their Marker row.
struct CuePoint {
    Uid uid;
    Uid parameterUid;   // -> Parameter
    float condition = 0.5f;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(parameterUid); T_FIELD(condition); }
};
struct HoldPoint {
    Uid uid;
    float duration = 2;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(duration); }
};

struct Loop {
    Uid uid;
    float start = 0, end = 8;
    template <class S, class F> static void fields(S& self, F&& f) { T_FIELD(uid); T_FIELD(start); T_FIELD(end); }
};

#undef T_FIELD

// ── Tables ─────────────────────────────────────────────────
// A table keeps its rows in insertion order and indexes them by uid.
// Pointers returned by find() are invalidated by add() and erase().
template <class T>
class Table {
public:
    std::vector<T> rows;

    T* find(const Uid& uid) {
        auto it = index_.find(uid);
        return it == index_.end() ? nullptr : &rows[it->second];
    }
    const T* find(const Uid& uid) const {
        auto it = index_.find(uid);
        return it == index_.end() ? nullptr : &rows[it->second];
    }
    bool contains(const Uid& uid) const { return index_.count(uid) != 0; }

    T& add(T row) {
        index_[row.uid] = rows.size();
        rows.push_back(std::move(row));
        return rows.back();
    }
    bool erase(const Uid& uid) {
        auto it = index_.find(uid);
        if (it == index_.end()) return false;
        rows.erase(rows.begin() + (std::ptrdiff_t)it->second);
        reindex();
        return true;
    }
    template <class Pred> size_t eraseIf(Pred pred) {
        size_t n = std::erase_if(rows, pred);
        if (n) reindex();
        return n;
    }
    void clear() { rows.clear(); index_.clear(); }
    void reindex() {
        index_.clear();
        for (size_t i = 0; i < rows.size(); ++i) index_[rows[i].uid] = i;
    }
    size_t size() const { return rows.size(); }
    auto begin() { return rows.begin(); }
    auto end() { return rows.end(); }
    auto begin() const { return rows.begin(); }
    auto end() const { return rows.end(); }

private:
    std::unordered_map<Uid, size_t> index_;
};

// Kinds reported by the uid registry.
enum class Kind {
    None, Project, Folder, Media, Image, Video, Audio, Shader, Graph, NodeType, Node, Link, Converter,
    Parameter, Timeline, Track, Clip, Effect, Marker, CuePoint, HoldPoint, Loop
};
const char* toString(Kind kind);

struct Model {
    Table<Project> projects;
    Table<Folder> folders;
    Table<Media> media;
    Table<Image> images;
    Table<Video> videos;
    Table<Audio> audios;
    Table<Shader> shaders;
    Table<Graph> graphs;
    Table<NodeType> nodeTypes;
    Table<Node> nodes;
    Table<Link> links;
    Table<Converter> converters;
    Table<Parameter> params;
    Table<Timeline> timelines;
    Table<Track> tracks;
    Table<Clip> clips;
    Table<Effect> effects;
    Table<Marker> markers;
    Table<CuePoint> cuePoints;
    Table<HoldPoint> holdPoints;
    Table<Loop> loops;

    // Calls f(kind, "TableName", table) for every table.
    template <class S, class F> static void forEachTable(S& self, F&& f) {
        f(Kind::Project, "Project", self.projects);
        f(Kind::Folder, "Folder", self.folders);
        f(Kind::Media, "Media", self.media);
        f(Kind::Image, "Image", self.images);
        f(Kind::Video, "Video", self.videos);
        f(Kind::Audio, "Audio", self.audios);
        f(Kind::Shader, "Shader", self.shaders);
        f(Kind::Graph, "Graph", self.graphs);
        f(Kind::NodeType, "NodeType", self.nodeTypes);
        f(Kind::Node, "Node", self.nodes);
        f(Kind::Link, "Link", self.links);
        f(Kind::Converter, "Converter", self.converters);
        f(Kind::Parameter, "Parameter", self.params);
        f(Kind::Timeline, "Timeline", self.timelines);
        f(Kind::Track, "Track", self.tracks);
        f(Kind::Clip, "Clip", self.clips);
        f(Kind::Effect, "Effect", self.effects);
        f(Kind::Marker, "Marker", self.markers);
        f(Kind::CuePoint, "CuePoint", self.cuePoints);
        f(Kind::HoldPoint, "HoldPoint", self.holdPoints);
        f(Kind::Loop, "Loop", self.loops);
    }

    // ── Global uid → object registry ──
    // Resolves polymorphic references (Parameter.ownerUid, Effect.ownerUid,
    // Clip.sourceUid, NodeType.backingUid). For "is a" subtypes the base kind
    // is reported (Media, Marker); use mediaKind()/markerKind() for the subtype.
    Kind kindOf(const Uid& uid) const;
    Kind mediaKind(const Uid& mediaUid) const;    // Image, Video, Audio or None
    Kind markerKind(const Uid& markerUid) const;  // CuePoint, HoldPoint or Marker
    std::string displayName(const Uid& uid) const;

    // ── Queries ──
    const Project* project() const { return projects.rows.empty() ? nullptr : &projects.rows.front(); }
    Parameter* findParam(const Uid& ownerUid, const std::string& name);
    const Parameter* findParam(const Uid& ownerUid, const std::string& name) const;
    const Link* linkInto(const Uid& targetParam) const;
    Uid nodeTypeFor(const Uid& backingUid) const;   // the NodeType backed by a Shader/Media/Converter
    const Converter* converterFor(ParamType toType) const;
    std::vector<const Track*> tracksBottomToTop() const;
};

inline bool isBuiltinPath(const std::string& path) { return path.rfind("tessera://", 0) == 0; }

// Project files: user shader sources and copied images, by project-relative
// path. Contents are immutable and shared between undo snapshots.
using FileData = std::shared_ptr<const std::string>;
using Files = std::map<std::string, FileData>;

// What the UI edits and what a snapshot restores.
struct Document {
    Model model;
    Files files;
};

} // namespace tessera
