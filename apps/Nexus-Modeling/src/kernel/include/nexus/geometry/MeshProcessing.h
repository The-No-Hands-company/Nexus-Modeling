#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Processing Utilities
//
//  Segmentation, denoising, hole features, patterning, developability,
//  and sewing/stitching.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ──────────── Mesh Segmentation ─────────────────────────────────────────────

struct Segment {
    std::vector<uint32_t> faceIndices;
    Vec3  representativeColor{0, 0, 0};
    float avgCurvature = 0.f;
};

// Segment mesh faces by a combination of dihedral angle and planarity.
// 'angleThresholdDeg': faces with dihedral > threshold become segment boundaries.
[[nodiscard]] std::vector<Segment> segmentMesh(const Mesh& mesh,
                                                float angleThresholdDeg = 30.f);

// ──────────── Mesh Denoising ────────────────────────────────────────────────

// Apply bilateral normal filtering to reduce noise while preserving features.
// sigmaC: spatial kernel width, sigmaS: normal difference kernel width.
[[nodiscard]] Mesh denoiseMeshBilateral(const Mesh& mesh,
                                          float sigmaC = 0.5f,
                                          float sigmaS = 0.2f,
                                          uint32_t iterations = 3);

// ──────────── Hole Feature ─────────────────────────────────────────────────

// Create a cylindrical hole (bore) through a mesh at a given position.
// Uses boolean subtraction internally.
[[nodiscard]] Mesh createHole(const Mesh& solid,
                               const Vec3& position,
                               const Vec3& direction,
                               float radius,
                               float depth);

// ──────────── Pattern / Array ──────────────────────────────────────────────

// Create a rectangular grid array of instances.
[[nodiscard]] Mesh createRectangularArray(const Mesh& instance,
                                           uint32_t countX, uint32_t countY,
                                           float spacingX, float spacingY);

// Create a circular polar array of instances around an axis.
[[nodiscard]] Mesh createCircularArray(const Mesh& instance,
                                        const Vec3& center,
                                        const Vec3& axis,
                                        uint32_t count,
                                        float radius);

// ──────────── Developable Surface ───────────────────────────────────────────

// Check if a mesh surface is developable (Gaussian curvature ≈ 0 everywhere).
// Returns true if all vertices have |K| < epsilon.
[[nodiscard]] bool isMeshDevelopable(const Mesh& mesh, float epsilon = 1e-5f);

// ──────────── Sewing / Stitching ───────────────────────────────────────────

// Weld vertices across disconnected mesh components that are within tolerance.
// Returns the welded mesh.  Useful for closing gaps in stitched assemblies.
[[nodiscard]] Mesh sewMesh(const Mesh& mesh, float tolerance = 1e-4f);

} // namespace nexus::geometry
