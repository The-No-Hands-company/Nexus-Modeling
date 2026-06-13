#include <nexus/geometry/NurbsSurface.h>
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

void basisFunsInternal(int32_t p, const std::vector<float>& knots,
                       int32_t span, float t, std::span<float> out) {
    out[0] = 1.f;
    std::vector<float> left(static_cast<size_t>(p + 1));
    std::vector<float> right(static_cast<size_t>(p + 1));
    for (int32_t j = 1; j <= p; ++j) {
        left[j]  = t - knots[span + 1 - j];
        right[j] = knots[span + j] - t;
        float saved = 0.f;
        for (int32_t r = 0; r < j; ++r) {
            float tmp  = out[r] / (right[r + 1] + left[j - r]);
            out[r] = saved + right[r + 1] * tmp;
            saved  = left[j - r] * tmp;
        }
        out[j] = saved;
    }
}

int32_t findSpanInternal(int32_t n, int32_t p, const std::vector<float>& knots,
                         float t) {
    if (t >= knots[n]) return n - 1;
    if (t <= knots[p]) return p;
    int32_t low = p, high = n, mid = (low + high) / 2;
    while (t < knots[mid] || t >= knots[mid + 1]) {
        if (t < knots[mid]) high = mid; else low = mid;
        mid = (low + high) / 2;
    }
    return mid;
}

} // namespace

int32_t NurbsSurface::findSpanU(float u) const noexcept {
    return findSpanInternal(m_nU, m_degU, m_knotU, u);
}

int32_t NurbsSurface::findSpanV(float v) const noexcept {
    return findSpanInternal(m_nV, m_degV, m_knotV, v);
}

Vec3 NurbsSurface::evaluate(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t su = findSpanU(u);
    int32_t sv = findSpanV(v);
    int32_t pu = m_degU, pv = m_degV;
    std::vector<float> Nu(static_cast<size_t>(pu + 1));
    std::vector<float> Nv(static_cast<size_t>(pv + 1));
    basisFunsInternal(pu, m_knotU, su, u, Nu);
    basisFunsInternal(pv, m_knotV, sv, v, Nv);

    if (isRational()) {
        Vec3  sum{};
        float wSum = 0.f;
        for (int32_t i = 0; i <= pu; ++i) {
            int32_t ci = su - pu + i;
            for (int32_t j = 0; j <= pv; ++j) {
                int32_t cj  = sv - pv + j;
                size_t  idx = static_cast<size_t>(ci * m_nV + cj);
                float wB = Nu[i] * Nv[j] * m_weights[idx];
                sum.x += wB * m_ctlPts[idx].x;
                sum.y += wB * m_ctlPts[idx].y;
                sum.z += wB * m_ctlPts[idx].z;
                wSum  += wB;
            }
        }
        if (wSum > kEpsilon) {
            float inv = 1.f / wSum;
            return {sum.x * inv, sum.y * inv, sum.z * inv};
        }
        return sum;
    }

    Vec3 pt{};
    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j <= pv; ++j) {
            int32_t cj  = sv - pv + j;
            float bf = Nu[i] * Nv[j];
            size_t idx = static_cast<size_t>(ci * m_nV + cj);
            pt.x += bf * m_ctlPts[idx].x;
            pt.y += bf * m_ctlPts[idx].y;
            pt.z += bf * m_ctlPts[idx].z;
        }
    }
    return pt;
}

Vec3 NurbsSurface::derivativeU(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t pu = m_degU, pv = m_degV;
    int32_t su = findSpanU(u);
    int32_t sv = findSpanV(v);
    std::vector<float> Nu(static_cast<size_t>(pu + 1));
    std::vector<float> Nv(static_cast<size_t>(pv + 1));
    basisFunsInternal(pu, m_knotU, su, u, Nu);
    basisFunsInternal(pv, m_knotV, sv, v, Nv);

    Vec3 dAu{};
    float dwu = 0.f;

    for (int32_t j = 0; j <= pv; ++j) {
        int32_t cj = sv - pv + j;
        for (int32_t i = 0; i < pu; ++i) {
            float denom = m_knotU[static_cast<size_t>(su + i + 1)] -
                          m_knotU[static_cast<size_t>(su - pu + i + 1)];
            float alpha = (std::abs(denom) > kEpsilon) ? static_cast<float>(pu) / denom : 0.f;
            int32_t idx0 = su - pu + i;
            int32_t idx1 = su - pu + i + 1;
            float   bf   = alpha * Nv[j];
            size_t  i0   = static_cast<size_t>(idx0 * m_nV + cj);
            size_t  i1   = static_cast<size_t>(idx1 * m_nV + cj);
            dAu.x += bf * (m_ctlPts[i1].x - m_ctlPts[i0].x);
            dAu.y += bf * (m_ctlPts[i1].y - m_ctlPts[i0].y);
            dAu.z += bf * (m_ctlPts[i1].z - m_ctlPts[i0].z);
        }
    }

    if (!isRational()) return dAu;

    for (int32_t j = 0; j <= pv; ++j) {
        int32_t cj = sv - pv + j;
        for (int32_t i = 0; i < pu; ++i) {
            float denom = m_knotU[static_cast<size_t>(su + i + 1)] -
                          m_knotU[static_cast<size_t>(su - pu + i + 1)];
            float alpha = (std::abs(denom) > kEpsilon) ? static_cast<float>(pu) / denom : 0.f;
            int32_t idx0 = su - pu + i;
            int32_t idx1 = su - pu + i + 1;
            float bf = alpha * Nv[j];
            size_t i0 = static_cast<size_t>(idx0 * m_nV + cj);
            size_t i1 = static_cast<size_t>(idx1 * m_nV + cj);
            dwu += bf * (m_weights[i1] - m_weights[i0]);
        }
    }

    Vec3 S = evaluate(u, v);
    float wSum = 0.f;
    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j <= pv; ++j) {
            int32_t cj = sv - pv + j;
            wSum += Nu[i] * Nv[j] * m_weights[static_cast<size_t>(ci * m_nV + cj)];
        }
    }
    if (std::abs(wSum) <= kEpsilon) return Vec3{};
    float invW = 1.f / wSum;
    return {(dAu.x - dwu * S.x) * invW,
            (dAu.y - dwu * S.y) * invW,
            (dAu.z - dwu * S.z) * invW};
}

Vec3 NurbsSurface::derivativeV(float u, float v) const noexcept {
    if (!isValid()) return Vec3{};
    int32_t pu = m_degU, pv = m_degV;
    int32_t su = findSpanU(u);
    int32_t sv = findSpanV(v);
    std::vector<float> Nu(static_cast<size_t>(pu + 1));
    std::vector<float> Nv(static_cast<size_t>(pv + 1));
    basisFunsInternal(pu, m_knotU, su, u, Nu);
    basisFunsInternal(pv, m_knotV, sv, v, Nv);

    Vec3 dAv{};
    float dwv = 0.f;

    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j < pv; ++j) {
            float d = m_knotV[static_cast<size_t>(sv + j + 1)] -
                      m_knotV[static_cast<size_t>(sv - pv + j + 1)];
            float alpha = (std::abs(d) > kEpsilon) ? static_cast<float>(pv) / d : 0.f;
            int32_t cj0 = sv - pv + j;
            int32_t cj1 = sv - pv + j + 1;
            float   bf  = Nu[i] * alpha;
            size_t  i0  = static_cast<size_t>(ci * m_nV + cj0);
            size_t  i1  = static_cast<size_t>(ci * m_nV + cj1);
            dAv.x += bf * (m_ctlPts[i1].x - m_ctlPts[i0].x);
            dAv.y += bf * (m_ctlPts[i1].y - m_ctlPts[i0].y);
            dAv.z += bf * (m_ctlPts[i1].z - m_ctlPts[i0].z);
        }
    }

    if (!isRational()) return dAv;

    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j < pv; ++j) {
            float d = m_knotV[static_cast<size_t>(sv + j + 1)] -
                      m_knotV[static_cast<size_t>(sv - pv + j + 1)];
            float alpha = (std::abs(d) > kEpsilon) ? static_cast<float>(pv) / d : 0.f;
            int32_t cj0 = sv - pv + j;
            int32_t cj1 = sv - pv + j + 1;
            float bf = Nu[i] * alpha;
            size_t i0 = static_cast<size_t>(ci * m_nV + cj0);
            size_t i1 = static_cast<size_t>(ci * m_nV + cj1);
            dwv += bf * (m_weights[i1] - m_weights[i0]);
        }
    }

    Vec3 S = evaluate(u, v);
    float wSum = 0.f;
    for (int32_t i = 0; i <= pu; ++i) {
        int32_t ci = su - pu + i;
        for (int32_t j = 0; j <= pv; ++j) {
            int32_t cj = sv - pv + j;
            wSum += Nu[i] * Nv[j] * m_weights[static_cast<size_t>(ci * m_nV + cj)];
        }
    }
    if (std::abs(wSum) <= kEpsilon) return Vec3{};
    float invW = 1.f / wSum;
    return {(dAv.x - dwv * S.x) * invW,
            (dAv.y - dwv * S.y) * invW,
            (dAv.z - dwv * S.z) * invW};
}

NurbsSurface NurbsSurface::insertKnotU(float u, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t pu = m_degU, pv = m_degV;
    int32_t nu = m_nU, nv = m_nV;
    int32_t mu = nu + pu + 1;
    int32_t k  = findSpanU(u);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knotU[i] - u) < 1e-10f; --i) ++s;
    s = std::min(s, pu);
    if (s == pu) return *this;

    std::vector<float> newKnotU;
    newKnotU.reserve(static_cast<size_t>(mu + r));
    for (int32_t i = 0; i <= k; ++i) newKnotU.push_back(m_knotU[i]);
    for (int32_t i = 0; i < r;  ++i) newKnotU.push_back(u);
    for (int32_t i = k + 1; i < mu; ++i) newKnotU.push_back(m_knotU[i]);

    int32_t newNu = nu + r;
    std::vector<Vec3> newPts(static_cast<size_t>(newNu * nv));

    for (int32_t row = 0; row < nv; ++row) {
        std::vector<Vec3> colPts(static_cast<size_t>(nu));
        for (int32_t ci = 0; ci < nu; ++ci)
            colPts[ci] = m_ctlPts[static_cast<size_t>(ci * nv + row)];

        std::vector<Vec3> newColPts(static_cast<size_t>(newNu));
        for (int32_t i = 0; i <= k - pu; ++i) newColPts[i] = colPts[i];
        for (int32_t i = k - s; i < nu; ++i) newColPts[i + r] = colPts[i];

        std::vector<Vec3> Rw;
        for (int32_t i = 0; i <= pu - s; ++i) Rw.push_back(colPts[k - pu + i]);

        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - pu + j;
            std::vector<Vec3> nextRw(static_cast<size_t>(pu - s - j + 1));
            for (int32_t i = 0; i <= pu - s - j; ++i) {
                float denom = m_knotU[i + k + 1] - m_knotU[L + i];
                float alpha = (std::abs(denom) > 1e-10f)
                                  ? (u - m_knotU[L + i]) / denom
                                  : 0.f;
                nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
                nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
                nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
            }
            newColPts[L] = nextRw[0];
            if (j < r) newColPts[k + r - j - s] = nextRw[pu - s - j];
            Rw = std::move(nextRw);
        }
        for (int32_t i = 1; i <= pu - s - r; ++i) newColPts[k - pu + r + i] = Rw[i];

        for (int32_t ci = 0; ci < newNu; ++ci)
            newPts[static_cast<size_t>(ci * nv + row)] = newColPts[ci];
    }

    if (isRational()) {
        std::vector<float> newWts(static_cast<size_t>(newNu * nv));
        for (int32_t row = 0; row < nv; ++row) {
            std::vector<float> colWt(static_cast<size_t>(nu));
            for (int32_t ci = 0; ci < nu; ++ci)
                colWt[ci] = m_weights[static_cast<size_t>(ci * nv + row)];

            std::vector<float> newColWt(static_cast<size_t>(newNu));
            for (int32_t i = 0; i <= k - pu; ++i) newColWt[i] = colWt[i];
            for (int32_t i = k - s; i < nu; ++i) newColWt[i + r] = colWt[i];

            std::vector<float> Rw;
            for (int32_t i = 0; i <= pu - s; ++i) Rw.push_back(colWt[k - pu + i]);
            for (int32_t j = 1; j <= r; ++j) {
                int32_t L = k - pu + j;
                std::vector<float> nextRw(static_cast<size_t>(pu - s - j + 1));
                for (int32_t i = 0; i <= pu - s - j; ++i) {
                    float denom = m_knotU[i + k + 1] - m_knotU[L + i];
                    float alpha = (std::abs(denom) > 1e-10f)
                                      ? (u - m_knotU[L + i]) / denom
                                      : 0.f;
                    nextRw[i] = alpha * Rw[i + 1] + (1.f - alpha) * Rw[i];
                }
                newColWt[L] = nextRw[0];
                if (j < r) newColWt[k + r - j - s] = nextRw[pu - s - j];
                Rw = std::move(nextRw);
            }
            for (int32_t i = 1; i <= pu - s - r; ++i) newColWt[k - pu + r + i] = Rw[i];
            for (int32_t ci = 0; ci < newNu; ++ci)
                newWts[static_cast<size_t>(ci * nv + row)] = newColWt[ci];
        }
        return NurbsSurface(pu, pv, std::move(newKnotU), m_knotV,
                            std::move(newPts), newNu, nv, std::move(newWts));
    }

    return NurbsSurface(pu, pv, std::move(newKnotU), m_knotV,
                        std::move(newPts), newNu, nv);
}

NurbsSurface NurbsSurface::insertKnotV(float v, int32_t r) const {
    if (r < 1 || !isValid()) return *this;
    int32_t pu = m_degU, pv = m_degV;
    int32_t nu = m_nU, nv = m_nV;
    int32_t mv = nv + pv + 1;
    int32_t k  = findSpanV(v);
    int32_t s  = 0;
    for (int32_t i = k; i >= 0 && std::abs(m_knotV[i] - v) < 1e-10f; --i) ++s;
    s = std::min(s, pv);
    if (s == pv) return *this;

    std::vector<float> newKnotV;
    newKnotV.reserve(static_cast<size_t>(mv + r));
    for (int32_t i = 0; i <= k; ++i) newKnotV.push_back(m_knotV[i]);
    for (int32_t i = 0; i < r;  ++i) newKnotV.push_back(v);
    for (int32_t i = k + 1; i < mv; ++i) newKnotV.push_back(m_knotV[i]);

    int32_t newNv = nv + r;
    std::vector<Vec3> newPts(static_cast<size_t>(nu * newNv));

    for (int32_t col = 0; col < nu; ++col) {
        std::vector<Vec3> rowPts(static_cast<size_t>(nv));
        for (int32_t ri = 0; ri < nv; ++ri)
            rowPts[ri] = m_ctlPts[static_cast<size_t>(col * nv + ri)];

        std::vector<Vec3> newRowPts(static_cast<size_t>(newNv));
        for (int32_t i = 0; i <= k - pv; ++i) newRowPts[i] = rowPts[i];
        for (int32_t i = k - s; i < nv; ++i) newRowPts[i + r] = rowPts[i];

        std::vector<Vec3> Rw;
        for (int32_t i = 0; i <= pv - s; ++i) Rw.push_back(rowPts[k - pv + i]);

        for (int32_t j = 1; j <= r; ++j) {
            int32_t L = k - pv + j;
            std::vector<Vec3> nextRw(static_cast<size_t>(pv - s - j + 1));
            for (int32_t i = 0; i <= pv - s - j; ++i) {
                float denom = m_knotV[i + k + 1] - m_knotV[L + i];
                float alpha = (std::abs(denom) > 1e-10f)
                                  ? (v - m_knotV[L + i]) / denom
                                  : 0.f;
                nextRw[i].x = alpha * Rw[i + 1].x + (1.f - alpha) * Rw[i].x;
                nextRw[i].y = alpha * Rw[i + 1].y + (1.f - alpha) * Rw[i].y;
                nextRw[i].z = alpha * Rw[i + 1].z + (1.f - alpha) * Rw[i].z;
            }
            newRowPts[L] = nextRw[0];
            if (j < r) newRowPts[k + r - j - s] = nextRw[pv - s - j];
            Rw = std::move(nextRw);
        }
        for (int32_t i = 1; i <= pv - s - r; ++i) newRowPts[k - pv + r + i] = Rw[i];

        for (int32_t ri = 0; ri < newNv; ++ri)
            newPts[static_cast<size_t>(col * newNv + ri)] = newRowPts[ri];
    }

    if (isRational()) {
        std::vector<float> newWts(static_cast<size_t>(nu * newNv));
        for (int32_t col = 0; col < nu; ++col) {
            std::vector<float> rowWt(static_cast<size_t>(nv));
            for (int32_t ri = 0; ri < nv; ++ri)
                rowWt[ri] = m_weights[static_cast<size_t>(col * nv + ri)];
            std::vector<float> newRowWt(static_cast<size_t>(newNv));
            for (int32_t i = 0; i <= k - pv; ++i) newRowWt[i] = rowWt[i];
            for (int32_t i = k - s; i < nv; ++i) newRowWt[i + r] = rowWt[i];
            std::vector<float> Rw;
            for (int32_t i = 0; i <= pv - s; ++i) Rw.push_back(rowWt[k - pv + i]);
            for (int32_t j = 1; j <= r; ++j) {
                int32_t L = k - pv + j;
                std::vector<float> nextRw(static_cast<size_t>(pv - s - j + 1));
                for (int32_t i = 0; i <= pv - s - j; ++i) {
                    float denom = m_knotV[i + k + 1] - m_knotV[L + i];
                    float alpha = (std::abs(denom) > 1e-10f)
                                      ? (v - m_knotV[L + i]) / denom
                                      : 0.f;
                    nextRw[i] = alpha * Rw[i + 1] + (1.f - alpha) * Rw[i];
                }
                newRowWt[L] = nextRw[0];
                if (j < r) newRowWt[k + r - j - s] = nextRw[pv - s - j];
                Rw = std::move(nextRw);
            }
            for (int32_t i = 1; i <= pv - s - r; ++i) newRowWt[k - pv + r + i] = Rw[i];
            for (int32_t ri = 0; ri < newNv; ++ri)
                newWts[static_cast<size_t>(col * newNv + ri)] = newRowWt[ri];
        }
        return NurbsSurface(pu, pv, m_knotU, std::move(newKnotV),
                            std::move(newPts), nu, newNv, std::move(newWts));
    }

    return NurbsSurface(pu, pv, m_knotU, std::move(newKnotV),
                        std::move(newPts), nu, newNv);
}

} // namespace nexus::geometry
