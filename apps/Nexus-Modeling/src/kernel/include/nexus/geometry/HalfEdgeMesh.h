#pragma once
// --- Nexus Geometry — HalfEdgeMesh

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

class HalfEdgeMesh {
public:
    static constexpr uint32_t kInvalid = 0xFFFFFFFFu;

    struct HEEdge { uint32_t next = kInvalid, prev = kInvalid, twin = kInvalid, src = kInvalid, face = kInvalid; };
    struct HEVertex { uint32_t edge = kInvalid; };
    struct HEFace { uint32_t edge = kInvalid; };

    [[nodiscard]] static std::optional<HalfEdgeMesh> fromMesh(const Mesh& mesh);

    Mesh toMesh() const;

    bool isManifold() const;
    bool isClosed() const;
    bool isTriangulated() const;

    uint32_t edgeCount() const { return static_cast<uint32_t>(m_edges.size()); }
    uint32_t vertexCount() const { return static_cast<uint32_t>(m_verts.size()); }
    uint32_t faceCount() const { return static_cast<uint32_t>(m_faces.size()); }

    std::vector<std::vector<uint32_t>> boundaryLoops() const;

    bool flipEdge(uint32_t he);
    bool splitEdge(uint32_t he);
    bool collapseEdge(uint32_t he, const nexus::render::Vec3& target);

    std::vector<uint32_t> vertexFaces(uint32_t v) const;
    std::vector<uint32_t> vertexNeighbors(uint32_t v) const;

    const HEEdge& edge(uint32_t i) const { return m_edges[i]; }
    const HEVertex& vertex(uint32_t i) const { return m_verts[i]; }
    const HEFace& face(uint32_t i) const { return m_faces[i]; }

    const std::vector<nexus::render::Vec3>& positions() const { return m_positions; }
    std::vector<nexus::render::Vec3>& positions() { return m_positions; }
    const std::vector<Vec2>& uvs() const { return m_uvs; }
    std::vector<Vec2>& uvs() { return m_uvs; }
    const std::vector<nexus::render::Vec3>& normals() const { return m_normals; }
    std::vector<nexus::render::Vec3>& normals() { return m_normals; }
    [[nodiscard]] bool hasUVs() const noexcept { return !m_uvs.empty(); }
    [[nodiscard]] bool hasNormals() const noexcept { return !m_normals.empty(); }

    uint32_t findEdge(uint32_t src, uint32_t dst) const;

private:
    std::vector<HEEdge> m_edges;
    std::vector<HEVertex> m_verts;
    std::vector<HEFace> m_faces;
    std::vector<nexus::render::Vec3> m_positions;
    std::vector<Vec2> m_uvs;
    std::vector<nexus::render::Vec3> m_normals;
    std::unordered_map<uint64_t, uint32_t> m_edgeMap;

    uint32_t addEdgePair(uint32_t src, uint32_t dst, uint32_t face);
    void updateEdgeMap(uint32_t ei);
};

} // namespace nexus::geometry
