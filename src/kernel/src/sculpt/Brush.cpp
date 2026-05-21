// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Brush falloff implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/sculpt/Brush.h>

#include <algorithm>

namespace nexus::sculpt {

float evaluateFalloff(FalloffShape shape, float t01) noexcept
{
    const float t = std::clamp(t01, 0.f, 1.f);
    switch (shape) {
        case FalloffShape::Constant: return 1.f;
        case FalloffShape::Linear:   return 1.f - t;
        case FalloffShape::Smooth: {
            // Cubic smoothstep on (1 - t): 3x² - 2x³ where x = 1 - t.
            const float x = 1.f - t;
            return x * x * (3.f - 2.f * x);
        }
        case FalloffShape::Sharp: {
            const float x = 1.f - t;
            return x * x;
        }
    }
    return 0.f;
}

} // namespace nexus::sculpt
