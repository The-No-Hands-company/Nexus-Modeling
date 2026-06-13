#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>

namespace nexus::geometry {

enum class SweepLoftDiagnostic : uint32_t {
    Success                 = 0,
    InvalidProfileCurve     = 1,
    InvalidPathCurve        = 2,
    PathTooShort            = 4,
    FrameComputationFailed  = 8,
};

struct SweepDesc {
    bool     useFrameFollowing   = true;
    uint32_t crossSectionCount   = 20;
    bool     outputAsNurbsSurface = false;
    float    startScale          = 1.f;
    float    endScale            = 1.f;
};

struct LoftDesc {
    uint32_t layerCount          = 10;
    bool     closedLoft          = false;
    bool     outputAsNurbsSurface = false;
};

class SweepLoftOperation {
public:
    [[nodiscard]] static SweepLoftDiagnostic
    sweep(const NurbsSurface& profile,
          const NurbsSurface& path,
          const SweepDesc&    desc,
          NurbsSurface&       outSurface,
          Mesh*               outMesh = nullptr);
};

} // namespace nexus::geometry
