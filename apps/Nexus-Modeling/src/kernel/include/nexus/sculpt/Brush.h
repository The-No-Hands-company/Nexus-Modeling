#pragma once
#include <nexus/render/Camera.h>

#include <cstdint>

namespace nexus::sculpt {

enum class BrushKind : uint8_t {
    Draw,
    Smooth,
    Inflate,
    Flatten,
    Pinch,
    Crease,
    Layer,
    Grab,
};

enum class FalloffShape : uint8_t {
    Constant,
    Linear,
    Smooth,
    Sharp,
};

float evaluateFalloff(FalloffShape shape, float t01) noexcept;

struct BrushParams {
    float radius = 0.5f;
    float strength = 0.5f;
    FalloffShape falloff = FalloffShape::Smooth;
    render::Vec3 direction = {0.f, 1.f, 0.f};
    bool useVertexNormal = true;
    float maxPerVertexDisplacement = 0.f;
};

struct BrushSample {
    render::Vec3 position;
    float pressure = 1.f;
    uint64_t sequence = 0;
};

} // namespace nexus::sculpt
