#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Geometry — Meshlets for mesh shader amplification
//
//  Meshlets are compact GPU-friendly groupings of vertices and primitives
//  for use with EXT_mesh_shader extensions. Each meshlet contains:
//  - A range of vertices from the original mesh (capped at hardware max ~64-128)
//  - A range of primitive indices (capped at hardware max ~81-128)
//  - Bounds for frustum culling
//  - LOD level for future multi-LOD support
// ─────────────────────────────────────────────────────────────────────────────

#include <nexus/render/Camera.h>

#include <cstdint>
#include <span>
#include <vector>

namespace nexus::geometry {

// ─────────────────────────────────────────────────────────────────────────────
//  Meshlet — compact GPU-friendly grouping of geometry
// ─────────────────────────────────────────────────────────────────────────────
struct Meshlet {
    uint32_t vertexOffset  = 0;    // start index in meshlet vertex list
    uint32_t vertexCount   = 0;    // number of unique verts in this meshlet
    uint32_t primitiveOffset = 0;  // start index in meshlet primitive list
    uint32_t primitiveCount  = 0;  // number of primitives (triangles / quads)
    
    // Bounds for frustum culling (world-space will be computed per instance)
    nexus::render::Vec3 boundsMin = {0.f, 0.f, 0.f};
    nexus::render::Vec3 boundsMax = {0.f, 0.f, 0.f};
    
    uint32_t lodLevel = 0;  // reserved for LOD selection; always 0 for now
};

struct MeshletGroup {
    std::vector<Meshlet> meshlets;
    std::vector<uint32_t> vertices;    // vertex indices remapped for meshlet locality
    std::vector<uint8_t> primitives;   // packed triangle indices (3 bytes per triangle)
};

// ─────────────────────────────────────────────────────────────────────────────
//  Meshlet generation contract
// ─────────────────────────────────────────────────────────────────────────────
//
// buildMeshletsFromMesh:
//   Input: triangle mesh (vertex list + index buffer)
//   Output: MeshletGroup (self-contained GPU buffer layout)
//
// Guarantees:
//   - Meshlets are deterministically ordered (same input always produces same meshlets)
//   - Vertex locality: each meshlet reindexes to local indices for L1 cache efficiency
//   - Primitive packing: 3-byte triplet per triangle for GPU unpacking
//   - Bounds are computed per-meshlet in local space (will be transformed per instance)
//   - No meshlet exceeds hardware limits (64 verts / 81 primitives assumed; caller adjusts)
//
struct MeshletBuilderConfig {
    uint32_t targetVerticesPerMeshlet  = 64;
    uint32_t targetPrimitivesPerMeshlet = 81;
    bool     computeBounds = true;
};

[[nodiscard]] MeshletGroup buildMeshletsFromMesh(
    std::span<const nexus::render::Vec3> positions,
    std::span<const uint32_t> indices,
    const MeshletBuilderConfig& config = {});

} // namespace nexus::geometry
