#include "editor/EditorSeams.h"

#include <algorithm>
#include <utility>

namespace nexus::tooling::editor {

// --- EditorDocument --------------------------------------------------------

void EditorDocument::setLabel(std::string label) { label_ = std::move(label); }
const std::string& EditorDocument::label() const noexcept { return label_; }

void EditorDocument::setDirty(bool dirty) noexcept { dirty_ = dirty; }
bool EditorDocument::isDirty() const noexcept { return dirty_; }

void EditorDocument::setSessionKind(std::string kind) { sessionKind_ = std::move(kind); }
const std::string& EditorDocument::sessionKind() const noexcept { return sessionKind_; }

// --- SelectionModel --------------------------------------------------------

void SelectionModel::clear() noexcept { ids_.clear(); }
void SelectionModel::add(const std::string& id) { ids_.insert(id); }
void SelectionModel::remove(const std::string& id) { ids_.erase(id); }
bool SelectionModel::contains(const std::string& id) const { return ids_.contains(id); }
std::size_t SelectionModel::size() const noexcept { return ids_.size(); }

std::vector<std::string> SelectionModel::ids() const
{
    return std::vector<std::string>(ids_.begin(), ids_.end());
}

// --- CommandStack ----------------------------------------------------------

CommandStack::CommandStack(std::size_t maxDepth)
    : maxDepth_(maxDepth == 0 ? 1 : maxDepth)
{
}

bool CommandStack::execute(std::unique_ptr<EditorCommand> cmd,
                           EditorDocument& doc,
                           SelectionModel& sel)
{
    if (!cmd) {
        return false;
    }
    if (!cmd->apply(doc, sel)) {
        return false;
    }
    redo_.clear();
    undo_.push_back(std::move(cmd));
    // Bounded undo: drop the oldest entry (front) when the cap is exceeded.
    if (undo_.size() > maxDepth_) {
        undo_.erase(undo_.begin());
    }
    return true;
}

bool CommandStack::canUndo() const noexcept { return !undo_.empty(); }
bool CommandStack::canRedo() const noexcept { return !redo_.empty(); }

bool CommandStack::undo(EditorDocument& doc, SelectionModel& sel)
{
    if (undo_.empty()) {
        return false;
    }
    auto cmd = std::move(undo_.back());
    undo_.pop_back();
    if (!cmd->undo(doc, sel)) {
        // Rollback failure leaves the command off both stacks rather than
        // re-pushing it in an inconsistent state. The seam stays usable; the
        // failure surfaces to the caller via the return value.
        return false;
    }
    redo_.push_back(std::move(cmd));
    return true;
}

bool CommandStack::redo(EditorDocument& doc, SelectionModel& sel)
{
    if (redo_.empty()) {
        return false;
    }
    auto cmd = std::move(redo_.back());
    redo_.pop_back();
    if (!cmd->apply(doc, sel)) {
        return false;
    }
    undo_.push_back(std::move(cmd));
    return true;
}

std::size_t CommandStack::undoDepth() const noexcept { return undo_.size(); }
std::size_t CommandStack::redoDepth() const noexcept { return redo_.size(); }
std::size_t CommandStack::maxDepth() const noexcept { return maxDepth_; }

std::vector<std::string> CommandStack::undoNames() const
{
    std::vector<std::string> out;
    out.reserve(undo_.size());
    for (auto it = undo_.rbegin(); it != undo_.rend(); ++it) {
        out.push_back((*it)->name());
    }
    return out;
}

std::vector<std::string> CommandStack::redoNames() const
{
    std::vector<std::string> out;
    out.reserve(redo_.size());
    for (auto it = redo_.rbegin(); it != redo_.rend(); ++it) {
        out.push_back((*it)->name());
    }
    return out;
}

// --- SetDocumentLabelCommand ----------------------------------------------

SetDocumentLabelCommand::SetDocumentLabelCommand(std::string newLabel)
    : newLabel_(std::move(newLabel))
{
}

std::string SetDocumentLabelCommand::name() const { return "SetDocumentLabel"; }

bool SetDocumentLabelCommand::apply(EditorDocument& doc, SelectionModel& /*sel*/)
{
    previousLabel_ = doc.label();
    previousDirty_ = doc.isDirty();
    doc.setLabel(newLabel_);
    doc.setDirty(true);
    return true;
}

bool SetDocumentLabelCommand::undo(EditorDocument& doc, SelectionModel& /*sel*/)
{
    doc.setLabel(previousLabel_);
    doc.setDirty(previousDirty_);
    return true;
}

// --- AddSelectionCommand --------------------------------------------------

AddSelectionCommand::AddSelectionCommand(std::vector<std::string> ids)
    : ids_(std::move(ids))
{
}

std::string AddSelectionCommand::name() const { return "AddSelection"; }

bool AddSelectionCommand::apply(EditorDocument& doc, SelectionModel& sel)
{
    addedThisApply_.clear();
    for (const auto& id : ids_) {
        if (!sel.contains(id)) {
            sel.add(id);
            addedThisApply_.push_back(id);
        }
    }
    if (!addedThisApply_.empty()) {
        doc.setDirty(true);
    }
    return true;
}

bool AddSelectionCommand::undo(EditorDocument& doc, SelectionModel& sel)
{
    for (const auto& id : addedThisApply_) {
        sel.remove(id);
    }
    addedThisApply_.clear();
    // Leaving doc.dirty alone on undo is intentional: even a no-op redo
    // history is itself a mutation worth surfacing.
    (void)doc;
    return true;
}

}  // namespace nexus::tooling::editor
