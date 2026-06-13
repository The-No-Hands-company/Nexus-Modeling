#pragma once
// --- Nexus Geometry — MeshRepair

#include <nexus/geometry/Mesh.h>

#include <cstdint>
#include <string>

namespace nexus::geometry {

struct RepairOptions {
    bool fillHoles = true;
    bool resolveNonManifold = false;
    uint32_t maxHoleEdges = 100;
};

struct RepairReport {
    bool ok = false;
    std::string error;
    uint32_t holesFilled = 0;
    uint32_t nonManifoldEdges = 0;
    uint32_t degenerateFaces = 0;
    bool duplicateVerticesRemoved = false;
    uint32_t zeroAreaFacesRemoved = 0;
    uint32_t isolatedVerticesRemoved = 0;
    bool faceOrientationFixed = false;
    bool holesFilledOp = false;
};

class MeshRepair {
public:
    [[nodiscard]] static RepairReport repair(Mesh& mesh, const RepairOptions& opts = {});

    [[nodiscard]] static bool removeDuplicateVertices(Mesh& mesh);
    [[nodiscard]] static bool removeZeroAreaFaces(Mesh& mesh, float minArea = 1e-8f);
    [[nodiscard]] static bool removeIsolatedVertices(Mesh& mesh);
    [[nodiscard]] static bool fixFaceOrientation(Mesh& mesh);
    [[nodiscard]] static bool fillHoles(Mesh& mesh, uint32_t maxHoleSize = 100);
    [[nodiscard]] static RepairReport repairAll(Mesh& mesh);
};

} // namespace nexus::geometry
