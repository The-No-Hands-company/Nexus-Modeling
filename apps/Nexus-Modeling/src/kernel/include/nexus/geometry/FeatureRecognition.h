#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Feature Recognition
//
//  Auto-detect holes, fillets, pockets, and other manufacturing features
//  from mesh geometry.  Uses half-edge topology and geometric heuristics.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

enum class RecognizedFeatureType : uint8_t {
    Hole,        // cylindrical through-hole or blind hole
    Fillet,      // constant-radius edge blend
    Pocket,      // recessed planar region
    Boss,        // raised cylindrical protrusion
    Slot,        // elongated cutout with parallel walls
    Chamfer,     // flat bevel on an edge
};

struct RecognizedFeature {
    RecognizedFeatureType type;
    std::string name;
    std::vector<uint32_t> faceIndices;
    std::vector<uint32_t> edgeIndices;
    std::vector<uint32_t> vertexIndices;
    Vec3  center{};
    Vec3  axis{0, 0, 1};
    float radiusOrDepth = 0.f;
};

struct FeatureRecognitionResult {
    std::vector<RecognizedFeature> features;
    uint32_t holesDetected   = 0;
    uint32_t filletsDetected  = 0;
    uint32_t pocketsDetected  = 0;
};

class FeatureRecognition {
public:
    // Analyze a mesh and detect manufacturing features.
    [[nodiscard]] static FeatureRecognitionResult recognize(
        const HalfEdgeMesh& mesh) noexcept;
};

// ──────────── GPU Acceleration ──────────────────────────────────────────────

struct GpuAccelOptions {
    bool enabled = false;
    uint32_t maxWorkgroupSize = 256;
};

class GpuAcceleration {
public:
    // Check if GPU acceleration is available on this system.
    [[nodiscard]] static bool isAvailable() noexcept;

    // Set global GPU acceleration options.
    static void setOptions(const GpuAccelOptions& opts) noexcept;
    [[nodiscard]] static const GpuAccelOptions& options() noexcept;
};

// ──────────── Auto-Healing ──────────────────────────────────────────────────

struct AutoHealReport {
    bool   valid = false;
    uint32_t duplicateVertsRemoved = 0;
    uint32_t zeroAreaFacesRemoved  = 0;
    uint32_t isolatedVertsRemoved  = 0;
    uint32_t holesFilled           = 0;
    uint32_t orientationFixed      = 0;
    std::vector<std::string> messages;
};

class MeshAutoHeal {
public:
    // One-click repair: deduplicate, remove degenerates, fill holes, fix orientation.
    [[nodiscard]] static AutoHealReport heal(Mesh& mesh) noexcept;
};

} // namespace nexus::geometry
