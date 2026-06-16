#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Mesh Analysis Toolkit
//
//  Collision detection, clearance, draft angle, thickness analysis.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshBVH.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ──────────── Collision / Interference ──────────────────────────────────────

struct CollisionResult {
    bool colliding = false;
    std::vector<std::pair<uint32_t, uint32_t>> trianglePairs; // (triA, triB)
};

// Triangle-triangle intersection test between two meshes.
[[nodiscard]] CollisionResult meshMeshCollision(const Mesh& a, const Mesh& b);

// ──────────── Minimum Clearance ─────────────────────────────────────────────

struct ClearanceResult {
    float distance = 0.f;
    Vec3  pointA{};
    Vec3  pointB{};
};

// Minimum distance between two meshes (positive = separated, 0 = touching).
[[nodiscard]] ClearanceResult meshMeshClearance(const Mesh& a, const Mesh& b);

// ──────────── Draft Angle Analysis ──────────────────────────────────────────

struct DraftAngleResult {
    float minAngle = 0.f;
    float maxAngle = 0.f;
    float avgAngle = 0.f;
    std::vector<float> perFaceAngles;
};

// Evaluate draft angles of all faces relative to a pull direction.
// Returns angles in degrees.  0° = parallel to pull (vertical walls),
// 90° = perpendicular (undercut).
[[nodiscard]] DraftAngleResult analyzeDraftAngle(const Mesh& mesh,
                                                  const Vec3& pullDirection);

// ──────────── Thickness Analysis ───────────────────────────────────────────

struct ThicknessResult {
    float minThickness = 0.f;
    float maxThickness = 0.f;
    float avgThickness = 0.f;
    std::vector<float> perVertexThickness;
};

// Estimate wall thickness by ray-casting inward from each vertex.
// Only meaningful for closed meshes with outward-pointing normals.
[[nodiscard]] ThicknessResult analyzeThickness(const Mesh& mesh,
                                                uint32_t rayCount = 4);

} // namespace nexus::geometry
