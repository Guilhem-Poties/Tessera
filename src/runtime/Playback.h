#pragma once

// Playback (architecture section 7.4): hold points pause, loops jump back,
// cue points jump the playhead when a watched value rises past a threshold.

#include "editor/EditorState.h"
#include "model/Model.h"

#include <unordered_map>

namespace tessera {

class Runtime;

class Playback {
public:
    // Advances the playhead. Called at the start of each frame.
    void update(const Document& doc, EditorState& ed, float dt);
    // Called at the end of the runtime frame, once live values are known.
    // Fires while paused too, which makes cue points live triggers.
    // Returns the cue point that fired, if any.
    Uid cueCheck(const Document& doc, EditorState& ed, const Runtime& rt);

    void play(EditorState& ed) const;
    void pause(EditorState& ed) const;
    void seek(EditorState& ed, float time) const;

    static const Loop* activeLoop(const Model& model);
    static float timelineEnd(const Model& model);

private:
    std::unordered_map<Uid, float> previous_;   // last value per CuePoint
};

} // namespace tessera
