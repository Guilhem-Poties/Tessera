// Converters (architecture section 5): one per parameter type, each accepts
// any source type. Settings are the link's parameters (or a converter node's
// inputs).

#include "runtime/Runtime.h"

#include <cmath>

namespace tessera {

LiveValue Runtime::convert(ParamType toType, const Settings& settings, const LiveValue& source, const ParamDecl* target,
                           const std::string& fboKey, int w, int h) {
    auto num = [&](const char* name, float def) {
        auto it = settings.find(name);
        return it == settings.end() ? def : toNumber(it->second);
    };
    auto col = [&](const char* name, Color def) {
        auto it = settings.find(name);
        return it == settings.end() ? def : toColor(it->second);
    };
    bool isColorish = std::holds_alternative<Color>(source) || std::holds_alternative<ColorList>(source) ||
                      std::holds_alternative<TexRef>(source);

    switch (toType) {
        case ParamType::Float: {
            // brightness for a colour or image, index for a choice; then map.
            float x = toNumber(source);
            float inMin = num("inMin", 0), inMax = num("inMax", 1), outMin = num("outMin", 0), outMax = num("outMax", 1);
            float t = inMax != inMin ? (x - inMin) / (inMax - inMin) : 0.0f;
            return outMin + t * (outMax - outMin);
        }
        case ParamType::Bool: {
            bool b = toNumber(source) > num("threshold", 0.5f);
            return num("invert", 0) > 0.5f ? !b : b;
        }
        case ParamType::Choice: {
            int n = target && !target->options.empty() ? (int)target->options.size() : 8;
            int idx;
            if (num("mode", 0) < 0.5f) {   // Spread: 0–1 across the options
                idx = std::min(n - 1, (int)std::floor(clamp01(toNumber(source)) * (float)n));
            } else {                        // Index
                auto i = std::get_if<int>(&source);
                idx = i ? *i : (int)std::floor(toNumber(source));
            }
            idx += (int)std::lround(num("offset", 0));
            return ((idx % n) + n) % n;
        }
        case ParamType::Color: {
            float shift = num("hueShift", 0), sat = num("saturation", 1);
            Color tint = col("tint", {1, 1, 1});
            Color c;
            if (isColorish) {
                c = hueShift(toColor(source), shift);
            } else {
                float x = toNumber(source);
                c = num("mode", 0) < 0.5f ? hsv2rgb(x + shift, sat, 1.0f) : Color{x, x, x};
            }
            return Color{c.r * tint.r, c.g * tint.g, c.b * tint.b};
        }
        case ParamType::ColorList: {
            int count = std::clamp((int)std::lround(num("count", 3)), 1, 4);
            float spread = num("spread", 0.33f), shift = num("hueShift", 0);
            ColorList out;
            if (auto list = std::get_if<ColorList>(&source)) {
                for (int i = 0; i < count; ++i)
                    out.push_back(list->empty() ? Color{} : hueShift((*list)[(size_t)i % list->size()], shift));
            } else if (auto tex = std::get_if<TexRef>(&source); tex && tex->tex) {
                const Readback& rb = readback(*tex);
                for (int i = 0; i < count; ++i)
                    out.push_back(hueShift(rb.row[std::min(7, (int)((i + 0.5f) / (float)count * 8.0f))], shift));
            } else {
                Color base = std::holds_alternative<Color>(source) ? std::get<Color>(source)
                                                                   : hsv2rgb(toNumber(source), 1.0f, 1.0f);
                for (int i = 0; i < count; ++i) out.push_back(hueShift(base, shift + spread * (float)i));
            }
            return out;
        }
        case ParamType::Texture: {
            float gain = num("gain", 1);
            auto tex = std::get_if<TexRef>(&source);
            if (tex && tex->tex && std::fabs(gain - 1.0f) < 1e-4f) return *tex;   // pass through
            gl::Fbo& target = fbo(fboKey, w, h);
            target.bind();
            glUseProgram(convTexProg_);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex && tex->tex ? tex->tex : gl::blackTexture());
            glUniform1i(glGetUniformLocation(convTexProg_, "src"), 0);
            glUniform1f(glGetUniformLocation(convTexProg_, "gain"), gain);
            int mode = tex ? 0 : std::holds_alternative<ColorList>(source) ? 2 : 1;
            glUniform1i(glGetUniformLocation(convTexProg_, "mode"), mode);
            Color c = toColor(source);
            glUniform3f(glGetUniformLocation(convTexProg_, "color"), c.r, c.g, c.b);
            if (auto list = std::get_if<ColorList>(&source)) {
                float arr[12] = {};
                int n = 0;
                for (const Color& x : *list) {
                    if (n == 4) break;
                    arr[n * 3] = x.r; arr[n * 3 + 1] = x.g; arr[n * 3 + 2] = x.b;
                    ++n;
                }
                glUniform3fv(glGetUniformLocation(convTexProg_, "colors"), 4, arr);
                glUniform1i(glGetUniformLocation(convTexProg_, "count"), n);
            }
            gl::drawFullscreen();
            ++passes;
            return TexRef{target.tex, target.w, target.h};
        }
        default:
            return source;
    }
}

} // namespace tessera
