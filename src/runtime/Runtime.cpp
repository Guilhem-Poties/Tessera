#include "runtime/Runtime.h"

#include "model/Declarations.h"

#include <cstdio>
#include <cstring>

namespace tessera {

LiveValue toLive(const Value& value) {
    return std::visit([](const auto& v) -> LiveValue { return v; }, value);
}

std::string describeLive(const LiveValue& value, const ParamDecl* decl) {
    char buf[64];
    if (auto f = std::get_if<float>(&value)) { std::snprintf(buf, sizeof buf, "%.3g", *f); return buf; }
    if (auto i = std::get_if<int>(&value)) {
        if (decl && *i >= 0 && *i < (int)decl->options.size()) return decl->options[(size_t)*i];
        return std::to_string(*i);
    }
    if (auto b = std::get_if<bool>(&value)) return *b ? "on" : "off";
    if (auto c = std::get_if<Color>(&value)) return toHex(*c);
    if (auto l = std::get_if<ColorList>(&value)) return std::to_string(l->size()) + " colors";
    if (auto s = std::get_if<std::string>(&value)) return *s;
    if (auto t = std::get_if<TexRef>(&value)) {
        if (!t->tex) return "none";
        std::snprintf(buf, sizeof buf, "%d×%d", t->w, t->h);
        return buf;
    }
    return "";
}

GLint Runtime::Program::loc(const std::string& name) {
    auto it = locations.find(name);
    if (it != locations.end()) return it->second;
    GLint l = glGetUniformLocation(id, name.c_str());
    locations[name] = l;
    return l;
}

// ── Internal passes ────────────────────────────────────────
static const char* kFitFrag = R"(#version 330 core
in vec2 vUv; out vec4 o;
uniform sampler2D src; uniform vec2 srcSize; uniform vec2 dstSize;
void main(){
  float sa = srcSize.x / srcSize.y, da = dstSize.x / dstSize.y;
  vec2 uv = vUv - 0.5;
  if (sa > da) uv.y *= sa / da; else uv.x *= da / sa;
  uv += 0.5;
  if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) { o = vec4(0.0); return; }
  vec4 c = texture(src, uv);
  o = vec4(c.rgb * c.a, c.a);   // media is straight alpha, Tessera is premultiplied
})";

static const char* kCompositeFrag = R"(#version 330 core
in vec2 vUv; out vec4 o;
uniform sampler2D dst; uniform sampler2D src;
uniform float opacity; uniform int mode; uniform float scale; uniform float rotation; uniform vec2 res;
void main(){
  vec4 d = texture(dst, vUv);
  vec2 asp = vec2(res.x / res.y, 1.0);
  vec2 p = (vUv - 0.5) * asp;
  float a = radians(rotation);
  p = mat2(cos(a), -sin(a), sin(a), cos(a)) * p;
  p /= max(scale, 1e-4);
  vec2 suv = p / asp + 0.5;
  vec4 s = (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) ? vec4(0.0) : texture(src, suv);
  s *= opacity;
  vec3 c;
  if (mode == 0) c = s.rgb + d.rgb * (1.0 - s.a);                                   // Normal
  else if (mode == 1) c = d.rgb + s.rgb;                                            // Add
  else if (mode == 2) c = s.rgb * d.rgb + s.rgb * (1.0 - d.a) + d.rgb * (1.0 - s.a); // Multiply
  else if (mode == 3) c = s.rgb + d.rgb - s.rgb * d.rgb;                            // Screen
  else c = s.rgb + d.rgb - 2.0 * min(s.rgb * d.a, d.rgb * s.a);                      // Difference
  o = vec4(c, s.a + d.a * (1.0 - s.a));
})";

static const char* kConvTexFrag = R"(#version 330 core
in vec2 vUv; out vec4 o;
uniform int mode; uniform sampler2D src; uniform float gain; uniform vec3 color;
uniform vec3 colors[4]; uniform int count;
void main(){
  if (mode == 0) { o = texture(src, vUv) * gain; return; }
  if (mode == 1) { o = vec4(color * gain, 1.0); return; }
  int n = max(count, 1);
  float x = vUv.x * float(n - 1);
  int i = int(floor(x));
  vec3 c = mix(colors[min(i, n - 1)], colors[min(i + 1, n - 1)], fract(x));
  o = vec4(c * gain, 1.0);
})";

static const char* kDownsampleFrag = R"(#version 330 core
in vec2 vUv; out vec4 o;
uniform sampler2D src;
void main(){
  vec4 acc = vec4(0.0);
  for (int y = 0; y < 4; y++)
    for (int x = 0; x < 4; x++)
      acc += texture(src, vUv + ((vec2(x, y) + 0.5) / 4.0 - 0.5) / 8.0);
  o = acc / 16.0;
})";

static GLuint internalProgram(const char* frag) {
    std::string log;
    GLuint p = gl::makeProgram(gl::fullscreenVertex(), frag, log);
    if (!p) std::fprintf(stderr, "Tessera: internal shader failed:\n%s\n", log.c_str());
    return p;
}

void Runtime::init() {
    gl::init();
    fitProg_ = internalProgram(kFitFrag);
    compositeProg_ = internalProgram(kCompositeFrag);
    convTexProg_ = internalProgram(kConvTexFrag);
    downsampleProg_ = internalProgram(kDownsampleFrag);
}

void Runtime::shutdown() {
    beginFrame();
    for (auto& [k, f] : fbos_) f.release();
    for (auto& [k, p] : programs_)
        if (p.id) glDeleteProgram(p.id);
    for (auto& [k, m] : media_) glDeleteTextures(1, &m.tex);
    for (GLuint p : {fitProg_, compositeProg_, convTexProg_, downsampleProg_}) glDeleteProgram(p);
    fbos_.clear();
    programs_.clear();
    media_.clear();
    gl::shutdown();
}

void Runtime::beginFrame() {
    for (gl::Fbo& f : deadFbos_) f.release();
    deadFbos_.clear();
    if (!deadTextures_.empty()) glDeleteTextures((GLsizei)deadTextures_.size(), deadTextures_.data());
    deadTextures_.clear();
    passes = 0;
}

void Runtime::collect(const Document& doc) {
    const Model& m = doc.model;
    for (auto it = fbos_.begin(); it != fbos_.end();) {
        const std::string& key = it->first;
        size_t colon = key.find(':');
        bool dead = false;
        if (colon != std::string::npos) {
            Uid uid = uidFromString(key.substr(colon + 1));
            std::string prefix = key.substr(0, colon);
            dead = prefix == "l" ? !m.links.contains(uid) : !m.nodes.contains(uid);
        }
        if (dead) {
            deadFbos_.push_back(it->second);
            it = fbos_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = programs_.begin(); it != programs_.end();) {
        if (!m.shaders.contains(it->first)) {
            if (it->second.id) glDeleteProgram(it->second.id);
            it = programs_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = media_.begin(); it != media_.end();) {
        if (!m.media.contains(it->first)) {
            deadTextures_.push_back(it->second.tex);
            it = media_.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = live.begin(); it != live.end();)
        it = m.params.contains(it->first) ? std::next(it) : live.erase(it);
}

// ── Programs ───────────────────────────────────────────────
Runtime::Program* Runtime::program(const Document& doc, const Uid& shaderUid) {
    const std::string* source = shaderSource(doc, shaderUid);
    if (!source) return nullptr;
    Program& p = programs_[shaderUid];
    if (p.built && p.source == *source) return &p;   // recompiled when the source changes
    if (p.id) glDeleteProgram(p.id);
    p = Program{};
    p.built = true;
    p.source = *source;
    const ShaderInfo* info = shaderInfo(doc, shaderUid);
    if (!info || !info->ok) {
        p.error = info ? info->error : "No shader";
        return &p;
    }
    std::string log;
    p.id = gl::makeProgram(gl::fullscreenVertex(), buildFragmentSource(*info), log);
    if (!p.id) p.error = log;
    return &p;
}

std::string Runtime::shaderError(const Document& doc, const Uid& shaderUid) {
    Program* p = program(doc, shaderUid);
    return p ? p->error : "Shader source not found";
}

bool Runtime::runShader(const Document& doc, const Uid& shaderUid, gl::Fbo& target, float time, const InputFn& input) {
    Program* prog = program(doc, shaderUid);
    const ShaderInfo* info = shaderInfo(doc, shaderUid);
    target.bind();
    if (!prog || !prog->id || !info || !info->ok) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        return false;
    }
    glUseProgram(prog->id);
    glUniform1f(prog->loc("TIME"), time);
    glUniform2f(prog->loc("RES"), (float)target.w, (float)target.h);
    int unit = 0;
    for (const ParamDecl& d : info->inputs) {
        LiveValue v = input(d);
        GLint l = prog->loc(d.name);
        switch (d.type) {
            case ParamType::Float: glUniform1f(l, toNumber(v)); break;
            case ParamType::Choice: {
                auto i = std::get_if<int>(&v);
                glUniform1i(l, i ? *i : (int)toNumber(v));
                break;
            }
            case ParamType::Bool: glUniform1i(l, toNumber(v) > 0.5f); break;
            case ParamType::Color: {
                Color c = toColor(v);
                glUniform3f(l, c.r, c.g, c.b);
                break;
            }
            case ParamType::ColorList: {
                float arr[12] = {};
                int n = 0;
                if (auto list = std::get_if<ColorList>(&v))
                    for (const Color& c : *list) {
                        if (n == 4) break;
                        arr[n * 3] = c.r; arr[n * 3 + 1] = c.g; arr[n * 3 + 2] = c.b;
                        ++n;
                    }
                glUniform3fv(l, 4, arr);
                glUniform1i(prog->loc(d.name + "_count"), n);
                break;
            }
            case ParamType::Texture: {
                auto t = std::get_if<TexRef>(&v);
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, t && t->tex ? t->tex : gl::blackTexture());
                glUniform1i(l, unit);
                ++unit;
                break;
            }
            default: break;
        }
    }
    gl::drawFullscreen();
    glActiveTexture(GL_TEXTURE0);
    ++passes;
    return true;
}

gl::Fbo& Runtime::fbo(const std::string& key, int w, int h, GLenum format) {
    gl::Fbo& f = fbos_[key];
    f.ensure(w, h, format);
    return f;
}

TexRef Runtime::fit(TexRef source, gl::Fbo& target) {
    target.bind();
    if (!source.tex) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
        return {target.tex, target.w, target.h};
    }
    glUseProgram(fitProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, source.tex);
    glUniform1i(glGetUniformLocation(fitProg_, "src"), 0);
    glUniform2f(glGetUniformLocation(fitProg_, "srcSize"), (float)source.w, (float)source.h);
    glUniform2f(glGetUniformLocation(fitProg_, "dstSize"), (float)target.w, (float)target.h);
    gl::drawFullscreen();
    ++passes;
    return {target.tex, target.w, target.h};
}

const LiveValue* Runtime::liveValue(const Uid& paramUid) const {
    auto it = live.find(paramUid);
    return it == live.end() ? nullptr : &it->second;
}

// ── Values ─────────────────────────────────────────────────
float Runtime::toNumber(const LiveValue& v) {
    if (auto f = std::get_if<float>(&v)) return *f;
    if (auto i = std::get_if<int>(&v)) return (float)*i;
    if (auto b = std::get_if<bool>(&v)) return *b ? 1.0f : 0.0f;
    if (auto c = std::get_if<Color>(&v)) return luma(*c);
    if (auto l = std::get_if<ColorList>(&v)) {
        if (l->empty()) return 0;
        float s = 0;
        for (const Color& c : *l) s += luma(c);
        return s / (float)l->size();
    }
    if (auto t = std::get_if<TexRef>(&v)) return t->tex ? readback(*t).brightness : 0.0f;
    return 0.0f;
}

Color Runtime::toColor(const LiveValue& v) {
    if (auto c = std::get_if<Color>(&v)) return *c;
    if (auto l = std::get_if<ColorList>(&v)) return l->empty() ? Color{} : l->front();
    if (auto t = std::get_if<TexRef>(&v)) return t->tex ? readback(*t).average : Color{};
    float x = toNumber(v);
    return {x, x, x};
}

const Readback& Runtime::readback(const TexRef& tex) {
    auto it = readbacks_.find(tex.tex);
    if (it != readbacks_.end()) return it->second;
    // This stalls the GPU; it only happens for links that need a value
    // from a texture. Pixel buffer objects would make it asynchronous.
    gl::Fbo& rb = fbo("rb", 8, 8, GL_RGBA32F);
    rb.bind();
    glUseProgram(downsampleProg_);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex.tex);
    glUniform1i(glGetUniformLocation(downsampleProg_, "src"), 0);
    gl::drawFullscreen();
    float px[8 * 8 * 4];
    glReadPixels(0, 0, 8, 8, GL_RGBA, GL_FLOAT, px);
    auto at = [&](int x, int y) {
        const float* p = px + (y * 8 + x) * 4;
        return Color{p[0], p[1], p[2]};
    };
    Readback r;
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 8; ++x) {
            Color c = at(x, y);
            r.average.r += c.r / 64; r.average.g += c.g / 64; r.average.b += c.b / 64;
        }
    r.brightness = luma(r.average);
    r.samples[0] = at(2, 2); r.samples[1] = at(5, 2); r.samples[2] = at(2, 5); r.samples[3] = at(5, 5);
    for (int x = 0; x < 8; ++x) r.row[x] = at(x, 4);
    return readbacks_[tex.tex] = r;
}

} // namespace tessera
