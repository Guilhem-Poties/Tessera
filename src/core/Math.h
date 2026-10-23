#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace tessera {

struct Vec2 {
    float x = 0, y = 0;
    bool operator==(const Vec2&) const = default;
};

struct Color {
    float r = 0, g = 0, b = 0;
    bool operator==(const Color&) const = default;
};

inline float clamp01(float v) { return std::clamp(v, 0.0f, 1.0f); }

inline float luma(const Color& c) { return 0.2126f * c.r + 0.7152f * c.g + 0.0722f * c.b; }

inline Color hsv2rgb(float h, float s, float v) {
    h = h - std::floor(h);
    auto f = [&](float n) {
        float k = std::fmod(n + h * 6.0f, 6.0f);
        return v - v * s * std::max(0.0f, std::min({k, 4.0f - k, 1.0f}));
    };
    return {f(5), f(3), f(1)};
}

inline void rgb2hsv(const Color& c, float& h, float& s, float& v) {
    float mx = std::max({c.r, c.g, c.b}), mn = std::min({c.r, c.g, c.b});
    float d = mx - mn;
    v = mx;
    s = mx > 0 ? d / mx : 0;
    if (d <= 1e-6f) { h = 0; return; }
    if (mx == c.r)      h = std::fmod((c.g - c.b) / d, 6.0f);
    else if (mx == c.g) h = (c.b - c.r) / d + 2.0f;
    else                h = (c.r - c.g) / d + 4.0f;
    h /= 6.0f;
    if (h < 0) h += 1.0f;
}

inline Color hueShift(const Color& c, float shift) {
    float h, s, v;
    rgb2hsv(c, h, s, v);
    return hsv2rgb(h + shift, s, v);
}

inline std::string toHex(const Color& c) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "#%02x%02x%02x",
                  (int)std::lround(clamp01(c.r) * 255), (int)std::lround(clamp01(c.g) * 255),
                  (int)std::lround(clamp01(c.b) * 255));
    return buf;
}

inline Color fromHex(const std::string& hex) {
    unsigned r = 0, g = 0, b = 0;
    const char* s = hex.c_str();
    if (*s == '#') ++s;
    if (std::sscanf(s, "%02x%02x%02x", &r, &g, &b) != 3) return {};
    return {r / 255.0f, g / 255.0f, b / 255.0f};
}

} // namespace tessera
