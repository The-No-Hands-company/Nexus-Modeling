#include <nexus/geometry/NurbsCurve.h>

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

int32_t NurbsCurve::findSpan(float u) const noexcept {
    int32_t n = m_ctlCount;
    if (static_cast<int32_t>(m_knots.size()) <= m_degree) return m_degree;
    if (u >= m_knots[n]) return n - 1;
    if (u <= m_knots[m_degree]) return m_degree;
    int32_t low  = m_degree;
    int32_t high = n;
    int32_t mid  = (low + high) / 2;
    while (u < m_knots[mid] || u >= m_knots[mid + 1]) {
        if (u < m_knots[mid]) high = mid;
        else                   low  = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

void NurbsCurve::basisFuns(int32_t span, float u, std::span<float> out) const noexcept {
    int32_t p = m_degree;
    out[0] = 1.f;
    std::vector<float> left(static_cast<size_t>(p + 1));
    std::vector<float> right(static_cast<size_t>(p + 1));
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = u - m_knots[span + 1 - j];
        right[j] = m_knots[span + j] - u;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            float tmp = out[r] / (right[r + 1] + left[j - r]);
            out[r] = saved + right[r + 1] * tmp;
            saved  = left[j - r] * tmp;
        }
        out[j] = saved;
    }
}

Vec3 NurbsCurve::evaluate(float u) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t span = findSpan(u);
    std::vector<float> N(static_cast<size_t>(m_degree + 1));
    basisFuns(span, u, N);
    if (isRational()) {
        Vec3  sum{};
        float wSum = 0.f;
        for (int32_t i = 0; i <= m_degree; ++i) {
            int32_t idx = span - m_degree + i;
            float wB = N[i] * m_weights[idx];
            sum.x += wB * m_ctlPts[idx].x;
            sum.y += wB * m_ctlPts[idx].y;
            sum.z += wB * m_ctlPts[idx].z;
            wSum  += wB;
        }
        if (wSum > kEpsilon) {
            float inv = 1.f / wSum;
            return {sum.x * inv, sum.y * inv, sum.z * inv};
        }
        return sum;
    }
    Vec3 pt{};
    for (int32_t i = 0; i <= m_degree; ++i) {
        int32_t idx = span - m_degree + i;
        pt.x += N[i] * m_ctlPts[idx].x;
        pt.y += N[i] * m_ctlPts[idx].y;
        pt.z += N[i] * m_ctlPts[idx].z;
    }
    return pt;
}

Vec3 NurbsCurve::derivative(float u, int32_t order) const noexcept {
    if (!isValid() || order < 0) return Vec3{};
    if (order == 0) return evaluate(u);

    int32_t p = m_degree;
    int32_t span = findSpan(u);
    std::vector<float> N(static_cast<size_t>(p + 1));
    basisFuns(span, u, N);

    std::vector<Vec3> PK(static_cast<size_t>(p + 1));
    std::vector<float> WK;
    if (isRational()) WK.resize(static_cast<size_t>(p + 1));

    for (int32_t i = 0; i <= p; ++i) {
        int32_t idx = span - p + i;
        if (isRational()) {
            float w = m_weights[idx];
            PK[i] = {m_ctlPts[idx].x * w, m_ctlPts[idx].y * w, m_ctlPts[idx].z * w};
            WK[i] = w;
        } else {
            PK[i] = m_ctlPts[idx];
        }
    }

    for (int32_t k = 1; k <= order; ++k) {
        if (k > p) return Vec3{};
        for (int32_t i = 0; i <= p - k; ++i) {
            int32_t idx = span - p + i;
            float denom = m_knots[static_cast<size_t>(idx + p + 1)] -
                          m_knots[static_cast<size_t>(idx + k)];
            if (std::abs(denom) > kEpsilon) {
                float alpha = static_cast<float>(p - k + 1) / denom;
                PK[i].x = alpha * (PK[i + 1].x - PK[i].x);
                PK[i].y = alpha * (PK[i + 1].y - PK[i].y);
                PK[i].z = alpha * (PK[i + 1].z - PK[i].z);
                if (isRational())
                    WK[i] = alpha * (WK[i + 1] - WK[i]);
            } else {
                PK[i] = Vec3{};
                if (isRational()) WK[i] = 0.f;
            }
        }
    }

    if (!isRational()) return PK[0];

    Vec3 Cpt = evaluate(u);
    float wVal = 0.f;
    for (int32_t i = 0; i <= p; ++i) {
        int32_t idx = span - p + i;
        wVal += N[i] * m_weights[idx];
    }
    if (wVal <= kEpsilon) return Vec3{};

    float invW = 1.f / wVal;
    Vec3 A_deriv = PK[0];
    float w_deriv = WK[0];

    return {(A_deriv.x - w_deriv * Cpt.x) * invW,
            (A_deriv.y - w_deriv * Cpt.y) * invW,
            (A_deriv.z - w_deriv * Cpt.z) * invW};
}

NurbsCurve NurbsCurve::insertKnot(float u, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t p  = m_degree;
    int32_t n  = m_ctlCount;
    int32_t m  = n + p + 1;
    int32_t k  = findSpan(u);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knots[i] - u) < kEpsilon; --i) ++s;
    s = std::min(s, p);
    if (s == p) return *this;

    std::vector<float> newKnots;
    newKnots.reserve(static_cast<size_t>(m + r));
    for (int32_t i = 0; i <= k; ++i) newKnots.push_back(m_knots[i]);
    for (int32_t i = 0; i < r;  ++i) newKnots.push_back(u);
    for (int32_t i = k + 1; i < m; ++i) newKnots.push_back(m_knots[i]);

    std::vector<Vec3> newPts(static_cast<size_t>(n + r));
    for (int32_t i = 0; i <= k - p; ++i) newPts[i] = m_ctlPts[i];
    for (int32_t i = k - s; i < n; ++i) newPts[i + r] = m_ctlPts[i];

    std::vector<Vec3> Rw;
    for (int32_t i = 0; i <= p - s; ++i) Rw.push_back(m_ctlPts[k - p + i]);

    for (int32_t j = 1; j <= r; ++j) {
        int32_t L = k - p + j;
        std::vector<Vec3> nextRw(static_cast<size_t>(p - s - j + 1));
        for (int32_t i = 0; i <= p - s - j; ++i) {
            float denom = m_knots[i + k + 1] - m_knots[L + i];
            float alpha = (std::abs(denom) > kEpsilon)
                              ? (u - m_knots[L + i]) / denom
                              : 0.f;
            nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
            nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
            nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
        }
        newPts[L] = nextRw[0];
        if (j < r) newPts[k + r - j - s] = nextRw[p - s - j];
        Rw = std::move(nextRw);
    }
    for (int32_t i = 1; i <= p - s - r; ++i) newPts[k - p + r + i] = Rw[i];

    bool rational = isRational();
    NurbsCurve result(m_degree, std::move(newKnots), std::move(newPts));
    if (rational) {
        std::vector<float> newWeights(static_cast<size_t>(n + r));
        for (int32_t i = 0; i <= k - p; ++i) newWeights[i] = m_weights[i];
        for (int32_t i = k - s; i < n; ++i) newWeights[i + r] = m_weights[i];

        std::vector<float> WRw;
        for (int32_t i = 0; i <= p - s; ++i) WRw.push_back(m_weights[k - p + i]);
        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - p + j;
            std::vector<float> nextWRw(static_cast<size_t>(p - s - j + 1));
            for (int32_t i = 0; i <= p - s - j; ++i) {
                float denom = m_knots[i + k + 1] - m_knots[L + i];
                float alpha = (std::abs(denom) > kEpsilon)
                                  ? (u - m_knots[L + i]) / denom
                                  : 0.f;
                nextWRw[i] = alpha * WRw[i + 1] + (1.f - alpha) * WRw[i];
            }
            newWeights[L] = nextWRw[0];
            if (j < r) newWeights[k + r - j - s] = nextWRw[p - s - j];
            WRw = std::move(nextWRw);
        }
        for (int32_t i = 1; i <= p - s - r; ++i) newWeights[k - p + r + i] = WRw[i];
        result.setWeights(std::move(newWeights));
    }
    return result;
}

} // namespace nexus::geometry
