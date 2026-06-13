#pragma once

#include <nexus/geometry/HalfEdgeMesh.h>

#include <cstdint>
#include <optional>

namespace nexus::geometry {

struct DecimationOptions {
    uint32_t targetFaceCount    = 0;
    float    targetRatio        = 0.5f;
    float    maxError           = 1e30f;
    bool     preserveBoundary   = true;
    bool     preserveAttributes = false;
    float    uvWeight           = 1.0f;
};

struct DecimationReport {
    uint32_t facesIn        = 0;
    uint32_t facesOut       = 0;
    uint32_t collapses      = 0;
    float    maxErrorApplied = 0.f;
};

class MeshDecimator {
public:
    [[nodiscard]] static std::optional<std::pair<HalfEdgeMesh, DecimationReport>>
    decimate(const HalfEdgeMesh& mesh, const DecimationOptions& opts = {});
};

} // namespace nexus::geometry
