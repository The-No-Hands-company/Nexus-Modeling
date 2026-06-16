#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Reference Geometry, Bodies, Sections, Suppression, Design Table,
//              View Management, Materials
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

// ──────────── Reference Geometry ────────────────────────────────────────────

enum class RefGeometryType : uint8_t { Plane, Axis, Point, CoordinateSystem };

struct RefGeometry {
    RefGeometryType type;
    std::string name;
    Vec3 origin{};
    Vec3 direction{0, 0, 1};
    std::vector<Vec3> points;
    parametric::FeatureId parentFeature = parametric::kInvalidFeatureId;
};

class CadRefGeometry {
public:
    // Create reference geometry relative to a feature.
    [[nodiscard]] static RefGeometry createPlane(
        const Vec3& point, const Vec3& normal,
        const std::string& name = "Plane") noexcept;

    [[nodiscard]] static RefGeometry createAxis(
        const Vec3& point, const Vec3& direction,
        const std::string& name = "Axis") noexcept;

    [[nodiscard]] static RefGeometry createPoint(
        const Vec3& point,
        const std::string& name = "Point") noexcept;

    // Create standard reference planes relative to a coordinate system.
    [[nodiscard]] static std::vector<RefGeometry> standardTriad() noexcept;
};

// ──────────── Body Management ──────────────────────────────────────────────

struct CadBody {
    std::string name = "Body";
    geometry::Mesh mesh;
    bool visible = true;
    bool solid = true;
};

class CadBodyManager {
public:
    // Split selected faces into a new body.
    [[nodiscard]] static std::vector<CadBody> splitFaces(
        const CadBody& source,
        const std::vector<uint32_t>& faceIndices) noexcept;

    // Combine multiple bodies into one.
    [[nodiscard]] static CadBody combine(
        const std::vector<CadBody>& bodies) noexcept;

    // Check if a body is watertight (closed solid).
    [[nodiscard]] static bool isWatertight(const CadBody& body) noexcept;
};

// ──────────── Section Views ────────────────────────────────────────────────

struct SectionPlane {
    Vec3 point{};
    Vec3 normal{0, 0, 1};
    bool enabled = false;
    bool showCap = true;
    Vec3 capColor{0.3f, 0.3f, 0.8f};
};

class CadSectionView {
public:
    // Create a section plane.
    [[nodiscard]] static SectionPlane create(const Vec3& point,
                                               const Vec3& normal) noexcept;

    // Apply section plane to a part, returning the sectioned mesh.
    [[nodiscard]] static geometry::Mesh apply(
        const CadDocument& doc,
        parametric::FeatureId featureId,
        const SectionPlane& plane) noexcept;

    // Generate cap faces for a section cut.
    [[nodiscard]] static geometry::Mesh generateCap(
        const geometry::Mesh& sectionedMesh,
        const SectionPlane& plane) noexcept;

    // Standard section planes.
    [[nodiscard]] static SectionPlane front() noexcept;
    [[nodiscard]] static SectionPlane top() noexcept;
    [[nodiscard]] static SectionPlane right() noexcept;
};

// ──────────── Feature Suppression ──────────────────────────────────────────

struct SuppressionState {
    parametric::FeatureId featureId;
    bool suppressed = false;
    std::vector<parametric::FeatureId> affectedChildren;
};

class CadSuppression {
public:
    // Suppress a feature and all its dependent children.
    [[nodiscard]] static std::vector<SuppressionState> suppress(
        CadDocument& doc,
        parametric::FeatureId featureId) noexcept;

    // Unsuppress a feature.
    [[nodiscard]] static bool unsuppress(
        CadDocument& doc,
        parametric::FeatureId featureId) noexcept;

    // Check if a feature is suppressed.
    [[nodiscard]] static bool isSuppressed(
        const CadDocument& doc,
        parametric::FeatureId featureId) noexcept;

    // Get all suppressed features.
    [[nodiscard]] static std::vector<parametric::FeatureId> suppressedList(
        const CadDocument& doc) noexcept;
};

// ──────────── Design Table ─────────────────────────────────────────────────

struct DesignTableRow {
    std::string name;
    std::vector<double> values;
};

class CadDesignTable {
public:
    // Add a parameter column.
    void addParameter(const std::string& name, double defaultValue);

    // Add a configuration row.
    void addRow(const std::string& name, const std::vector<double>& values);

    // Apply a row to a document.
    [[nodiscard]] bool apply(const std::string& rowName,
                              CadDocument& doc) const noexcept;

    // Export table as CSV.
    [[nodiscard]] std::string exportCSV() const noexcept;

    [[nodiscard]] const std::vector<std::string>& parameters() const noexcept;
    [[nodiscard]] const std::vector<DesignTableRow>& rows() const noexcept;

private:
    std::vector<std::string> m_params;
    std::vector<double> m_defaults;
    std::vector<DesignTableRow> m_rows;
};

// ──────────── View / Camera Management ─────────────────────────────────────

struct CadView {
    std::string name;
    Vec3 cameraPosition{0, 0, 10};
    Vec3 target{0, 0, 0};
    Vec3 up{0, 1, 0};
    float fov = 45.f;
};

class CadViewManager {
public:
    // Save current view.
    void saveView(const std::string& name,
                  const Vec3& position, const Vec3& target, const Vec3& up);

    // Recall a saved view.
    [[nodiscard]] CadView recall(const std::string& name) const;

    // Standard views.
    [[nodiscard]] static CadView frontView() noexcept;
    [[nodiscard]] static CadView topView() noexcept;
    [[nodiscard]] static CadView rightView() noexcept;
    [[nodiscard]] static CadView isometricView() noexcept;

    [[nodiscard]] const std::vector<CadView>& savedViews() const noexcept;

private:
    std::vector<CadView> m_views;
};

// ──────────── Materials / Appearances Library ──────────────────────────────

struct CadAppearance {
    std::string name;
    Vec3  diffuseColor{0.7f, 0.7f, 0.7f};
    Vec3  specularColor{0.3f, 0.3f, 0.3f};
    float roughness = 0.5f;
    float metallic  = 0.0f;
    float transparency = 0.0f;
};

class CadMaterialLibrary {
public:
    // Add a custom material.
    void add(const CadAppearance& mat);

    // Apply a material to a feature by name.
    void applyToFeature(const std::string& materialName,
                         CadDocument& doc,
                         parametric::FeatureId featureId) noexcept;

    // Standard library.
    [[nodiscard]] static std::vector<CadAppearance> standardLibrary() noexcept;

    [[nodiscard]] const std::vector<CadAppearance>& materials() const noexcept;

private:
    std::vector<CadAppearance> m_materials;
};

} // namespace nexus::cad
