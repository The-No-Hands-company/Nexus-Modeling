#pragma once

#include <nexus/geometry/Mesh.h>

#include <cstdint>

namespace nexus::geometry {

struct TopologyInfo {
    uint32_t vertices = 0;
    uint32_t edges = 0;
    uint32_t faces = 0;
    uint32_t components = 0;
    uint32_t boundaryLoops = 0;
    int euler = 0;
    int genus = 0;
    bool isManifold = false;
    bool isClosed = false;
    bool isTriangulated = false;
};

class MeshTopologyAnalyser {
public:
    [[nodiscard]] static TopologyInfo analyse(const Mesh& mesh);
};

} // namespace nexus::geometry
