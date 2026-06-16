#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Feature Tree, Workplane, Grid/Snap, Feature Editor
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

// ──────────── Feature Tree ──────────────────────────────────────────────────

struct FeatureTreeNode {
    parametric::FeatureId id = parametric::kInvalidFeatureId;
    std::string           name;
    parametric::FeatureKind kind;
    std::vector<FeatureTreeNode> children;
    bool expanded = true;
    bool visible  = true;
};

class CadFeatureTree {
public:
    // Build a tree from the document's feature history.
    [[nodiscard]] static std::vector<FeatureTreeNode> build(
        const CadDocument& doc) noexcept;

    // Find a node by feature ID.
    [[nodiscard]] static const FeatureTreeNode* find(
        const std::vector<FeatureTreeNode>& tree,
        parametric::FeatureId id) noexcept;
};

// ──────────── Workplane ─────────────────────────────────────────────────────

struct Workplane {
    Vec3  origin   = {0, 0, 0};
    Vec3  normal   = {0, 0, 1};
    Vec3  uAxis    = {1, 0, 0};
    Vec3  vAxis    = {0, 1, 0};
    std::string name = "XY Plane";

    // Create a workplane from origin, normal, and a reference direction.
    [[nodiscard]] static Workplane fromOriginNormal(
        const Vec3& origin,
        const Vec3& normal,
        const Vec3& refDir = {1, 0, 0}) noexcept;

    // Standard planes.
    [[nodiscard]] static Workplane XY() noexcept;
    [[nodiscard]] static Workplane XZ() noexcept;
    [[nodiscard]] static Workplane YZ() noexcept;
};

class CadWorkplaneManager {
public:
    void setActive(const Workplane& wp);
    [[nodiscard]] const Workplane& active() const noexcept { return m_active; }

    void addCustom(const Workplane& wp);
    [[nodiscard]] const std::vector<Workplane>& custom() const noexcept { return m_custom; }

private:
    Workplane m_active = Workplane::XY();
    std::vector<Workplane> m_custom;
};

// ──────────── Grid/Snap ─────────────────────────────────────────────────────

struct GridSnapConfig {
    float  gridSpacing    = 1.0f;
    float  snapRadius     = 0.5f;
    bool   snapToGrid     = true;
    bool   snapToVertex   = true;
    bool   snapToEdge     = true;
    bool   snapToMidpoint = true;
};

class CadGridSnap {
public:
    // Snap a 3D point to the nearest grid/snap target on the active workplane.
    [[nodiscard]] Vec3 snap(const Vec3& point,
                             const Workplane& workplane,
                             const CadDocument& doc) const noexcept;

    void setConfig(const GridSnapConfig& cfg) { m_config = cfg; }
    [[nodiscard]] const GridSnapConfig& config() const noexcept { return m_config; }

private:
    GridSnapConfig m_config;
};

// ──────────── Feature Editor ────────────────────────────────────────────────

class CadFeatureEditor {
public:
    // Change the extrusion height of a feature.
    [[nodiscard]] static bool setHeight(
        CadDocument& doc,
        parametric::FeatureId id,
        float newHeight) noexcept;

    // Change the sketch of a feature.
    [[nodiscard]] static bool setSketchPoints(
        CadDocument& doc,
        parametric::FeatureId id,
        const std::vector<std::pair<double, double>>& xyPoints) noexcept;

    // Toggle end caps.
    [[nodiscard]] static bool setCapEnds(
        CadDocument& doc,
        parametric::FeatureId id,
        bool enabled) noexcept;

    // Suppress (hide) or unsuppress a feature.
    [[nodiscard]] static bool setSuppressed(
        CadDocument& doc,
        parametric::FeatureId id,
        bool suppressed) noexcept;
};

} // namespace nexus::cad
