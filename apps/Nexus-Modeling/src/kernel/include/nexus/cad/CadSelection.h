#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus CAD — Selection Manager
//
//  Tracks what the user has selected: faces, edges, vertices, sketch entities.
//  Supports ray-cast picking using the mesh BVH.
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nexus::cad {

using Vec3 = nexus::render::Vec3;

enum class SelectionKind : uint8_t {
    None,
    Face,
    Edge,
    Vertex,
    SketchEntity,
};

struct SelectionItem {
    SelectionKind kind = SelectionKind::None;
    uint32_t      index = 0;          // face/edge/vertex index
    std::string   label;
};

class CadSelection {
public:
    // ── Selection state ─────────────────────────────────────────────
    void clear();
    void addFace(uint32_t faceIndex);
    void addEdge(uint32_t edgeIndex);
    void addVertex(uint32_t vertexIndex);
    void addSketchEntity(uint32_t entityId);

    [[nodiscard]] const std::vector<SelectionItem>& items() const noexcept { return m_items; }
    [[nodiscard]] bool empty() const noexcept { return m_items.empty(); }
    [[nodiscard]] size_t count() const noexcept { return m_items.size(); }

    // ── Ray-cast picking ────────────────────────────────────────────
    // Pick the closest face in 'mesh' intersected by the ray.
    // Returns the face index, or -1 if no hit.
    [[nodiscard]] int32_t pickFace(const geometry::Mesh& mesh,
                                    const Vec3& rayOrigin,
                                    const Vec3& rayDirection) const noexcept;

    // ── Bounds-based selection ──────────────────────────────────────
    // Select all faces whose centroid falls within a screen-space rectangle.
    [[nodiscard]] std::vector<uint32_t> pickFacesInRect(
        const geometry::Mesh& mesh,
        const Vec3& rectMin,
        const Vec3& rectMax) const noexcept;

private:
    std::vector<SelectionItem> m_items;
};

} // namespace nexus::cad
