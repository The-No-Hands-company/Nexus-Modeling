#include <nexus/geometry/NurbsSurface.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <utility>

namespace nexus::geometry {

// ─── helpers ─────────────────────────────────────────────────────────────────

static std::vector<double> clampedKnots(uint32_t n, uint32_t p)
{
    const uint32_t m  = n + p + 1;
    std::vector<double> t(m, 0.0);
    const uint32_t inner = m - 2 * (p + 1);
    for (uint32_t i = 0; i < p + 1; ++i) { t[i] = 0.0; }
    for (uint32_t i = 0; i < inner; ++i) {
        t[p + 1 + i] = static_cast<double>(i + 1) / (inner + 1);
    }
    for (uint32_t i = 0; i < p + 1; ++i) { t[m - 1 - i] = 1.0; }
    return t;
}

// ─── create ──────────────────────────────────────────────────────────────────

std::optional<NurbsSurface> NurbsSurface::create(NurbsSurfaceData data)
{
    const uint32_t nu = data.nU, nv = data.nV;
    const uint32_t pu = data.degreeU, pv = data.degreeV;
    if (pu == 0 || pv == 0) { return std::nullopt; }
    if (nu < pu + 1 || nv < pv + 1) { return std::nullopt; }
    if (data.controlPoints.size() != nu * nv) { return std::nullopt; }
    if (data.knotsU.size() != nu + pu + 1) { return std::nullopt; }
    if (data.knotsV.size() != nv + pv + 1) { return std::nullopt; }
    if (data.weights.size() != nu * nv) {
        data.weights.assign(nu * nv, 1.0);
    }
    return NurbsSurface(std::move(data));
}

std::optional<NurbsSurface> NurbsSurface::fromGrid(
    const std::vector<nexus::render::Vec3>& pts,
    uint32_t nU, uint32_t nV,
    uint32_t degreeU, uint32_t degreeV)
{
    if (pts.size() != nU * nV) { return std::nullopt; }
    if (degreeU == 0 || degreeV == 0) { return std::nullopt; }
    if (nU < degreeU + 1 || nV < degreeV + 1) { return std::nullopt; }

    NurbsSurfaceData d;
    d.degreeU = degreeU;
    d.degreeV = degreeV;
    d.nU      = nU;
    d.nV      = nV;
    d.controlPoints = pts;
    d.weights.assign(nU * nV, 1.0);
    d.knotsU  = clampedKnots(nU, degreeU);
    d.knotsV  = clampedKnots(nV, degreeV);
    return NurbsSurface(std::move(d));
}

// ─── Basis functions ─────────────────────────────────────────────────────────

double NurbsSurface::basisU(uint32_t i, uint32_t p, double u) const noexcept
{
    const auto& t = m_data.knotsU;
    if (p == 0) {
        if (u == t.back() && i + 1 < t.size() && t[i] <= u && u <= t[i + 1]) return 1.0;
        return (t[i] <= u && u < t[i + 1]) ? 1.0 : 0.0;
    }
    double left = 0.0, right = 0.0;
    const double d1 = t[i + p] - t[i];
    const double d2 = t[i + p + 1] - t[i + 1];
    if (d1 > 1e-15) left  = (u - t[i])           / d1 * basisU(i, p - 1, u);
    if (d2 > 1e-15) right = (t[i + p + 1] - u)   / d2 * basisU(i + 1, p - 1, u);
    return left + right;
}

double NurbsSurface::basisV(uint32_t j, uint32_t q, double v) const noexcept
{
    const auto& t = m_data.knotsV;
    if (q == 0) {
        if (v == t.back() && j + 1 < t.size() && t[j] <= v && v <= t[j + 1]) return 1.0;
        return (t[j] <= v && v < t[j + 1]) ? 1.0 : 0.0;
    }
    double left = 0.0, right = 0.0;
    const double d1 = t[j + q] - t[j];
    const double d2 = t[j + q + 1] - t[j + 1];
    if (d1 > 1e-15) left  = (v - t[j])           / d1 * basisV(j, q - 1, v);
    if (d2 > 1e-15) right = (t[j + q + 1] - v)   / d2 * basisV(j + 1, q - 1, v);
    return left + right;
}

// ─── Evaluate ────────────────────────────────────────────────────────────────

nexus::render::Vec3 NurbsSurface::evaluate(double u, double v) const noexcept
{
    u = std::clamp(u, paramMinU(), paramMaxU());
    v = std::clamp(v, paramMinV(), paramMaxV());

    const uint32_t nu = m_data.nU, nv = m_data.nV;
    const uint32_t pu = m_data.degreeU, pv = m_data.degreeV;

    double wx = 0, wy = 0, wz = 0, wsum = 0;
    for (uint32_t i = 0; i < nu; ++i) {
        const double Nu = basisU(i, pu, u);
        if (Nu == 0.0) { continue; }
        for (uint32_t j = 0; j < nv; ++j) {
            const double Nv  = basisV(j, pv, v);
            const double w   = m_data.weights[i * nv + j];
            const double Nw  = Nu * Nv * w;
            const auto&  pt  = m_data.controlPoints[i * nv + j];
            wx   += Nw * pt.x;
            wy   += Nw * pt.y;
            wz   += Nw * pt.z;
            wsum += Nw;
        }
    }
    if (wsum < 1e-15) { return m_data.controlPoints[0]; }
    return {static_cast<float>(wx / wsum),
            static_cast<float>(wy / wsum),
            static_cast<float>(wz / wsum)};
}

// ─── Derivatives ─────────────────────────────────────────────────────────────

nexus::render::Vec3 NurbsSurface::derivU(double u, double v) const noexcept
{
    const double h  = (paramMaxU() - paramMinU()) * 1e-6;
    const double u0 = std::max(u - h, paramMinU());
    const double u1 = std::min(u + h, paramMaxU());
    const auto p0 = evaluate(u0, v), p1 = evaluate(u1, v);
    const float inv = static_cast<float>(1.0 / (u1 - u0));
    return {(p1.x - p0.x) * inv, (p1.y - p0.y) * inv, (p1.z - p0.z) * inv};
}

nexus::render::Vec3 NurbsSurface::derivV(double u, double v) const noexcept
{
    const double h  = (paramMaxV() - paramMinV()) * 1e-6;
    const double v0 = std::max(v - h, paramMinV());
    const double v1 = std::min(v + h, paramMaxV());
    const auto p0 = evaluate(u, v0), p1 = evaluate(u, v1);
    const float inv = static_cast<float>(1.0 / (v1 - v0));
    return {(p1.x - p0.x) * inv, (p1.y - p0.y) * inv, (p1.z - p0.z) * inv};
}

nexus::render::Vec3 NurbsSurface::normal(double u, double v) const noexcept
{
    const auto su = derivU(u, v);
    const auto sv = derivV(u, v);
    float nx = su.y * sv.z - su.z * sv.y;
    float ny = su.z * sv.x - su.x * sv.z;
    float nz = su.x * sv.y - su.y * sv.x;
    const float len = std::sqrt(nx * nx + ny * ny + nz * nz);
    if (len < 1e-12f) { return {0, 0, 1}; }
    return {nx / len, ny / len, nz / len};
}

// ─── Tessellation ────────────────────────────────────────────────────────────

Mesh NurbsSurface::tessellate(uint32_t samplesU, uint32_t samplesV) const
{
    if (samplesU < 2) { samplesU = 2; }
    if (samplesV < 2) { samplesV = 2; }

    Mesh out;
    MeshAttributes attrs;
    std::vector<nexus::render::Vec3> pts;
    pts.reserve(samplesU * samplesV);

    const double uLo = paramMinU(), uHi = paramMaxU();
    const double vLo = paramMinV(), vHi = paramMaxV();

    for (uint32_t i = 0; i < samplesU; ++i) {
        const double u = uLo + (uHi - uLo) * (static_cast<double>(i) / (samplesU - 1));
        for (uint32_t j = 0; j < samplesV; ++j) {
            const double v = vLo + (vHi - vLo) * (static_cast<double>(j) / (samplesV - 1));
            pts.push_back(evaluate(u, v));
        }
    }

    attrs.setPositions(std::move(pts));
    out.attributes() = std::move(attrs);

    // Quads: (i,j), (i+1,j), (i+1,j+1), (i,j+1) → two triangles each.
    auto idx = [&](uint32_t i, uint32_t j) { return i * samplesV + j; };
    for (uint32_t i = 0; i + 1 < samplesU; ++i) {
        for (uint32_t j = 0; j + 1 < samplesV; ++j) {
            Face f0; f0.indices = {idx(i, j), idx(i+1, j), idx(i+1, j+1)};
            Face f1; f1.indices = {idx(i, j), idx(i+1, j+1), idx(i, j+1)};
            out.topology().addFace(std::move(f0));
            out.topology().addFace(std::move(f1));
        }
    }
    return out;
}

Mesh NurbsSurface::tessellateAdaptive(float tolerance) const
{
    if (tolerance <= 0.f) { tolerance = 1e-3f; }
    // Start with a 4×4 base grid and adaptively subdivide.
    // For simplicity we use the regular tessellate with enough samples to
    // achieve the tolerance, by estimating the maximum derivative magnitude.
    const uint32_t baseSamples = 4;
    uint32_t samplesU = baseSamples, samplesV = baseSamples;

    // Estimate maximum chord error by doubling until it's small enough.
    for (uint32_t iter = 0; iter < 8; ++iter) {
        const double uLo = paramMinU(), uHi = paramMaxU();
        const double vLo = paramMinV(), vHi = paramMaxV();
        float maxErr = 0.f;
        for (uint32_t i = 0; i + 1 < samplesU; ++i) {
            const double u0 = uLo + (uHi - uLo) * (static_cast<double>(i) / (samplesU - 1));
            const double u1 = uLo + (uHi - uLo) * (static_cast<double>(i + 1) / (samplesU - 1));
            const double um = (u0 + u1) * 0.5;
            for (uint32_t j = 0; j + 1 < samplesV; ++j) {
                const double v0 = vLo + (vHi - vLo) * (static_cast<double>(j) / (samplesV - 1));
                const double v1 = vLo + (vHi - vLo) * (static_cast<double>(j + 1) / (samplesV - 1));
                const double vm = (v0 + v1) * 0.5;
                const auto mid  = evaluate(um, vm);
                const auto lin  = evaluate(u0, v0);
                const auto linB = evaluate(u1, v1);
                const float cx = (lin.x + linB.x) * 0.5f;
                const float cy = (lin.y + linB.y) * 0.5f;
                const float cz = (lin.z + linB.z) * 0.5f;
                const float dx = mid.x - cx, dy = mid.y - cy, dz = mid.z - cz;
                maxErr = std::max(maxErr, std::sqrt(dx*dx + dy*dy + dz*dz));
            }
        }
        if (maxErr <= tolerance) { break; }
        samplesU = std::min(samplesU * 2, 128u);
        samplesV = std::min(samplesV * 2, 128u);
    }

    return tessellate(samplesU, samplesV);
}

// ─── Iso-parameter curves ─────────────────────────────────────────────────────

std::optional<NurbsCurve> NurbsSurface::isoU(double uConst) const
{
    uConst = std::clamp(uConst, paramMinU(), paramMaxU());
    const uint32_t nv = m_data.nV, pv = m_data.degreeV, pu = m_data.degreeU, nu = m_data.nU;

    // Iso-u curve: C(v) = Σⱼ [Σᵢ Nᵢₚ(u) wᵢⱼ Pᵢⱼ / Σᵢ Nᵢₚ(u) wᵢⱼ] * Nⱼq(v)
    // Compute the effective control points for the iso-u curve.
    std::vector<nexus::render::Vec3> pts(nv);
    std::vector<double> weights(nv, 0.0);

    for (uint32_t j = 0; j < nv; ++j) {
        double wx = 0, wy = 0, wz = 0, wsum = 0;
        for (uint32_t i = 0; i < nu; ++i) {
            const double Nu = basisU(i, pu, uConst);
            const double w  = m_data.weights[i * nv + j];
            const double Nw = Nu * w;
            const auto& pt  = m_data.controlPoints[i * nv + j];
            wx += Nw * pt.x; wy += Nw * pt.y; wz += Nw * pt.z;
            wsum += Nw;
        }
        if (wsum < 1e-15) { pts[j] = {0, 0, 0}; weights[j] = 0.0; }
        else {
            pts[j] = {static_cast<float>(wx / wsum),
                      static_cast<float>(wy / wsum),
                      static_cast<float>(wz / wsum)};
            weights[j] = wsum;
        }
    }

    NurbsCurveData cd;
    cd.degree = pv;
    cd.controlPoints = std::move(pts);
    cd.weights = std::move(weights);
    cd.knots = m_data.knotsV;
    cd.knotType = NurbsKnotType::Clamped;
    return NurbsCurve::create(std::move(cd));
}

std::optional<NurbsCurve> NurbsSurface::isoV(double vConst) const
{
    vConst = std::clamp(vConst, paramMinV(), paramMaxV());
    const uint32_t nu = m_data.nU, pu = m_data.degreeU, pv = m_data.degreeV, nv = m_data.nV;

    std::vector<nexus::render::Vec3> pts(nu);
    std::vector<double> weights(nu, 0.0);

    for (uint32_t i = 0; i < nu; ++i) {
        double wx = 0, wy = 0, wz = 0, wsum = 0;
        for (uint32_t j = 0; j < nv; ++j) {
            const double Nv = basisV(j, pv, vConst);
            const double w  = m_data.weights[i * nv + j];
            const double Nw = Nv * w;
            const auto& pt  = m_data.controlPoints[i * nv + j];
            wx += Nw * pt.x; wy += Nw * pt.y; wz += Nw * pt.z;
            wsum += Nw;
        }
        if (wsum < 1e-15) { pts[i] = {0, 0, 0}; weights[i] = 0.0; }
        else {
            pts[i] = {static_cast<float>(wx / wsum),
                      static_cast<float>(wy / wsum),
                      static_cast<float>(wz / wsum)};
            weights[i] = wsum;
        }
    }

    NurbsCurveData cd;
    cd.degree = pu;
    cd.controlPoints = std::move(pts);
    cd.weights = std::move(weights);
    cd.knots = m_data.knotsU;
    cd.knotType = NurbsKnotType::Clamped;
    return NurbsCurve::create(std::move(cd));
}

// ─── Knot insertion ───────────────────────────────────────────────────────────

std::optional<NurbsSurface> NurbsSurface::insertKnotU(double u) const
{
    if (u < paramMinU() || u > paramMaxU()) { return std::nullopt; }

    const auto& t  = m_data.knotsU;
    const uint32_t p  = m_data.degreeU;
    const uint32_t nu = m_data.nU;
    const uint32_t nv = m_data.nV;

    // Find span k.
    uint32_t k = 0;
    for (uint32_t i = p; i < nu; ++i) {
        if (u >= t[i] && u < t[i + 1]) { k = i; break; }
        if (i == nu - 1) { k = nu - 1; }
    }
    if (u >= t.back()) { k = nu - 1; }

    // New knot vector.
    std::vector<double> newKnotsU;
    newKnotsU.reserve(t.size() + 1);
    for (uint32_t i = 0; i <= k; ++i)          { newKnotsU.push_back(t[i]); }
    newKnotsU.push_back(u);
    for (uint32_t i = k + 1; i < t.size(); ++i) { newKnotsU.push_back(t[i]); }

    const uint32_t newNu = nu + 1;

    // New control points: for each v-column, apply the 1-D knot insertion.
    std::vector<nexus::render::Vec3> newPts(newNu * nv);
    std::vector<double> newWeights(newNu * nv);

    for (uint32_t j = 0; j < nv; ++j) {
        for (uint32_t i = 0; i <= newNu - 1; ++i) {
            if (i <= k - p) {
                newPts[i * nv + j]     = m_data.controlPoints[i * nv + j];
                newWeights[i * nv + j] = m_data.weights[i * nv + j];
            } else if (i >= k + 1) {
                newPts[i * nv + j]     = m_data.controlPoints[(i - 1) * nv + j];
                newWeights[i * nv + j] = m_data.weights[(i - 1) * nv + j];
            } else {
                const double denom = t[i + p] - t[i];
                const double alpha = (denom < 1e-15) ? 0.0 : (u - t[i]) / denom;
                const double beta  = 1.0 - alpha;
                const auto& pa = m_data.controlPoints[(i - 1) * nv + j];
                const auto& pb = m_data.controlPoints[i * nv + j];
                const double wa = m_data.weights[(i - 1) * nv + j];
                const double wb = m_data.weights[i * nv + j];
                newPts[i * nv + j] = {
                    static_cast<float>(beta * pa.x + alpha * pb.x),
                    static_cast<float>(beta * pa.y + alpha * pb.y),
                    static_cast<float>(beta * pa.z + alpha * pb.z)};
                newWeights[i * nv + j] = beta * wa + alpha * wb;
            }
        }
    }

    NurbsSurfaceData nd = m_data;
    nd.nU            = newNu;
    nd.knotsU        = std::move(newKnotsU);
    nd.controlPoints = std::move(newPts);
    nd.weights       = std::move(newWeights);
    return NurbsSurface::create(std::move(nd));
}

std::optional<NurbsSurface> NurbsSurface::insertKnotV(double v) const
{
    if (v < paramMinV() || v > paramMaxV()) { return std::nullopt; }

    const auto& t  = m_data.knotsV;
    const uint32_t q  = m_data.degreeV;
    const uint32_t nu = m_data.nU;
    const uint32_t nv = m_data.nV;

    uint32_t k = 0;
    for (uint32_t j = q; j < nv; ++j) {
        if (v >= t[j] && v < t[j + 1]) { k = j; break; }
        if (j == nv - 1) { k = nv - 1; }
    }
    if (v >= t.back()) { k = nv - 1; }

    std::vector<double> newKnotsV;
    newKnotsV.reserve(t.size() + 1);
    for (uint32_t j = 0; j <= k; ++j)          { newKnotsV.push_back(t[j]); }
    newKnotsV.push_back(v);
    for (uint32_t j = k + 1; j < t.size(); ++j) { newKnotsV.push_back(t[j]); }

    const uint32_t newNv = nv + 1;

    std::vector<nexus::render::Vec3> newPts(nu * newNv);
    std::vector<double> newWeights(nu * newNv);

    for (uint32_t i = 0; i < nu; ++i) {
        for (uint32_t j = 0; j <= newNv - 1; ++j) {
            if (j <= k - q) {
                newPts[i * newNv + j]     = m_data.controlPoints[i * nv + j];
                newWeights[i * newNv + j] = m_data.weights[i * nv + j];
            } else if (j >= k + 1) {
                newPts[i * newNv + j]     = m_data.controlPoints[i * nv + (j - 1)];
                newWeights[i * newNv + j] = m_data.weights[i * nv + (j - 1)];
            } else {
                const double denom = t[j + q] - t[j];
                const double alpha = (denom < 1e-15) ? 0.0 : (v - t[j]) / denom;
                const double beta  = 1.0 - alpha;
                const auto& pa = m_data.controlPoints[i * nv + (j - 1)];
                const auto& pb = m_data.controlPoints[i * nv + j];
                const double wa = m_data.weights[i * nv + (j - 1)];
                const double wb = m_data.weights[i * nv + j];
                newPts[i * newNv + j] = {
                    static_cast<float>(beta * pa.x + alpha * pb.x),
                    static_cast<float>(beta * pa.y + alpha * pb.y),
                    static_cast<float>(beta * pa.z + alpha * pb.z)};
                newWeights[i * newNv + j] = beta * wa + alpha * wb;
            }
        }
    }

    NurbsSurfaceData nd = m_data;
    nd.nV            = newNv;
    nd.knotsV        = std::move(newKnotsV);
    nd.controlPoints = std::move(newPts);
    nd.weights       = std::move(newWeights);
    return NurbsSurface::create(std::move(nd));
}

} // namespace nexus::geometry
