#include <nexus/geometry/NurbsCurveSplit.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

constexpr float kEpsilon = 1e-10f;

} // namespace

NurbsCurveSplitResult NurbsCurveSplit::split(const NurbsCurve& curve,
                                              float param) {
    NurbsCurveSplitResult result;
    if (!curve.isValid()) return result;

    auto [uMin, uMax] = curve.domain();
    if (param <= uMin + kEpsilon || param >= uMax - kEpsilon) {
        result.valid = false;
        return result;
    }

    int32_t p = curve.degree();
    int32_t k = curve.findSpan(param);

    // Insert knot to multiplicity p at split point
    NurbsCurve refined = curve;
    int32_t multiplicity = 0;
    const auto& knots = curve.knots();
    for (int32_t i = k; i >= 0 && std::abs(knots[i] - param) < kEpsilon; --i)
        ++multiplicity;
    int32_t needed = p - multiplicity;
    if (needed > 0)
        refined = curve.insertKnot(param, needed);

    // After refinement, param has multiplicity p, so we can split cleanly
    const auto& refKnots = refined.knots();
    const auto& refCtl   = refined.controlPoints();
    int32_t refN = refined.controlPointCount();
    int32_t splitIdx = refined.findSpan(param);

    // Left part: control points 0..splitIdx, clamp knots
    int32_t leftN = splitIdx + 1;
    std::vector<float> leftKnots;
    std::vector<Vec3>  leftCtl;

    for (int32_t i = 0; i <= splitIdx + p + 1; ++i) {
        if (i < static_cast<int32_t>(refKnots.size()))
            leftKnots.push_back(std::min(refKnots[i], param));
    }
    // Ensure last p+1 knots equal param for clamping
    for (int32_t i = 0; i <= p; ++i)
        leftKnots[leftKnots.size() - 1 - i] = param;

    leftCtl.reserve(static_cast<size_t>(leftN));
    for (int32_t i = 0; i < leftN; ++i)
        leftCtl.push_back(refCtl[i]);

    // Right part: control points splitIdx-p .. end, shift knots
    int32_t rightN = refN - (splitIdx - p);
    std::vector<float> rightKnots;
    std::vector<Vec3>  rightCtl;

    for (int32_t i = 0; i <= p; ++i)
        rightKnots.push_back(param);
    for (int32_t i = splitIdx + 1; i < static_cast<int32_t>(refKnots.size()); ++i)
        rightKnots.push_back(refKnots[i]);

    rightCtl.reserve(static_cast<size_t>(rightN));
    for (int32_t i = splitIdx - p; i < refN; ++i)
        rightCtl.push_back(refCtl[i]);

    NurbsCurve left(p, std::move(leftKnots), std::move(leftCtl));
    NurbsCurve right(p, std::move(rightKnots), std::move(rightCtl));

    if (curve.isRational()) {
        const auto& refWt = refined.weights();
        std::vector<float> leftWt(static_cast<size_t>(leftN));
        for (int32_t i = 0; i < leftN; ++i) leftWt[i] = refWt[i];
        left.setWeights(std::move(leftWt));

        std::vector<float> rightWt(static_cast<size_t>(rightN));
        for (int32_t i = 0; i < rightN; ++i)
            rightWt[i] = refWt[static_cast<size_t>(splitIdx - p + i)];
        right.setWeights(std::move(rightWt));
    }

    result.left  = left;
    result.right = right;
    result.valid = true;
    return result;
}

} // namespace nexus::geometry
