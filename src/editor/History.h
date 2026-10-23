#pragma once

// Persistence and undo (architecture section 2): each commit pushes a
// snapshot of the model, the node positions and the project files. Undo and
// redo restore a snapshot.

#include "editor/EditorState.h"
#include "model/Model.h"

#include <string>
#include <vector>

namespace tessera {

class History {
public:
    void reset(const Document& doc, const EditorState& ed);
    void commit(const Document& doc, const EditorState& ed, const std::string& label);
    bool undo(Document& doc, EditorState& ed);
    bool redo(Document& doc, EditorState& ed);
    bool canUndo() const { return cursor_ > 0; }
    bool canRedo() const { return cursor_ + 1 < (int)stack_.size(); }
    const std::string& undoLabel() const;
    const std::string& redoLabel() const;
    bool dirty() const { return cursor_ != savedAt_; }
    void markSaved() { savedAt_ = cursor_; }

private:
    struct Snapshot {
        Document doc;
        std::unordered_map<Uid, Vec2> positions;
        std::string label;
    };
    void restore(const Snapshot& s, Document& doc, EditorState& ed) const;

    std::vector<Snapshot> stack_;
    int cursor_ = -1;
    int savedAt_ = 0;
    static constexpr size_t kMaxSnapshots = 200;
};

} // namespace tessera
