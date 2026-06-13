#include <nexus/geometry/NurbsSurfaceSplit.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

} // namespace

NurbsSurfaceSplitResult NurbsSurfaceSplit::split(const NurbsSurface& surface,
                                                   float param,
                                                   SplitDirection direction) {
    NurbsSurfaceSplitResult result;
    if (!surface.isValid()) return result;

    int32_t pu = surface.degreeU();
    int32_t pv = surface.degreeV();
    int32_t nu = surface.controlPointCountU();
    int32_t nv = surface.controlPointCountV();
    const auto& ku = surface.knotU();
    const auto& kv = surface.knotV();
    bool rational = surface.isRational();

    if (direction == SplitDirection::U) {
        float uMin = ku[static_cast<size_t>(pu)];
        float uMax = ku[static_cast<size_t>(ku.size() - 1 - static_cast<size_t>(pu))];
        if (param <= uMin + kEpsilon || param >= uMax - kEpsilon) return result;

        // Insert knot to full multiplicity pu
        NurbsSurface refined = surface;
        int32_t k = surface.findSpanU(param);
        int32_t mult = 0;
        for (int32_t i = k; i >= 0 && std::abs(ku[i] - param) < kEpsilon; --i) ++mult;
        int32_t needed = pu - mult;
        if (needed > 0) refined = surface.insertKnotU(param, needed);

        const auto& rKu  = refined.knotU();
        const auto& rCtl = refined.controlPoints();
        int32_t rNu = refined.controlPointCountU();
        int32_t si = refined.findSpanU(param);

        // Left
        int32_t leftNu = si + 1;
        std::vector<float> leftKU;
        for (int32_t i = 0; i <= si + pu + 1; ++i)
            leftKU.push_back((i < static_cast<int32_t>(rKu.size()))
                             ? std::min(rKu[i], param) : param);
        for (int32_t i = 0; i <= pu; ++i)
            leftKU[leftKU.size() - 1 - static_cast<size_t>(i)] = param;

        std::vector<Vec3> leftCtl(static_cast<size_t>(leftNu * nv));
        for (int32_t i = 0; i < leftNu; ++i)
            for (int32_t j = 0; j < nv; ++j)
                leftCtl[static_cast<size_t>(i * nv + j)] =
                    rCtl[static_cast<size_t>(i * nv + j)];

        NurbsSurface left(pu, pv, std::move(leftKU), surface.knotV(),
                          std::move(leftCtl), leftNu, nv);

        // Right
        int32_t rightNu = rNu - (si - pu);
        std::vector<float> rightKU;
        for (int32_t i = 0; i <= pu; ++i) rightKU.push_back(param);
        for (int32_t i = si + 1; i < static_cast<int32_t>(rKu.size()); ++i)
            rightKU.push_back(rKu[i]);

        std::vector<Vec3> rightCtl(static_cast<size_t>(rightNu * nv));
        for (int32_t i = 0; i < rightNu; ++i)
            for (int32_t j = 0; j < nv; ++j)
                rightCtl[static_cast<size_t>(i * nv + j)] =
                    rCtl[static_cast<size_t>((si - pu + i) * nv + j)];

        NurbsSurface right(pu, pv, std::move(rightKU), surface.knotV(),
                           std::move(rightCtl), rightNu, nv);

        if (rational) {
            const auto& rWt = refined.weights();
            std::vector<float> leftWt(static_cast<size_t>(leftNu * nv));
            for (int32_t i = 0; i < leftNu; ++i)
                for (int32_t j = 0; j < nv; ++j)
                    leftWt[static_cast<size_t>(i * nv + j)] =
                        rWt[static_cast<size_t>(i * nv + j)];

            std::vector<float> rightWt(static_cast<size_t>(rightNu * nv));
            for (int32_t i = 0; i < rightNu; ++i)
                for (int32_t j = 0; j < nv; ++j)
                    rightWt[static_cast<size_t>(i * nv + j)] =
                        rWt[static_cast<size_t>((si - pu + i) * nv + j)];

            left = NurbsSurface(pu, pv, const_cast<NurbsSurface&>(left).knotU(),
                                left.knotV(),
                                const_cast<NurbsSurface&>(left).controlPoints(),
                                leftNu, nv, std::move(leftWt));
            right = NurbsSurface(pu, pv, const_cast<NurbsSurface&>(right).knotU(),
                                 right.knotV(),
                                 const_cast<NurbsSurface&>(right).controlPoints(),
                                 rightNu, nv, std::move(rightWt));
        }

        result.left  = left;
        result.right = right;
        result.valid = true;
    } else {
        // SplitDirection::V
        float vMin = kv[static_cast<size_t>(pv)];
        float vMax = kv[static_cast<size_t>(kv.size() - 1 - static_cast<size_t>(pv))];
        if (param <= vMin + kEpsilon || param >= vMax - kEpsilon) return result;

        NurbsSurface refined = surface;
        int32_t k = surface.findSpanV(param);
        int32_t mult = 0;
        for (int32_t i = k; i >= 0 && std::abs(kv[i] - param) < kEpsilon; --i) ++mult;
        int32_t needed = pv - mult;
        if (needed > 0) refined = surface.insertKnotV(param, needed);

        const auto& rKv  = refined.knotV();
        const auto& rCtl = refined.controlPoints();
        int32_t rNv = refined.controlPointCountV();
        int32_t si = refined.findSpanV(param);

        int32_t leftNv = si + 1;
        std::vector<float> leftKV;
        for (int32_t i = 0; i <= si + pv + 1; ++i)
            leftKV.push_back((i < static_cast<int32_t>(rKv.size()))
                             ? std::min(rKv[i], param) : param);
        for (int32_t i = 0; i <= pv; ++i)
            leftKV[leftKV.size() - 1 - static_cast<size_t>(i)] = param;

        std::vector<Vec3> leftCtl(static_cast<size_t>(nu * leftNv));
        for (int32_t i = 0; i < nu; ++i)
            for (int32_t j = 0; j < leftNv; ++j)
                leftCtl[static_cast<size_t>(i * leftNv + j)] =
                    rCtl[static_cast<size_t>(i * rNv + j)];

        NurbsSurface left(pu, pv, surface.knotU(), std::move(leftKV),
                          std::move(leftCtl), nu, leftNv);

        int32_t rightNv = rNv - (si - pv);
        std::vector<float> rightKV;
        for (int32_t i = 0; i <= pv; ++i) rightKV.push_back(param);
        for (int32_t i = si + 1; i < static_cast<int32_t>(rKv.size()); ++i)
            rightKV.push_back(rKv[i]);

        std::vector<Vec3> rightCtl(static_cast<size_t>(nu * rightNv));
        for (int32_t i = 0; i < nu; ++i)
            for (int32_t j = 0; j < rightNv; ++j)
                rightCtl[static_cast<size_t>(i * rightNv + j)] =
                    rCtl[static_cast<size_t>(i * rNv + si - pv + j)];

        NurbsSurface right(pu, pv, surface.knotU(), std::move(rightKV),
                           std::move(rightCtl), nu, rightNv);

        if (rational) {
            const auto& rWt = refined.weights();
            std::vector<float> leftWt(static_cast<size_t>(nu * leftNv));
            for (int32_t i = 0; i < nu; ++i)
                for (int32_t j = 0; j < leftNv; ++j)
                    leftWt[static_cast<size_t>(i * leftNv + j)] =
                        rWt[static_cast<size_t>(i * rNv + j)];
            std::vector<float> rightWt(static_cast<size_t>(nu * rightNv));
            for (int32_t i = 0; i < nu; ++i)
                for (int32_t j = 0; j < rightNv; ++j)
                    rightWt[static_cast<size_t>(i * rightNv + j)] =
                        rWt[static_cast<size_t>(i * rNv + si - pv + j)];
            left = NurbsSurface(pu, pv, const_cast<NurbsSurface&>(left).knotU(),
                                left.knotV(),
                                const_cast<NurbsSurface&>(left).controlPoints(),
                                nu, leftNv, std::move(leftWt));
            right = NurbsSurface(pu, pv, const_cast<NurbsSurface&>(right).knotU(),
                                 right.knotV(),
                                 const_cast<NurbsSurface&>(right).controlPoints(),
                                 nu, rightNv, std::move(rightWt));
        }
        result.left = left;
        result.right = right;
        result.valid = true;
    }
    return result;
}

} // namespace nexus::geometry
