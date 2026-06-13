#include <nexus/geometry/NurbsKnotRefinement.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

NurbsCurve NurbsKnotRefinement::insertKnot(const NurbsCurve& curve, float u) {
    if (!curve.isValid()) return curve;
    auto [uMin, uMax] = curve.domain();
    if (u < uMin || u > uMax) return curve;
    return curve.insertKnot(u, 1);
}

NurbsCurve NurbsKnotRefinement::refineCurve(const NurbsCurve& curve,
                                             const std::vector<float>& knots) {
    if (!curve.isValid()) return curve;
    NurbsCurve result = curve;
    for (float u : knots) {
        if (u >= curve.domain().first && u <= curve.domain().second)
            result = result.insertKnot(u, 1);
    }
    return result;
}

NurbsSurface NurbsKnotRefinement::insertKnotU(const NurbsSurface& surface,
                                              float u) {
    if (!surface.isValid()) return surface;
    auto [uMin, uMax] = surface.domainU();
    if (u < uMin || u > uMax) return surface;
    return surface.insertKnotU(u, 1);
}

NurbsSurface NurbsKnotRefinement::insertKnotV(const NurbsSurface& surface,
                                              float v) {
    if (!surface.isValid()) return surface;
    auto [vMin, vMax] = surface.domainV();
    if (v < vMin || v > vMax) return surface;
    return surface.insertKnotV(v, 1);
}

} // namespace nexus::geometry
