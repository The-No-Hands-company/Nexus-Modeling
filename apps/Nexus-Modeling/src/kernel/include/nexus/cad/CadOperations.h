#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — CAD Operations (fillet, chamfer, shell, draft, mirror)
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>

namespace nexus::cad {

// ──────────── FilletCommand ─────────────────────────────────────────────────

class FilletCommand : public CadCommand {
public:
    FilletCommand(parametric::FeatureId featureId,
                  const std::vector<uint32_t>& edgeIndices,
                  float radius);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    std::vector<uint32_t> m_edgeIndices;
    float m_radius;
};

// ──────────── ChamferCommand ────────────────────────────────────────────────

class ChamferCommand : public CadCommand {
public:
    ChamferCommand(parametric::FeatureId featureId,
                   const std::vector<uint32_t>& edgeIndices,
                   float distance);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    std::vector<uint32_t> m_edgeIndices;
    float m_distance;
};

// ──────────── MirrorCommand ─────────────────────────────────────────────────

class MirrorCommand : public CadCommand {
public:
    MirrorCommand(parametric::FeatureId featureId,
                  const Vec3& planePoint,
                  const Vec3& planeNormal);

    [[nodiscard]] std::string description() const override;
    bool execute(CadDocument& doc) override;
    bool undo(CadDocument& doc) override;

private:
    parametric::FeatureId m_featureId;
    Vec3 m_planePoint;
    Vec3 m_planeNormal;
    parametric::FeatureId m_createdId = parametric::kInvalidFeatureId;
};

} // namespace nexus::cad
