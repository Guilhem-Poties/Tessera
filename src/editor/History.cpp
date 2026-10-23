#include "editor/History.h"

namespace tessera {

const char* toString(PlayState state) {
    switch (state) {
        case PlayState::Paused: return "Paused";
        case PlayState::Playing: return "Playing";
        case PlayState::Holding: return "Holding";
    }
    return "Paused";
}

void History::reset(const Document& doc, const EditorState& ed) {
    stack_.clear();
    stack_.push_back({doc, ed.nodePositions, "Open"});
    cursor_ = 0;
    savedAt_ = 0;
}

void History::commit(const Document& doc, const EditorState& ed, const std::string& label) {
    stack_.resize((size_t)cursor_ + 1);
    stack_.push_back({doc, ed.nodePositions, label});
    if (stack_.size() > kMaxSnapshots) {
        stack_.erase(stack_.begin());
        --savedAt_;
    }
    cursor_ = (int)stack_.size() - 1;
}

void History::restore(const Snapshot& s, Document& doc, EditorState& ed) const {
    doc = s.doc;
    ed.nodePositions = s.positions;
}

bool History::undo(Document& doc, EditorState& ed) {
    if (!canUndo()) return false;
    --cursor_;
    restore(stack_[(size_t)cursor_], doc, ed);
    return true;
}

bool History::redo(Document& doc, EditorState& ed) {
    if (!canRedo()) return false;
    ++cursor_;
    restore(stack_[(size_t)cursor_], doc, ed);
    return true;
}

const std::string& History::undoLabel() const {
    static const std::string none;
    return canUndo() ? stack_[(size_t)cursor_].label : none;
}

const std::string& History::redoLabel() const {
    static const std::string none;
    return canRedo() ? stack_[(size_t)cursor_ + 1].label : none;
}

} // namespace tessera
