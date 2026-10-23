// Frame compositing (architecture section 7.3): tracks bottom to top, the
// active clip of each, its effect chain, then blend onto the accumulator.

#include "runtime/Runtime.h"

#include "model/Declarations.h"

#include <algorithm>

namespace tessera {

TexRef Runtime::composite(const Document& doc, float time, int w, int h) {
    const Model& m = doc.model;
    activeGraphTimes.clear();
    gl::Fbo* acc[2] = {&fbo("accA", w, h, GL_RGBA16F), &fbo("accB", w, h, GL_RGBA16F)};
    int cur = 0;
    acc[cur]->bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    for (const Track* track : m.tracksBottomToTop()) {
        // Active clip at the playhead: the latest start wins.
        const Clip* clip = nullptr;
        for (const Clip& c : m.clips)
            if (c.trackUid == track->uid && time >= c.start && time < c.start + c.duration &&
                (!clip || c.start >= clip->start))
                clip = &c;
        if (!clip) continue;

        float clipTime = (clip->startMode == kStartModeContinuous ? time : time - clip->start) * clip->speed;
        Kind kind = m.kindOf(clip->sourceUid);
        TexRef layer;

        auto paramsOf = [&](const Uid& owner) {
            Settings s;
            for (const Parameter& p : m.params)
                if (p.ownerUid == owner) s[p.name] = toLive(p.value);
            return s;
        };

        if (kind == Kind::Graph) {
            layer = evaluateGraph(doc, clip->sourceUid, clipTime, true);
            activeGraphTimes[clip->sourceUid] = clipTime;
        } else if (kind == Kind::Media) {
            layer = fit(mediaTexture(doc, clip->sourceUid), fbo("clipsrc", w, h));
        } else if (kind == Kind::Shader) {
            // Effect clip: processes the composite of every track below,
            // like an adjustment layer. Skips the effect chain.
            Settings s = paramsOf(clip->uid);
            gl::Fbo& adj = fbo("adj", w, h);
            TexRef below{acc[cur]->tex, w, h};
            runShader(doc, clip->sourceUid, adj, clipTime, [&](const ParamDecl& d) -> LiveValue {
                if (d.type == ParamType::Texture) return below;
                auto it = s.find(d.name);
                return it == s.end() ? LiveValue{} : it->second;
            });
            layer = {adj.tex, adj.w, adj.h};
        } else {
            continue;
        }

        if (kind != Kind::Shader) {
            std::vector<const Effect*> chain;
            for (const Effect& e : m.effects)
                if (e.ownerUid == clip->uid) chain.push_back(&e);
            std::sort(chain.begin(), chain.end(), [](const Effect* a, const Effect* b) { return a->order < b->order; });
            int fx = 0;
            for (const Effect* e : chain) {
                Settings s = paramsOf(e->uid);
                gl::Fbo& target = fbo(fx ? "fx1" : "fx0", w, h);
                TexRef input = layer;
                runShader(doc, e->shaderUid, target, clipTime, [&](const ParamDecl& d) -> LiveValue {
                    if (d.type == ParamType::Texture) return input;
                    auto it = s.find(d.name);
                    return it == s.end() ? LiveValue{} : it->second;
                });
                layer = {target.tex, target.w, target.h};
                fx ^= 1;
            }
        }

        // Composite: opacity = clip × track; the clip's blend mode wins
        // unless it is Normal.
        BlendMode mode = clip->blendMode != BlendMode::Normal ? clip->blendMode : track->blendMode;
        gl::Fbo& dst = *acc[cur ^ 1];
        dst.bind();
        glUseProgram(compositeProg_);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, acc[cur]->tex);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, layer.tex ? layer.tex : gl::blackTexture());
        glUniform1i(glGetUniformLocation(compositeProg_, "dst"), 0);
        glUniform1i(glGetUniformLocation(compositeProg_, "src"), 1);
        glUniform1f(glGetUniformLocation(compositeProg_, "opacity"), clip->opacity * track->opacity);
        glUniform1i(glGetUniformLocation(compositeProg_, "mode"), (int)mode);
        glUniform1f(glGetUniformLocation(compositeProg_, "scale"), clip->scale);
        glUniform1f(glGetUniformLocation(compositeProg_, "rotation"), clip->rotation);
        glUniform2f(glGetUniformLocation(compositeProg_, "res"), (float)w, (float)h);
        gl::drawFullscreen();
        glActiveTexture(GL_TEXTURE0);
        ++passes;
        cur ^= 1;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return {acc[cur]->tex, w, h};
}

} // namespace tessera
