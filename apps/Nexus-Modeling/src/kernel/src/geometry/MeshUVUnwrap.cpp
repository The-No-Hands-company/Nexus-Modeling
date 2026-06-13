#include <nexus/geometry/MeshUVUnwrap.h>

#include "CotangentLaplacian.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

using Vec3 = nexus::render::Vec3;
using internal::FanEntry;
using internal::buildCotangentFan;

Mesh MeshUVUnwrap::unwrap(const Mesh& mesh, const UnwrapOptions& opts) {
    if (opts.method == UnwrapMethod::LSCM) {
        return unwrapLSCM(mesh, opts);
    }
    return unwrapPCA(mesh, opts);
}

Mesh MeshUVUnwrap::unwrapPCA(const Mesh& mesh, const UnwrapOptions& opts) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    const auto& pos = mesh.attributes().positions();
    size_t V = pos.size();
    if (V < 3) return result;

    Vec3 centroid = {0, 0, 0};
    for (const auto& p : pos) centroid = centroid + p;
    centroid = centroid * (1.f / static_cast<float>(V));

    float cov[3][3] = {};
    for (const auto& p : pos) {
        float dx = p.x - centroid.x;
        float dy = p.y - centroid.y;
        float dz = p.z - centroid.z;
        cov[0][0] += dx * dx; cov[0][1] += dx * dy; cov[0][2] += dx * dz;
        cov[1][0] += dy * dx; cov[1][1] += dy * dy; cov[1][2] += dy * dz;
        cov[2][0] += dz * dx; cov[2][1] += dz * dy; cov[2][2] += dz * dz;
    }

    Vec3 b = {1, 0, 0};
    for (int iter = 0; iter < 20; ++iter) {
        Vec3 Ab = {
            cov[0][0] * b.x + cov[0][1] * b.y + cov[0][2] * b.z,
            cov[1][0] * b.x + cov[1][1] * b.y + cov[1][2] * b.z,
            cov[2][0] * b.x + cov[2][1] * b.y + cov[2][2] * b.z,
        };
        float len = Ab.length();
        if (len < 1e-10f) break;
        b = Ab * (1.f / len);
    }

    float aEigVal = 0.f;
    {
        Vec3 Ab = {
            cov[0][0] * b.x + cov[0][1] * b.y + cov[0][2] * b.z,
            cov[1][0] * b.x + cov[1][1] * b.y + cov[1][2] * b.z,
            cov[2][0] * b.x + cov[2][1] * b.y + cov[2][2] * b.z,
        };
        aEigVal = Ab.length();
    }

    Vec3 normal = b;
    Vec3 t0;

    float cx = std::fabs(normal.x), cy = std::fabs(normal.y), cz = std::fabs(normal.z);
    if (cx <= cy && cx <= cz)
        t0 = {0.f, -normal.z, normal.y};
    else if (cy <= cx && cy <= cz)
        t0 = {-normal.z, 0.f, normal.x};
    else
        t0 = {-normal.y, normal.x, 0.f};
    t0 = t0.normalize();

    Vec3 vPower;
    {
        float d = t0.dot(normal);
        vPower = normal * d;
    }
    Vec3 tangent = t0 - vPower;
    tangent = tangent.normalize();

    float tAngle = 0.f;
    {
        float dot = tangent.dot(b);
        dot = std::max(-1.f, std::min(1.f, dot));
        tAngle = std::acos(dot);
    }
    if (tangent.cross(b).dot(normal) < 0.f) tAngle = -tAngle;

    Vec3 t1 = tangent * std::cos(tAngle) + (normal.cross(tangent)) * std::sin(tAngle);
    Vec3 t2 = tangent * -std::sin(tAngle) + (normal.cross(tangent)) * std::cos(tAngle);

    float cVal = 0.f;
    {
        Vec3 Ac = {
            cov[0][0] * t2.x + cov[0][1] * t2.y + cov[0][2] * t2.z,
            cov[1][0] * t2.x + cov[1][1] * t2.y + cov[1][2] * t2.z,
            cov[2][0] * t2.x + cov[2][1] * t2.y + cov[2][2] * t2.z,
        };
        cVal = t2.dot(Ac);
    }

    Vec3 axisU, axisV;
    if (aEigVal >= cVal) {
        axisU = t1;
        axisV = t2;
    } else {
        axisU = t2;
        axisV = t1;
    }

    float eig1 = std::sqrt(std::max(aEigVal, cVal));
    float eig2 = std::sqrt(std::min(aEigVal, cVal));
    float scaleU = eig1 > 1e-6f ? 1.f / eig1 : 1.f;
    float scaleV = eig2 > 1e-6f ? 1.f / eig2 : 1.f;

    std::vector<Vec2> uvs;
    uvs.reserve(V);
    for (const auto& p : pos) {
        float dx = p.x - centroid.x;
        float dy = p.y - centroid.y;
        float dz = p.z - centroid.z;
        float u = (dx * axisU.x + dy * axisU.y + dz * axisU.z) * scaleU;
        float v = (dx * axisV.x + dy * axisV.y + dz * axisV.z) * scaleV;
        uvs.push_back({u, v});
    }

    if (opts.normalize && !uvs.empty()) {
        float minU = std::numeric_limits<float>::max();
        float maxU = std::numeric_limits<float>::lowest();
        float minV = std::numeric_limits<float>::max();
        float maxV = std::numeric_limits<float>::lowest();
        for (const auto& uv : uvs) {
            minU = std::min(minU, uv.u);
            maxU = std::max(maxU, uv.u);
            minV = std::min(minV, uv.v);
            maxV = std::max(maxV, uv.v);
        }
        float rangeU = maxU - minU;
        float rangeV = maxV - minV;
        float range = std::max(rangeU, rangeV);
        if (range > 1e-10f) {
            float s = opts.scale / range;
            for (auto& uv : uvs) {
                uv.u = (uv.u - minU) * s;
                uv.v = (uv.v - minV) * s;
            }
        }
    }

    result.attributes().setUVs(std::move(uvs));
    return result;
}

namespace {

void solveLaplacianDense(std::vector<std::vector<std::pair<uint32_t, float>>>& adjacency,
                         std::vector<float>& rowSum,
                         std::vector<float>& values,
                         const std::vector<bool>& constrained,
                         uint32_t V,
                         uint32_t maxIterations = 5000,
                         float tolerance = 1e-7f) {
    const float omega = 1.0f;
    for (uint32_t iter = 0; iter < maxIterations; ++iter) {
        float maxChange = 0.f;
        for (size_t i = 0; i < V; ++i) {
            if (constrained[i]) continue;
            if (rowSum[i] < 1e-10f) continue;

            float sum = 0.f;
            for (const auto& [j, w] : adjacency[i]) {
                sum += w * values[j];
            }
            float newVal = (1.f - omega) * values[i] + omega * (sum / rowSum[i]);
            float change = std::fabs(newVal - values[i]);
            if (change > maxChange) maxChange = change;
            values[i] = newVal;
        }
        if (maxChange < tolerance && iter > 2) break;
    }
}

} // namespace

Mesh MeshUVUnwrap::unwrapLSCM(const Mesh& mesh, const UnwrapOptions& opts) {
    Mesh result = mesh;
    if (!result.isValid()) return result;

    const auto& pos = mesh.attributes().positions();
    const size_t V = pos.size();
    if (V < 3) return result;

    std::vector<std::vector<FanEntry>> fan;
    std::vector<float> vertexAreas;
    buildCotangentFan(mesh, fan, vertexAreas);

    std::vector<std::vector<std::pair<uint32_t, float>>> adjacency(V);
    std::vector<float> rowSum(V, 0.f);

    for (size_t i = 0; i < V; ++i) {
        std::unordered_map<uint32_t, float> edgeWeight;
        for (const auto& entry : fan[i]) {
            edgeWeight[entry.to] += std::fabs(entry.cot);
        }
        for (const auto& [neighbor, w] : edgeWeight) {
            adjacency[i].emplace_back(neighbor, w);
            rowSum[i] += w;
        }
    }

    uint32_t pinA = opts.pinVertexA;
    uint32_t pinB = opts.pinVertexB;

    if (pinA >= V) pinA = 0;
    if (pinB >= V) pinB = 0;

    if (pinA == pinB) {
        float maxDistSq = 0.f;
        for (size_t i = 0; i < V; ++i) {
            for (size_t j = i + 1; j < V; ++j) {
                float dx = pos[i].x - pos[j].x;
                float dy = pos[i].y - pos[j].y;
                float dz = pos[i].z - pos[j].z;
                float d2 = dx * dx + dy * dy + dz * dz;
                if (d2 > maxDistSq) {
                    maxDistSq = d2;
                    pinA = static_cast<uint32_t>(i);
                    pinB = static_cast<uint32_t>(j);
                }
            }
        }
    }

    if (pinA >= V || pinB >= V || pinA == pinB) {
        return result;
    }

    float dx = pos[pinA].x - pos[pinB].x;
    float dy = pos[pinA].y - pos[pinB].y;
    float dz = pos[pinA].z - pos[pinB].z;
    float pinDist = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (pinDist < 1e-8f) pinDist = 1.f;

    std::vector<bool> constrained(V, false);
    constrained[pinA] = true;
    constrained[pinB] = true;

    std::vector<float> u(V, 0.f);
    std::vector<float> v(V, 0.f);
    u[pinA] = 0.f; v[pinA] = 0.f;
    u[pinB] = pinDist; v[pinB] = 0.f;

    solveLaplacianDense(adjacency, rowSum, u, constrained, static_cast<uint32_t>(V));
    solveLaplacianDense(adjacency, rowSum, v, constrained, static_cast<uint32_t>(V));

    std::vector<Vec2> uvs;
    uvs.reserve(V);
    for (size_t i = 0; i < V; ++i) {
        uvs.push_back({u[i], v[i]});
    }

    if (opts.normalize && !uvs.empty()) {
        float minU = std::numeric_limits<float>::max();
        float maxU = std::numeric_limits<float>::lowest();
        float minV = std::numeric_limits<float>::max();
        float maxV = std::numeric_limits<float>::lowest();
        for (const auto& uv : uvs) {
            minU = std::min(minU, uv.u);
            maxU = std::max(maxU, uv.u);
            minV = std::min(minV, uv.v);
            maxV = std::max(maxV, uv.v);
        }
        float rangeU = maxU - minU;
        float rangeV = maxV - minV;
        float range = std::max(rangeU, rangeV);
        if (range > 1e-10f) {
            float s = opts.scale / range;
            for (auto& uv : uvs) {
                uv.u = (uv.u - minU) * s;
                uv.v = (uv.v - minV) * s;
            }
        }
    }

    result.attributes().setUVs(std::move(uvs));
    return result;
}

} // namespace nexus::geometry
