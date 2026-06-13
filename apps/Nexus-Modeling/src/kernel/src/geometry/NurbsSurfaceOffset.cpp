#include <nexus/geometry/NurbsSurfaceOffset.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEps = 1e-10f;

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline Vec3 normalize(const Vec3& v) {
    float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (len < 1e-8f) return {0.f, 0.f, 1.f};
    return {v.x / len, v.y / len, v.z / len};
}

std::vector<float> uniformKnots(int32_t n, int32_t degree) {
    int32_t m = n + degree + 1;
    std::vector<float> k(static_cast<size_t>(m));
    for (int32_t i = 0; i <= degree; ++i) k[i] = 0.f;
    for (int32_t i = n; i < m; ++i) k[i] = 1.f;
    int32_t inner = n - degree - 1;
    if (inner > 0) {
        float step = 1.f / static_cast<float>(inner + 1);
        for (int32_t i = 1; i <= inner; ++i)
            k[static_cast<size_t>(degree + i)] = static_cast<float>(i) * step;
    }
    return k;
}

std::vector<float> chordLengthParams(const std::vector<Vec3>& pts) {
    int32_t n = static_cast<int32_t>(pts.size());
    std::vector<float> p(static_cast<size_t>(n), 0.f);
    if (n <= 1) return p;
    float total = 0.f;
    for (int32_t i = 1; i < n; ++i) {
        float dx = pts[i].x - pts[i - 1].x;
        float dy = pts[i].y - pts[i - 1].y;
        float dz = pts[i].z - pts[i - 1].z;
        total += std::sqrt(dx * dx + dy * dy + dz * dz);
        p[i] = total;
    }
    if (total > kEps) {
        float inv = 1.f / total;
        for (int32_t i = 1; i < n; ++i) p[i] *= inv;
    }
    return p;
}

float bsplineBasis(int32_t i, int32_t p, float u,
                   const std::vector<float>& knots) {
    if (p == 0) {
        if (u >= knots[i] && u < knots[i + 1]) return 1.f;
        int32_t m = static_cast<int32_t>(knots.size()) - 1;
        if (i + 1 == m && u >= knots[i] && u <= knots[i + 1]) return 1.f;
        return 0.f;
    }
    float left = 0.f, right = 0.f;
    float d1 = knots[i + p] - knots[i];
    float d2 = knots[i + p + 1] - knots[i + 1];
    if (std::abs(d1) > kEps)
        left = (u - knots[i]) / d1 *
               bsplineBasis(i, p - 1, u, knots);
    if (std::abs(d2) > kEps)
        right = (knots[i + p + 1] - u) / d2 *
                bsplineBasis(i + 1, p - 1, u, knots);
    return left + right;
}

void gaussSolve(std::vector<std::vector<float>>& A, std::vector<float>& b) {
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
        if (maxAbs < kEps) continue;
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
        if (std::abs(A[i][i]) > kEps)
            b[i] = sum / A[i][i];
    }
}

std::vector<Vec3> interpolateCurve(const std::vector<Vec3>& pts,
                                   int32_t degree,
                                   const std::vector<float>& knots) {
    int32_t n = static_cast<int32_t>(pts.size());
    if (n < 2) return pts;

    auto params = chordLengthParams(pts);

    std::vector<std::vector<float>> A(static_cast<size_t>(n),
        std::vector<float>(static_cast<size_t>(n), 0.f));
    for (int32_t k = 0; k < n; ++k) {
        float u = params[k];
        for (int32_t i = 0; i < n; ++i)
            A[k][i] = bsplineBasis(i, degree, u, knots);
    }

    auto solveCoord = [&](auto getCoord) {
        std::vector<float> b(static_cast<size_t>(n));
        for (int32_t k = 0; k < n; ++k)
            b[k] = getCoord(pts[k]);
        auto At = A;
        gaussSolve(At, b);
        return b;
    };

    auto xSol = solveCoord([](const Vec3& v) { return v.x; });
    auto ySol = solveCoord([](const Vec3& v) { return v.y; });
    auto zSol = solveCoord([](const Vec3& v) { return v.z; });

    std::vector<Vec3> ctl(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i)
        ctl[i] = {xSol[i], ySol[i], zSol[i]};
    return ctl;
}

std::vector<Vec3> interpolateGrid(const std::vector<Vec3>& grid,
                                   int32_t nU, int32_t nV,
                                   int32_t degreeU, int32_t degreeV) {
    if (nU < 2 || nV < 2) return grid;

    if (degreeU >= nU) degreeU = std::max(1, nU - 1);
    if (degreeV >= nV) degreeV = std::max(1, nV - 1);

    auto ku = uniformKnots(nU, degreeU);
    auto kv = uniformKnots(nV, degreeV);

    std::vector<Vec3> rowCtl(static_cast<size_t>(nU * nV));
    for (int32_t j = 0; j < nV; ++j) {
        std::vector<Vec3> row(static_cast<size_t>(nU));
        for (int32_t i = 0; i < nU; ++i)
            row[i] = grid[static_cast<size_t>(i * nV + j)];
        auto ctl = interpolateCurve(row, degreeU, ku);
        for (int32_t i = 0; i < nU; ++i)
            rowCtl[static_cast<size_t>(i * nV + j)] = ctl[i];
    }

    std::vector<Vec3> ctl(static_cast<size_t>(nU * nV));
    for (int32_t i = 0; i < nU; ++i) {
        std::vector<Vec3> col(static_cast<size_t>(nV));
        for (int32_t j = 0; j < nV; ++j)
            col[j] = rowCtl[static_cast<size_t>(i * nV + j)];
        auto ctlCol = interpolateCurve(col, degreeV, kv);
        for (int32_t j = 0; j < nV; ++j)
            ctl[static_cast<size_t>(i * nV + j)] = ctlCol[j];
    }

    return ctl;
}

} // namespace

NurbsSurface NurbsSurfaceOffset::offset(const NurbsSurface& surface,
                                         const NurbsSurfaceOffsetOptions& opts) {
    if (!surface.isValid()) return NurbsSurface{};

    auto [uMin, uMax] = surface.domainU();
    auto [vMin, vMax] = surface.domainV();

    int32_t nU = opts.samplesU;
    int32_t nV = opts.samplesV;
    float du = (uMax - uMin) / static_cast<float>(std::max(1, nU - 1));
    float dv = (vMax - vMin) / static_cast<float>(std::max(1, nV - 1));

    std::vector<Vec3> grid(static_cast<size_t>(nU * nV));
    for (int32_t i = 0; i < nU; ++i) {
        float u = uMin + static_cast<float>(i) * du;
        for (int32_t j = 0; j < nV; ++j) {
            float v = vMin + static_cast<float>(j) * dv;
            Vec3 S = surface.evaluate(u, v);
            Vec3 Su = surface.derivativeU(u, v);
            Vec3 Sv = surface.derivativeV(u, v);
            Vec3 normal = normalize(cross(Su, Sv));

            size_t idx = static_cast<size_t>(i * nV + j);
            grid[idx] = Vec3{
                S.x + opts.distance * normal.x,
                S.y + opts.distance * normal.y,
                S.z + opts.distance * normal.z,
            };
        }
    }

    int32_t degU = opts.fitDegree;
    int32_t degV = opts.fitDegree;
    std::vector<float> ku = uniformKnots(nU, degU);
    std::vector<float> kv = uniformKnots(nV, degV);

    std::vector<Vec3> ctl = interpolateGrid(grid, nU, nV, degU, degV);

    return NurbsSurface(degU, degV, std::move(ku), std::move(kv),
                        std::move(ctl), nU, nV);
}

} // namespace nexus::geometry
