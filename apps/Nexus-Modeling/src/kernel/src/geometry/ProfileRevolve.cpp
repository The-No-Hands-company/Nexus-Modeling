#include <nexus/geometry/ProfileRevolve.h>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

float degToRad(float deg) noexcept
{
    return deg * static_cast<float>(std::numbers::pi) / 180.f;
}

Vec3 rotateAroundAxis(const Vec3& point, const Vec3& axisOrigin,
                       const Vec3& axisDir, float angleRad) noexcept
{
    const Vec3 axis = axisDir.normalize();
    const Vec3 rel = point - axisOrigin;

    const float cosA = std::cos(angleRad);
    const float sinA = std::sin(angleRad);
    const float dotProd = rel.dot(axis);
    const Vec3  crossProd = axis.cross(rel);

    return axisOrigin + Vec3{
        rel.x * cosA + crossProd.x * sinA + axis.x * dotProd * (1.f - cosA),
        rel.y * cosA + crossProd.y * sinA + axis.y * dotProd * (1.f - cosA),
        rel.z * cosA + crossProd.z * sinA + axis.z * dotProd * (1.f - cosA)
    };
}

int32_t profileSampleCount(const NurbsCurve& profile) noexcept
{
    int32_t n = profile.controlPointCount();
    if (n < 2) n = 20;
    if (n > 200) n = 200;
    return n;
}

} // namespace

RevolveReport RevolveOperation::revolve(const NurbsCurve&  profile,
                                         const RevolveDesc& desc,
                                         NurbsSurface&       outSurface,
                                         Mesh*               outMesh) noexcept
{
    outSurface = {};
    if (outMesh) *outMesh = {};

    RevolveReport report{};

    if (!profile.isValid()) {
        report.diagnostic = RevolveDiagnostic::InvalidProfileCurve;
        report.converged  = false;
        return report;
    }

    float axisLen = desc.axisDirection.length();
    if (axisLen < 1e-10f) {
        report.diagnostic = RevolveDiagnostic::DegenerateAxis;
        report.converged  = false;
        return report;
    }

    const float startRad = degToRad(desc.startAngleDeg);
    const float endRad   = degToRad(desc.endAngleDeg);
    if (startRad >= endRad) {
        report.diagnostic = RevolveDiagnostic::InvalidAngles;
        report.converged  = false;
        return report;
    }

    const uint32_t angularSteps = std::max(desc.angularSamples, 3u);
    const int32_t  profSteps    = profileSampleCount(profile);
    const auto     profDom      = profile.domain();

    // Sample profile
    std::vector<Vec3> profPts(static_cast<size_t>(profSteps));
    for (int32_t j = 0; j < profSteps; ++j) {
        float t = (profSteps == 1) ? 0.5f
            : profDom.first + (profDom.second - profDom.first) * static_cast<float>(j) / static_cast<float>(profSteps - 1);
        profPts[static_cast<size_t>(j)] = profile.evaluate(t);
    }

    // Generate revolved vertex grid: [angularSteps × profSteps]
    const uint32_t as = angularSteps;
    const uint32_t ps = static_cast<uint32_t>(profSteps);
    std::vector<Vec3> allVerts;
    allVerts.reserve(static_cast<size_t>(as) * static_cast<size_t>(ps));

    const float angleRange = endRad - startRad;

    for (uint32_t i = 0; i < as; ++i) {
        float angle = startRad + angleRange * static_cast<float>(i) / static_cast<float>(as - 1);

        for (uint32_t j = 0; j < ps; ++j) {
            Vec3 pt = rotateAroundAxis(profPts[j], desc.axisOrigin, desc.axisDirection, angle);
            allVerts.push_back(pt);
        }
    }

    report.vertexCount = static_cast<uint32_t>(allVerts.size());

    // Output NURBS surface if requested (before caps — surface is open).
    if (desc.outputAsNurbsSurface && static_cast<int32_t>(ps) > 1 && static_cast<int32_t>(as) > 1) {
        int32_t degU = std::min(3, static_cast<int32_t>(ps) - 1);
        int32_t degV = std::min(3, static_cast<int32_t>(as) - 1);
        degU = std::max(degU, 1);
        degV = std::max(degV, 1);
        int32_t nU = static_cast<int32_t>(ps);
        int32_t nV = static_cast<int32_t>(as);

        std::vector<float> knotU(static_cast<size_t>(nU + degU + 1));
        for (int32_t j = 0; j <= degU; ++j) knotU[static_cast<size_t>(j)] = 0.f;
        for (int32_t j = 1; j < nU - degU; ++j) {
            knotU[static_cast<size_t>(degU + j)] = static_cast<float>(j) / static_cast<float>(nU - degU);
        }
        for (int32_t j = 0; j <= degU; ++j) knotU[knotU.size() - 1 - static_cast<size_t>(j)] = 1.f;

        std::vector<float> knotV(static_cast<size_t>(nV + degV + 1));
        for (int32_t j = 0; j <= degV; ++j) knotV[static_cast<size_t>(j)] = 0.f;
        for (int32_t j = 1; j < nV - degV; ++j) {
            knotV[static_cast<size_t>(degV + j)] = static_cast<float>(j) / static_cast<float>(nV - degV);
        }
        for (int32_t j = 0; j <= degV; ++j) knotV[knotV.size() - 1 - static_cast<size_t>(j)] = 1.f;

        outSurface = NurbsSurface(degU, degV,
                                   std::move(knotU), std::move(knotV),
                                   allVerts, nU, nV);
    }

    // Output mesh if requested
    if (outMesh) {
        const bool addCaps = desc.capEnds && angleRange >= degToRad(359.99f);
        Vec3 bottomCenter, topCenter;
        if (addCaps) {
            bottomCenter = desc.axisOrigin;
            bottomCenter.z = allVerts[0].z;
            topCenter = desc.axisOrigin;
            topCenter.z   = allVerts[static_cast<size_t>(ps) - 1].z;
            allVerts.push_back(bottomCenter);
            allVerts.push_back(topCenter);
        }

        Mesh result;
        result.attributes().setPositions(std::move(allVerts));
        auto& topo = result.topology();

        for (uint32_t i = 0; i + 1 < as; ++i) {
            for (uint32_t j = 0; j + 1 < ps; ++j) {
                uint32_t v00 = i * ps + j;
                uint32_t v01 = (i + 1) * ps + j;
                uint32_t v11 = (i + 1) * ps + (j + 1);
                uint32_t v10 = i * ps + (j + 1);

                topo.addFace(Face{{v00, v01, v11}});
                topo.addFace(Face{{v00, v11, v10}});
            }
        }

        report.faceCount = static_cast<uint32_t>(topo.faceCount());

        if (addCaps) {
            const uint32_t bCtr = as * ps;
            const uint32_t tCtr = as * ps + 1;
            for (uint32_t i = 0; i < as; ++i) {
                uint32_t a = i * ps;
                uint32_t b = ((i + 1) % as) * ps;
                topo.addFace(Face{{bCtr, a, b}});
                a += (ps - 1);
                b += (ps - 1);
                topo.addFace(Face{{tCtr, b, a}});
            }
            report.capFaceCount = as * 2;
            report.faceCount = static_cast<uint32_t>(topo.faceCount());
            report.vertexCount = static_cast<uint32_t>(result.attributes().vertexCount());
        }

        *outMesh = std::move(result);
    }

    return report;
}

} // namespace nexus::geometry
