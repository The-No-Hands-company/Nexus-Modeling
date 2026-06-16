#pragma once
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/NurbsCurve.h>
#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>
#include <cstdint>
#include <vector>

namespace nexus::geometry {
using Vec3 = nexus::render::Vec3;

// ── Face-Face Fillet ──────────────────────────────────────────────────
struct FaceFilletOptions { float radius=5.f; uint32_t segments=8; bool constantRadius=true; };
[[nodiscard]] Mesh faceFillet(const Mesh& solid, uint32_t faceA, uint32_t faceB, const FaceFilletOptions& opts = {}) noexcept;

// ── Surface Extension ─────────────────────────────────────────────────
struct SurfaceExtensionOptions { float distance=10.f; bool extendU=true; bool extendMin=true; };
[[nodiscard]] NurbsSurface extendSurface(const NurbsSurface& surf, const SurfaceExtensionOptions& opts = {}) noexcept;

// ── Face Offset with Draft ────────────────────────────────────────────
struct FaceOffsetOptions { float offset=5.f; float draftAngleDeg=0.f; Vec3 pullDirection{0,0,1}; };
[[nodiscard]] Mesh offsetFaceWithDraft(const HalfEdgeMesh& mesh, uint32_t face, const FaceOffsetOptions& opts = {}) noexcept;

// ── Curve Imprint ─────────────────────────────────────────────────────
struct ImprintResult { Mesh mesh; std::vector<uint32_t> newEdges; bool success=false; };
[[nodiscard]] ImprintResult imprintCurveOnMesh(const Mesh& mesh, const NurbsCurve& curve, const Vec3& projectionDir = {0,0,1}) noexcept;
}
