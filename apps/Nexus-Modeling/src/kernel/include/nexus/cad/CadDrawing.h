#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Drawing Views, BOM, Configuration, Interference, Render Modes
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

// ──────────── Drawing Views ─────────────────────────────────────────────────

enum class DrawingViewType : uint8_t {
    Front, Top, Right, Isometric, Section, Detail, Auxiliary
};

struct DrawingView {
    DrawingViewType type = DrawingViewType::Front;
    Vec3  cameraPosition{0, 0, 10};
    Vec3  target{0, 0, 0};
    Vec3  up{0, 1, 0};
    float scale = 1.0f;
    std::string label;
    std::vector<uint32_t> featureIds;
};

class CadDrawing {
public:
    // Create standard views from a part.
    [[nodiscard]] static std::vector<DrawingView> generateStandardViews(
        const CadDocument& doc) noexcept;

    // Create a section view at a given plane.
    [[nodiscard]] static DrawingView generateSectionView(
        const CadDocument& doc,
        const Vec3& planePoint,
        const Vec3& planeNormal) noexcept;

    // Export all views as a combined 2D mesh (flattened projections).
    [[nodiscard]] static geometry::Mesh exportViewsAsMesh(
        const std::vector<DrawingView>& views) noexcept;
};

// ──────────── Bill of Materials ────────────────────────────────────────────

struct BomItem {
    uint32_t index = 0;
    std::string partNumber;
    std::string description;
    std::string material;
    uint32_t quantity = 1;
    double   mass = 0.0;
};

class CadBOM {
public:
    // Generate BOM from an assembly.
    [[nodiscard]] static std::vector<BomItem> generate(
        const CadAssembly& assembly) noexcept;

    // Export BOM as CSV text.
    [[nodiscard]] static std::string exportCSV(
        const std::vector<BomItem>& items) noexcept;

    // Export BOM as JSON.
    [[nodiscard]] static std::string exportJSON(
        const std::vector<BomItem>& items) noexcept;
};

// ──────────── Configuration System ─────────────────────────────────────────

struct ConfigurationParameter {
    std::string name;
    double      defaultValue = 0.0;
    double      minValue     = 0.0;
    double      maxValue     = 100.0;
};

struct Configuration {
    std::string name;
    std::vector<std::pair<std::string, double>> values;
};

class CadConfiguration {
public:
    // Define a configurable parameter.
    void addParameter(const ConfigurationParameter& param);

    // Create a configuration with specific values.
    [[nodiscard]] Configuration createConfig(
        const std::string& name,
        const std::vector<std::pair<std::string, double>>& values) const noexcept;

    // Apply a configuration to a document.
    [[nodiscard]] bool apply(const Configuration& config,
                              CadDocument& doc) const noexcept;

    [[nodiscard]] const std::vector<ConfigurationParameter>& parameters() const noexcept;

private:
    std::vector<ConfigurationParameter> m_params;
};

// ──────────── Interference Detection ───────────────────────────────────────

struct InterferenceResult {
    bool interference = false;
    std::vector<std::pair<uint32_t, uint32_t>> interferingParts; // (partA, partB)
    std::vector<std::pair<uint32_t, uint32_t>> interferingFaces; // (faceA, faceB)
};

class CadInterference {
public:
    // Check for interference between all parts in an assembly.
    [[nodiscard]] static InterferenceResult detect(
        const CadAssembly& assembly) noexcept;

    // Check interference between two specific parts.
    [[nodiscard]] static InterferenceResult detectBetween(
        const CadPart& partA, const CadPart& partB) noexcept;
};

// ──────────── Render Modes ─────────────────────────────────────────────────

enum class CadRenderMode : uint8_t {
    Shaded,
    Wireframe,
    HiddenLine,
    ShadedWithEdges,
    Transparent,
    XRay,
};

struct CadRenderStyle {
    CadRenderMode mode = CadRenderMode::Shaded;
    Vec3  edgeColor{0, 0, 0};
    Vec3  faceColor{0.7f, 0.7f, 0.7f};
    float edgeWidth = 1.0f;
    float transparency = 0.0f;
};

class CadRenderModes {
public:
    // Apply a render style to a part.
    static void applyStyle(const CadRenderStyle& style,
                            CadDocument& doc) noexcept;

    // Extract edge geometry for wireframe rendering.
    [[nodiscard]] static geometry::Mesh extractEdges(
        const CadDocument& doc) noexcept;

    // Generate hidden-line view of a part.
    [[nodiscard]] static geometry::Mesh generateHiddenLine(
        const CadDocument& doc,
        const Vec3& viewDirection) noexcept;

    [[nodiscard]] static CadRenderStyle defaultStyle() noexcept;
};

} // namespace nexus::cad
