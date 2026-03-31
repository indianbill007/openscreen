#include "EditorState.h"

namespace openscreen {

EditorHistory::EditorHistory(EditorState initial)
    : present_(std::move(initial)) {}

const EditorState& EditorHistory::state() const {
    return present_;
}

void EditorHistory::checkpoint() {
    past_.push_back(present_);
    if (past_.size() > MAX_HISTORY) {
        past_.pop_front();
    }
}

void EditorHistory::pushState(const EditorState& newState) {
    checkpoint();
    present_ = newState;
    future_.clear();
    dirty_ = false;
}

void EditorHistory::updateState(const EditorState& newState) {
    if (!dirty_) {
        checkpoint();
        future_.clear();
        dirty_ = true;
    }
    present_ = newState;
}

void EditorHistory::commitState() {
    dirty_ = false;
}

bool EditorHistory::undo() {
    if (past_.empty()) {
        return false;
    }
    future_.push_front(present_);
    present_ = past_.back();
    past_.pop_back();
    dirty_ = false;
    return true;
}

bool EditorHistory::redo() {
    if (future_.empty()) {
        return false;
    }
    past_.push_back(present_);
    if (past_.size() > MAX_HISTORY) {
        past_.pop_front();
    }
    present_ = future_.front();
    future_.pop_front();
    dirty_ = false;
    return true;
}

bool EditorHistory::canUndo() const {
    return !past_.empty();
}

bool EditorHistory::canRedo() const {
    return !future_.empty();
}

} // namespace openscreen
