#include <nexus/geometry/NurbsSurfaceFit.h>
#include <nexus/geometry/NurbsCurveFit.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

float bSplineBasis(int32_t i, int32_t p, float u,
                   const std::vector<float>& knots) {
    return NurbsCurveFit::bSplineBasis(i, p, u, knots);
}

} // namespace

std::vector<float> NurbsSurfaceFit::uniformParams(int32_t nParams) {
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

std::vector<float> NurbsSurfaceFit::chordLengthParams(const std::vector<Vec3>& pts) {
    int32_t n = static_cast<int32_t>(pts.size());
    std::vector<float> p(static_cast<size_t>(n), 0.f);
    float total = 0.f;
    for (int32_t i = 1; i < n; ++i) {
        float dx = pts[i].x - pts[i - 1].x;
        float dy = pts[i].y - pts[i - 1].y;
        float dz = pts[i].z - pts[i - 1].z;
        total += std::sqrt(dx * dx + dy * dy + dz * dz);
        p[i] = total;
    }
    if (total > kEpsilon) {
        float inv = 1.f / total;
        for (int32_t i = 0; i < n; ++i) p[i] *= inv;
    }
    return p;
}

std::vector<float> NurbsSurfaceFit::averagedKnotVector(
    const std::vector<float>& params, int32_t degree, int32_t nCtl) {
    int32_t n = nCtl + degree + 1;
    std::vector<float> knots(static_cast<size_t>(n));
    for (int32_t i = 0; i <= degree; ++i) knots[i] = 0.f;
    for (int32_t i = nCtl; i < n; ++i) knots[i] = 1.f;
    int32_t nParams = static_cast<int32_t>(params.size());
    for (int32_t i = 1; i < nCtl - degree; ++i) {
        float sum = 0.f;
        int32_t count = 0;
        for (int32_t j = i; j < i + degree && j < nParams; ++j) {
            sum += params[j];
            ++count;
        }
        knots[static_cast<size_t>(degree + i)] =
            (count > 0) ? sum / static_cast<float>(count) : 0.f;
    }
    return knots;
}

void NurbsSurfaceFit::solveInterpolation(const std::vector<Vec3>& pts,
                                          const std::vector<float>& params,
                                          const std::vector<float>& knots,
                                          int32_t degree,
                                          std::vector<Vec3>& outCtl) {
    int32_t n = static_cast<int32_t>(pts.size());
    int32_t nCtl = n;
    outCtl.resize(static_cast<size_t>(nCtl));

    std::vector<std::vector<float>> A;
    A.resize(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        A[i].resize(static_cast<size_t>(n), 0.f);
        for (int32_t j = 0; j < n; ++j)
            A[i][j] = bSplineBasis(j, degree, params[i], knots);
    }

    // Solve per-coordinate using Gaussian elimination
    auto solve = [&](auto getCoord) {
        auto AA = A;
        std::vector<float> b(static_cast<size_t>(n));
        for (int32_t i = 0; i < n; ++i) b[i] = getCoord(pts[i]);

        // Gaussian elimination with partial pivoting
        for (int32_t c = 0; c < n; ++c) {
            int32_t pivot = c;
            float maxVal = std::abs(AA[c][c]);
            for (int32_t r = c + 1; r < n; ++r) {
                if (std::abs(AA[r][c]) > maxVal) {
                    maxVal = std::abs(AA[r][c]);
                    pivot = r;
                }
            }
            if (maxVal < kEpsilon) continue;
            if (pivot != c) {
                std::swap(AA[c], AA[pivot]);
                std::swap(b[c], b[pivot]);
            }
            for (int32_t r = c + 1; r < n; ++r) {
                float factor = AA[r][c] / AA[c][c];
                AA[r][c] = 0.f;
                for (int32_t j = c + 1; j < n; ++j)
                    AA[r][j] -= factor * AA[c][j];
                b[r] -= factor * b[c];
            }
        }
        for (int32_t c = n - 1; c >= 0; --c) {
            float sum = b[c];
            for (int32_t j = c + 1; j < n; ++j)
                sum -= AA[c][j] * b[j];
            if (std::abs(AA[c][c]) > kEpsilon)
                b[c] = sum / AA[c][c];
        }
        return b;
    };

    auto xSol = solve([](const Vec3& v) { return v.x; });
    auto ySol = solve([](const Vec3& v) { return v.y; });
    auto zSol = solve([](const Vec3& v) { return v.z; });

    for (int32_t i = 0; i < n; ++i)
        outCtl[i] = {xSol[i], ySol[i], zSol[i]};
}

void NurbsSurfaceFit::solveApproximation(const std::vector<Vec3>& pts,
                                          const std::vector<float>& params,
                                          const std::vector<float>& knots,
                                          int32_t degree, int32_t nCtl,
                                          std::vector<Vec3>& outCtl) {
    int32_t nPts = static_cast<int32_t>(pts.size());
    outCtl.resize(static_cast<size_t>(nCtl));

    std::vector<std::vector<float>> basis;
    basis.resize(static_cast<size_t>(nPts));
    for (int32_t k = 0; k < nPts; ++k) {
        basis[k].resize(static_cast<size_t>(nCtl), 0.f);
        for (int32_t i = 0; i < nCtl; ++i)
            basis[k][i] = bSplineBasis(i, degree, params[k], knots);
    }

    std::vector<std::vector<float>> A;
    A.resize(static_cast<size_t>(nCtl));
    for (int32_t i = 0; i < nCtl; ++i) {
        A[i].resize(static_cast<size_t>(nCtl), 0.f);
        for (int32_t j = 0; j < nCtl; ++j) {
            float sum = 0.f;
            for (int32_t k = 0; k < nPts; ++k)
                sum += basis[k][i] * basis[k][j];
            A[i][j] = sum;
        }
    }

    auto solve = [&](auto getCoord) {
        auto AA = A;
        std::vector<float> b(static_cast<size_t>(nCtl), 0.f);
        for (int32_t i = 0; i < nCtl; ++i) {
            float sum = 0.f;
            for (int32_t k = 0; k < nPts; ++k)
                sum += basis[k][i] * getCoord(pts[k]);
            b[i] = sum;
        }

        for (int32_t c = 0; c < nCtl; ++c) {
            int32_t pivot = c;
            float maxVal = std::abs(AA[c][c]);
            for (int32_t r = c + 1; r < nCtl; ++r) {
                if (std::abs(AA[r][c]) > maxVal) {
                    maxVal = std::abs(AA[r][c]);
                    pivot = r;
                }
            }
            if (maxVal < kEpsilon) continue;
            if (pivot != c) {
                std::swap(AA[c], AA[pivot]);
                std::swap(b[c], b[pivot]);
            }
            for (int32_t r = c + 1; r < nCtl; ++r) {
                float factor = AA[r][c] / AA[c][c];
                AA[r][c] = 0.f;
                for (int32_t j = c + 1; j < nCtl; ++j)
                    AA[r][j] -= factor * AA[c][j];
                b[r] -= factor * b[c];
            }
        }
        for (int32_t c = nCtl - 1; c >= 0; --c) {
            float sum = b[c];
            for (int32_t j = c + 1; j < nCtl; ++j)
                sum -= AA[c][j] * b[j];
            if (std::abs(AA[c][c]) > kEpsilon)
                b[c] = sum / AA[c][c];
        }
        return b;
    };

    auto xSol = solve([](const Vec3& v) { return v.x; });
    auto ySol = solve([](const Vec3& v) { return v.y; });
    auto zSol = solve([](const Vec3& v) { return v.z; });

    for (int32_t i = 0; i < nCtl; ++i)
        outCtl[i] = {xSol[i], ySol[i], zSol[i]};
}

NurbsSurface NurbsSurfaceFit::approximateGrid(const std::vector<Vec3>& grid,
                                               int32_t nu, int32_t nv,
                                               int32_t nCtlU, int32_t nCtlV,
                                               bool useUniform) {
    int32_t deg = 3;

    std::vector<Vec3> rowInterp(static_cast<size_t>(nCtlU * nv));
    std::vector<float> avgKnotU(static_cast<size_t>(nCtlU + deg + 1));
    std::vector<float> avgKnotV(static_cast<size_t>(nCtlV + deg + 1));

    std::vector<float> sumKnotU(static_cast<size_t>(nCtlU + deg + 1), 0.f);
    std::vector<float> sumKnotV(static_cast<size_t>(nCtlV + deg + 1), 0.f);

    for (int32_t j = 0; j < nv; ++j) {
        std::vector<Vec3> row(static_cast<size_t>(nu));
        for (int32_t i = 0; i < nu; ++i)
            row[i] = grid[static_cast<size_t>(i * nv + j)];

        std::vector<float> params = useUniform
            ? uniformParams(static_cast<int32_t>(row.size()))
            : chordLengthParams(row);
        std::vector<float> knots = averagedKnotVector(params, deg, nCtlU);
        std::vector<Vec3> ctl;
        solveApproximation(row, params, knots, deg, nCtlU, ctl);

        for (int32_t i = 0; i < nCtlU; ++i)
            rowInterp[static_cast<size_t>(i * nv + j)] = ctl[i];

        for (size_t k = 0; k < sumKnotU.size(); ++k)
            sumKnotU[k] += knots[k];
    }

    for (size_t k = 0; k < sumKnotU.size(); ++k)
        avgKnotU[k] = sumKnotU[k] / static_cast<float>(nv);

    std::vector<Vec3> ctlGrid(static_cast<size_t>(nCtlU * nCtlV));
    for (int32_t i = 0; i < nCtlU; ++i) {
        std::vector<Vec3> col(static_cast<size_t>(nv));
        for (int32_t j = 0; j < nv; ++j)
            col[j] = rowInterp[static_cast<size_t>(i * nv + j)];

        std::vector<float> params = useUniform
            ? uniformParams(static_cast<int32_t>(col.size()))
            : chordLengthParams(col);
        std::vector<float> knots = averagedKnotVector(params, deg, nCtlV);
        std::vector<Vec3> ctl;
        solveApproximation(col, params, knots, deg, nCtlV, ctl);

        for (int32_t j = 0; j < nCtlV; ++j)
            ctlGrid[static_cast<size_t>(i * nCtlV + j)] = ctl[j];

        for (size_t k = 0; k < sumKnotV.size(); ++k)
            sumKnotV[k] += knots[k];
    }

    for (size_t k = 0; k < sumKnotV.size(); ++k)
        avgKnotV[k] = sumKnotV[k] / static_cast<float>(nCtlU);

    return NurbsSurface(deg, deg, std::move(avgKnotU), std::move(avgKnotV),
                        std::move(ctlGrid), nCtlU, nCtlV);
}

NurbsSurface NurbsSurfaceFit::fit(const std::vector<Vec3>& grid,
                                   int32_t nu, int32_t nv,
                                   const NurbsSurfaceFitOptions& opts) {
    if (static_cast<int32_t>(grid.size()) != nu * nv || nu < 2 || nv < 2)
        return NurbsSurface{};

    if (opts.useApproximation) {
        int32_t deg = 3;
        int32_t nCtlU = opts.controlPointsU > 0 ? opts.controlPointsU : nu / 2;
        int32_t nCtlV = opts.controlPointsV > 0 ? opts.controlPointsV : nv / 2;
        if (nCtlU < deg + 1) nCtlU = deg + 1;
        if (nCtlV < deg + 1) nCtlV = deg + 1;
        return approximateGrid(grid, nu, nv, nCtlU, nCtlV, opts.uniformParams);
    }

    int32_t deg = 3;
    // Two-pass curve interpolation: rows first, then columns

    // Pass 1: interpolate each row
    std::vector<Vec3> rowInterp(static_cast<size_t>(nu * nv));
    std::vector<float> avgKnotU(static_cast<size_t>(nu + deg + 1));
    std::vector<float> avgKnotV(static_cast<size_t>(nv + deg + 1));

    std::vector<float> sumKnotU(static_cast<size_t>(nu + deg + 1), 0.f);
    std::vector<float> sumKnotV(static_cast<size_t>(nv + deg + 1), 0.f);

    for (int32_t j = 0; j < nv; ++j) {
        std::vector<Vec3> row(static_cast<size_t>(nu));
        for (int32_t i = 0; i < nu; ++i)
            row[i] = grid[static_cast<size_t>(i * nv + j)];

        std::vector<float> params = opts.uniformParams
            ? uniformParams(static_cast<int32_t>(row.size()))
            : chordLengthParams(row);
        std::vector<float> knots  = averagedKnotVector(params, deg, nu);
        std::vector<Vec3> ctl;
        solveInterpolation(row, params, knots, deg, ctl);

        for (int32_t i = 0; i < nu; ++i)
            rowInterp[static_cast<size_t>(i * nv + j)] = ctl[i];

        for (size_t k = 0; k < sumKnotU.size(); ++k)
            sumKnotU[k] += knots[k];
    }

    for (size_t k = 0; k < sumKnotU.size(); ++k)
        avgKnotU[k] = sumKnotU[k] / static_cast<float>(nv);

    // Pass 2: interpolate each column of row-interpolated ctl
    std::vector<Vec3> ctlGrid(static_cast<size_t>(nu * nv));
    for (int32_t i = 0; i < nu; ++i) {
        std::vector<Vec3> col(static_cast<size_t>(nv));
        for (int32_t j = 0; j < nv; ++j)
            col[j] = rowInterp[static_cast<size_t>(i * nv + j)];

        std::vector<float> params = opts.uniformParams
            ? uniformParams(static_cast<int32_t>(col.size()))
            : chordLengthParams(col);
        std::vector<float> knots  = averagedKnotVector(params, deg, nv);
        std::vector<Vec3> ctl;
        solveInterpolation(col, params, knots, deg, ctl);

        for (int32_t j = 0; j < nv; ++j)
            ctlGrid[static_cast<size_t>(i * nv + j)] = ctl[j];

        for (size_t k = 0; k < sumKnotV.size(); ++k)
            sumKnotV[k] += knots[k];
    }

    for (size_t k = 0; k < sumKnotV.size(); ++k)
        avgKnotV[k] = sumKnotV[k] / static_cast<float>(nu);

    return NurbsSurface(deg, deg, std::move(avgKnotU), std::move(avgKnotV),
                        std::move(ctlGrid), nu, nv);
}

} // namespace nexus::geometry
