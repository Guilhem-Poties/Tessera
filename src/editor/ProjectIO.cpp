#include "editor/ProjectIO.h"

#include "model/Builtins.h"
#include "model/Operations.h"
#include "model/Serialize.h"

#include <stb_image.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace tessera {

namespace {

bool readFile(const fs::path& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool writeFile(const fs::path& path, const std::string& data) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    out.write(data.data(), (std::streamsize)data.size());
    return (bool)out;
}

std::string extension(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext;
}

bool isProjectRelative(const std::string& path) {
    return !path.empty() && !isBuiltinPath(path) && !fs::path(path).is_absolute();
}

Kind mediaKindForExtension(const std::string& ext) {
    for (const char* e : {".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif", ".psd", ".hdr"})
        if (ext == e) return Kind::Image;
    for (const char* e : {".mp4", ".mov", ".webm", ".mkv", ".avi", ".m4v"})
        if (ext == e) return Kind::Video;
    for (const char* e : {".wav", ".mp3", ".ogg", ".flac", ".aif", ".aiff", ".m4a"})
        if (ext == e) return Kind::Audio;
    return Kind::None;
}

} // namespace

bool isMediaFile(const std::string& path) { return mediaKindForExtension(extension(path)) != Kind::None; }

bool isShaderFile(const std::string& path) {
    std::string ext = extension(path);
    return ext == ".tsh" || ext == ".glsl" || ext == ".frag";
}

bool saveProject(const Document& doc, const EditorState& ed, const std::string& dir, std::string& error) {
    json positions = json::object();
    for (const auto& [uid, pos] : ed.nodePositions)
        if (doc.model.nodes.contains(uid)) positions[toString(uid)] = {pos.x, pos.y};

    json root = {
        {"format", "tessera"},
        {"version", 1},
        {"model", modelToJson(doc.model)},
        {"editor", {{"nodePositions", positions}}},
    };
    fs::path base(dir);
    if (!writeFile(base / kProjectFileName, root.dump(2))) {
        error = "Can't write " + (base / kProjectFileName).string();
        return false;
    }
    for (const auto& [path, data] : doc.files) {
        if (!data || !isProjectRelative(path)) continue;
        if (!writeFile(base / fs::path(path), *data)) {
            error = "Can't write " + (base / path).string();
            return false;
        }
    }
    return true;
}

bool loadProject(const std::string& path, Document& doc, EditorState& ed, std::string& error) {
    fs::path file(path);
    if (fs::is_directory(file)) file /= kProjectFileName;
    std::string text;
    if (!readFile(file, text)) {
        error = "Can't read " + file.string();
        return false;
    }
    json root;
    try {
        root = json::parse(text);
    } catch (const json::exception& e) {
        error = std::string("Invalid project file: ") + e.what();
        return false;
    }
    if (root.value("format", std::string()) != "tessera" || !root.contains("model")) {
        error = "Not a Tessera project";
        return false;
    }
    Document loaded;
    if (!modelFromJson(root["model"], loaded.model, error)) return false;
    if (!loaded.model.project()) {
        error = "The project has no Project row";
        return false;
    }
    ensureBuiltins(loaded.model);

    // Project files: user shaders and copied images.
    fs::path base = file.parent_path();
    std::vector<std::string> missing;
    auto load = [&](const std::string& p) {
        if (!isProjectRelative(p)) return;
        std::string data;
        if (readFile(base / fs::path(p), data)) loaded.files[p] = std::make_shared<const std::string>(std::move(data));
        else missing.push_back(p);
    };
    for (const Shader& s : loaded.model.shaders) load(s.path);
    for (const Media& m : loaded.model.media) load(m.path);

    EditorState fresh;
    if (root.contains("editor") && root["editor"].contains("nodePositions"))
        for (const auto& [key, value] : root["editor"]["nodePositions"].items())
            if (value.is_array() && value.size() == 2)
                fresh.nodePositions[uidFromString(key)] = {value[0].get<float>(), value[1].get<float>()};
    for (const Graph& g : loaded.model.graphs) {
        fresh.openGraphs.push_back(g.uid);
        break;
    }
    if (!fresh.openGraphs.empty()) fresh.focusGraph = fresh.openGraphs.front();
    fresh.projectPath = base.string();
    fresh.selectedFolder = loaded.model.project()->rootFolderUid;

    doc = std::move(loaded);
    ed = std::move(fresh);
    if (!missing.empty()) error = "Missing project files: " + missing.front() + (missing.size() > 1 ? "..." : "");
    return true;
}

Uid importMedia(Document& doc, const Uid& folderUid, const std::string& filePath, std::string& error) {
    Kind kind = mediaKindForExtension(extension(filePath));
    if (kind == Kind::None) {
        error = "Unsupported media type: " + filePath;
        return {};
    }
    std::error_code ec;
    auto size = fs::file_size(filePath, ec);
    if (ec) {
        error = "Can't read " + filePath;
        return {};
    }
    std::string absolute = fs::absolute(filePath).generic_string();
    if (kind != Kind::Image) {
        // No decoder in the prototype: the duration is a placeholder.
        return ops::addMedia(doc, folderUid, absolute, kind, 0, 0, 10.0f, nullptr);
    }
    std::string data;
    if (!readFile(filePath, data)) {
        error = "Can't read " + filePath;
        return {};
    }
    int w = 0, h = 0, comp = 0;
    if (!stbi_info_from_memory((const stbi_uc*)data.data(), (int)data.size(), &w, &h, &comp)) {
        error = "Can't decode " + filePath;
        return {};
    }
    if (size < 1536 * 1024) {
        Uid folder = doc.model.folders.contains(folderUid) ? folderUid : doc.model.project()->rootFolderUid;
        std::string path = ops::uniquePath(doc, ops::folderDir(doc.model, folder) + fs::path(filePath).filename().string());
        return ops::addMedia(doc, folder, path, kind, w, h, 0, std::make_shared<const std::string>(std::move(data)));
    }
    return ops::addMedia(doc, folderUid, absolute, kind, w, h, 0, nullptr);
}

Uid importShader(Document& doc, const Uid& folderUid, const std::string& filePath, std::string& error) {
    std::string source;
    if (!readFile(filePath, source)) {
        error = "Can't read " + filePath;
        return {};
    }
    return ops::addShaderFile(doc, folderUid, fs::path(filePath).filename().string(), source);
}

Document makeDemoProject(EditorState& ed) {
    Document doc = ops::newProject("Demo");
    Model& m = doc.model;
    auto type = [](const char* builtinPath) { return nameUid(std::string("nodetype:") + builtinPath); };
    auto builtin = [](const char* label) { return nameUid(std::string("nodetype:builtin:") + label); };
    auto shader = [](const char* builtinPath) { return nameUid(std::string("shader:") + builtinPath); };
    auto param = [&](const Uid& node, const char* name) { return m.findParam(node, name)->uid; };
    auto outputOf = [&](const Uid& graph) {
        for (const Node& n : m.nodes)
            if (n.graphUid == graph && n.typeUid == builtin("Output")) return n.uid;
        return Uid{};
    };

    // Graph 1: Plasma → Kaleidoscope → Output, with an LFO on the segments.
    Uid g1 = ops::addGraph(doc, "Plasma scope");
    Uid plasma = ops::addNode(doc, g1, type("tessera://shaders/plasma.tsh"));
    Uid kaleido = ops::addNode(doc, g1, type("tessera://shaders/kaleidoscope.tsh"));
    Uid lfo = ops::addNode(doc, g1, builtin("LFO"));
    Uid out1 = outputOf(g1);
    ops::link(doc, param(plasma, "out"), param(kaleido, "image"));
    ops::link(doc, param(lfo, "value"), param(kaleido, "segments"));
    ops::link(doc, param(kaleido, "out"), param(out1, "image"));
    m.findParam(lfo, "rate")->value = 0.1f;
    ed.nodePositions[plasma] = {0, 0};
    ed.nodePositions[lfo] = {0, 230};
    ed.nodePositions[kaleido] = {240, 40};
    ed.nodePositions[out1] = {480, 60};

    // Graph 2: Shapes + Gradient → Mix → Tessellate → Output.
    Uid g2 = ops::addGraph(doc, "Shape grid");
    Uid shapes = ops::addNode(doc, g2, type("tessera://shaders/shapes.tsh"));
    Uid grad = ops::addNode(doc, g2, type("tessera://shaders/gradient.tsh"));
    Uid mix = ops::addNode(doc, g2, type("tessera://shaders/mix.tsh"));
    Uid tess = ops::addNode(doc, g2, type("tessera://shaders/tessellate.tsh"));
    Uid out2 = outputOf(g2);
    ops::link(doc, param(grad, "out"), param(mix, "a"));
    ops::link(doc, param(shapes, "out"), param(mix, "b"));
    ops::link(doc, param(mix, "out"), param(tess, "image"));
    ops::link(doc, param(tess, "out"), param(out2, "image"));
    m.findParam(mix, "mode")->value = 1;   // Add
    ed.nodePositions[grad] = {0, 0};
    ed.nodePositions[shapes] = {0, 220};
    ed.nodePositions[mix] = {240, 80};
    ed.nodePositions[tess] = {480, 80};
    ed.nodePositions[out2] = {720, 100};

    // Timeline: the two graphs, a hue shift effect clip on top, markers, a loop.
    auto tracks = m.tracksBottomToTop();
    Uid t1 = tracks[0]->uid, t2 = tracks[1]->uid;
    Uid t3 = ops::addTrack(doc);
    ops::addClip(doc, t1, g1, 0, 16);
    Uid c2 = ops::addClip(doc, t2, g2, 6, 12);
    m.clips.find(c2)->blendMode = BlendMode::Screen;
    m.clips.find(c2)->opacity = 0.8f;
    ops::addEffect(doc, c2, shader("tessera://shaders/pixelate.tsh"));
    Uid fx = ops::addClip(doc, t3, shader("tessera://shaders/hue-shift.tsh"), 10, 4);
    m.findParam(fx, "hue")->value = 0.5f;
    ops::addMarker(doc, 0, Kind::Marker, "Intro");
    ops::addMarker(doc, 6, Kind::HoldPoint, "Breath");
    ops::addLoop(doc, 0, 18);

    ed.openGraph(g1);
    ed.selectedFolder = m.project()->rootFolderUid;
    return doc;
}

} // namespace tessera
