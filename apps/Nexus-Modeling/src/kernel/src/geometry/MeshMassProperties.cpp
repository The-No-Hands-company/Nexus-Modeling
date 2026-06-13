#include <nexus/geometry/MeshMassProperties.h>
#include "EigenSolver3x3.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;

MassProperties MeshMassProperties::compute(const Mesh& mesh) {
    MassProperties result;
    if (!mesh.isValid()) return result;

    const auto& topo = mesh.topology();
    const auto& pos = mesh.attributes().positions();

    std::vector<float> C(10, 0.f);
    Vec3 ref = pos[0];

    for (size_t fi = 0; fi < topo.faceCount(); ++fi) {
        const Face& face = topo.face(fi);
        if (face.indices.size() < 3) continue;
        for (size_t vi = 0; vi + 2 < face.indices.size(); ++vi) {
            Vec3 A = pos[face.indices[0]] - ref;
            Vec3 B = pos[face.indices[vi + 1]] - ref;
            Vec3 Cv = pos[face.indices[vi + 2]] - ref;

            float a1 = B.x - A.x, b1 = B.y - A.y, c1 = B.z - A.z;
            float a2 = Cv.x - A.x, b2 = Cv.y - A.y, c2 = Cv.z - A.z;
            float d0 = b1 * c2 - c1 * b2;
            float d1 = a1 * c2 - c1 * a2;
            float d2 = a1 * b2 - b1 * a2;

            float w = -A.x * d0 + A.y * d1 - A.z * d2;
            w *= (1.f / 6.f);

            C[0] += w;
            C[1] += w * (A.x + B.x + Cv.x);
            C[2] += w * (A.y + B.y + Cv.y);
            C[3] += w * (A.z + B.z + Cv.z);

            float x[3] = {A.x, B.x, Cv.x};
            float y[3] = {A.y, B.y, Cv.y};
            float z[3] = {A.z, B.z, Cv.z};

            for (int j = 0; j < 3; ++j) {
                int j1 = (j + 1) % 3;
                C[4] += w * (x[j] * x[j] + x[j] * x[j1] + x[j1] * x[j1]);
                C[5] += w * (y[j] * y[j] + y[j] * y[j1] + y[j1] * y[j1]);
                C[6] += w * (z[j] * z[j] + z[j] * z[j1] + z[j1] * z[j1]);
                C[7] += w * (x[j] * y[j] + x[j] * y[j1] + x[j1] * y[j]) / 2.f;
                C[8] += w * (x[j] * z[j] + x[j] * z[j1] + x[j1] * z[j]) / 2.f;
                C[9] += w * (y[j] * z[j] + y[j] * z[j1] + y[j1] * z[j]) / 2.f;
            }
        }
    }

    float volume = C[0];
    if (std::fabs(volume) < 1e-10f) return result;

    float invVol = 1.f / volume;
    Vec3 centroid = {C[1] * invVol * 0.25f + ref.x,
                     C[2] * invVol * 0.25f + ref.y,
                     C[3] * invVol * 0.25f + ref.z};

    result.volume = std::fabs(volume);
    result.centroid = centroid;

    float I[3][3] = {};
    float denom = 1.f / 20.f;
    I[0][0] = (C[5] + C[6]) * denom;
    I[1][1] = (C[4] + C[6]) * denom;
    I[2][2] = (C[4] + C[5]) * denom;
    I[0][1] = I[1][0] = -C[7] * denom;
    I[0][2] = I[2][0] = -C[8] * denom;
    I[1][2] = I[2][1] = -C[9] * denom;

    float cx = centroid.x, cy = centroid.y, cz = centroid.z;
    float rx = cx - ref.x, ry = cy - ref.y, rz = cz - ref.z;
    I[0][0] -= volume * (ry * ry + rz * rz);
    I[1][1] -= volume * (rx * rx + rz * rz);
    I[2][2] -= volume * (rx * rx + ry * ry);
    I[0][1] += volume * rx * ry;
    I[0][2] += volume * rx * rz;
    I[1][2] += volume * ry * rz;
    I[1][0] = I[0][1];
    I[2][0] = I[0][2];
    I[2][1] = I[1][2];

    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            result.inertia[i][j] = I[i][j];

    float mPacked[6] = {
        I[0][0], I[1][1], I[2][2],
        I[0][1], I[0][2], I[1][2]
    };
    nexus::geometry::internal::Eigen3x3 eig =
        nexus::geometry::internal::solveEigenSymmetric3x3(mPacked);

    int order[3] = {0, 1, 2};
    std::sort(order, order + 3, [&](int a, int b) { return eig.val[a] > eig.val[b]; });

    for (int i = 0; i < 3; ++i) {
        result.principalMoments[i] = eig.val[order[i]];
        result.principalAxes[i] = eig.vec[order[i]];
    }

    return result;
}

} // namespace nexus::geometry
