#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Part & Assembly Hierarchy
//
//  CadPart: a single physical component (owns a CadDocument or subset).
//  CadAssembly: a collection of CadParts with assembly constraints.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/cad/CadDocument.h>
#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nexus::cad {

class CadPart {
public:
    CadPart() = default;
    explicit CadPart(std::string name);

    [[nodiscard]] const std::string& name() const noexcept { return m_name; }
    void setName(const std::string& n) { m_name = n; }

    [[nodiscard]] CadDocument& document() noexcept { return m_document; }
    [[nodiscard]] const CadDocument& document() const noexcept { return m_document; }

    // Combined mesh of all features in this part.
    [[nodiscard]] geometry::Mesh combinedMesh() const noexcept;

    [[nodiscard]] bool isValid() const noexcept { return m_valid; }

private:
    std::string  m_name = "Unnamed";
    CadDocument  m_document;
    bool         m_valid = true;
};

// ──────────── Assembly ──────────────────────────────────────────────────────

struct AssemblyConstraint {
    uint32_t partA  = 0;
    uint32_t partB  = 0;
    enum Type { Mate, Align, Gear, Distance, Angle } type = Mate;
    double   value  = 0.0;  // distance or angle value
};

class CadAssembly {
public:
    CadAssembly() = default;

    // Add a part to the assembly.
    [[nodiscard]] uint32_t addPart(std::shared_ptr<CadPart> part);

    // Add an assembly constraint between two parts.
    void addConstraint(const AssemblyConstraint& c);

    // Remove a part.
    bool removePart(uint32_t index);

    // Access.
    [[nodiscard]] size_t partCount() const noexcept { return m_parts.size(); }
    [[nodiscard]] CadPart* part(uint32_t index) noexcept;
    [[nodiscard]] const CadPart* part(uint32_t index) const noexcept;
    [[nodiscard]] const std::vector<AssemblyConstraint>& constraints() const noexcept;

private:
    std::vector<std::shared_ptr<CadPart>> m_parts;
    std::vector<AssemblyConstraint>       m_constraints;
};

// ──────────── Measure Tools ─────────────────────────────────────────────────

struct MeasureResult {
    bool   valid = false;
    double distance = 0.0;
    double angle    = 0.0;
    double area     = 0.0;
    double volume   = 0.0;
};

class CadMeasure {
public:
    // Distance between two points.
    [[nodiscard]] static MeasureResult distanceBetween(
        const CadPart& part,
        uint32_t faceA, uint32_t faceB) noexcept;

    // Angle between two faces.
    [[nodiscard]] static MeasureResult angleBetween(
        const CadPart& part,
        uint32_t faceA, uint32_t faceB) noexcept;

    // Area of a face.
    [[nodiscard]] static MeasureResult faceArea(
        const CadPart& part, uint32_t faceIndex) noexcept;

    // Volume of a closed solid part.
    [[nodiscard]] static MeasureResult solidVolume(
        const CadPart& part) noexcept;
};

} // namespace nexus::cad
