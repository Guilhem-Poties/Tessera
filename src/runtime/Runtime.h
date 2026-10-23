#pragma once

// The runtime (architecture section 7): compiled shader programs, FBOs,
// media textures and live values, all keyed by model uids. It only reads the
// model and never writes back into it.

#include "model/Model.h"
#include "model/ShaderFormat.h"
#include "runtime/Gl.h"

#include <functional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace tessera {

struct TexRef {
    GLuint tex = 0;
    int w = 0, h = 0;
    bool operator==(const TexRef&) const = default;
};

// A runtime value: like a stored Value, plus texture handles.
using LiveValue = std::variant<std::monostate, float, int, bool, Color, ColorList, std::string, TexRef>;
LiveValue toLive(const Value& value);
std::string describeLive(const LiveValue& value, const ParamDecl* decl);

// Texture → value: an 8×8 GPU readback (average colour, brightness, samples).
struct Readback {
    Color average;
    float brightness = 0;
    Color samples[4];
    Color row[8];   // the middle row, for "sampled across its middle"
};

class Runtime {
public:
    using Settings = std::unordered_map<std::string, LiveValue>;
    using InputFn = std::function<LiveValue(const ParamDecl&)>;

    struct Program {
        GLuint id = 0;
        bool built = false;
        std::string source;   // the .tsh it was built from
        std::string error;    // header or GLSL error
        std::unordered_map<std::string, GLint> locations;
        GLint loc(const std::string& name);
    };

    void init();
    void shutdown();

    // Called once per frame before any rendering: frees resources released
    // by collect() during the previous frame (ImGui may still have drawn them).
    void beginFrame();
    // Called after each commit: releases FBOs, programs and textures whose
    // node, link, shader or media no longer exists.
    void collect(const Document& doc);

    // ── Shader programs, keyed by Shader uid ──
    Program* program(const Document& doc, const Uid& shaderUid);
    std::string shaderError(const Document& doc, const Uid& shaderUid);
    bool runShader(const Document& doc, const Uid& shaderUid, gl::Fbo& target, float time, const InputFn& input);

    gl::Fbo& fbo(const std::string& key, int w, int h, GLenum format = GL_RGBA8);
    TexRef mediaTexture(const Document& doc, const Uid& mediaUid);
    TexRef fit(TexRef source, gl::Fbo& target);   // letterboxes a texture into a target

    // ── Graph evaluation (section 7.2) ──
    // `outputOnly` evaluates only what the Output node depends on.
    TexRef evaluateGraph(const Document& doc, const Uid& graphUid, float time, bool outputOnly);

    // ── Converters (section 5) ──
    LiveValue convert(ParamType toType, const Settings& settings, const LiveValue& source, const ParamDecl* target,
                      const std::string& fboKey, int w, int h);
    float toNumber(const LiveValue& v);
    Color toColor(const LiveValue& v);
    const Readback& readback(const TexRef& tex);

    // ── Compositor (section 7.3) ──
    TexRef composite(const Document& doc, float time, int w, int h);

    // Live values: computed outputs and linked inputs, per Parameter uid.
    const LiveValue* liveValue(const Uid& paramUid) const;
    std::unordered_map<Uid, LiveValue> live;

    // The graph played by each track's active clip in the last composite,
    // with its clip time, so that previews can follow the timeline.
    std::unordered_map<Uid, float> activeGraphTimes;

    int passes = 0;   // shader passes this frame

private:
    void evaluateNode(const Document& doc, const Uid& nodeUid, float time, int w, int h,
                      const std::unordered_map<Uid, const Link*>& incoming, TexRef& result);

    std::unordered_map<Uid, Program> programs_;
    std::unordered_map<std::string, gl::Fbo> fbos_;
    struct MediaTex {
        GLuint tex = 0;
        int w = 0, h = 0;
        std::string path;
        const std::string* data = nullptr;
    };
    std::unordered_map<Uid, MediaTex> media_;
    std::unordered_map<GLuint, Readback> readbacks_;

    std::vector<gl::Fbo> deadFbos_;
    std::vector<GLuint> deadTextures_;

    GLuint fitProg_ = 0, compositeProg_ = 0, convTexProg_ = 0, downsampleProg_ = 0;
};

} // namespace tessera
