// Graph editor (architecture section 8): one tab per graph, with pan, zoom
// and fit. Nodes show a live preview and their parameter values. Dragging
// between ports creates a link, dragging from a linked input re-routes it.
// Every link shows its converter. Double-click opens quick-add.
//
// Drawn by hand with ImDrawList so that everything scales with the zoom.

#include "model/Declarations.h"
#include "model/Operations.h"
#include "ui/App.h"
#include "ui/Theme.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cmath>

namespace tessera::ui {

namespace {

constexpr float kNodeWidth = 180;
constexpr float kHeaderHeight = 24;
constexpr float kRowHeight = 20;
constexpr float kPortRadius = 5;

struct Row {
    Uid param;
    ParamDecl decl;
    float y;   // canvas units, row centre
};

struct NodeLayout {
    Uid node;
    std::string label, category;
    Vec2 pos;                  // canvas units
    float height = 0;
    bool preview = false;      // has a texture to show
    float previewHeight = 0;
    std::vector<Row> rows;
    std::string error;
};

struct Interaction {
    enum Mode { None, Pan, Move, LinkDrag, Scrub } mode = None;
    Uid param;         // link origin, or scrubbed parameter
    bool fromOutput = true;
    Uid node;          // moved node
    ImVec2 pressMouse;
    Vec2 startPos;
    float startValue = 0;
    bool changed = false;
    Vec2 quickAddPos;  // canvas units
    bool openQuickAdd = false;
};

Interaction g_drag;

struct View {
    ImVec2 origin;
    GraphView* v;
    ImVec2 toScreen(Vec2 p) const { return ImVec2(origin.x + v->pan.x + p.x * v->zoom, origin.y + v->pan.y + p.y * v->zoom); }
    Vec2 toCanvas(ImVec2 s) const { return {(s.x - origin.x - v->pan.x) / v->zoom, (s.y - origin.y - v->pan.y) / v->zoom}; }
};

std::vector<NodeLayout> layoutGraph(App& app, const Uid& graph) {
    const Model& m = app.doc.model;
    std::vector<NodeLayout> out;
    int index = 0;
    for (const Node& n : m.nodes) {
        if (n.graphUid != graph) continue;
        const NodeType* type = m.nodeTypes.find(n.typeUid);
        NodeLayout L;
        L.node = n.uid;
        L.label = type ? type->label : "?";
        L.category = type ? type->category : "";
        auto pos = app.ed.nodePositions.find(n.uid);
        if (pos == app.ed.nodePositions.end()) {
            app.ed.nodePositions[n.uid] = {40.0f + 30.0f * (float)index, 40.0f + 30.0f * (float)index};
            pos = app.ed.nodePositions.find(n.uid);
        }
        L.pos = pos->second;
        std::vector<ParamDecl> decls = nodeTypeDecls(app.doc, n.typeUid);
        for (const ParamDecl& d : decls)
            if (d.type == ParamType::Texture) L.preview = true;
        if (type && type->kind == "shader") L.error = app.rt.shaderError(app.doc, type->backingUid);
        float y = kHeaderHeight;
        if (L.preview) {
            L.previewHeight = (kNodeWidth - 12) * 9.0f / 16.0f + 8;
            y += L.previewHeight;
        }
        // Inputs first, then outputs.
        for (int pass = 0; pass < 2; ++pass)
            for (const ParamDecl& d : decls) {
                if (d.output != (pass == 1)) continue;
                const Parameter* p = m.findParam(n.uid, d.name);
                if (!p) continue;
                L.rows.push_back({p->uid, d, y + kRowHeight * 0.5f});
                y += kRowHeight;
            }
        L.height = y + 6;
        out.push_back(std::move(L));
        ++index;
    }
    return out;
}

Vec2 portPos(const NodeLayout& L, const Row& r) {
    return {r.decl.output ? L.pos.x + kNodeWidth : L.pos.x, L.pos.y + r.y};
}

bool hasPort(const ParamDecl& d) { return d.output || (d.linkable && d.type != ParamType::Label); }

ImVec2 bezierPoint(ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d, float t) {
    float u = 1 - t;
    float w0 = u * u * u, w1 = 3 * u * u * t, w2 = 3 * u * t * t, w3 = t * t * t;
    return ImVec2(a.x * w0 + b.x * w1 + c.x * w2 + d.x * w3, a.y * w0 + b.y * w1 + c.y * w2 + d.y * w3);
}

void drawLinkCurve(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float zoom, float thickness) {
    float dx = std::max(40.0f * zoom, std::fabs(b.x - a.x) * 0.5f);
    dl->AddBezierCubic(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, col, thickness);
}

std::string rowValue(App& app, const Row& r) {
    const Model& m = app.doc.model;
    const Parameter* p = m.params.find(r.param);
    if (!p) return "";
    if (r.decl.type == ParamType::Texture || r.decl.type == ParamType::Any) return "";
    if (const LiveValue* v = app.rt.liveValue(r.param)) return describeLive(*v, &r.decl);
    return moduleFor(r.decl.type).describe(p->value, r.decl);
}

void fitView(App& app, const std::vector<NodeLayout>& layouts, GraphView& view, ImVec2 size) {
    if (layouts.empty()) {
        view = GraphView{};
        return;
    }
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (const NodeLayout& L : layouts) {
        x0 = std::min(x0, L.pos.x); y0 = std::min(y0, L.pos.y);
        x1 = std::max(x1, L.pos.x + kNodeWidth); y1 = std::max(y1, L.pos.y + L.height);
    }
    float margin = 40;
    float zoom = std::min((size.x - 2 * margin) / std::max(1.0f, x1 - x0), (size.y - 2 * margin) / std::max(1.0f, y1 - y0));
    view.zoom = std::clamp(zoom, 0.25f, 1.5f);
    view.pan = {size.x * 0.5f - (x0 + x1) * 0.5f * view.zoom, size.y * 0.5f - (y0 + y1) * 0.5f * view.zoom};
    (void)app;
}

void quickAddPopup(App& app, const Uid& graph) {
    if (g_drag.openQuickAdd) {
        ImGui::OpenPopup("Quick add");
        g_drag.openQuickAdd = false;
    }
    ImGui::SetNextWindowSize(ImVec2(240, 320), ImGuiCond_Appearing);
    if (!ImGui::BeginPopup("Quick add")) return;
    static std::string filter;
    if (ImGui::IsWindowAppearing()) {
        filter.clear();
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::SetNextItemWidth(-1);
    bool enter = ImGui::InputTextWithHint("##q", "Add node...", &filter, ImGuiInputTextFlags_EnterReturnsTrue);
    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };
    Uid chosen;
    ImGui::BeginChild("types");
    std::string lastCategory;
    for (const NodeType& t : app.doc.model.nodeTypes) {
        if (!filter.empty() && lower(t.label).find(lower(filter)) == std::string::npos &&
            lower(t.category).find(lower(filter)) == std::string::npos)
            continue;
        if (enter && chosen.is_nil()) chosen = t.uid;
        if (t.category != lastCategory) {
            ImGui::TextColored(toVec4(categoryColor(t.category)), "%s", t.category.c_str());
            lastCategory = t.category;
        }
        std::string label = "   " + t.label + "##" + toString(t.uid);
        if (ImGui::Selectable(label.c_str())) chosen = t.uid;
    }
    ImGui::EndChild();
    if (!chosen.is_nil()) {
        Uid node = ops::addNode(app.doc, graph, chosen);
        app.ed.nodePositions[node] = g_drag.quickAddPos;
        app.select(node);
        app.commit("Add " + app.doc.model.displayName(node));
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void drawCanvas(App& app, const Uid& graphUid) {
    Model& m = app.doc.model;
    ImGuiIO& io = ImGui::GetIO();
    GraphView& gv = app.ed.graphViews[graphUid];
    ImVec2 origin = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    if (size.x < 50 || size.y < 50) return;
    View view{origin, &gv};
    float zoom = gv.zoom;

    ImGui::InvisibleButton("canvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight |
                                               ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();

    // Drop assets from the Assets panel.
    if (ImGui::BeginDragDropTarget()) {
        Uid dropped = acceptUidPayload();
        if (!dropped.is_nil()) {
            Uid type = ops::nodeTypeForAsset(m, dropped);
            if (!type.is_nil()) {
                Uid node = ops::addNode(app.doc, graphUid, type);
                app.ed.nodePositions[node] = view.toCanvas(io.MousePos);
                app.select(node);
                app.commit("Add " + m.displayName(node));
            } else {
                app.setStatus("Drop a node type, shader or media here");
            }
        }
        ImGui::EndDragDropTarget();
    }

    std::vector<NodeLayout> layouts = layoutGraph(app, graphUid);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);
    dl->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(22, 24, 29, 255));

    // Grid
    float step = 32 * zoom;
    if (step > 6) {
        for (float x = std::fmod(gv.pan.x, step); x < size.x; x += step)
            dl->AddLine(ImVec2(origin.x + x, origin.y), ImVec2(origin.x + x, origin.y + size.y), IM_COL32(255, 255, 255, 10));
        for (float y = std::fmod(gv.pan.y, step); y < size.y; y += step)
            dl->AddLine(ImVec2(origin.x, origin.y + y), ImVec2(origin.x + size.x, origin.y + y), IM_COL32(255, 255, 255, 10));
    }

    ImFont* font = ImGui::GetFont();
    float fontSize = ImGui::GetFontSize() * zoom;
    bool drawText = zoom > 0.45f;
    auto text = [&](ImVec2 p, ImU32 col, const std::string& s, float maxWidth = 0) {
        if (!drawText || s.empty()) return;
        if (maxWidth > 0) {
            ImVec4 clip(p.x, p.y - 2, p.x + maxWidth, p.y + fontSize + 2);
            dl->AddText(font, fontSize, p, col, s.c_str(), nullptr, 0, &clip);
        } else {
            dl->AddText(font, fontSize, p, col, s.c_str());
        }
    };
    auto textWidth = [&](const std::string& s) { return font->CalcTextSizeA(fontSize, FLT_MAX, 0, s.c_str()).x; };

    // Port lookup
    struct PortHit {
        Uid param;
        ImVec2 pos;
        bool output;
        ParamType type;
    };
    std::vector<PortHit> ports;
    for (const NodeLayout& L : layouts)
        for (const Row& r : L.rows)
            if (hasPort(r.decl)) ports.push_back({r.param, view.toScreen(portPos(L, r)), r.decl.output, r.decl.type});
    auto portAt = [&](ImVec2 mouse) -> const PortHit* {
        float radius = std::max(8.0f, (kPortRadius + 4) * zoom);
        for (const PortHit& p : ports) {
            float dx = mouse.x - p.pos.x, dy = mouse.y - p.pos.y;
            if (dx * dx + dy * dy <= radius * radius) return &p;
        }
        return nullptr;
    };
    auto portPosOf = [&](const Uid& param) -> ImVec2 {
        for (const PortHit& p : ports)
            if (p.param == param) return p.pos;
        return ImVec2(-1e6f, -1e6f);
    };

    // ── Links ──
    struct LabelHit {
        Uid link;
        ImVec2 min, max;
    };
    std::vector<LabelHit> labels;
    for (const Link& l : m.links) {
        if (l.graphUid != graphUid) continue;
        ImVec2 a = portPosOf(l.sourceParam), b = portPosOf(l.targetParam);
        const Parameter* src = m.params.find(l.sourceParam);
        ImU32 col = src ? paramTypeColor(src->paramType()) : kMuted;
        bool selected = app.ed.selection == l.uid;
        drawLinkCurve(dl, a, b, selected ? IM_COL32(255, 255, 255, 255) : col, zoom, selected ? 3.0f : 2.0f);
        // Every link runs through a converter: show it on the link.
        float dx = std::max(40.0f * zoom, std::fabs(b.x - a.x) * 0.5f);
        ImVec2 mid = bezierPoint(a, ImVec2(a.x + dx, a.y), ImVec2(b.x - dx, b.y), b, 0.5f);
        const Converter* conv = m.converters.find(l.converterUid);
        std::string label = conv ? conv->label : "?";
        if (drawText) {
            float w = textWidth(label) + 10 * zoom, h = fontSize + 4 * zoom;
            ImVec2 mn(mid.x - w * 0.5f, mid.y - h * 0.5f), mx(mid.x + w * 0.5f, mid.y + h * 0.5f);
            dl->AddRectFilled(mn, mx, selected ? IM_COL32(110, 85, 160, 255) : IM_COL32(52, 44, 74, 240), h * 0.5f);
            dl->AddRect(mn, mx, kConverterPurple, h * 0.5f);
            text(ImVec2(mn.x + 5 * zoom, mn.y + 2 * zoom), IM_COL32(225, 215, 245, 255), label);
            labels.push_back({l.uid, mn, mx});
        } else {
            dl->AddCircleFilled(mid, 4, kConverterPurple);
            labels.push_back({l.uid, ImVec2(mid.x - 5, mid.y - 5), ImVec2(mid.x + 5, mid.y + 5)});
        }
    }

    // ── Nodes ──
    for (const NodeLayout& L : layouts) {
        ImVec2 mn = view.toScreen(L.pos);
        ImVec2 mx = view.toScreen({L.pos.x + kNodeWidth, L.pos.y + L.height});
        bool selected = app.ed.selection == L.node;
        float round = 6 * zoom;
        dl->AddRectFilled(ImVec2(mn.x + 3, mn.y + 4), ImVec2(mx.x + 3, mx.y + 4), IM_COL32(0, 0, 0, 70), round);
        dl->AddRectFilled(mn, mx, IM_COL32(36, 40, 48, 250), round);
        ImU32 headerCol = L.error.empty() ? categoryColor(L.category) : IM_COL32(190, 60, 60, 255);
        ImVec2 hmx = view.toScreen({L.pos.x + kNodeWidth, L.pos.y + kHeaderHeight});
        dl->AddRectFilled(mn, hmx, headerCol, round, ImDrawFlags_RoundTop);
        text(ImVec2(mn.x + 8 * zoom, mn.y + 4 * zoom), IM_COL32(255, 255, 255, 255),
             L.label + (L.error.empty() ? "" : "  (!)"), (kNodeWidth - 12) * zoom);
        dl->AddRect(mn, mx, selected ? IM_COL32(255, 255, 255, 230) : IM_COL32(70, 76, 90, 255), round, 0, selected ? 2.0f : 1.0f);

        if (L.preview) {
            ImVec2 pmn = view.toScreen({L.pos.x + 6, L.pos.y + kHeaderHeight + 4});
            ImVec2 pmx = view.toScreen({L.pos.x + kNodeWidth - 6, L.pos.y + kHeaderHeight + L.previewHeight - 4});
            dl->AddRectFilled(pmn, pmx, IM_COL32(0, 0, 0, 255));
            // The node's texture output, or for the Output node its input.
            TexRef tex;
            for (const Row& r : L.rows)
                if (r.decl.type == ParamType::Texture && (r.decl.output || L.label == "Output"))
                    if (const LiveValue* v = app.rt.liveValue(r.param))
                        if (auto t = std::get_if<TexRef>(v)) tex = *t;
            if (tex.tex) dl->AddImage((ImTextureID)(intptr_t)tex.tex, pmn, pmx, ImVec2(0, 1), ImVec2(1, 0));
        }

        for (const Row& r : L.rows) {
            ImVec2 rowMin = view.toScreen({L.pos.x, L.pos.y + r.y - kRowHeight * 0.5f});
            float ty = rowMin.y + (kRowHeight * zoom - fontSize) * 0.5f;
            bool linked = !r.decl.output && m.linkInto(r.param);
            ImU32 nameCol = IM_COL32(200, 205, 215, 255);
            std::string value = rowValue(app, r);
            if (r.decl.output) {
                float w = textWidth(r.decl.name);
                text(ImVec2(mx.x - 12 * zoom - w, ty), nameCol, r.decl.name);
                text(ImVec2(mn.x + 10 * zoom, ty), IM_COL32(140, 148, 160, 255), value, (kNodeWidth * 0.5f) * zoom);
            } else {
                text(ImVec2(mn.x + 12 * zoom, ty), nameCol, r.decl.name, kNodeWidth * 0.45f * zoom);
                if (r.decl.type == ParamType::Color && !linked) {
                    if (const Parameter* p = m.params.find(r.param)) {
                        Color c = valueAsColor(p->value);
                        ImVec2 sw(mx.x - 30 * zoom, rowMin.y + 4 * zoom);
                        dl->AddRectFilled(sw, ImVec2(mx.x - 10 * zoom, rowMin.y + (kRowHeight - 4) * zoom),
                                          ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, 1)), 2);
                    }
                } else {
                    float w = textWidth(value);
                    ImU32 valCol = linked ? IM_COL32(180, 160, 230, 255) : IM_COL32(235, 200, 120, 255);
                    text(ImVec2(mx.x - 10 * zoom - w, ty), valCol, value);
                }
            }
            if (hasPort(r.decl)) {
                ImVec2 pp = view.toScreen(portPos(L, r));
                ImU32 pc = paramTypeColor(r.decl.type);
                bool connected = linked || r.decl.output;
                dl->AddCircleFilled(pp, kPortRadius * zoom, connected ? pc : IM_COL32(36, 40, 48, 255));
                dl->AddCircle(pp, kPortRadius * zoom, pc, 0, 1.5f);
            }
        }
    }

    // ── Interaction ──
    ImVec2 mouse = io.MousePos;
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        g_drag = Interaction{};
        g_drag.pressMouse = mouse;
        if (const PortHit* port = portAt(mouse)) {
            g_drag.mode = Interaction::LinkDrag;
            g_drag.param = port->param;
            g_drag.fromOutput = port->output;
            if (!port->output)
                if (const Link* existing = m.linkInto(port->param)) {
                    // Re-route: pick the link up by its source end.
                    g_drag.param = existing->sourceParam;
                    g_drag.fromOutput = true;
                    ops::deleteEntity(app.doc, existing->uid);
                    g_drag.changed = true;
                }
        } else {
            bool hit = false;
            for (const LabelHit& lh : labels)
                if (mouse.x >= lh.min.x && mouse.x <= lh.max.x && mouse.y >= lh.min.y && mouse.y <= lh.max.y) {
                    app.select(lh.link);
                    hit = true;
                    break;
                }
            for (auto it = layouts.rbegin(); !hit && it != layouts.rend(); ++it) {
                const NodeLayout& L = *it;
                ImVec2 mn = view.toScreen(L.pos), mx = view.toScreen({L.pos.x + kNodeWidth, L.pos.y + L.height});
                if (mouse.x < mn.x || mouse.x > mx.x || mouse.y < mn.y || mouse.y > mx.y) continue;
                hit = true;
                app.select(L.node);
                g_drag.mode = Interaction::Move;
                g_drag.node = L.node;
                g_drag.startPos = L.pos;
                // Rows: scrub floats, toggle bools, cycle choices.
                for (const Row& r : L.rows) {
                    ImVec2 rmn = view.toScreen({L.pos.x, L.pos.y + r.y - kRowHeight * 0.5f});
                    ImVec2 rmx = view.toScreen({L.pos.x + kNodeWidth, L.pos.y + r.y + kRowHeight * 0.5f});
                    if (r.decl.output || mouse.y < rmn.y || mouse.y > rmx.y || mouse.x < rmn.x + (kNodeWidth * 0.4f) * zoom) continue;
                    if (m.linkInto(r.param)) break;
                    Parameter* p = m.params.find(r.param);
                    if (!p) break;
                    if (r.decl.type == ParamType::Float) {
                        g_drag.mode = Interaction::Scrub;
                        g_drag.param = r.param;
                        g_drag.startValue = valueAsFloat(p->value);
                    } else if (r.decl.type == ParamType::Bool) {
                        p->value = !(valueAsFloat(p->value) > 0.5f);
                        g_drag.mode = Interaction::None;
                        app.commit("Toggle " + r.decl.name);
                    } else if (r.decl.type == ParamType::Choice && !r.decl.options.empty()) {
                        p->value = ((int)valueAsFloat(p->value) + 1) % (int)r.decl.options.size();
                        g_drag.mode = Interaction::None;
                        app.commit("Change " + r.decl.name);
                    }
                    break;
                }
            }
            if (!hit) {
                app.select(graphUid);
                g_drag.mode = Interaction::None;
            }
        }
    }
    if (hovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle))) {
        g_drag = Interaction{};
        g_drag.mode = Interaction::Pan;
    }
    if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !portAt(mouse)) {
        bool onNode = false;
        for (const NodeLayout& L : layouts) {
            ImVec2 mn = view.toScreen(L.pos), mx = view.toScreen({L.pos.x + kNodeWidth, L.pos.y + L.height});
            onNode |= mouse.x >= mn.x && mouse.x <= mx.x && mouse.y >= mn.y && mouse.y <= mx.y;
        }
        if (!onNode) {
            g_drag.quickAddPos = view.toCanvas(mouse);
            g_drag.openQuickAdd = true;
            g_drag.mode = Interaction::None;
        }
    }

    switch (g_drag.mode) {
        case Interaction::Pan:
            if (ImGui::IsMouseDown(ImGuiMouseButton_Right) || ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
                gv.pan.x += io.MouseDelta.x;
                gv.pan.y += io.MouseDelta.y;
            } else {
                g_drag.mode = Interaction::None;
            }
            break;
        case Interaction::Move:
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                float dx = (mouse.x - g_drag.pressMouse.x) / zoom, dy = (mouse.y - g_drag.pressMouse.y) / zoom;
                if (std::fabs(dx) + std::fabs(dy) > 2 / zoom) g_drag.changed = true;
                if (g_drag.changed) app.ed.nodePositions[g_drag.node] = {g_drag.startPos.x + dx, g_drag.startPos.y + dy};
            } else {
                if (g_drag.changed) app.commit("Move node");
                g_drag.mode = Interaction::None;
            }
            break;
        case Interaction::Scrub:
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                Parameter* p = m.params.find(g_drag.param);
                auto decl = p ? declFor(app.doc, *p) : std::nullopt;
                if (p && decl) {
                    float range = decl->max - decl->min;
                    float speed = range / (160.0f * zoom) * (io.KeyShift ? 0.1f : 1.0f);
                    float v = g_drag.startValue + (mouse.x - g_drag.pressMouse.x) * speed;
                    if (!io.KeyAlt) v = std::clamp(v, decl->min, decl->max);
                    if (v != valueAsFloat(p->value)) {
                        p->value = v;
                        g_drag.changed = true;
                    }
                }
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
            } else {
                if (g_drag.changed) app.commit("Edit value");
                g_drag.mode = Interaction::None;
            }
            break;
        case Interaction::LinkDrag: {
            ImVec2 from = portPosOf(g_drag.param);
            const Parameter* p = m.params.find(g_drag.param);
            ImU32 col = p ? paramTypeColor(p->paramType()) : kMuted;
            if (g_drag.fromOutput) drawLinkCurve(dl, from, mouse, col, zoom, 2);
            else drawLinkCurve(dl, mouse, from, col, zoom, 2);
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                std::string why;
                const PortHit* target = portAt(mouse);
                if (target && target->param != g_drag.param) {
                    Uid link = ops::link(app.doc, g_drag.param, target->param, &why);
                    if (!link.is_nil()) {
                        app.select(link);
                        app.commit("Link");
                    } else {
                        app.setStatus(why);
                        if (g_drag.changed) app.commit("Unlink");
                    }
                } else if (g_drag.changed) {
                    app.commit("Unlink");   // a re-routed link dropped on nothing is removed
                }
                g_drag.mode = Interaction::None;
            }
            break;
        }
        default: break;
    }

    // Zoom around the mouse.
    if (hovered && io.MouseWheel != 0) {
        Vec2 before = view.toCanvas(mouse);
        gv.zoom = std::clamp(gv.zoom * std::pow(1.15f, io.MouseWheel), 0.25f, 2.5f);
        gv.pan = {mouse.x - origin.x - before.x * gv.zoom, mouse.y - origin.y - before.y * gv.zoom};
    }

    if (layouts.size() <= 1) {
        const char* hint = "Double-click to add a node. Drag between ports to link.";
        ImVec2 ts = ImGui::CalcTextSize(hint);
        dl->AddText(ImVec2(origin.x + (size.x - ts.x) * 0.5f, origin.y + size.y - ts.y - 12), IM_COL32(140, 148, 160, 255), hint);
    }
    dl->PopClipRect();

    quickAddPopup(app, graphUid);
}

} // namespace

static Uid g_fitGraph;

void drawGraphEditor(App& app) {
    if (!ImGui::Begin("Graph editor")) {
        ImGui::End();
        return;
    }
    Model& m = app.doc.model;

    if (ImGui::Button("+ Graph")) {
        Uid g = ops::addGraph(app.doc, "Graph " + std::to_string(m.graphs.size() + 1));
        app.ed.openGraph(g);
        app.select(g);
        app.commit("New graph");
    }
    if (m.graphs.contains(app.ed.activeGraph)) {
        GraphView& gv = app.ed.graphViews[app.ed.activeGraph];
        ImGui::SameLine();
        if (ImGui::Button("Fit")) g_fitGraph = app.ed.activeGraph;
        ImGui::SameLine();
        ImGui::TextDisabled("%d%%", (int)std::lround(gv.zoom * 100));
        ImGui::SameLine();
        Graph* g = m.graphs.find(app.ed.activeGraph);
        ImGui::TextDisabled("· %d×%d · RMB/MMB pan · wheel zoom", (int)g->resolution.x, (int)g->resolution.y);
    }

    if (app.ed.openGraphs.empty()) {
        app.ed.activeGraph = {};
        ImVec2 avail = ImGui::GetContentRegionAvail();
        const char* msg = "Create a graph to start patching.";
        ImVec2 ts = ImGui::CalcTextSize(msg);
        ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (avail.x - ts.x) * 0.5f, ImGui::GetCursorPosY() + avail.y * 0.4f));
        ImGui::TextDisabled("%s", msg);
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("graphs", ImGuiTabBarFlags_Reorderable | ImGuiTabBarFlags_AutoSelectNewTabs)) {
        std::vector<Uid> open = app.ed.openGraphs;
        for (const Uid& g : open) {
            const Graph* graph = m.graphs.find(g);
            if (!graph) continue;
            bool keep = true;
            ImGuiTabItemFlags flags = app.ed.focusGraph == g ? ImGuiTabItemFlags_SetSelected : 0;
            std::string label = graph->name + "###" + toString(g);
            if (ImGui::BeginTabItem(label.c_str(), &keep, flags)) {
                app.ed.activeGraph = g;
                if (!app.ed.graphViews.count(g)) g_fitGraph = g;   // first time shown
                if (g_fitGraph == g) {
                    GraphView& gv = app.ed.graphViews[g];
                    std::vector<NodeLayout> layouts = layoutGraph(app, g);
                    fitView(app, layouts, gv, ImGui::GetContentRegionAvail());
                    g_fitGraph = {};
                }
                drawCanvas(app, g);
                ImGui::EndTabItem();
            }
            if (!keep) std::erase(app.ed.openGraphs, g);
        }
        app.ed.focusGraph = {};
        ImGui::EndTabBar();
    }
    ImGui::End();
}

} // namespace tessera::ui
