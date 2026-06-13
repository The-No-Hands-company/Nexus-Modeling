#include <nexus/geometry/SweepLoft.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

namespace {

Vec3 sampleProfileCurve(const NurbsSurface& profile, float u) {
    auto vDom = profile.domainV();
    float vMid = (vDom.first + vDom.second) * 0.5f;
    return profile.evaluate(u, vMid);
}

Vec3 samplePathCurve(const NurbsSurface& path, float u) {
    auto vDom = path.domainV();
    float vMid = (vDom.first + vDom.second) * 0.5f;
    return path.evaluate(u, vMid);
}

Vec3 pathDerivative(const NurbsSurface& path, float u) {
    auto vDom = path.domainV();
    float vMid = (vDom.first + vDom.second) * 0.5f;
    return path.derivativeU(u, vMid);
}

struct LocalFrame {
    Vec3 tangent;
    Vec3 normal;
    Vec3 binormal;
};

LocalFrame computeFrame(const Vec3& tangent, bool useFrameFollowing,
                         const Vec3& prevTangent, bool hasPrev) {
    LocalFrame frame;
    frame.tangent = tangent;
    float tLen = tangent.length();
    if (tLen < 1e-10f) {
        frame.tangent  = {0, 0, 1};
        frame.normal   = {0, 1, 0};
        frame.binormal = {1, 0, 0};
        return frame;
    }
    frame.tangent = tangent * (1.f / tLen);

    if (useFrameFollowing && hasPrev) {
        Vec3 prevT = prevTangent;
        float ptLen = prevT.length();
        if (ptLen > 1e-10f) {
            prevT = prevT * (1.f / ptLen);
        }
        Vec3 rotAxis = prevT.cross(frame.tangent);
        float rotLen = rotAxis.length();
        if (rotLen > 1e-10f) {
            rotAxis = rotAxis * (1.f / rotLen);
            frame.normal   = rotAxis;
            frame.binormal = frame.tangent.cross(frame.normal);
            float bLen = frame.binormal.length();
            if (bLen > 1e-10f) {
                frame.binormal = frame.binormal * (1.f / bLen);
                frame.normal   = frame.binormal.cross(frame.tangent);
                return frame;
            }
        }
    }

    Vec3 worldUp{0, 1, 0};
    if (std::abs(frame.tangent.dot(worldUp)) > 0.999f) {
        worldUp = {1, 0, 0};
    }
    frame.binormal = frame.tangent.cross(worldUp);
    float bLen = frame.binormal.length();
    if (bLen < 1e-10f) {
        frame.binormal = {1, 0, 0};
    } else {
        frame.binormal = frame.binormal * (1.f / bLen);
    }
    frame.normal = frame.binormal.cross(frame.tangent);
    return frame;
}

Vec3 transformPoint(const Vec3& profilePt, const LocalFrame& frame,
                     const Vec3& pathPt, float scale) {
    Vec3 scaled = profilePt * scale;
    Vec3 result = pathPt
                + frame.binormal * scaled.x
                + frame.normal   * scaled.y
                + frame.tangent  * scaled.z;
    return result;
}

int32_t profileSampleCount(const NurbsSurface& profile) {
    int32_t n = profile.controlPointCountU();
    if (n < 2) n = 20;
    if (n > 200) n = 200;
    return n;
}

} // namespace

SweepLoftDiagnostic SweepLoftOperation::sweep(
    const NurbsSurface& profile,
    const NurbsSurface& path,
    const SweepDesc&    desc,
    NurbsSurface&       outSurface,
    Mesh*               outMesh) {

    outSurface = {};
    if (outMesh) *outMesh = {};

    if (!profile.isValid()) return SweepLoftDiagnostic::InvalidProfileCurve;
    if (!path.isValid())    return SweepLoftDiagnostic::InvalidPathCurve;

    auto pathDom = path.domainU();
    auto profDom = profile.domainU();

    float pathLen = pathDom.second - pathDom.first;
    if (pathLen < 1e-10f) return SweepLoftDiagnostic::PathTooShort;

    uint32_t pathSamples = std::max(desc.crossSectionCount, 2u);
    int32_t profSamples = profileSampleCount(profile);

    // Sample path
    std::vector<Vec3> pathPts(pathSamples);
    std::vector<Vec3> pathTans(pathSamples);
    std::vector<LocalFrame> frames(pathSamples);

    for (uint32_t i = 0; i < pathSamples; ++i) {
        float t = (pathSamples == 1) ? 0.5f
            : pathDom.first + pathLen * static_cast<float>(i) / static_cast<float>(pathSamples - 1);
        pathPts[i]  = samplePathCurve(path, t);
        pathTans[i] = pathDerivative(path, t);

        Vec3 prevTan{0,0,1};
        bool hasPrev = false;
        if (i > 0) {
            prevTan = pathTans[i - 1];
            hasPrev = true;
        }
        frames[i] = computeFrame(pathTans[i], desc.useFrameFollowing, prevTan, hasPrev);
        if (!desc.useFrameFollowing && i > 0) {
            // Copy previous frame's normal, then re-orthogonalize against new tangent.
            // If the new tangent is nearly parallel to the old normal (>5 deg cosine),
            // the cross product degenerates — fall back to a stable axis.
            Vec3 oldNormal = frames[i - 1].normal;
            float dup = std::abs(frames[i].tangent.dot(oldNormal));
            if (dup > 0.996f) {
                Vec3 fallback{0, 1, 0};
                if (std::abs(frames[i].tangent.dot(fallback)) > 0.996f) {
                    fallback = {0, 0, 1};
                }
                frames[i].binormal = frames[i].tangent.cross(fallback);
            } else {
                frames[i].binormal = frames[i].tangent.cross(oldNormal);
            }
            float blen = frames[i].binormal.length();
            if (blen > 1e-10f) {
                frames[i].binormal = frames[i].binormal * (1.f / blen);
                frames[i].normal = frames[i].binormal.cross(frames[i].tangent);
            } else {
                frames[i].normal   = frames[i - 1].normal;
                frames[i].binormal = frames[i - 1].binormal;
            }
        }
    }

    // Sample profile
    std::vector<Vec3> profilePts(static_cast<size_t>(profSamples));
    for (int32_t j = 0; j < profSamples; ++j) {
        float s = (profSamples == 1) ? 0.5f
            : profDom.first + (profDom.second - profDom.first) * static_cast<float>(j) / static_cast<float>(profSamples - 1);
        profilePts[static_cast<size_t>(j)] = sampleProfileCurve(profile, s);
    }

    // Generate cross-section vertices
    uint32_t ps = pathSamples;
    uint32_t pp = static_cast<uint32_t>(profSamples);

    std::vector<Vec3> allVerts;
    allVerts.reserve(static_cast<size_t>(ps) * static_cast<size_t>(pp));

    for (uint32_t i = 0; i < ps; ++i) {
        float t = (ps == 1) ? 0.5f
            : static_cast<float>(i) / static_cast<float>(ps - 1);
        float scl = desc.startScale + (desc.endScale - desc.startScale) * t;

        for (uint32_t j = 0; j < pp; ++j) {
            Vec3 pt = transformPoint(profilePts[j], frames[i], pathPts[i], scl);
            allVerts.push_back(pt);
        }
    }

    // Output NURBS surface if requested
    if (desc.outputAsNurbsSurface) {
        int32_t degU = std::min(3, static_cast<int32_t>(pp) - 1);
        int32_t degV = std::min(3, static_cast<int32_t>(ps) - 1);
        degU = std::max(degU, 1);
        degV = std::max(degV, 1);
        int32_t nU = static_cast<int32_t>(pp);
        int32_t nV = static_cast<int32_t>(ps);

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
        Mesh result;
        result.attributes().setPositions(allVerts);
        auto& topo = result.topology();

        for (uint32_t i = 0; i + 1 < ps; ++i) {
            for (uint32_t j = 0; j + 1 < pp; ++j) {
                uint32_t v00 = i * pp + j;
                uint32_t v01 = (i + 1) * pp + j;
                uint32_t v11 = (i + 1) * pp + (j + 1);
                uint32_t v10 = i * pp + (j + 1);

                // Two triangles per quad
                topo.addFace(Face{{v00, v01, v11}});
                topo.addFace(Face{{v00, v11, v10}});
            }
        }

        *outMesh = std::move(result);
    }

    return SweepLoftDiagnostic::Success;
}

} // namespace nexus::geometry
