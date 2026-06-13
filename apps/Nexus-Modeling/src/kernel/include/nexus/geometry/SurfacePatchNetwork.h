#pragma once
// ── Nexus Geometry — SurfacePatchNetwork

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/NurbsSurface.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nexus::geometry {

class SurfacePatchNetwork {
public:
    struct EdgeId {
        int32_t patch = -1;
        int32_t edge  = -1;

        bool operator==(const EdgeId&) const = default;
    };

    struct Connection {
        EdgeId a;
        EdgeId b;
    };

    void addPatch(const NurbsSurface& surface);

    void connect(int32_t patchA, int32_t edgeA, int32_t patchB, int32_t edgeB);

    [[nodiscard]] std::vector<EdgeId> neighbors(int32_t patch) const;

    [[nodiscard]] Mesh tessellate(int32_t samplesU, int32_t samplesV) const;

    [[nodiscard]] size_t patchCount() const noexcept { return m_patches.size(); }

private:
    std::vector<NurbsSurface> m_patches;
    std::vector<Connection> m_connections;
};

} // namespace nexus::geometry
