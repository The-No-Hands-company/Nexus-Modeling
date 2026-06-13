#pragma once
// ─── Nexus Geometry ── NurbsCurve ─────────────────────────────────────

#include <nexus/render/Camera.h>

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

class NurbsCurve {
public:
    NurbsCurve() = default;

    NurbsCurve(int32_t deg, std::vector<float> knots, std::vector<Vec3> ctl)
        : m_degree(deg), m_knots(std::move(knots)), m_ctlPts(std::move(ctl))
    {
        m_ctlCount = static_cast<int32_t>(m_ctlPts.size());
    }

    NurbsCurve(int32_t deg, std::vector<float> knots, std::vector<Vec3> ctl,
               std::vector<float> w)
        : m_degree(deg), m_knots(std::move(knots)), m_ctlPts(std::move(ctl)),
          m_weights(std::move(w))
    {
        m_ctlCount = static_cast<int32_t>(m_ctlPts.size());
    }

    void setDegree(int32_t d) noexcept { m_degree = d; }
    [[nodiscard]] int32_t degree() const noexcept { return m_degree; }

    void setKnots(std::vector<float> k) { m_knots = std::move(k); }
    [[nodiscard]] const std::vector<float>& knots() const noexcept { return m_knots; }

    void setControlPoints(std::vector<Vec3> ctl) {
        m_ctlPts = std::move(ctl);
        m_ctlCount = static_cast<int32_t>(m_ctlPts.size());
    }
    [[nodiscard]] const std::vector<Vec3>& controlPoints() const noexcept { return m_ctlPts; }
    [[nodiscard]] int32_t controlPointCount() const noexcept { return m_ctlCount; }

    void setWeights(std::vector<float> w) { m_weights = std::move(w); }
    [[nodiscard]] const std::vector<float>& weights() const noexcept { return m_weights; }
    [[nodiscard]] bool isRational() const noexcept { return !m_weights.empty(); }

    [[nodiscard]] bool isValid() const noexcept {
        if (m_degree < 1) return false;
        int32_t n = m_ctlCount;
        if (static_cast<int32_t>(m_knots.size()) != n + m_degree + 1) return false;
        if (n < m_degree + 1) return false;
        if (!m_weights.empty() && static_cast<int32_t>(m_weights.size()) != n) return false;
        return true;
    }

    [[nodiscard]] int32_t findSpan(float u) const noexcept;
    void basisFuns(int32_t span, float u,
                   std::span<float> out) const noexcept;

    [[nodiscard]] Vec3 evaluate(float u) const noexcept;
    [[nodiscard]] Vec3 derivative(float u, int32_t order = 1) const noexcept;

    [[nodiscard]] NurbsCurve insertKnot(float u, int32_t r = 1) const;

    [[nodiscard]] std::pair<float, float> domain() const noexcept {
        if (m_knots.empty()) return {0.f, 1.f};
        return {m_knots[static_cast<size_t>(m_degree)],
                m_knots[m_knots.size() - 1 - static_cast<size_t>(m_degree)]};
    }

private:
    int32_t            m_degree   = 3;
    int32_t            m_ctlCount = 0;
    std::vector<float> m_knots;
    std::vector<Vec3>  m_ctlPts;
    std::vector<float> m_weights;
};

} // namespace nexus::geometry
