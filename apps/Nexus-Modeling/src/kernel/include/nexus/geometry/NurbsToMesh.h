#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>

namespace nexus::geometry {

enum class TessellationMode : uint8_t {
    ChordHeight,
    ScreenSpace,
};

struct TessellationOptions {
    int32_t          initialResU = 12;
    int32_t          initialResV = 12;
    int32_t          maxDepth    = 4;
    float            tolerance   = 0.001f;
    TessellationMode mode        = TessellationMode::ChordHeight;
    float            screenSpaceHeight = 1080.f;
    float            screenSpaceFOV    = 60.f;
};

struct TessellationResult {
    Mesh    mesh;
    int32_t totalTriangles  = 0;
    int32_t maxDepthReached = 0;
};

TessellationResult tessellateNurbs(const NurbsSurface& surface,
                                  const TessellationOptions& opts = {});

} // namespace nexus::geometry
