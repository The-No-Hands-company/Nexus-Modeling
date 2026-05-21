#pragma once

#include <nexus/eval/EvalGraph.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/SceneGraph.h>

#include <cstdint>

namespace nexus::eval {

enum class GeometryNodeKind : uint32_t {
    Transform = 0x00010000u,
    Merge = 0x00010001u,
};

class GeometryTransformNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept {
        return GeometryNodeKind::Transform;
    }

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "GeometryTransform";
    }

    // Computes a transformed copy of the input mesh.
    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& inputGeometry,
        const nexus::render::Transform& transform,
        nexus::geometry::Mesh& outGeometry);
};

class GeometryMergeNode {
public:
    [[nodiscard]] static constexpr GeometryNodeKind kind() noexcept {
        return GeometryNodeKind::Merge;
    }

    [[nodiscard]] static constexpr const char* name() noexcept {
        return "GeometryMerge";
    }

    // Computes a deterministic merge where geometryA content precedes geometryB.
    [[nodiscard]] static bool compute(
        const nexus::geometry::Mesh& geometryA,
        const nexus::geometry::Mesh& geometryB,
        nexus::geometry::Mesh& outGeometry);
};

} // namespace nexus::eval
