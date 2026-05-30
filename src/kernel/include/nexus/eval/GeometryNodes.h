#pragma once

#include <nexus/eval/EvalGraph.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/render/SceneGraph.h>

#include <cstdint>

namespace nexus::eval {

enum class GeometryNodeKind : uint32_t {
    Transform = 0x00010000u,
    Merge     = 0x00010001u,
    Boolean   = 0x00010002u,
    Remesh    = 0x00010003u,
    Bevel     = 0x00010004u,
    Inset     = 0x00010005u,
};

class GeometryTransformNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Transform; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryTransform"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& inputGeometry,
        const nexus::render::Transform& transform,
        nexus::geometry::Mesh& outGeometry);
};

class GeometryMergeNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Merge; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryMerge"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& geometryA,
        const nexus::geometry::Mesh& geometryB,
        nexus::geometry::Mesh& outGeometry);
};

// Wraps BooleanOperation::compute() as an eval-graph node.
// Returns false when the operation reports an invalid (non-valid) result.
class GeometryBooleanNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Boolean; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryBoolean"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& meshA,
        const nexus::geometry::Mesh& meshB,
        nexus::geometry::BooleanOperationType operation,
        nexus::geometry::Mesh& outGeometry);
};

// Wraps RemeshOperation::apply() as an eval-graph node.
// Returns false when the report is not valid.
class GeometryRemeshNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Remesh; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryRemesh"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& input,
        const nexus::geometry::RemeshDesc& desc,
        nexus::geometry::Mesh& outGeometry);
};

// Wraps BevelChamferOperation::apply() as an eval-graph node.
// Returns false when the report is not valid.
class GeometryBevelNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Bevel; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryBevel"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& input,
        const nexus::geometry::BevelChamferDesc& desc,
        nexus::geometry::Mesh& outGeometry);
};

// Wraps InsetFacesOperation::applyToAllFaces() as an eval-graph node.
// Returns false when the report is not valid.
class GeometryInsetNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept { return GeometryNodeKind::Inset; }
    [[nodiscard]] static constexpr const char* name() noexcept { return "GeometryInset"; }

    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& input,
        const nexus::geometry::InsetDesc& desc,
        nexus::geometry::Mesh& outGeometry);
};

} // namespace nexus::eval
