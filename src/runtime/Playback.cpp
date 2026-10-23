#include "runtime/Playback.h"

#include "runtime/Runtime.h"

#include <algorithm>

namespace tessera {

const Loop* Playback::activeLoop(const Model& model) {
    // Loop is not attached to the timeline yet (open decision): the first one is used.
    return model.loops.rows.empty() ? nullptr : &model.loops.rows.front();
}

float Playback::timelineEnd(const Model& model) {
    float end = 0;
    for (const Clip& c : model.clips) end = std::max(end, c.start + c.duration);
    for (const Marker& mk : model.markers) end = std::max(end, mk.position);
    return end;
}

void Playback::play(EditorState& ed) const {
    if (ed.playState == PlayState::Paused) ed.playState = PlayState::Playing;
}

void Playback::pause(EditorState& ed) const {
    ed.playState = PlayState::Paused;
    ed.holdRemaining = 0;
}

void Playback::seek(EditorState& ed, float time) const {
    ed.playhead = std::max(0.0f, time);
    if (ed.playState == PlayState::Holding) {
        ed.playState = PlayState::Playing;
        ed.holdRemaining = 0;
    }
}

void Playback::update(const Document& doc, EditorState& ed, float dt) {
    const Model& m = doc.model;
    if (ed.playState == PlayState::Holding) {
        ed.holdRemaining -= dt;
        if (ed.holdRemaining > 0) return;
        dt = -ed.holdRemaining;   // the rest of this frame plays on
        ed.holdRemaining = 0;
        ed.playState = PlayState::Playing;
    }
    if (ed.playState != PlayState::Playing) return;

    float from = ed.playhead, to = from + dt;

    // Crossing a hold point pauses playback for its duration.
    const HoldPoint* hold = nullptr;
    float holdAt = to;
    for (const HoldPoint& h : m.holdPoints) {
        const Marker* mk = m.markers.find(h.uid);
        if (mk && mk->position > from && mk->position <= to && mk->position <= holdAt) {
            hold = &h;
            holdAt = mk->position;
        }
    }
    if (hold) {
        ed.playhead = holdAt;
        ed.playState = PlayState::Holding;
        ed.holdRemaining = hold->duration;
        return;
    }

    // With loop playback on, reaching the loop end jumps back to its start.
    if (ed.loopEnabled)
        if (const Loop* loop = activeLoop(m); loop && loop->end > loop->start && from < loop->end && to >= loop->end)
            to = loop->start + (to - loop->end);
    ed.playhead = to;
}

Uid Playback::cueCheck(const Document& doc, EditorState& ed, const Runtime& rt) {
    const Model& m = doc.model;
    Uid fired;
    for (const CuePoint& cue : m.cuePoints) {
        const Marker* mk = m.markers.find(cue.uid);
        const Parameter* p = m.params.find(cue.parameterUid);
        if (!mk || !p) {
            previous_.erase(cue.uid);
            continue;
        }
        float value;
        if (const LiveValue* v = rt.liveValue(p->uid)) {
            value = std::visit([](const auto& x) -> float {
                using X = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<X, float>) return x;
                else if constexpr (std::is_same_v<X, int>) return (float)x;
                else if constexpr (std::is_same_v<X, bool>) return x ? 1.0f : 0.0f;
                else if constexpr (std::is_same_v<X, Color>) return luma(x);
                else return 0.0f;
            }, *v);
        } else {
            value = valueAsFloat(p->value);
        }
        auto it = previous_.find(cue.uid);
        bool rising = it != previous_.end() && it->second <= cue.condition && value > cue.condition;
        previous_[cue.uid] = value;
        if (rising) {
            ed.playhead = mk->position;
            if (ed.playState == PlayState::Holding) ed.playState = PlayState::Playing;
            ed.holdRemaining = 0;
            fired = cue.uid;
        }
    }
    return fired;
}

} // namespace tessera
