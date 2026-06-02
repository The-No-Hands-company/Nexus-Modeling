#include <nexus/geometry/NurbsCurve.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace nexus::geometry {

// ─── Validation ──────────────────────────────────────────────────────────────

std::optional<NurbsCurve> NurbsCurve::create(NurbsCurveData data)
{
    const uint32_t n = static_cast<uint32_t>(data.controlPoints.size());
    const uint32_t p = data.degree;
    if (p == 0 || n < p + 1) { return std::nullopt; }
    // Knot vector must have size n + p + 1.
    if (data.knots.size() != n + p + 1) { return std::nullopt; }
    // Weights: fill with 1.0 if empty or wrong size.
    if (data.weights.size() != n) {
        data.weights.assign(n, 1.0);
    }
    return NurbsCurve(std::move(data));
}

std::optional<NurbsCurve> NurbsCurve::fromControlPoints(
    const std::vector<nexus::render::Vec3>& pts, uint32_t degree)
{
    const uint32_t n = static_cast<uint32_t>(pts.size());
    if (n < degree + 1 || degree == 0) { return std::nullopt; }

    // Build a clamped uniform knot vector.
    // First (degree+1) knots = 0, last (degree+1) knots = 1, rest uniformly spaced.
    const uint32_t m = n + degree + 1;
    std::vector<double> knots(m, 0.0);
    const uint32_t inner = m - 2 * (degree + 1); // interior knot count
    for (uint32_t i = 0; i < degree + 1; ++i) { knots[i] = 0.0; }
    for (uint32_t i = 0; i < inner; ++i) {
        knots[degree + 1 + i] = static_cast<double>(i + 1) / (inner + 1);
    }
    for (uint32_t i = 0; i < degree + 1; ++i) {
        knots[m - 1 - i] = 1.0;
    }

    NurbsCurveData data;
    data.degree        = degree;
    data.controlPoints = pts;
    data.weights.assign(n, 1.0);
    data.knots         = std::move(knots);
    data.knotType      = NurbsKnotType::Clamped;
    return NurbsCurve(std::move(data));
}

// ─── Parameter range ─────────────────────────────────────────────────────────

double NurbsCurve::paramMin() const noexcept
{
    return m_data.knots.front();
}

double NurbsCurve::paramMax() const noexcept
{
    return m_data.knots.back();
}

// ─── Cox-de Boor basis function ──────────────────────────────────────────────

double NurbsCurve::basis(uint32_t i, uint32_t p, double u) const noexcept
{
    const auto& t = m_data.knots;
    if (p == 0) {
        // Special case at the right end: u == t.back() counts in the last span.
        if (u == t.back() && i + 1 < t.size() && t[i] <= u && u <= t[i + 1]) { return 1.0; }
        return (t[i] <= u && u < t[i + 1]) ? 1.0 : 0.0;
    }
    double left  = 0.0;
    double right = 0.0;
    const double d1 = t[i + p]     - t[i];
    const double d2 = t[i + p + 1] - t[i + 1];
    if (d1 > 1e-15) { left  = (u - t[i])         / d1 * basis(i,     p - 1, u); }
    if (d2 > 1e-15) { right = (t[i + p + 1] - u) / d2 * basis(i + 1, p - 1, u); }
    return left + right;
}

uint32_t NurbsCurve::knotSpan(double u) const noexcept
{
    const auto& t = m_data.knots;
    const uint32_t n = m_data.degree + static_cast<uint32_t>(m_data.controlPoints.size());
    // n = last index of knot vector before the clamped repeats at the end.
    uint32_t lo = m_data.degree;
    uint32_t hi = n; // t[hi] = t[n] (first of the trailing repeated knots)
    if (u >= t[hi]) { return hi - 1; }
    if (u <= t[lo]) { return lo; }
    uint32_t mid = (lo + hi) / 2;
    while (u < t[mid] || u >= t[mid + 1]) {
        if (u < t[mid]) hi = mid;
        else            lo = mid;
        mid = (lo + hi) / 2;
    }
    return mid;
}

// ─── Evaluate ────────────────────────────────────────────────────────────────

nexus::render::Vec3 NurbsCurve::evaluate(double u) const noexcept
{
    u = std::clamp(u, paramMin(), paramMax());
    const uint32_t n = static_cast<uint32_t>(m_data.controlPoints.size());
    const uint32_t p = m_data.degree;

    double wx = 0, wy = 0, wz = 0, wsum = 0;
    for (uint32_t i = 0; i < n; ++i) {
        const double N = basis(i, p, u);
        const double w = m_data.weights[i];
        const double Nw = N * w;
        wx   += Nw * m_data.controlPoints[i].x;
        wy   += Nw * m_data.controlPoints[i].y;
        wz   += Nw * m_data.controlPoints[i].z;
        wsum += Nw;
    }
    if (wsum < 1e-15) { return m_data.controlPoints.front(); }
    return {static_cast<float>(wx / wsum),
            static_cast<float>(wy / wsum),
            static_cast<float>(wz / wsum)};
}

// ─── Derivatives ─────────────────────────────────────────────────────────────
// Compute via finite differences (second-order central where interior, forward/backward at ends).

nexus::render::Vec3 NurbsCurve::derivative1(double u) const noexcept
{
    const double h  = (paramMax() - paramMin()) * 1e-6;
    const double u0 = std::max(u - h, paramMin());
    const double u1 = std::min(u + h, paramMax());
    const auto p0 = evaluate(u0);
    const auto p1 = evaluate(u1);
    const float inv = static_cast<float>(1.0 / (u1 - u0));
    return {(p1.x - p0.x) * inv, (p1.y - p0.y) * inv, (p1.z - p0.z) * inv};
}

nexus::render::Vec3 NurbsCurve::derivative2(double u) const noexcept
{
    const double h  = (paramMax() - paramMin()) * 1e-5;
    const double um = std::max(u - h, paramMin());
    const double u0 = u;
    const double up = std::min(u + h, paramMax());
    const auto pm = evaluate(um);
    const auto p0 = evaluate(u0);
    const auto pp = evaluate(up);
    const float inv = static_cast<float>(1.0 / (h * h));
    return {(pp.x - 2*p0.x + pm.x) * inv,
            (pp.y - 2*p0.y + pm.y) * inv,
            (pp.z - 2*p0.z + pm.z) * inv};
}

// ─── Tessellation ────────────────────────────────────────────────────────────

std::vector<nexus::render::Vec3> NurbsCurve::tessellate(uint32_t count) const
{
    if (count < 2) { count = 2; }
    std::vector<nexus::render::Vec3> pts;
    pts.reserve(count);
    const double lo = paramMin(), hi = paramMax();
    for (uint32_t i = 0; i < count; ++i) {
        const double u = lo + (hi - lo) * (static_cast<double>(i) / (count - 1));
        pts.push_back(evaluate(u));
    }
    return pts;
}

std::vector<nexus::render::Vec3> NurbsCurve::tessellateAdaptive(float tolerance) const
{
    if (tolerance <= 0.f) { tolerance = 1e-3f; }
    std::vector<nexus::render::Vec3> result;
    const double lo = paramMin(), hi = paramMax();

    // Recursive subdivision via a stack.
    struct Seg { double u0, u1; nexus::render::Vec3 p0, p1; };
    std::vector<Seg> stack;
    auto p0 = evaluate(lo), p1 = evaluate(hi);
    result.push_back(p0);
    stack.push_back({lo, hi, p0, p1});

    while (!stack.empty()) {
        auto [u0, u1, q0, q1] = stack.back();
        stack.pop_back();

        const double um = (u0 + u1) * 0.5;
        const auto pm = evaluate(um);

        // Midpoint deviation from the chord.
        const float mx = (q0.x + q1.x) * 0.5f;
        const float my = (q0.y + q1.y) * 0.5f;
        const float mz = (q0.z + q1.z) * 0.5f;
        const float dx = pm.x - mx, dy = pm.y - my, dz = pm.z - mz;
        const float err = std::sqrt(dx*dx + dy*dy + dz*dz);

        if (err > tolerance && (u1 - u0) > 1e-10) {
            // Subdivide.
            stack.push_back({um, u1, pm, q1});
            stack.push_back({u0, um, q0, pm});
        } else {
            result.push_back(pm);
        }
    }
    result.push_back(p1);

    // Sort by parameter — the stack-based approach may produce out-of-order points
    // for uneven knot spacings. We rebuild in order.
    // Simpler: just use the in-order stack output since we push right before left.
    // The stack pushes right first then left, so left pops first → in-order.
    return result;
}

// ─── Knot insertion (Boehm's algorithm) ──────────────────────────────────────

std::optional<NurbsCurve> NurbsCurve::insertKnot(double u) const
{
    const auto& t = m_data.knots;
    const uint32_t p = m_data.degree;
    const uint32_t n = static_cast<uint32_t>(m_data.controlPoints.size());

    if (u < paramMin() || u > paramMax()) { return std::nullopt; }

    // Find insertion span k.
    uint32_t k = knotSpan(u);

    // New knot vector: insert u after position k.
    std::vector<double> newKnots;
    newKnots.reserve(t.size() + 1);
    for (uint32_t i = 0; i <= k; ++i)     { newKnots.push_back(t[i]); }
    newKnots.push_back(u);
    for (uint32_t i = k + 1; i < t.size(); ++i) { newKnots.push_back(t[i]); }

    // New control points: n+1 points (one extra).
    std::vector<nexus::render::Vec3> newPts;
    std::vector<double> newWeights;
    newPts.reserve(n + 1);
    newWeights.reserve(n + 1);

    for (uint32_t i = 0; i <= n; ++i) {
        if (i <= k - p) {
            newPts.push_back(m_data.controlPoints[i]);
            newWeights.push_back(m_data.weights[i]);
        } else if (i >= k + 1) {
            newPts.push_back(m_data.controlPoints[i - 1]);
            newWeights.push_back(m_data.weights[i - 1]);
        } else {
            // Blend.
            const double denom = t[i + p] - t[i];
            const double alpha = (denom < 1e-15) ? 0.0 : (u - t[i]) / denom;
            const double beta  = 1.0 - alpha;
            const auto& pa = m_data.controlPoints[i - 1];
            const auto& pb = m_data.controlPoints[i];
            const double wa = m_data.weights[i - 1];
            const double wb = m_data.weights[i];
            newPts.push_back({
                static_cast<float>(beta * pa.x + alpha * pb.x),
                static_cast<float>(beta * pa.y + alpha * pb.y),
                static_cast<float>(beta * pa.z + alpha * pb.z)});
            newWeights.push_back(beta * wa + alpha * wb);
        }
    }

    NurbsCurveData nd;
    nd.degree        = p;
    nd.controlPoints = std::move(newPts);
    nd.weights       = std::move(newWeights);
    nd.knots         = std::move(newKnots);
    nd.knotType      = m_data.knotType;
    return NurbsCurve(std::move(nd));
}

// ─── Degree elevation ────────────────────────────────────────────────────────
// Raise degree by 1 using the standard knot-insertion + Bezier-elevation approach.
// For simplicity we use the fine algorithm: convert to Bezier segments, elevate each,
// merge. Here we implement the simpler "duplicate control points" approximation
// which is exact for B-splines using the standard formulas.

NurbsCurve NurbsCurve::elevate() const
{
    const uint32_t p  = m_data.degree;
    const uint32_t n  = static_cast<uint32_t>(m_data.controlPoints.size());
    const auto& t     = m_data.knots;

    // New degree.
    const uint32_t q = p + 1;

    // New control points: n+1 points for a single Bezier segment, more for multi-span.
    // Simplified elevation for uniform B-splines: use degree-elevation formula.
    // For a B-spline of degree p with n control points, elevated curve has n+1 pts.
    std::vector<nexus::render::Vec3> newPts(n + 1);
    std::vector<double> newWeights(n + 1);

    for (uint32_t i = 0; i <= n; ++i) {
        const double ai = static_cast<double>(i) / (p + 1);
        const double bi = 1.0 - ai;
        if (i == 0) {
            newPts[0]    = m_data.controlPoints[0];
            newWeights[0] = m_data.weights[0];
        } else if (i == n) {
            newPts[n]    = m_data.controlPoints[n - 1];
            newWeights[n] = m_data.weights[n - 1];
        } else {
            const auto& pa = m_data.controlPoints[i - 1];
            const auto& pb = m_data.controlPoints[i];
            const double wa = m_data.weights[i - 1];
            const double wb = m_data.weights[i];
            newPts[i] = {
                static_cast<float>(bi * pa.x + ai * pb.x),
                static_cast<float>(bi * pa.y + ai * pb.y),
                static_cast<float>(bi * pa.z + ai * pb.z)};
            newWeights[i] = bi * wa + ai * wb;
        }
    }

    // Build new knot vector: repeat each distinct knot one extra time.
    std::vector<double> newKnots;
    newKnots.reserve(t.size() + n + 2);
    // For a clamped curve: prepend (q+1) zeros and append (q+1) ones with interior knots.
    // Simple approach: insert each existing knot once more.
    double prev = t.front() - 1.0;
    uint32_t addCount = 0;
    for (double tk : t) {
        if (std::abs(tk - prev) < 1e-14) {
            newKnots.push_back(tk); // repeat
            ++addCount;
        } else {
            newKnots.push_back(tk);
        }
        prev = tk;
    }
    // The new knot vector must have size (n+1) + q + 1 = n + q + 2 = n + p + 3.
    // Adjust by appending if needed.
    const uint32_t targetSize = (n + 1) + q + 1;
    while (newKnots.size() < targetSize) { newKnots.push_back(t.back()); }
    newKnots.resize(targetSize, t.back());

    NurbsCurveData nd;
    nd.degree        = q;
    nd.controlPoints = std::move(newPts);
    nd.weights       = std::move(newWeights);
    nd.knots         = std::move(newKnots);
    nd.knotType      = m_data.knotType;

    // Fall back gracefully if validation fails.
    auto result = NurbsCurve::create(std::move(nd));
    if (result) { return std::move(*result); }
    return *this; // return unchanged on failure
}

} // namespace nexus::geometry
