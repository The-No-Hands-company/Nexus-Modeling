#include <nexus/sculpt/Brush.h>

#include <algorithm>

namespace nexus::sculpt {

float evaluateFalloff(FalloffShape shape, float t01) noexcept {
    switch (shape) {
    case FalloffShape::Constant:
        return 1.f;
    case FalloffShape::Linear:
        return 1.f - t01;
    case FalloffShape::Smooth: {
        float t = std::clamp(t01, 0.f, 1.f);
        return 1.f - (3.f * t * t - 2.f * t * t * t);
    }
    case FalloffShape::Sharp: {
        float t = std::clamp(t01, 0.f, 1.f);
        return (1.f - t) * (1.f - t);
    }
    default:
        return 1.f - t01;
    }
}

} // namespace nexus::sculpt
