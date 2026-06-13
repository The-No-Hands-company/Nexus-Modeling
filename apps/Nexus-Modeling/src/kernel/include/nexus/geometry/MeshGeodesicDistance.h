#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <vector>

namespace nexus::geometry {

struct GeodesicResult {
    std::vector<float> distances;
    std::vector<int32_t> predecessors;
};

class MeshGeodesicDistance {
public:
    [[nodiscard]] static GeodesicResult fromVertex(const Mesh& mesh, uint32_t sourceVertex);

    [[nodiscard]] static GeodesicResult fromVertices(const Mesh& mesh, const std::vector<uint32_t>& sources);
};

} // namespace nexus::geometry
