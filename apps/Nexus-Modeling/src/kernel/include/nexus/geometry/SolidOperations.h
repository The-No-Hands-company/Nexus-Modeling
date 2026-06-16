#pragma once
// ────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Advanced Solid Operations
//
//  Body operations: surface-surface intersection,
//  local face operations, defeaturing, body splitting, mid-surface.
// ────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

// ── Surface-Surface Intersection ──────────────────────────────────────────

struct SSIResult {
    std::vector<NurbsCurve> curves;
    bool converged = false;
    uint32_t iterations = 0;
};
[[nodiscard]] SSIResult surfaceIntersection(
    const NurbsSurface& surfA, const NurbsSurface& surfB,
    uint32_t maxIterations = 64, float tolerance = 1e-4f) noexcept;

// ── Local Face Operations ─────────────────────────────────────────────────

enum class TweakDirection : uint8_t { AlongNormal, AlongAxis };

struct TweakFaceOptions {
    float distance = 1.f;
    TweakDirection direction = TweakDirection::AlongNormal;
    Vec3 axis{0, 0, 1};
    bool extendAdjacent = true; // auto-extend neighbor faces to match
};

// Offset a single face and heal adjacent faces to close the solid.
[[nodiscard]] std::optional<Mesh> tweakFace(
    const HalfEdgeMesh& mesh, uint32_t faceIndex,
    const TweakFaceOptions& opts = {}) noexcept;

// Replace a face's geometry with a new surface, trimming neighbor faces.
[[nodiscard]] std::optional<Mesh> replaceFace(
    const HalfEdgeMesh& mesh, uint32_t faceIndex,
    const NurbsSurface& newSurface) noexcept;

// Delete a face and attempt to close the hole by extending adjacent faces.
[[nodiscard]] std::optional<Mesh> deleteFace(
    const HalfEdgeMesh& mesh, uint32_t faceIndex) noexcept;

// ── Defeaturing ───────────────────────────────────────────────────────────

struct DefeatureOptions {
    float holeMaxRadius = 5.f;       // holes smaller than this get filled
    float filletMaxRadius = 3.f;     // fillets smaller than this get removed
    float bossMaxHeight = 2.f;       // low bosses get flattened
    bool preserveSharpEdges = true;
};

struct DefeatureReport {
    Mesh result;
    uint32_t holesRemoved = 0;
    uint32_t filletsRemoved = 0;
    uint32_t bossesRemoved = 0;
};

// Automatically remove small manufacturing features from imported geometry.
[[nodiscard]] DefeatureReport defeature(
    const Mesh& mesh, const DefeatureOptions& opts = {}) noexcept;

// ── Body Splitting ─────────────────────────────────────────────────────────

// Split a closed mesh into two bodies along a plane.
// Returns {bodyA, bodyB} where bodyA is on the positive side of the plane.
[[nodiscard]] std::pair<Mesh, Mesh> splitBody(
    const Mesh& solid, const Vec3& planePoint, const Vec3& planeNormal) noexcept;

// Split a body using an arbitrary surface.
[[nodiscard]] std::pair<Mesh, Mesh> splitBodyBySurface(
    const Mesh& solid, const NurbsSurface& surface) noexcept;

// ── Mid-Surface Extraction ────────────────────────────────────────────────

struct MidSurfaceOptions {
    float maxThickness = 10.f;   // parts thicker than this won't get mid-surfaced
    float minThickness = 0.5f;   // below this, treat as zero thickness
    uint32_t sampleCount = 64;   // rays per face for thickness sampling
};

// Extract the mid-surface from a thin-walled solid part.
// Returns an open mesh representing the mid-plane of the solid.
[[nodiscard]] Mesh extractMidSurface(
    const Mesh& solid, const MidSurfaceOptions& opts = {}) noexcept;

} // namespace nexus::geometry
