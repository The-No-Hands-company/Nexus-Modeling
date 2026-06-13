#pragma once
// ─── Nexus Geometry ── NurbsSurface ──────────────────────────────────

#include <nexus/render/Camera.h>

#include <cstdint>
#include <span>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

class NurbsSurface {
public:
    NurbsSurface() = default;

    NurbsSurface(int32_t degreeU, int32_t degreeV,
                 std::vector<float> knotU, std::vector<float> knotV,
                 std::vector<Vec3> ctl, int32_t nU, int32_t nV)
        : m_degU(degreeU), m_degV(degreeV),
          m_nU(nU), m_nV(nV),
          m_knotU(std::move(knotU)), m_knotV(std::move(knotV)),
          m_ctlPts(std::move(ctl)) {}

    NurbsSurface(int32_t degreeU, int32_t degreeV,
                 std::vector<float> knotU, std::vector<float> knotV,
                 std::vector<Vec3> ctl, int32_t nU, int32_t nV,
                 std::vector<float> w)
        : m_degU(degreeU), m_degV(degreeV),
          m_nU(nU), m_nV(nV),
          m_knotU(std::move(knotU)), m_knotV(std::move(knotV)),
          m_weights(std::move(w)), m_ctlPts(std::move(ctl)) {}

    [[nodiscard]] int32_t degreeU() const noexcept { return m_degU; }
    [[nodiscard]] int32_t degreeV() const noexcept { return m_degV; }

    [[nodiscard]] int32_t controlPointCountU() const noexcept { return m_nU; }
    [[nodiscard]] int32_t controlPointCountV() const noexcept { return m_nV; }

    [[nodiscard]] const std::vector<float>& knotU() const noexcept { return m_knotU; }
    [[nodiscard]] const std::vector<float>& knotV() const noexcept { return m_knotV; }

    [[nodiscard]] const std::vector<Vec3>&  controlPoints() const noexcept { return m_ctlPts; }
    [[nodiscard]] const std::vector<float>& weights() const noexcept { return m_weights; }
    [[nodiscard]] bool isRational() const noexcept { return !m_weights.empty(); }

    [[nodiscard]] Vec3 controlPoint(int32_t i, int32_t j) const noexcept {
        return m_ctlPts[static_cast<size_t>(i * m_nV + j)];
    }

    [[nodiscard]] bool isValid() const noexcept {
        if (m_degU < 1 || m_degV < 1) return false;
        if (static_cast<int32_t>(m_knotU.size()) != m_nU + m_degU + 1) return false;
        if (static_cast<int32_t>(m_knotV.size()) != m_nV + m_degV + 1) return false;
        if (static_cast<int32_t>(m_ctlPts.size()) != m_nU * m_nV) return false;
        if (!m_weights.empty() &&
            static_cast<int32_t>(m_weights.size()) != m_nU * m_nV) return false;
        return true;
    }

    [[nodiscard]] int32_t findSpanU(float u) const noexcept;
    [[nodiscard]] int32_t findSpanV(float v) const noexcept;

    [[nodiscard]] Vec3 evaluate(float u, float v) const noexcept;
    [[nodiscard]] Vec3 derivativeU(float u, float v) const noexcept;
    [[nodiscard]] Vec3 derivativeV(float u, float v) const noexcept;

    [[nodiscard]] NurbsSurface insertKnotU(float u, int32_t r = 1) const;
    [[nodiscard]] NurbsSurface insertKnotV(float v, int32_t r = 1) const;

    [[nodiscard]] std::pair<float, float> domainU() const noexcept {
        if (m_knotU.empty()) return {0.f, 1.f};
        return {m_knotU[static_cast<size_t>(m_degU)],
                m_knotU[m_knotU.size() - 1 - static_cast<size_t>(m_degU)]};
    }
    [[nodiscard]] std::pair<float, float> domainV() const noexcept {
        if (m_knotV.empty()) return {0.f, 1.f};
        return {m_knotV[static_cast<size_t>(m_degV)],
                m_knotV[m_knotV.size() - 1 - static_cast<size_t>(m_degV)]};
    }

private:
    int32_t            m_degU  = 3;
    int32_t            m_degV  = 3;
    int32_t            m_nU    = 0;
    int32_t            m_nV    = 0;
    std::vector<float> m_knotU;
    std::vector<float> m_knotV;
    std::vector<float> m_weights;
    std::vector<Vec3>  m_ctlPts;
};

} // namespace nexus::geometry
