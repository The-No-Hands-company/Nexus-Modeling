#pragma once
// ─── Nexus Geometry ── NurbsCurveOffset ─────────────────────────────────

#include <nexus/geometry/NurbsCurve.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

struct NurbsCurveOffsetOptions {
    float   distance     = 1.f;
    Vec3    planeNormal  = {0.f, 0.f, 1.f};
    int32_t samples      = 200;
    float   fitTolerance = 1e-3f;
    int32_t fitDegree    = 3;
};

enum class NurbsCurveOffsetDiagnostic : uint32_t {
    Success              = 0,
    SelfIntersectWarning = 1u << 0,
    FitFailed            = 1u << 1,
};

struct NurbsCurveOffsetReport {
    NurbsCurveOffsetDiagnostic code     = NurbsCurveOffsetDiagnostic::Success;
    bool                       valid    = false;
    std::vector<std::string>   messages;

    [[nodiscard]] bool hasWarning(NurbsCurveOffsetDiagnostic w) const noexcept {
        return (static_cast<uint32_t>(code) & static_cast<uint32_t>(w)) != 0;
    }
    void addMessage(const std::string& m) { messages.push_back(m); }
};

class NurbsCurveOffset {
public:
    static std::vector<Vec3> offset(const NurbsCurve& curve,
                                     const NurbsCurveOffsetOptions& opts = {});

    static NurbsCurve fitToNurbs(const NurbsCurve& curve,
                                 const NurbsCurveOffsetOptions& opts = {},
                                 NurbsCurveOffsetReport* report = nullptr);
};

} // namespace nexus::geometry
