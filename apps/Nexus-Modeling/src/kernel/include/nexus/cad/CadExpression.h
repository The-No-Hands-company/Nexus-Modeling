#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Parametric Expressions, Rollback, Diff, Explode, Mass, Metadata
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

// ──────────── Parametric Expressions ───────────────────────────────────────

class CadExpressionEngine {
public:
    // Define a named variable with an initial value.
    void define(const std::string& name, double value);

    // Set a variable to an expression (e.g., "Width * 2", "Height + 10").
    // Supported operators: + - * / ( ) and references to other variables.
    bool setExpression(const std::string& name, const std::string& expr);

    // Evaluate a variable (recomputes all dependencies).
    [[nodiscard]] double evaluate(const std::string& name) const;

    // Get all variable names.
    [[nodiscard]] std::vector<std::string> variables() const;

    [[nodiscard]] double get(const std::string& name) const;
    void set(const std::string& name, double value);
    void clear();

private:
    std::unordered_map<std::string, double> m_variables;
    std::unordered_map<std::string, std::string> m_expressions;
};

// ──────────── Feature Rollback ─────────────────────────────────────────────

class CadRollback {
public:
    explicit CadRollback(CadDocument& doc);

    // Move to a specific point in the undo history.
    // featureIndex 0 = before any features, N = after N features.
    bool rollTo(size_t featureIndex);

    // Get current rollback position.
    [[nodiscard]] size_t currentPosition() const noexcept { return m_position; }

    // Get the total number of positions (features + 1).
    [[nodiscard]] size_t totalPositions() const noexcept;

private:
    CadDocument& m_doc;
    size_t m_position = 0;
};

// ──────────── Model Comparison ─────────────────────────────────────────────

struct CadDiffResult {
    bool identical = true;
    std::vector<std::string> differences;
    uint32_t addedFaces    = 0;
    uint32_t removedFaces  = 0;
    uint32_t modifiedFaces = 0;
    double   maxDeviation  = 0.0;
    double   avgDeviation  = 0.0;
};

class CadDiff {
public:
    // Compare two parts and report differences.
    [[nodiscard]] static CadDiffResult compare(
        const CadPart& partA, const CadPart& partB) noexcept;

    // Compare two versions of the same document at different rollback points.
    [[nodiscard]] static CadDiffResult compareVersions(
        const CadDocument& docA, const CadDocument& docB) noexcept;
};

// ──────────── Exploded Views ───────────────────────────────────────────────

struct ExplosionAxis {
    Vec3 direction{0, 0, 1};
    float spacing = 5.0f;
};

struct ExplodedPart {
    uint32_t partIndex;
    Vec3  originalPosition{};
    Vec3  explodedPosition{};
};

class CadExplode {
public:
    // Generate exploded positions for an assembly along an axis.
    [[nodiscard]] static std::vector<ExplodedPart> explode(
        const CadAssembly& assembly,
        const ExplosionAxis& axis = {}) noexcept;

    // Animate from assembled to exploded (returns step positions).
    [[nodiscard]] static std::vector<std::vector<Vec3>> animateExplosion(
        const CadAssembly& assembly,
        uint32_t steps = 10) noexcept;
};

// ──────────── Mass Properties + Materials ──────────────────────────────────

struct CadMaterial {
    std::string name = "Steel";
    double density   = 7.85;  // g/cm³
    Vec3  color{0.7f, 0.7f, 0.7f};
    std::string grade;
};

struct CadMassProperties {
    double volume     = 0.0;
    double mass       = 0.0;
    Vec3  centroid{};
    double surfaceArea = 0.0;
    CadMaterial material;
};

class CadMassProps {
public:
    // Compute mass properties for a part with a given material.
    [[nodiscard]] static CadMassProperties compute(
        const CadPart& part,
        const CadMaterial& material) noexcept;

    // Compute aggregate mass properties for an assembly.
    [[nodiscard]] static CadMassProperties computeAssembly(
        const CadAssembly& assembly,
        const std::vector<CadMaterial>& materials) noexcept;

    // List standard materials.
    [[nodiscard]] static std::vector<CadMaterial> standardMaterials() noexcept;
};

// ──────────── Custom Properties / Design Intent ────────────────────────────

class CadMetadata {
public:
    void setProperty(const std::string& key, const std::string& value);
    [[nodiscard]] std::string property(const std::string& key) const;
    [[nodiscard]] bool hasProperty(const std::string& key) const noexcept;
    [[nodiscard]] std::vector<std::string> keys() const;

    void setDesignIntent(const std::string& intent);
    [[nodiscard]] const std::string& designIntent() const noexcept;

    void clear();

private:
    std::unordered_map<std::string, std::string> m_props;
    std::string m_designIntent;
};

} // namespace nexus::cad
