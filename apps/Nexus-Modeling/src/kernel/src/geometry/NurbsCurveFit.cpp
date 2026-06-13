#include <nexus/geometry/NurbsCurveFit.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

} // namespace

float NurbsCurveFit::bSplineBasis(int32_t i, int32_t p, float u,
                                   const std::vector<float>& knots) {
    if (p == 0) {
        return (u >= knots[i] && u < knots[i + 1]) ? 1.f : 0.f;
    }
    float left = 0.f, right = 0.f;
    float d1 = knots[i + p] - knots[i];
    float d2 = knots[i + p + 1] - knots[i + 1];
    if (std::abs(d1) > kEpsilon)
        left = (u - knots[i]) / d1 * bSplineBasis(i, p - 1, u, knots);
    if (std::abs(d2) > kEpsilon)
        right = (knots[i + p + 1] - u) / d2 * bSplineBasis(i + 1, p - 1, u, knots);
    return left + right;
}

std::vector<float> NurbsCurveFit::chordLengthParams(const std::vector<Vec3>& pts) {
    int32_t n = static_cast<int32_t>(pts.size());
    std::vector<float> params(static_cast<size_t>(n), 0.f);
    float total = 0.f;
    for (int32_t i = 1; i < n; ++i) {
        float dx = pts[i].x - pts[i - 1].x;
        float dy = pts[i].y - pts[i - 1].y;
        float dz = pts[i].z - pts[i - 1].z;
        total += std::sqrt(dx * dx + dy * dy + dz * dz);
        params[i] = total;
    }
    if (total > kEpsilon) {
        float inv = 1.f / total;
        for (int32_t i = 0; i < n; ++i) params[i] *= inv;
    }
    return params;
}

std::vector<float> NurbsCurveFit::uniformParams(const std::vector<Vec3>&, int32_t nParams) {
    std::vector<float> params(static_cast<size_t>(nParams));
    if (nParams <= 1) {
        if (nParams == 1) params[0] = 0.5f;
        return params;
    }
    float step = 1.f / static_cast<float>(nParams - 1);
    for (int32_t i = 0; i < nParams; ++i)
        params[i] = static_cast<float>(i) * step;
    return params;
}

std::vector<float> NurbsCurveFit::averageKnots(const std::vector<float>& params,
                                                int32_t degree, int32_t nCtl) {
    int32_t nKnots = nCtl + degree + 1;
    std::vector<float> knots(static_cast<size_t>(nKnots), 0.f);
    for (int32_t i = 0; i <= degree; ++i) {
        knots[i] = 0.f;
        knots[nKnots - 1 - i] = 1.f;
    }
    int32_t nParams = static_cast<int32_t>(params.size());
    for (int32_t i = 1; i < nCtl - degree; ++i) {
        float sum = 0.f;
        int32_t count = 0;
        for (int32_t j = i; j < i + degree && j < nParams; ++j) {
            sum += params[j];
            ++count;
        }
        knots[static_cast<size_t>(degree + i)] = count > 0 ? sum / static_cast<float>(count) : 0.f;
    }
    return knots;
}

void NurbsCurveFit::gaussianElimination(std::vector<std::vector<float>>& A,
                                        std::vector<float>& b) {
    int32_t n = static_cast<int32_t>(A.size());
    for (int32_t i = 0; i < n; ++i) {
        int32_t pivot = i;
        float maxAbs = std::abs(A[i][i]);
        for (int32_t r = i + 1; r < n; ++r) {
            if (std::abs(A[r][i]) > maxAbs) {
                maxAbs = std::abs(A[r][i]);
                pivot = r;
            }
        }
        if (maxAbs < kEpsilon) continue;
        if (pivot != i) {
            std::swap(A[i], A[pivot]);
            std::swap(b[i], b[pivot]);
        }
        for (int32_t j = i + 1; j < n; ++j) {
            float factor = A[j][i] / A[i][i];
            for (int32_t k = i; k < n; ++k)
                A[j][k] -= factor * A[i][k];
            b[j] -= factor * b[i];
        }
    }
    for (int32_t i = n - 1; i >= 0; --i) {
        float sum = b[i];
        for (int32_t j = i + 1; j < n; ++j)
            sum -= A[i][j] * b[j];
        if (std::abs(A[i][i]) > kEpsilon)
            b[i] = sum / A[i][i];
    }
}

NurbsCurveFitResult NurbsCurveFit::fit(const std::vector<Vec3>& points,
                                        const NurbsCurveFitOptions& opts) {
    NurbsCurveFitResult result;
    int32_t nPts = static_cast<int32_t>(points.size());
    if (nPts < 2) return result;

    int32_t degree = opts.degree;
    int32_t nCtl   = opts.controlPoints > 0 ? opts.controlPoints
                                             : std::min(nPts, degree + 5);
    if (nCtl < degree + 1) nCtl = degree + 1;

    std::vector<float> params = opts.uniformParams
        ? uniformParams(points, nPts)
        : chordLengthParams(points);

    std::vector<float> knots = averageKnots(params, degree, nCtl);

    NurbsCurve curve;
    curve.setDegree(degree);
    curve.setKnots(knots);

    std::vector<std::vector<float>> basis;
    basis.resize(static_cast<size_t>(nPts));
    for (int32_t k = 0; k < nPts; ++k) {
        float u = params[k];
        basis[k].resize(static_cast<size_t>(nCtl), 0.f);
        for (int32_t i = 0; i < nCtl; ++i)
            basis[k][i] = bSplineBasis(i, degree, u, knots);
    }

    int32_t n = nCtl;
    std::vector<std::vector<float>> A;
    A.resize(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        A[i].resize(static_cast<size_t>(n), 0.f);
        for (int32_t j = 0; j < n; ++j) {
            float sum = 0.f;
            for (int32_t k = 0; k < nPts; ++k)
                sum += basis[k][i] * basis[k][j];
            A[i][j] = sum;
        }
    }

    auto solveCoord = [&](auto getCoord) {
        std::vector<float> bVec(static_cast<size_t>(n), 0.f);
        for (int32_t i = 0; i < n; ++i) {
            float sum = 0.f;
            for (int32_t k = 0; k < nPts; ++k)
                sum += basis[k][i] * getCoord(points[k]);
            bVec[i] = sum;
        }
        auto At = A;
        gaussianElimination(At, bVec);
        return bVec;
    };

    auto xSol = solveCoord([](const Vec3& v) { return v.x; });
    auto ySol = solveCoord([](const Vec3& v) { return v.y; });
    auto zSol = solveCoord([](const Vec3& v) { return v.z; });

    std::vector<Vec3> ctl(static_cast<size_t>(nCtl));
    for (int32_t i = 0; i < nCtl; ++i)
        ctl[i] = {xSol[i], ySol[i], zSol[i]};
    curve.setControlPoints(ctl);

    float sumSqErr = 0.f;
    float maxErr = 0.f;
    for (int32_t k = 0; k < nPts; ++k) {
        Vec3 eval = curve.evaluate(params[k]);
        float dx = eval.x - points[k].x;
        float dy = eval.y - points[k].y;
        float dz = eval.z - points[k].z;
        float e2 = dx * dx + dy * dy + dz * dz;
        sumSqErr += e2;
        if (e2 > maxErr) maxErr = e2;
    }

    result.curve      = curve;
    result.rmsError   = std::sqrt(sumSqErr / static_cast<float>(nPts));
    result.maxError   = std::sqrt(maxErr);
    result.iterations = 1;
    result.converged  = true;
    return result;
}

} // namespace nexus::geometry
