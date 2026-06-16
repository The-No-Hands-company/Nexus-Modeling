#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Document Model
//
//  The CadDocument is the top-level container for a CAD project.  It owns
//  the feature history, manages selection state, and provides the undo/redo
//  command stack.  Every CAD operation flows through the document.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadSelection.h>
#include <nexus/parametric/FeatureHistory.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <vector>

namespace nexus::cad {

class CadCommand;
class CadDocument;

// ──────────── Document State ───────────────────────────────────────────────

enum class CadTool : uint8_t {
    Select,
    Sketch,
    Extrude,
    Revolve,
    Fillet,
    Chamfer,
    Dimension,
    Constrain,
};

struct CadDocumentState {
    CadTool        activeTool = CadTool::Select;
    bool           modified   = false;
    std::string    filePath;
};

// ──────────── CadDocument ───────────────────────────────────────────────────

class CadDocument {
public:
    CadDocument();
    ~CadDocument();

    // ── Feature history ─────────────────────────────────────────────
    [[nodiscard]] parametric::FeatureHistory& history() noexcept { return m_history; }
    [[nodiscard]] const parametric::FeatureHistory& history() const noexcept { return m_history; }

    // ── Selection ───────────────────────────────────────────────────
    [[nodiscard]] CadSelection&       selection()       noexcept { return m_selection; }
    [[nodiscard]] const CadSelection& selection() const noexcept { return m_selection; }

    // ── High-level CAD operations (convenience wrappers) ────────────
    // These create undoable commands, execute them, and push to undo stack.
    [[nodiscard]] parametric::FeatureId addSketch(
        parametric::SketchProfile sketch);

    [[nodiscard]] parametric::FeatureId addExtrude(
        parametric::FeatureId sketchId,
        geometry::CurveExtrudeDesc desc = {});

    [[nodiscard]] parametric::FeatureId addRevolve(
        parametric::FeatureId sketchId,
        geometry::RevolveDesc desc = {});

    bool deleteFeature(parametric::FeatureId id);

    // ── Undo / Redo ─────────────────────────────────────────────────
    bool undo();
    bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return !m_undoStack.empty(); }
    [[nodiscard]] bool canRedo() const noexcept { return !m_redoStack.empty(); }

    // ── Execute arbitrary command with undo support ─────────────────
    bool executeCommand(std::unique_ptr<CadCommand> cmd);

    // ── Document state ──────────────────────────────────────────────
    [[nodiscard]] const CadDocumentState& state() const noexcept { return m_state; }
    void setActiveTool(CadTool tool);
    [[nodiscard]] CadTool activeTool() const noexcept { return m_state.activeTool; }
    void markModified() { m_state.modified = true; }
    void markSaved()    { m_state.modified = false; }
    [[nodiscard]] bool isModified() const noexcept { return m_state.modified; }

    // ── Serialization ───────────────────────────────────────────────
    [[nodiscard]] std::vector<uint8_t> serialize() const noexcept;
    [[nodiscard]] bool deserialize(const uint8_t* data, size_t size) noexcept;

private:
    parametric::FeatureHistory m_history;
    CadSelection               m_selection;
    CadDocumentState           m_state;

    std::deque<std::unique_ptr<CadCommand>> m_undoStack;
    std::deque<std::unique_ptr<CadCommand>> m_redoStack;
};

} // namespace nexus::cad
