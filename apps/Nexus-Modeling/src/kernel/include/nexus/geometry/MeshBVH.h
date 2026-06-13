#pragma once

#include <nexus/geometry/Mesh.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace nexus::geometry {

struct Ray {
    nexus::render::Vec3 origin;
    nexus::render::Vec3 direction;
};

struct RayHit {
    float t = std::numeric_limits<float>::max();
    uint32_t triangleIndex = 0;
    float u = 0.f;
    float v = 0.f;
};

struct ClosestPointHit {
    float distanceSquared = std::numeric_limits<float>::max();
    uint32_t triangleIndex = 0;
    float baryU = 0.f;
    float baryV = 0.f;
    nexus::render::Vec3 point = {0.f, 0.f, 0.f};
    bool valid = false;
};

class MeshBVH {
public:
    struct Tri {
        nexus::render::Vec3 v0;
        nexus::render::Vec3 e1;
        nexus::render::Vec3 e2;
        uint32_t srcFace = 0;

        nexus::render::Vec3 center() const noexcept {
            return {
                v0.x + (e1.x + e2.x) * (1.f / 3.f),
                v0.y + (e1.y + e2.y) * (1.f / 3.f),
                v0.z + (e1.z + e2.z) * (1.f / 3.f),
            };
        }

        nexus::render::Vec3 normal() const noexcept {
            return e1.cross(e2).normalize();
        }
    };

    struct Node {
        nexus::render::Vec3 min;
        nexus::render::Vec3 max;
        int32_t leftChild = -1;
        int32_t firstTri = 0;
        int32_t triCount = 0;
        bool isLeaf = false;
    };

    void build(const Mesh& mesh);
    [[nodiscard]] RayHit raycast(const Ray& ray) const noexcept;
    [[nodiscard]] ClosestPointHit closestPoint(const nexus::render::Vec3& query) const noexcept;
    [[nodiscard]] bool isValid() const noexcept;

    [[nodiscard]] const std::vector<Node>& nodes() const noexcept { return m_nodes; }
    [[nodiscard]] const std::vector<Tri>& tris() const noexcept { return m_tris; }

private:
    struct BuildTri {
        nexus::render::Vec3 centroid;
        nexus::render::Vec3 aabbMin;
        nexus::render::Vec3 aabbMax;
        uint32_t srcFace = 0;
    };

    int32_t buildNode(std::vector<BuildTri>& items, int32_t first, int32_t count);
    [[nodiscard]] static bool intersectAabb(const Node& node, const Ray& ray, float& tMin, float& tMax) noexcept;
    [[nodiscard]] static bool intersectTri(const Tri& tri, const Ray& ray, float& t, float& u, float& v) noexcept;

    std::vector<Node> m_nodes;
    std::vector<Tri> m_tris;
    bool m_valid = false;
};

} // namespace nexus::geometry
