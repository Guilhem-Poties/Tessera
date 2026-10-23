// Timeline (architecture section 8): tracks with clips (graph, media or
// effect), markers, cue points, hold points and loops. Supports scrubbing,
// snapping to 0.25 s (hold Alt to disable) and zoom.

#include "model/Declarations.h"
#include "model/Operations.h"
#include "runtime/Playback.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>

#include <algorithm>
#include <cmath>

namespace tessera::ui {

namespace {

constexpr float kHeaderWidth = 110;
constexpr float kRulerHeight = 40;
constexpr float kTrackHeight = 44;
constexpr float kEdge = 6;

struct Drag {
    enum Mode { None, Scrub, MoveClip, ResizeLeft, ResizeRight, MoveMarker, LoopStart, LoopEnd } mode = None;
    Uid uid;
    float grab = 0;   // seconds between the mouse and the clip start
    bool changed = false;
};
Drag g_drag;

std::string formatTime(float t) {
    int cs = (int)std::lround(std::max(0.0f, t) * 100);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%02d:%02d.%02d", cs / 6000, (cs / 100) % 60, cs % 100);
    return buf;
}

float snap(float t) {
    if (ImGui::GetIO().KeyAlt) return std::max(0.0f, t);
    return std::max(0.0f, std::round(t * 4.0f) / 4.0f);
}

ImU32 clipColor(const Model& m, const Clip& c) {
    switch (m.kindOf(c.sourceUid)) {
        case Kind::Graph: return kGraphBlue;
        case Kind::Media: return kAssetGreen;
        case Kind::Shader: return kTimelinePink;
        default: return kMuted;
    }
}

void transport(App& app) {
    EditorState& ed = app.ed;
    bool playing = ed.playState != PlayState::Paused;
    if (ImGui::Button(playing ? "Pause" : "Play", ImVec2(60, 0))) {
        if (playing) app.playback.pause(ed);
        else app.playback.play(ed);
    }
    ImGui::SameLine();
    if (ImGui::Button("|<")) app.playback.seek(ed, 0);
    ImGui::SameLine();
    ImGui::Text("%s", formatTime(ed.playhead).c_str());
    ImGui::SameLine();
    if (ed.playState == PlayState::Holding) ImGui::TextColored(toVec4(kParamAmber), "Holding %.1fs", ed.holdRemaining);
    else ImGui::TextDisabled("%s", toString(ed.playState));
    ImGui::SameLine();
    ImGui::Checkbox("Loop playback", &ed.loopEnabled);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(110);
    ImGui::SliderFloat("Zoom", &ed.timelineZoom, 8, 300, "%.0f px/s", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
    Document& doc = app.doc;
    if (ImGui::SmallButton("+ Track")) {
        app.select(ops::addTrack(doc));
        app.commit("Add track");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Marker")) {
        app.select(ops::addMarker(doc, ed.playhead, Kind::Marker));
        app.commit("Add marker");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Cue")) {
        app.select(ops::addMarker(doc, ed.playhead, Kind::CuePoint));
        app.commit("Add cue point");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Hold")) {
        app.select(ops::addMarker(doc, ed.playhead, Kind::HoldPoint));
        app.commit("Add hold point");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("+ Loop")) {
        if (const Loop* l = Playback::activeLoop(doc.model)) {
            app.select(l->uid);
        } else {
            app.select(ops::addLoop(doc, snap(ed.playhead), snap(ed.playhead) + 8));
            app.commit("Add loop");
        }
        ed.loopEnabled = true;
    }
}

} // namespace

void drawTimeline(App& app) {
    if (!ImGui::Begin("Timeline")) {
        ImGui::End();
        return;
    }
    transport(app);

    Model& m = app.doc.model;
    EditorState& ed = app.ed;
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 200 || size.y < 60) {
        ImGui::End();
        return;
    }
    ImGui::InvisibleButton("timeline", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(22, 24, 29, 255));

    float zoom = ed.timelineZoom;
    float left = origin.x + kHeaderWidth;
    auto timeToX = [&](float t) { return left + (t - ed.timelineScroll) * zoom; };
    auto xToTime = [&](float x) { return (x - left) / zoom + ed.timelineScroll; };

    // Tracks: top row is the highest order.
    std::vector<const Track*> tracks = m.tracksBottomToTop();
    std::reverse(tracks.begin(), tracks.end());
    auto trackTop = [&](size_t i) { return origin.y + kRulerHeight + (float)i * kTrackHeight; };
    auto trackAt = [&](float y) -> const Track* {
        if (y < origin.y + kRulerHeight) return nullptr;
        size_t i = (size_t)((y - origin.y - kRulerHeight) / kTrackHeight);
        return i < tracks.size() ? tracks[i] : nullptr;
    };

    // ── Ruler ──
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + kRulerHeight), IM_COL32(30, 33, 40, 255));
    float stepSec = 1;
    for (float s : {0.25f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 30.0f, 60.0f})
        if (s * zoom >= 60) { stepSec = s; break; }
    float t0 = std::floor(ed.timelineScroll / stepSec) * stepSec;
    for (float t = t0; timeToX(t) < origin.x + size.x; t += stepSec) {
        float x = timeToX(t);
        if (x < left) continue;
        dl->AddLine(ImVec2(x, origin.y + kRulerHeight - 10), ImVec2(x, origin.y + kRulerHeight), IM_COL32(140, 148, 160, 255));
        dl->AddLine(ImVec2(x, origin.y + kRulerHeight), ImVec2(x, origin.y + size.y), IM_COL32(255, 255, 255, 12));
        dl->AddText(ImVec2(x + 3, origin.y + 8), IM_COL32(140, 148, 160, 255), formatTime(t).substr(0, 5).c_str());
    }

    // Loop region at the top of the ruler.
    const Loop* loop = Playback::activeLoop(m);
    if (loop) {
        float x0 = std::max(left, timeToX(loop->start)), x1 = timeToX(loop->end);
        ImU32 col = ed.loopEnabled ? IM_COL32(200, 90, 134, 200) : IM_COL32(120, 90, 110, 140);
        if (x1 > left) dl->AddRectFilled(ImVec2(x0, origin.y + 1), ImVec2(x1, origin.y + 7), col, 2);
    }

    // ── Tracks and clips ──
    struct ClipRect {
        Uid uid;
        ImVec2 min, max;
    };
    std::vector<ClipRect> clipRects;
    for (size_t i = 0; i < tracks.size(); ++i) {
        const Track* tr = tracks[i];
        float y0 = trackTop(i), y1 = y0 + kTrackHeight;
        dl->AddRectFilled(ImVec2(origin.x, y0), ImVec2(left, y1 - 1),
                          ed.selection == tr->uid ? IM_COL32(60, 50, 70, 255) : IM_COL32(32, 35, 42, 255));
        std::string name = m.displayName(tr->uid);
        dl->AddText(ImVec2(origin.x + 8, y0 + 6), IM_COL32(210, 214, 222, 255), name.c_str());
        char sub[48];
        std::snprintf(sub, sizeof sub, "%s · %.0f%%", toString(tr->blendMode), tr->opacity * 100);
        dl->AddText(ImVec2(origin.x + 8, y0 + 22), IM_COL32(130, 138, 150, 255), sub);
        dl->AddLine(ImVec2(origin.x, y1 - 1), ImVec2(origin.x + size.x, y1 - 1), IM_COL32(255, 255, 255, 14));

        for (const Clip& c : m.clips) {
            if (c.trackUid != tr->uid) continue;
            float x0 = timeToX(c.start), x1 = timeToX(c.start + c.duration);
            if (x1 < left || x0 > origin.x + size.x) continue;
            ImVec2 mn(std::max(x0, left), y0 + 3), mx(x1, y1 - 4);
            ImU32 col = clipColor(m, c);
            bool effectClip = m.kindOf(c.sourceUid) == Kind::Shader;
            ImVec4 cv = toVec4(col);
            dl->AddRectFilled(mn, mx, ImGui::ColorConvertFloat4ToU32(ImVec4(cv.x * 0.55f, cv.y * 0.55f, cv.z * 0.55f, 1)), 4);
            if (effectClip) {   // hatched, like an adjustment layer
                dl->PushClipRect(mn, mx, true);
                float h = mx.y - mn.y;
                for (float x = mn.x - h; x < mx.x; x += 10)
                    dl->AddLine(ImVec2(x, mx.y), ImVec2(x + h, mn.y), IM_COL32(255, 255, 255, 22));
                dl->PopClipRect();
            }
            dl->AddRectFilled(mn, ImVec2(mx.x, mn.y + 3), col, 4, ImDrawFlags_RoundTop);
            bool selected = ed.selection == c.uid;
            dl->AddRect(mn, mx, selected ? IM_COL32(255, 255, 255, 230) : col, 4, 0, selected ? 2.0f : 1.0f);
            int fx = 0;
            for (const Effect& e : m.effects) fx += e.ownerUid == c.uid;
            std::string label = (effectClip ? "FX  " : "") + m.displayName(c.sourceUid);
            if (fx) label += "   +" + std::to_string(fx) + " fx";
            ImVec4 clip(mn.x + 4, mn.y, mx.x - 4, mx.y);
            dl->AddText(nullptr, 0, ImVec2(mn.x + 6, mn.y + 6), IM_COL32(240, 240, 245, 255), label.c_str(), nullptr, 0, &clip);
            char info[48];
            std::snprintf(info, sizeof info, "%s%s", c.blendMode != BlendMode::Normal ? toString(c.blendMode) : "",
                          c.opacity < 1 ? " · faded" : "");
            dl->AddText(nullptr, 0, ImVec2(mn.x + 6, mn.y + 21), IM_COL32(200, 205, 215, 160), info, nullptr, 0, &clip);
            clipRects.push_back({c.uid, ImVec2(x0, y0 + 3), mx});
        }
    }
    if (tracks.empty()) dl->AddText(ImVec2(left + 10, origin.y + kRulerHeight + 10), IM_COL32(140, 148, 160, 255), "Add a track to place clips.");

    // ── Markers (in the ruler) ──
    struct MarkerRect {
        Uid uid;
        float x;
    };
    std::vector<MarkerRect> markerRects;
    for (const Marker& mk : m.markers) {
        float x = timeToX(mk.position);
        if (x < left - 6 || x > origin.x + size.x + 6) continue;
        Kind k = m.markerKind(mk.uid);
        ImU32 col = k == Kind::CuePoint ? IM_COL32(230, 140, 60, 255) : k == Kind::HoldPoint ? kRuntimeTeal : kParamAmber;
        float yb = origin.y + kRulerHeight;
        bool selected = ed.selection == mk.uid;
        dl->AddTriangleFilled(ImVec2(x - 6, yb - 12), ImVec2(x + 6, yb - 12), ImVec2(x, yb - 2), col);
        if (selected) dl->AddTriangle(ImVec2(x - 7, yb - 13), ImVec2(x + 7, yb - 13), ImVec2(x, yb - 1), IM_COL32(255, 255, 255, 255), 1.5f);
        dl->AddLine(ImVec2(x, yb), ImVec2(x, origin.y + size.y), (col & 0x00FFFFFF) | 0x50000000);
        if (k == Kind::HoldPoint)
            if (const HoldPoint* h = m.holdPoints.find(mk.uid))
                dl->AddRectFilled(ImVec2(x, yb - 4), ImVec2(x + h->duration * zoom * 0.25f, yb - 2), col);
        std::string label = m.displayName(mk.uid);
        dl->AddText(ImVec2(x + 8, yb - 14), col, label.c_str());
        markerRects.push_back({mk.uid, x});
    }

    // Playhead
    float px = timeToX(ed.playhead);
    if (px >= left) {
        dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + size.y), IM_COL32(255, 80, 80, 255), 2);
        dl->AddTriangleFilled(ImVec2(px - 6, origin.y), ImVec2(px + 6, origin.y), ImVec2(px, origin.y + 8), IM_COL32(255, 80, 80, 255));
    }

    // ── Interaction ──
    ImVec2 mouse = io.MousePos;
    float mouseT = xToTime(mouse.x);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_drag = Drag{};
        bool inRuler = mouse.y < origin.y + kRulerHeight;
        if (mouse.x < left) {
            if (const Track* tr = trackAt(mouse.y)) app.select(tr->uid);
        } else if (inRuler) {
            for (const MarkerRect& r : markerRects)
                if (std::fabs(mouse.x - r.x) < 7 && mouse.y > origin.y + kRulerHeight - 16) {
                    app.select(r.uid);
                    g_drag.mode = Drag::MoveMarker;
                    g_drag.uid = r.uid;
                    break;
                }
            if (g_drag.mode == Drag::None && loop && mouse.y < origin.y + 10) {
                if (std::fabs(mouse.x - timeToX(loop->start)) < 8) g_drag.mode = Drag::LoopStart;
                else if (std::fabs(mouse.x - timeToX(loop->end)) < 8) g_drag.mode = Drag::LoopEnd;
                if (g_drag.mode != Drag::None) {
                    g_drag.uid = loop->uid;
                    app.select(loop->uid);
                }
            }
            if (g_drag.mode == Drag::None) g_drag.mode = Drag::Scrub;
        } else {
            for (auto it = clipRects.rbegin(); it != clipRects.rend(); ++it) {
                const ClipRect& r = *it;
                if (mouse.x < r.min.x || mouse.x > r.max.x || mouse.y < r.min.y || mouse.y > r.max.y) continue;
                app.select(r.uid);
                g_drag.uid = r.uid;
                const Clip* c = m.clips.find(r.uid);
                if (mouse.x - r.min.x < kEdge) g_drag.mode = Drag::ResizeLeft;
                else if (r.max.x - mouse.x < kEdge) g_drag.mode = Drag::ResizeRight;
                else g_drag.mode = Drag::MoveClip;
                g_drag.grab = mouseT - c->start;
                break;
            }
            if (g_drag.mode == Drag::None) app.select({});
        }
    }
    if (hovered && g_drag.mode == Drag::None && mouse.x > left) {
        for (const ClipRect& r : clipRects)
            if (mouse.y >= r.min.y && mouse.y <= r.max.y &&
                (std::fabs(mouse.x - r.min.x) < kEdge || std::fabs(mouse.x - r.max.x) < kEdge))
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }

    if (g_drag.mode != Drag::None) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            switch (g_drag.mode) {
                case Drag::Scrub: app.playback.seek(ed, mouseT); break;
                case Drag::MoveClip:
                    if (Clip* c = m.clips.find(g_drag.uid)) {
                        float start = snap(mouseT - g_drag.grab);
                        if (start != c->start) { c->start = start; g_drag.changed = true; }
                        if (const Track* tr = trackAt(mouse.y); tr && tr->uid != c->trackUid) {
                            c->trackUid = tr->uid;
                            g_drag.changed = true;
                        }
                    }
                    break;
                case Drag::ResizeLeft:
                    if (Clip* c = m.clips.find(g_drag.uid)) {
                        float end = c->start + c->duration;
                        float start = std::min(snap(mouseT), end - 0.25f);
                        if (start != c->start) { c->start = start; c->duration = end - start; g_drag.changed = true; }
                    }
                    break;
                case Drag::ResizeRight:
                    if (Clip* c = m.clips.find(g_drag.uid)) {
                        float d = std::max(0.25f, snap(mouseT) - c->start);
                        if (d != c->duration) { c->duration = d; g_drag.changed = true; }
                    }
                    break;
                case Drag::MoveMarker:
                    if (Marker* mk = m.markers.find(g_drag.uid)) {
                        float p = snap(mouseT);
                        if (p != mk->position) { mk->position = p; g_drag.changed = true; }
                    }
                    break;
                case Drag::LoopStart:
                case Drag::LoopEnd:
                    if (Loop* l = m.loops.find(g_drag.uid)) {
                        float t = snap(mouseT);
                        if (g_drag.mode == Drag::LoopStart) t = std::min(t, l->end - 0.25f);
                        else t = std::max(t, l->start + 0.25f);
                        float& field = g_drag.mode == Drag::LoopStart ? l->start : l->end;
                        if (t != field) { field = t; g_drag.changed = true; }
                    }
                    break;
                default: break;
            }
        } else {
            if (g_drag.changed) {
                const char* what = g_drag.mode == Drag::MoveClip ? "Move clip"
                                 : g_drag.mode == Drag::MoveMarker ? "Move marker"
                                 : g_drag.mode == Drag::LoopStart || g_drag.mode == Drag::LoopEnd ? "Edit loop"
                                 : "Resize clip";
                app.commit(what);
            }
            g_drag = Drag{};
        }
    }

    // Scroll and zoom
    if (hovered) {
        if (io.KeyCtrl && io.MouseWheel != 0) {
            float before = mouseT;
            ed.timelineZoom = std::clamp(ed.timelineZoom * std::pow(1.15f, io.MouseWheel), 8.0f, 300.0f);
            ed.timelineScroll = std::max(0.0f, before - (mouse.x - left) / ed.timelineZoom);
        } else if (io.MouseWheel != 0 || io.MouseWheelH != 0) {
            ed.timelineScroll = std::max(0.0f, ed.timelineScroll - (io.MouseWheel + io.MouseWheelH) * 60.0f / ed.timelineZoom);
        }
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right))
            ed.timelineScroll = std::max(0.0f, ed.timelineScroll - io.MouseDelta.x / ed.timelineZoom);
    }
    dl->PopClipRect();

    // ── Drop: graph or media → content clip; filter shader → effect clip,
    // or an Effect when dropped on a content clip.
    if (ImGui::BeginDragDropTarget()) {
        Uid dropped = acceptUidPayload();
        if (!dropped.is_nil()) {
            Document& doc = app.doc;
            Kind kind = m.kindOf(dropped);
            if (kind == Kind::NodeType) {
                const NodeType* t = m.nodeTypes.find(dropped);
                dropped = t->backingUid;
                kind = m.kindOf(dropped);
            }
            const Track* track = trackAt(mouse.y);
            Uid trackUid = track ? track->uid : Uid{};
            float t = snap(mouseT);
            const Clip* under = nullptr;
            for (const ClipRect& r : clipRects)
                if (mouse.x >= r.min.x && mouse.x <= r.max.x && mouse.y >= r.min.y && mouse.y <= r.max.y) under = m.clips.find(r.uid);
            if (kind == Kind::Shader && !isFilterShader(doc, dropped)) {
                app.setStatus("Only filter shaders (one `image` input) can be effects");
            } else if (kind == Kind::Shader && under && m.kindOf(under->sourceUid) != Kind::Shader) {
                Uid clip = under->uid;
                ops::addEffect(doc, clip, dropped);
                app.select(clip);
                app.commit("Add effect");
            } else if (kind == Kind::Graph || kind == Kind::Media || kind == Kind::Shader) {
                if (trackUid.is_nil()) trackUid = ops::addTrack(doc);
                float duration = 8;
                if (kind == Kind::Shader) duration = 4;
                if (const Video* v = m.videos.find(dropped)) duration = std::max(1.0f, v->duration);
                Uid clip = ops::addClip(doc, trackUid, dropped, t, duration);
                app.select(clip);
                app.commit("Add clip");
            } else {
                app.setStatus("Drop a graph, media or filter shader on the timeline");
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::End();
}

} // namespace tessera::ui
