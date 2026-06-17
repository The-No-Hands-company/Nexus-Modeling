#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Command Framework
//
//  Abstract base for undoable CAD commands.  Each command encapsulates a
//  single CAD operation and can be executed and undone.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/parametric/FeatureHistory.h>
#include <nexus/parametric/ParametricSketchProfile.h>

#include <memory>
#include <string>

namespace nexus::cad {

class CadDocument;

// ──────────── CadCommand (abstract base) ────────────────────────────────────

class CadCommand {
public:
    virtual ~CadCommand() = default;

    [[nodiscard]] virtual std::string description() const = 0;
    virtual bool execute(CadDocument& doc) = 0;
    virtual bool undo(CadDocument& doc) = 0;

    [[nodiscard]] bool wasExecuted() const noexcept { return m_executed; }

protected:
    bool m_executed = false;
};

// ──────────── AddSketchCommand ──────────────────────────────────────────────

class AddSketchCommand : public CadCommand {
public:
    explicit AddSketchCommand(parametric::SketchProfile sketch);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::SketchProfile m_sketch;
    parametric::FeatureId     m_createdId = parametric::kInvalidFeatureId;
};

// ──────────── AddExtrudeCommand ─────────────────────────────────────────────

class AddExtrudeCommand : public CadCommand {
public:
    AddExtrudeCommand(parametric::FeatureId sketchId,
                      geometry::CurveExtrudeDesc desc);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId      m_sketchId;
    geometry::CurveExtrudeDesc m_desc;
    parametric::FeatureId      m_createdId = parametric::kInvalidFeatureId;
};

// ──────────── AddRevolveCommand ─────────────────────────────────────────────

class AddRevolveCommand : public CadCommand {
public:
    AddRevolveCommand(parametric::FeatureId sketchId,
                      geometry::RevolveDesc desc);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId   m_sketchId;
    geometry::RevolveDesc   m_desc;
    parametric::FeatureId   m_createdId = parametric::kInvalidFeatureId;
};

// ──────────── SetParameterCommand ───────────────────────────────────────────

class SetHeightCommand : public CadCommand {
public:
    SetHeightCommand(parametric::FeatureId id, float oldHeight, float newHeight);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    float m_oldHeight;
    float m_newHeight;
};

// ──────────── TransformCommand (undoable gizmo/move) ──────────────────────

class TransformCommand : public CadCommand {
public:
    TransformCommand(parametric::FeatureId id, geometry::Mesh savedState)
        : m_featureId(id), m_savedMesh(std::move(savedState)) {}
    [[nodiscard]] std::string description() const override { return "Transform"; }
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    geometry::Mesh m_savedMesh;
    geometry::Mesh m_newMesh;
};

// ──────────── DeleteFeatureCommand ───────────────────────────────────────

class DeleteFeatureCommand : public CadCommand {
public:
    explicit DeleteFeatureCommand(parametric::FeatureId id) : m_featureId(id) {}
    [[nodiscard]] std::string description() const override { return "Delete feature"; }
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    std::optional<nexus::geometry::Mesh> m_savedMesh;
};

} // namespace nexus::cad
