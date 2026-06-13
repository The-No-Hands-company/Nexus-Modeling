#pragma once
// ─── Nexus Geometry ── NurbsSurfaceFit ─────────────────────────────────

#include <nexus/geometry/NurbsSurface.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsSurfaceFitOptions {
    bool    uniformParams    = false;
    bool    useApproximation = false;
    int32_t controlPointsU   = 0;
    int32_t controlPointsV   = 0;
};

class NurbsSurfaceFit {
public:
    static NurbsSurface fit(const std::vector<Vec3>& grid,
                             int32_t nu, int32_t nv,
                             const NurbsSurfaceFitOptions& opts = {});

private:
    static std::vector<float> chordLengthParams(const std::vector<Vec3>& pts);
    static std::vector<float> uniformParams(int32_t nParams);
    static std::vector<float> averagedKnotVector(const std::vector<float>& params,
                                                  int32_t degree, int32_t nCtl);
    static void solveInterpolation(const std::vector<Vec3>& pts,
                                    const std::vector<float>& params,
                                    const std::vector<float>& knots,
                                    int32_t degree,
                                    std::vector<Vec3>& outCtl);
    static void solveApproximation(const std::vector<Vec3>& pts,
                                    const std::vector<float>& params,
                                    const std::vector<float>& knots,
                                    int32_t degree, int32_t nCtl,
                                    std::vector<Vec3>& outCtl);
    static NurbsSurface approximateGrid(const std::vector<Vec3>& grid,
                                         int32_t nu, int32_t nv,
                                         int32_t nCtlU, int32_t nCtlV,
                                         bool useUniform);
};

} // namespace nexus::geometry
