// PR-16 (Week 4 Day 1): Editor-state seam scaffolding for the tooling app.
//
// These are deliberately minimal interfaces — not a full document/selection/
// command system. Their purpose is to fix the shape of the seams the editor
// will grow into during Month 2 so that:
//
//   - the seam types live in one place rather than being re-invented in each
//     subcommand;
//   - the app can prove the seams compose end-to-end via --editor-seams-demo
//     without committing to a UX surface yet;
//   - kernel internals stay untouched — these types deliberately hold no
//     pointer into nexus::geometry or nexus::asset.
//
// Scope: anything richer (transactions, multi-document state, async commands,
// kernel-bound selection) is OUT of scope for Month 1 and must be designed
// against these seams in a future PR.

#pragma once

#include <cstddef>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace nexus::tooling::editor {

// Minimal document seam. Holds the app-side label + dirty bit + a string tag
// identifying which kernel-facing session (if any) is currently loaded.
//
// Intentional non-goals: no kernel handle, no scene graph, no per-object
// metadata. Those grow on top of this seam, not inside it.
class EditorDocument {
public:
    EditorDocument() = default;

    void setLabel(std::string label);
    const std::string& label() const noexcept;

    void setDirty(bool dirty) noexcept;
    bool isDirty() const noexcept;

    void setSessionKind(std::string kind);
    const std::string& sessionKind() const noexcept;

private:
    std::string label_{"untitled"};
    std::string sessionKind_{"none"};
    bool dirty_{false};
};

// Minimal selection seam. Stores a deterministic set of opaque string ids so
// the seam itself never has to know about meshes, faces, or scene entries.
class SelectionModel {
public:
    void clear() noexcept;
    void add(const std::string& id);
    void remove(const std::string& id);
    bool contains(const std::string& id) const;
    std::size_t size() const noexcept;
    std::vector<std::string> ids() const;   // sorted for determinism

private:
    std::set<std::string> ids_;
};

// Minimal command seam. Concrete commands must be cheap to construct and
// must capture whatever previous state they need at apply() time so undo()
// is symmetrical.
class EditorCommand {
public:
    virtual ~EditorCommand() = default;
    virtual std::string name() const = 0;
    virtual bool apply(EditorDocument& doc, SelectionModel& sel) = 0;
    virtual bool undo(EditorDocument& doc, SelectionModel& sel) = 0;
};

// Bounded undo/redo seam. Pushing a new command clears the redo stack, which
// matches every editor convention we care about. Bound is intentional — the
// app must never grow unbounded memory just because the user undoes a lot.
class CommandStack {
public:
    explicit CommandStack(std::size_t maxDepth = 64);

    // Calls cmd->apply(). On success, transfers ownership onto the undo
    // stack and clears redo. On failure, drops cmd and leaves stacks alone.
    bool execute(std::unique_ptr<EditorCommand> cmd,
                 EditorDocument& doc,
                 SelectionModel& sel);

    bool canUndo() const noexcept;
    bool canRedo() const noexcept;

    bool undo(EditorDocument& doc, SelectionModel& sel);
    bool redo(EditorDocument& doc, SelectionModel& sel);

    std::size_t undoDepth() const noexcept;
    std::size_t redoDepth() const noexcept;
    std::size_t maxDepth() const noexcept;

    // Top-of-stack command names, most-recent first; deterministic for tests.
    std::vector<std::string> undoNames() const;
    std::vector<std::string> redoNames() const;

private:
    std::size_t maxDepth_;
    std::vector<std::unique_ptr<EditorCommand>> undo_;
    std::vector<std::unique_ptr<EditorCommand>> redo_;
};

// One concrete command per seam axis, kept here so the demo + tests have a
// shared trivial implementation to exercise. Real commands belong outside.

class SetDocumentLabelCommand final : public EditorCommand {
public:
    explicit SetDocumentLabelCommand(std::string newLabel);
    std::string name() const override;
    bool apply(EditorDocument& doc, SelectionModel& sel) override;
    bool undo(EditorDocument& doc, SelectionModel& sel) override;

private:
    std::string newLabel_;
    std::string previousLabel_;
    bool previousDirty_{false};
};

class AddSelectionCommand final : public EditorCommand {
public:
    explicit AddSelectionCommand(std::vector<std::string> ids);
    std::string name() const override;
    bool apply(EditorDocument& doc, SelectionModel& sel) override;
    bool undo(EditorDocument& doc, SelectionModel& sel) override;

private:
    std::vector<std::string> ids_;
    std::vector<std::string> addedThisApply_;  // only the ids we actually inserted
};

}  // namespace nexus::tooling::editor
