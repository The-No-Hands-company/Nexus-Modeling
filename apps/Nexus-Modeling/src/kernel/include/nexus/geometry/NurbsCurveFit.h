#pragma once
// ─── Nexus Geometry ── NurbsCurveFit ──────────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsCurveFitOptions {
    int32_t degree           = 3;
    int32_t controlPoints    = 0;
    bool    uniformParams    = false;
    int32_t maxIterations    = 10;
    float   tolerance        = 1e-4f;
};

struct NurbsCurveFitResult {
    NurbsCurve curve;
    float      rmsError  = 0.f;
    float      maxError  = 0.f;
    int32_t    iterations = 0;
    bool       converged  = false;
};

class NurbsCurveFit {
public:
    static NurbsCurveFitResult fit(const std::vector<Vec3>& points,
                                   const NurbsCurveFitOptions& opts = {});

    static float bSplineBasis(int32_t i, int32_t p, float u,
                              const std::vector<float>& knots);

private:
    static std::vector<float> chordLengthParams(const std::vector<Vec3>& pts);
    static std::vector<float> uniformParams(const std::vector<Vec3>& pts,
                                            int32_t nParams);
    static std::vector<float> averageKnots(const std::vector<float>& params,
                                           int32_t degree, int32_t nCtl);
    static void gaussianElimination(std::vector<std::vector<float>>& A,
                                     std::vector<float>& b);
};

} // namespace nexus::geometry
