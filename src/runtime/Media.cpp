// Media textures, keyed by Media uid. Images are decoded with stb_image.
// Video and audio need a decoder (FFmpeg / miniaudio); until then they show a
// placeholder.

#include "runtime/Runtime.h"

#include <stb_image.h>

#include <cmath>
#include <vector>

namespace tessera {

namespace {

GLuint upload(const unsigned char* rgba, int w, int h) {
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return tex;
}

using Pixels = std::vector<unsigned char>;

void put(Pixels& px, int w, int x, int y, Color c, float a = 1) {
    unsigned char* p = &px[((size_t)y * w + x) * 4];
    p[0] = (unsigned char)(clamp01(c.r) * 255);
    p[1] = (unsigned char)(clamp01(c.g) * 255);
    p[2] = (unsigned char)(clamp01(c.b) * 255);
    p[3] = (unsigned char)(clamp01(a) * 255);
}

// Rows are generated bottom-up, matching OpenGL texture orientation.
Pixels testCard(int w, int h) {
    Pixels px((size_t)w * h * 4);
    const Color bars[] = {{.75f, .75f, .75f}, {.75f, .75f, 0}, {0, .75f, .75f}, {0, .75f, 0},
                          {.75f, 0, .75f}, {.75f, 0, 0}, {0, 0, .75f}};
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float u = (x + 0.5f) / w, v = (y + 0.5f) / h;
            Color c;
            if (v > 0.33f) c = bars[std::min(6, (int)(u * 7))];
            else if (v > 0.2f) c = Color{u, u, u};
            else c = hsv2rgb(u, 0.8f, 0.9f);
            float dx = (u - 0.5f) * w / h, dy = v - 0.6f;
            float r = std::sqrt(dx * dx + dy * dy);
            if (std::fabs(r - 0.25f) < 0.008f) c = {1, 1, 1};
            put(px, w, x, y, c);
        }
    return px;
}

Pixels checker(int w, int h) {
    Pixels px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            bool on = ((x / 64) + (y / 64)) % 2 == 0;
            put(px, w, x, y, on ? Color{0.92f, 0.92f, 0.92f} : Color{0.12f, 0.12f, 0.14f});
        }
    return px;
}

// Diagonal stripes for media that can't be decoded.
Pixels placeholder(int w, int h, Color tint) {
    Pixels px((size_t)w * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            bool on = ((x + y) / 24) % 2 == 0;
            Color c = on ? tint : Color{tint.r * 0.4f, tint.g * 0.4f, tint.b * 0.4f};
            put(px, w, x, y, c);
        }
    return px;
}

} // namespace

TexRef Runtime::mediaTexture(const Document& doc, const Uid& mediaUid) {
    const Model& m = doc.model;
    const Media* media = m.media.find(mediaUid);
    if (!media) return {};
    auto fileIt = doc.files.find(media->path);
    const std::string* data = fileIt == doc.files.end() ? nullptr : fileIt->second.get();

    MediaTex& t = media_[mediaUid];
    if (t.tex && t.path == media->path && t.data == data) return {t.tex, t.w, t.h};
    if (t.tex) deadTextures_.push_back(t.tex);
    t = MediaTex{};
    t.path = media->path;
    t.data = data;

    Pixels px;
    Kind kind = m.mediaKind(mediaUid);
    if (media->path == "tessera://media/test-card.png") {
        t.w = 640; t.h = 360; px = testCard(t.w, t.h);
    } else if (media->path == "tessera://media/checker.png") {
        t.w = 512; t.h = 512; px = checker(t.w, t.h);
    } else if (kind == Kind::Image) {
        stbi_set_flip_vertically_on_load(1);
        int comp = 0;
        unsigned char* decoded = data
            ? stbi_load_from_memory((const stbi_uc*)data->data(), (int)data->size(), &t.w, &t.h, &comp, 4)
            : stbi_load(media->path.c_str(), &t.w, &t.h, &comp, 4);
        if (decoded) {
            px.assign(decoded, decoded + (size_t)t.w * t.h * 4);
            stbi_image_free(decoded);
        } else {
            t.w = 320; t.h = 180; px = placeholder(t.w, t.h, {0.8f, 0.1f, 0.6f});   // missing: needs relinking
        }
    } else {
        // Video / audio: no decoder in the prototype.
        t.w = 320; t.h = 180;
        px = placeholder(t.w, t.h, kind == Kind::Video ? Color{0.3f, 0.5f, 0.9f} : Color{0.9f, 0.6f, 0.2f});
    }
    t.tex = upload(px.data(), t.w, t.h);
    return {t.tex, t.w, t.h};
}

} // namespace tessera
