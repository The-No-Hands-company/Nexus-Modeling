#include <nexus/geometry/Meshlet.h>

#include <algorithm>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace nexus::geometry {

namespace {

struct MeshletBuilder {
    const MeshletBuilderConfig config;
    const std::span<const nexus::render::Vec3> positions;
    const std::span<const uint32_t> indices;
    
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> meshletVertices;
    std::vector<uint8_t> meshletPrimitives;
    
    // Track which vertices are in current meshlet
    std::unordered_set<uint32_t> currentVertices;
    std::vector<uint32_t> currentTriangleIndices;  // indices into the input index buffer
    
    MeshletBuilder(std::span<const nexus::render::Vec3> pos, std::span<const uint32_t> idx, const MeshletBuilderConfig& cfg)
        : config(cfg), positions(pos), indices(idx)
    {
        currentVertices.reserve(config.targetVerticesPerMeshlet);
        currentTriangleIndices.reserve(config.targetPrimitivesPerMeshlet);
    }
    
    void finalizeMeshlet()
    {
        if (currentTriangleIndices.empty()) {
            return;
        }
        
        // Create vertex remapping: original vertex ID -> local meshlet index
        std::unordered_map<uint32_t, uint32_t> vertexMap;
        nexus::render::Vec3 boundsMin = positions[indices[currentTriangleIndices[0]]];
        nexus::render::Vec3 boundsMax = boundsMin;
        
        for (uint32_t triIdx : currentTriangleIndices) {
            for (uint32_t i = 0; i < 3; ++i) {
                const uint32_t vertexID = indices[triIdx + i];
                if (vertexMap.find(vertexID) == vertexMap.end()) {
                    vertexMap[vertexID] = static_cast<uint32_t>(vertexMap.size());
                    meshletVertices.push_back(vertexID);
                    
                    const nexus::render::Vec3& pos = positions[vertexID];
                    boundsMin.x = std::min(boundsMin.x, pos.x);
                    boundsMin.y = std::min(boundsMin.y, pos.y);
                    boundsMin.z = std::min(boundsMin.z, pos.z);
                    boundsMax.x = std::max(boundsMax.x, pos.x);
                    boundsMax.y = std::max(boundsMax.y, pos.y);
                    boundsMax.z = std::max(boundsMax.z, pos.z);
                }
            }
        }
        
        Meshlet meshlet{};
        meshlet.vertexOffset = 0;  // will update after all meshlets
        meshlet.vertexCount = static_cast<uint32_t>(vertexMap.size());
        meshlet.primitiveOffset = static_cast<uint32_t>(meshletPrimitives.size());
        meshlet.primitiveCount = static_cast<uint32_t>(currentTriangleIndices.size());
        meshlet.boundsMin = boundsMin;
        meshlet.boundsMax = boundsMax;
        meshlet.lodLevel = 0;
        
        // Pack primitives as 3-byte triplets
        for (uint32_t triIdx : currentTriangleIndices) {
            uint8_t remapped[3];
            for (uint32_t i = 0; i < 3; ++i) {
                const uint32_t vertexID = indices[triIdx + i];
                remapped[i] = static_cast<uint8_t>(vertexMap[vertexID]);
            }
            meshletPrimitives.push_back(remapped[0]);
            meshletPrimitives.push_back(remapped[1]);
            meshletPrimitives.push_back(remapped[2]);
        }
        
        meshlets.push_back(meshlet);
        currentVertices.clear();
        currentTriangleIndices.clear();
    }
    
    bool canAddTriangle(uint32_t triIdx)
    {
        // Count new unique vertices this triangle would add
        std::unordered_set<uint32_t> newVertices;
        for (uint32_t i = 0; i < 3; ++i) {
            const uint32_t vertexID = indices[triIdx + i];
            if (currentVertices.find(vertexID) == currentVertices.end()) {
                newVertices.insert(vertexID);
            }
        }
        
        return (currentVertices.size() + newVertices.size() <= config.targetVerticesPerMeshlet) &&
               (currentTriangleIndices.size() + 1 <= config.targetPrimitivesPerMeshlet);
    }
    
    void addTriangle(uint32_t triIdx)
    {
        for (uint32_t i = 0; i < 3; ++i) {
            currentVertices.insert(indices[triIdx + i]);
        }
        currentTriangleIndices.push_back(triIdx);
    }
    
    MeshletGroup build()
    {
        const uint32_t triangleCount = static_cast<uint32_t>(indices.size()) / 3;
        
        for (uint32_t triIdx = 0; triIdx < triangleCount; ++triIdx) {
            const uint32_t indexStart = triIdx * 3;
            
            if (currentTriangleIndices.size() >= config.targetPrimitivesPerMeshlet) {
                finalizeMeshlet();
            }
            
            if (canAddTriangle(indexStart)) {
                addTriangle(indexStart);
            } else {
                finalizeMeshlet();
                if (canAddTriangle(indexStart)) {
                    addTriangle(indexStart);
                }
            }
        }
        
        finalizeMeshlet();
        
        // Update vertex offsets now that we know final layout
        uint32_t vertexOffset = 0;
        for (auto& meshlet : meshlets) {
            meshlet.vertexOffset = vertexOffset;
            vertexOffset += meshlet.vertexCount;
        }
        
        MeshletGroup result{};
        result.meshlets = std::move(meshlets);
        result.vertices = std::move(meshletVertices);
        result.primitives = std::move(meshletPrimitives);
        return result;
    }
};

}  // namespace

[[nodiscard]] MeshletGroup buildMeshletsFromMesh(
    std::span<const nexus::render::Vec3> positions,
    std::span<const uint32_t> indices,
    const MeshletBuilderConfig& config)
{
    if (positions.empty() || indices.empty() || indices.size() % 3 != 0) {
        return MeshletGroup{};
    }
    
    MeshletBuilder builder(positions, indices, config);
    return builder.build();
}

}  // namespace nexus::geometry
