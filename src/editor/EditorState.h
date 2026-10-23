#pragma once

// Editor state (architecture section 2): not part of the model. Node
// positions are saved with the project; the rest is per session.

#include "core/Math.h"
#include "core/Uid.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace tessera {

struct GraphView {
    Vec2 pan{40, 40};   // screen offset of the canvas origin, in pixels
    float zoom = 1.0f;
};

enum class PlayState { Paused, Playing, Holding };
const char* toString(PlayState state);

struct EditorState {
    // Saved with the project and part of undo snapshots.
    std::unordered_map<Uid, Vec2> nodePositions;

    // Graph editor
    std::unordered_map<Uid, GraphView> graphViews;
    std::vector<Uid> openGraphs;
    Uid activeGraph;
    Uid focusGraph;   // tab to select on the next frame

    Uid selection;
    Uid selectedFolder;

    // Playback
    float playhead = 0;
    PlayState playState = PlayState::Paused;
    float holdRemaining = 0;
    bool loopEnabled = false;

    // Timeline view
    float timelineZoom = 48;   // pixels per second
    float timelineScroll = 0;  // seconds at the left edge

    // Output
    int outputWidth = 640, outputHeight = 360;

    // Shader editor buffers, by Shader uid
    std::unordered_map<Uid, std::string> shaderBuffers;

    std::string projectPath;   // directory of the saved project, empty if never saved
    std::string status;        // last message for the status bar
    float statusTime = 0;

    void openGraph(const Uid& graph) {
        bool open = false;
        for (const Uid& g : openGraphs) open |= g == graph;
        if (!open) openGraphs.push_back(graph);
        focusGraph = graph;
    }
};

} // namespace tessera
