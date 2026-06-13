#pragma once

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsCurve.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace nexus::geometry {

struct TrimCurve2D {
    std::vector<Vec2> points;
    std::optional<NurbsCurve> nurbsCurve;
    bool closed = true;
};

struct TrimLoop {
    std::vector<TrimCurve2D> curves;
    bool isOuterLoop = true;
};

class SurfaceTrim {
public:
    std::vector<TrimLoop> loops;

    bool isEmpty() const;
    size_t curveCount() const;
    void addLoop(const TrimLoop& loop);
    [[nodiscard]] bool containsPoint(float u, float v) const;

private:
    [[nodiscard]] static bool segmentRayIntersect(const Vec2& a, const Vec2& b,
                                                  float pu, float pv) noexcept;
};

} // namespace nexus::geometry
