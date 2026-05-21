#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Brush primitives (Slice 1)
//
//  Pure-data brush definitions. All geometry mutation happens in SculptSession.
//  This header is headless-safe and deterministic; no GPU dependencies.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>  // Vec3

#include <cstdint>

namespace nexus::sculpt {

/// Brush kind. Each kind has a deterministic displacement formula in SculptSession.
enum class BrushKind : uint8_t {
    Draw,     ///< Push vertices along BrushParams::direction (or vertex normal).
    Smooth,   ///< Pull each touched vertex toward the average of its 1-ring neighbors.
    Inflate,  ///< Push along vertex normal (positive strength = outward).
    Flatten,  ///< Project touched vertices onto the brush plane (origin = sample, normal = direction).
    Pinch,    ///< Pull touched vertices toward the sample position (tangential only).
};

/// Radial falloff curve used to weight per-vertex influence within the brush radius.
/// All curves return 1.0 at the center (t=0) and 0.0 at the edge (t=1).
enum class FalloffShape : uint8_t {
    Constant,  ///< 1.0 everywhere inside the radius (hard disc).
    Linear,    ///< 1 - t.
    Smooth,    ///< Cubic smoothstep: 1 - (3t² - 2t³). Default.
    Sharp,     ///< (1 - t)², drops off quickly.
};

/// Returns the radial falloff weight for a normalized distance t in [0, 1].
/// Inputs outside [0, 1] are clamped. Output is in [0, 1].
[[nodiscard]] float evaluateFalloff(FalloffShape shape, float t01) noexcept;

/// Brush configuration applied uniformly across the samples of a single stroke.
/// Mutable per-sample state (position, pressure) lives in BrushSample.
struct BrushParams {
    float radius   = 0.5f;   ///< World-space brush radius. Vertices outside are ignored.
    float strength = 0.5f;   ///< Per-sample displacement magnitude in [-N, N] (signed).
    FalloffShape falloff = FalloffShape::Smooth;

    /// World-space direction used by Draw/Flatten when useVertexNormal == false.
    /// Must be non-zero; SculptSession normalizes internally.
    nexus::render::Vec3 direction = {0.f, 1.f, 0.f};

    /// If true, Draw/Inflate displace along each vertex's smoothed normal instead
    /// of `direction`. Mesh must have valid per-vertex normals.
    bool useVertexNormal = true;

    /// Cap accumulated per-vertex displacement within a stroke. 0 = uncapped.
    /// Useful for Layer-style behavior on top of Draw.
    float maxPerVertexDisplacement = 0.f;
};

/// A single brush stamp along a stroke. Strokes are ordered by `sequence`;
/// SculptSession asserts strictly increasing sequence numbers.
struct BrushSample {
    nexus::render::Vec3 position = {};
    float    pressure = 1.f;
    uint64_t sequence = 0;
};

} // namespace nexus::sculpt
