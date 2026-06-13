#pragma once

#include <nexus/geometry/HalfEdgeMesh.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <unordered_map>

namespace nexus::geometry {

struct CreaseEdgeSet {
    std::unordered_map<uint64_t, float> edges;

    void set(uint32_t a, uint32_t b, float sharpness = 1.0f) {
        uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | static_cast<uint64_t>(std::max(a, b));
        if (sharpness > 0.0f) {
            edges[key] = sharpness;
        } else {
            edges.erase(key);
        }
    }

    [[nodiscard]] float get(uint32_t a, uint32_t b) const {
        uint64_t key = (static_cast<uint64_t>(std::min(a, b)) << 32) | static_cast<uint64_t>(std::max(a, b));
        auto it = edges.find(key);
        return it != edges.end() ? it->second : 0.0f;
    }

    [[nodiscard]] float get(uint64_t key) const {
        auto it = edges.find(key);
        return it != edges.end() ? it->second : 0.0f;
    }

    [[nodiscard]] bool empty() const { return edges.empty(); }
};

struct SubdivisionOptions {
    uint32_t levels = 1;
    CreaseEdgeSet creases;
};

class SubdivisionSurface {
public:
    [[nodiscard]] static std::optional<HalfEdgeMesh>
    catmullClark(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts = {});

    [[nodiscard]] static std::optional<HalfEdgeMesh>
    loopSubdivide(const HalfEdgeMesh& mesh, const SubdivisionOptions& opts = {});
};

} // namespace nexus::geometry
