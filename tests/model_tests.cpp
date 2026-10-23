// Tests for the data model and its operations. Pure model code: no window,
// no OpenGL context.

#include "editor/History.h"
#include "editor/ProjectIO.h"
#include "model/Builtins.h"
#include "model/Declarations.h"
#include "model/Operations.h"
#include "model/Serialize.h"
#include "model/ShaderFormat.h"
#include "runtime/Playback.h"
#include "runtime/Runtime.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace tessera;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        ++g_checks;                                                                  \
        if (!(cond)) {                                                               \
            ++g_failures;                                                            \
            std::printf("  FAILED %s:%d: %s\n", __FILE__, __LINE__, #cond);          \
        }                                                                            \
    } while (0)

static Uid builtinType(const char* label) { return nameUid(std::string("nodetype:builtin:") + label); }
static Uid shaderType(const char* file) { return nameUid(std::string("nodetype:tessera://shaders/") + file); }
static Uid builtinShader(const char* file) { return nameUid(std::string("shader:tessera://shaders/") + file); }
static Uid param(const Document& d, const Uid& owner, const char* name) { return d.model.findParam(owner, name)->uid; }

static void testBuiltins() {
    Document d = ops::newProject("t");
    CHECK(d.model.shaders.size() == builtinShaders().size());
    CHECK(d.model.converters.size() == convertibleTypes().size());
    for (const Shader& s : d.model.shaders) {
        const ShaderInfo* info = shaderInfo(d, s.uid);
        CHECK(info && info->ok);
        CHECK(s.folderUid.is_nil());
    }
    CHECK(isFilterShader(d, builtinShader("hue-shift.tsh")));
    CHECK(!isFilterShader(d, builtinShader("plasma.tsh")));
    CHECK(shaderInfo(d, builtinShader("mix.tsh"))->role == ShaderRole::Mixer);
    // ensureBuiltins is idempotent
    size_t before = d.model.nodeTypes.size();
    ensureBuiltins(d.model);
    CHECK(d.model.nodeTypes.size() == before);
}

static void testShaderFormat() {
    ShaderInfo ok = parseTsh(newShaderTemplate());
    CHECK(ok.ok);
    CHECK(ok.inputs.size() == 2);
    CHECK(ok.bodyLine == 7);
    CHECK(buildFragmentSource(ok).find("#line 7") != std::string::npos);
    ShaderInfo bad = parseTsh("/* @tessera { \"label\": \"x\", } */ vec4 render(vec2 uv){ return vec4(1); }");
    CHECK(!bad.ok && bad.error.find("line 1") == 0);
    ShaderInfo badName = parseTsh("/* @tessera {\"inputs\":[{\"name\":\"1x\",\"type\":\"float\"}]} */ vec4 render(vec2 uv){return vec4(0);}");
    CHECK(!badName.ok);
    ShaderInfo choice = parseTsh(R"(/* @tessera {"inputs":[{"name":"m","type":"choice","options":["a","b"],"default":"b"}]} */
vec4 render(vec2 uv){ return vec4(0); })");
    CHECK(choice.ok && std::get<int>(choice.inputs[0].def) == 1);
}

static void testLinking() {
    Document d = ops::newProject("t");
    Uid g = ops::addGraph(d, "g");
    Uid plasma = ops::addNode(d, g, shaderType("plasma.tsh"));
    Uid kaleido = ops::addNode(d, g, shaderType("kaleidoscope.tsh"));
    Uid lfo = ops::addNode(d, g, builtinType("LFO"));
    CHECK(d.model.findParam(plasma, "out") && d.model.findParam(kaleido, "segments"));

    // Every link runs through the converter for the input's type.
    Uid l1 = ops::link(d, param(d, plasma, "out"), param(d, kaleido, "image"));
    CHECK(!l1.is_nil());
    CHECK(d.model.converters.find(d.model.links.find(l1)->converterUid)->toType == "texture");
    // Parameters may be passed input first.
    Uid l2 = ops::link(d, param(d, kaleido, "segments"), param(d, lfo, "value"));
    CHECK(!l2.is_nil());
    const Link* link2 = d.model.links.find(l2);
    CHECK(link2->sourceParam == param(d, lfo, "value"));
    // To float settings are Parameters owned by the Link, with outMin/outMax from the target range.
    const Parameter* outMax = d.model.findParam(l2, "outMax");
    CHECK(outMax && valueAsFloat(outMax->value) == 16.0f);

    // Loops are refused.
    std::string why;
    CHECK(ops::link(d, param(d, kaleido, "out"), param(d, plasma, "speed"), &why).is_nil());
    CHECK(why.find("loop") != std::string::npos);
    // Same node, output → output, labels…
    CHECK(!ops::canLink(d, param(d, plasma, "out"), param(d, plasma, "speed")));
    CHECK(!ops::canLink(d, param(d, plasma, "out"), param(d, lfo, "value")));

    // One link per input: linking again replaces it.
    Uid noise = ops::addNode(d, g, shaderType("noise.tsh"));
    Uid l3 = ops::link(d, param(d, noise, "out"), param(d, kaleido, "image"));
    CHECK(!l3.is_nil() && !d.model.links.contains(l1));
    CHECK(d.model.findParam(l1, "gain") == nullptr);   // the old link's settings are gone

    // `any` input on a converter node uses the converter of the source's type.
    Uid toColor = ops::addNode(d, g, nameUid("nodetype:converter:color"));
    Uid l4 = ops::link(d, param(d, lfo, "value"), param(d, toColor, "in"));
    CHECK(d.model.converters.find(d.model.links.find(l4)->converterUid)->toType == "float");

    // Links across graphs are refused.
    Uid g2 = ops::addGraph(d, "g2");
    Uid other = ops::addNode(d, g2, shaderType("pixelate.tsh"));
    CHECK(!ops::canLink(d, param(d, noise, "out"), param(d, other, "image")));
}

static void testDeleteCascade() {
    Document d = ops::newProject("t");
    Uid g = ops::addGraph(d, "g");
    Uid a = ops::addNode(d, g, shaderType("plasma.tsh"));
    Uid b = ops::addNode(d, g, shaderType("hue-shift.tsh"));
    Uid l = ops::link(d, param(d, a, "out"), param(d, b, "image"));
    Uid track = d.model.tracksBottomToTop().front()->uid;
    Uid clip = ops::addClip(d, track, g, 0, 4);
    Uid fx = ops::addEffect(d, clip, builtinShader("pixelate.tsh"));
    CHECK(!fx.is_nil() && d.model.findParam(fx, "size"));
    CHECK(ops::addEffect(d, clip, builtinShader("plasma.tsh")).is_nil());   // not a filter

    CHECK(ops::deleteEntity(d, a));
    CHECK(!d.model.links.contains(l));
    CHECK(d.model.findParam(l, "gain") == nullptr);
    for (const Parameter& p : d.model.params) CHECK(p.ownerUid != a);

    CHECK(ops::deleteEntity(d, g));   // graph → nodes, clips, effects
    CHECK(!d.model.clips.contains(clip) && !d.model.effects.contains(fx));
    CHECK(d.model.nodes.size() == 0);

    // Built-ins are read-only, the root folder stays.
    CHECK(!ops::deleteEntity(d, builtinShader("plasma.tsh")));
    CHECK(!ops::deleteEntity(d, d.model.project()->rootFolderUid));

    // Every remaining reference resolves.
    Model::forEachTable(d.model, [&](Kind, const char*, const auto& table) {
        for (const auto& r : table)
            for (const Reference& ref : referencesFrom(d.model, r.uid)) CHECK(d.model.kindOf(ref.target) != Kind::None);
    });
}

static void testShaderRecompile() {
    Document d = ops::newProject("t");
    Uid shader = ops::newShader(d, {});
    Uid type = d.model.nodeTypeFor(shader);
    Uid g = ops::addGraph(d, "g");
    Uid node = ops::addNode(d, g, type);
    Parameter* speed = d.model.findParam(node, "speed");
    CHECK(speed != nullptr);
    speed->value = 3.0f;
    Uid lfo = ops::addNode(d, g, builtinType("LFO"));
    ops::link(d, param(d, lfo, "value"), param(d, node, "speed"));

    // Rename the label, drop `tint`, add `size`: values are kept, links too.
    std::string src = R"(/* @tessera {"label":"Waves","category":"Generator","inputs":[
      {"name":"speed","type":"float","default":0.5,"min":0,"max":4},
      {"name":"size","type":"float","default":2}]} */
vec4 render(vec2 uv){ return vec4(uv * size * speed, 0.0, 1.0); })";
    CHECK(ops::setShaderSource(d, shader, src));
    CHECK(d.model.shaders.find(shader)->label == "Waves");
    CHECK(d.model.nodeTypes.find(type)->label == "Waves");
    CHECK(valueAsFloat(d.model.findParam(node, "speed")->value) == 3.0f);
    CHECK(d.model.findParam(node, "tint") == nullptr);
    CHECK(d.model.findParam(node, "size") != nullptr);
    CHECK(d.model.linkInto(param(d, node, "speed")) != nullptr);

    // A broken header keeps the previous parameters and reports a line.
    std::string error;
    CHECK(!ops::setShaderSource(d, shader, "/* @tessera {oops} */", &error));
    CHECK(error.find("line") != std::string::npos);
    CHECK(d.model.findParam(node, "size") != nullptr);

    // Built-ins can't be edited in place.
    CHECK(!ops::setShaderSource(d, builtinShader("plasma.tsh"), src));
}

static void testJsonRoundTrip() {
    EditorState ed;
    Document d = makeDemoProject(ed);
    json j = modelToJson(d.model);
    Model back;
    std::string error;
    CHECK(modelFromJson(j, back, error));
    CHECK(modelToJson(back) == j);
    CHECK(back.params.size() == d.model.params.size());
    // Parameter values come back with their types.
    for (const Parameter& p : d.model.params) {
        const Parameter* q = back.params.find(p.uid);
        CHECK(q && q->value.index() == p.value.index());
    }
}

static void testSaveLoad() {
    EditorState ed;
    Document d = makeDemoProject(ed);
    Uid shader = ops::newShader(d, {});
    auto dir = std::filesystem::temp_directory_path() / ("tessera-test-" + shortUid(newUid()));
    std::string error;
    CHECK(saveProject(d, ed, dir.string(), error));
    Document loaded;
    EditorState led;
    CHECK(loadProject(dir.string(), loaded, led, error));
    CHECK(error.empty());
    CHECK(modelToJson(loaded.model) == modelToJson(d.model));
    CHECK(loaded.files.size() == d.files.size());
    CHECK(*loaded.files.at(d.model.shaders.find(shader)->path) == *d.files.at(d.model.shaders.find(shader)->path));
    CHECK(led.nodePositions.size() == ed.nodePositions.size());
    std::filesystem::remove_all(dir);
}

static void testHistory() {
    EditorState ed;
    Document d = ops::newProject("t");
    History h;
    h.reset(d, ed);
    Uid g = ops::addGraph(d, "g");
    ed.nodePositions[newUid()] = {1, 2};
    h.commit(d, ed, "Add graph");
    CHECK(h.dirty());
    CHECK(h.undo(d, ed));
    CHECK(!d.model.graphs.contains(g));
    CHECK(ed.nodePositions.empty());
    CHECK(h.redo(d, ed));
    CHECK(d.model.graphs.contains(g));
    CHECK(!h.canRedo());
}

static void testTimelineOps() {
    Document d = ops::newProject("t");
    Uid track = ops::addTrack(d);
    CHECK(d.model.tracks.find(track)->order == 2);
    Uid fxClip = ops::addClip(d, track, builtinShader("kaleidoscope.tsh"), 1, 2);
    CHECK(!fxClip.is_nil());
    CHECK(d.model.findParam(fxClip, "segments") != nullptr);   // effect clip settings
    CHECK(d.model.findParam(fxClip, "image") == nullptr);
    CHECK(ops::addClip(d, track, builtinShader("noise.tsh"), 1, 2).is_nil());   // generators aren't clip sources yet
    Uid cue = ops::addMarker(d, 3, Kind::CuePoint, "Drop");
    CHECK(d.model.markerKind(cue) == Kind::CuePoint);
    CHECK(d.model.displayName(cue) == "Drop");
    Uid clip = ops::addClip(d, track, ops::addGraph(d, "g"), 0, 4);
    Uid e1 = ops::addEffect(d, clip, builtinShader("pixelate.tsh"));
    Uid e2 = ops::addEffect(d, clip, builtinShader("hue-shift.tsh"));
    ops::moveEffect(d, e2, -1);
    CHECK(d.model.effects.find(e2)->order == 0 && d.model.effects.find(e1)->order == 1);
}

static void testPlayback() {
    Document d = ops::newProject("t");
    EditorState ed;
    Playback pb;
    Runtime rt;   // never initialised: no live values, so cues read stored values

    // Hold points pause playback for their duration, then playback continues.
    Uid hold = ops::addMarker(d, 1.0f, Kind::HoldPoint);
    d.model.holdPoints.find(hold)->duration = 0.5f;
    pb.play(ed);
    for (int i = 0; i < 9; ++i) pb.update(d, ed, 0.125f);   // 1.125 s of frames
    CHECK(ed.playState == PlayState::Holding && ed.playhead == 1.0f);
    for (int i = 0; i < 5; ++i) pb.update(d, ed, 0.125f);
    CHECK(ed.playState == PlayState::Playing && ed.playhead > 1.0f);

    // Loops jump back to their start when loop playback is on.
    ops::addLoop(d, 2.0f, 3.0f);
    ed.loopEnabled = true;
    ed.playhead = 2.9f;
    pb.update(d, ed, 0.2f);
    CHECK(std::fabs(ed.playhead - 2.1f) < 1e-4f);

    // Cue points fire on a rising value, even while paused.
    pb.pause(ed);
    Uid g = ops::addGraph(d, "g");
    Uid value = ops::addNode(d, g, builtinType("Value"));
    Uid cue = ops::addMarker(d, 7.0f, Kind::CuePoint);
    d.model.cuePoints.find(cue)->parameterUid = param(d, value, "value");
    d.model.cuePoints.find(cue)->condition = 0.5f;
    d.model.findParam(value, "value")->value = 0.2f;
    CHECK(pb.cueCheck(d, ed, rt).is_nil());   // first sample never fires
    d.model.findParam(value, "value")->value = 0.8f;
    CHECK(pb.cueCheck(d, ed, rt) == cue);
    CHECK(ed.playhead == 7.0f);
    CHECK(pb.cueCheck(d, ed, rt).is_nil());   // stays above: no retrigger
}

int main() {
    struct Test {
        const char* name;
        void (*fn)();
    };
    const Test tests[] = {
        {"builtins", testBuiltins},         {"shader format", testShaderFormat}, {"linking", testLinking},
        {"delete cascade", testDeleteCascade}, {"shader recompile", testShaderRecompile},
        {"json round trip", testJsonRoundTrip}, {"save and load", testSaveLoad}, {"history", testHistory},
        {"timeline ops", testTimelineOps}, {"playback", testPlayback},
    };
    for (const Test& t : tests) {
        int before = g_failures;
        t.fn();
        std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", t.name);
    }
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures ? 1 : 0;
}
