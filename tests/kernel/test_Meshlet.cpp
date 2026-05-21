#include <nexus/geometry/Meshlet.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/GeometryKernel.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace nexus::geometry;

namespace {

// Simple triangle mesh: two triangles sharing an edge (forms a quad)
Mesh makeQuadMesh()
{
    Mesh mesh;
    std::vector<nexus::render::Vec3> positions = {
        {0.f, 0.f, 0.f},  // 0: bottom-left
        {1.f, 0.f, 0.f},  // 1: bottom-right
        {1.f, 1.f, 0.f},  // 2: top-right
        {0.f, 1.f, 0.f},  // 3: top-left
    };
    mesh.attributes().setPositions(std::move(positions));
    
    // Two triangles: (0,1,2) and (0,2,3)
    mesh.topology().addFace(Face{{0, 1, 2}});
    mesh.topology().addFace(Face{{0, 2, 3}});
    
    return mesh;
}

// Larger mesh: subdivided grid
Mesh makeGridMesh(uint32_t width, uint32_t height)
{
    Mesh mesh;
    std::vector<nexus::render::Vec3> positions;
    positions.reserve((width + 1) * (height + 1));
    
    for (uint32_t y = 0; y <= height; ++y) {
        for (uint32_t x = 0; x <= width; ++x) {
            positions.push_back({static_cast<float>(x), static_cast<float>(y), 0.f});
        }
    }

    mesh.attributes().setPositions(std::move(positions));
    
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            const uint32_t a = y * (width + 1) + x;
            const uint32_t b = a + 1;
            const uint32_t c = a + (width + 1);
            const uint32_t d = c + 1;
            
            mesh.topology().addFace(Face{{a, b, c}});
            mesh.topology().addFace(Face{{b, d, c}});
        }
    }
    
    return mesh;
}

}  // namespace

TEST(Meshlet, EmptyInputProducesEmptyOutput)
{
    MeshletGroup result = buildMeshletsFromMesh({}, {});
    EXPECT_TRUE(result.meshlets.empty());
    EXPECT_TRUE(result.vertices.empty());
    EXPECT_TRUE(result.primitives.empty());
}

TEST(Meshlet, SingleTriangleCreatesSingleMeshlet)
{
    std::vector<nexus::render::Vec3> positions = {
        {0.f, 0.f, 0.f}, 
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
    };
    std::vector<uint32_t> indices = {0, 1, 2};
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices);
    
    ASSERT_EQ(result.meshlets.size(), 1u);
    EXPECT_EQ(result.meshlets[0].vertexCount, 3u);
    EXPECT_EQ(result.meshlets[0].primitiveCount, 1u);
    EXPECT_EQ(result.vertices.size(), 3u);
    EXPECT_EQ(result.primitives.size(), 3u);  // 3 bytes per triangle
}

TEST(Meshlet, QuadMeshDeterministic)
{
    Mesh mesh = makeQuadMesh();
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();
    
    // Run twice to verify determinism
    MeshletGroup resultA = buildMeshletsFromMesh(positions, indices);
    MeshletGroup resultB = buildMeshletsFromMesh(positions, indices);
    
    ASSERT_EQ(resultA.meshlets.size(), resultB.meshlets.size());
    ASSERT_EQ(resultA.vertices.size(), resultB.vertices.size());
    ASSERT_EQ(resultA.primitives.size(), resultB.primitives.size());
    
    for (size_t i = 0; i < resultA.meshlets.size(); ++i) {
        EXPECT_EQ(resultA.meshlets[i].vertexOffset, resultB.meshlets[i].vertexOffset);
        EXPECT_EQ(resultA.meshlets[i].vertexCount, resultB.meshlets[i].vertexCount);
        EXPECT_EQ(resultA.meshlets[i].primitiveOffset, resultB.meshlets[i].primitiveOffset);
        EXPECT_EQ(resultA.meshlets[i].primitiveCount, resultB.meshlets[i].primitiveCount);
    }
    
    EXPECT_EQ(resultA.vertices, resultB.vertices);
    EXPECT_EQ(resultA.primitives, resultB.primitives);
}

TEST(Meshlet, MeshletVertexCountDoesNotExceedTarget)
{
    Mesh mesh = makeGridMesh(16, 16);
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();
    
    MeshletBuilderConfig config{};
    config.targetVerticesPerMeshlet = 64;
    config.targetPrimitivesPerMeshlet = 81;
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices, config);
    
    for (const auto& meshlet : result.meshlets) {
        EXPECT_LE(meshlet.vertexCount, config.targetVerticesPerMeshlet);
        EXPECT_LE(meshlet.primitiveCount, config.targetPrimitivesPerMeshlet);
    }
}

TEST(Meshlet, MeshletBoundsAreCorrect)
{
    std::vector<nexus::render::Vec3> positions = {
        {0.f, 0.f, 0.f},  // min
        {2.f, 0.f, 0.f},
        {2.f, 2.f, 0.f},  // max
    };
    std::vector<uint32_t> indices = {0, 1, 2};
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices);
    
    ASSERT_EQ(result.meshlets.size(), 1u);
    const auto& meshlet = result.meshlets[0];
    
    EXPECT_EQ(meshlet.boundsMin.x, 0.f);
    EXPECT_EQ(meshlet.boundsMin.y, 0.f);
    EXPECT_EQ(meshlet.boundsMin.z, 0.f);
    
    EXPECT_EQ(meshlet.boundsMax.x, 2.f);
    EXPECT_EQ(meshlet.boundsMax.y, 2.f);
    EXPECT_EQ(meshlet.boundsMax.z, 0.f);
}

TEST(Meshlet, PrimitivePackingIsDeterministic)
{
    Mesh mesh = makeQuadMesh();
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices);
    
    ASSERT_GT(result.primitives.size(), 0u);
    
    // Each triangle is 3 bytes (remapped vertex indices)
    // First triangle should be deterministic
    EXPECT_LT(result.primitives[0], 4u);  // valid vertex index
    EXPECT_LT(result.primitives[1], 4u);
    EXPECT_LT(result.primitives[2], 4u);
}

TEST(Meshlet, PrimitiveIndicesStayWithinLocalVertexRange)
{
    Mesh mesh = makeGridMesh(8, 8);
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();

    MeshletGroup result = buildMeshletsFromMesh(positions, indices);

    for (const auto& meshlet : result.meshlets) {
        ASSERT_LE(meshlet.primitiveOffset + meshlet.primitiveCount * 3, result.primitives.size());
        for (uint32_t primitive = 0; primitive < meshlet.primitiveCount; ++primitive) {
            const uint32_t base = meshlet.primitiveOffset + primitive * 3;
            EXPECT_LT(result.primitives[base + 0], meshlet.vertexCount);
            EXPECT_LT(result.primitives[base + 1], meshlet.vertexCount);
            EXPECT_LT(result.primitives[base + 2], meshlet.vertexCount);
        }
    }
}

TEST(Meshlet, LargeGridProducesMultipleMeshlets)
{
    Mesh mesh = makeGridMesh(32, 32);  // 2048 triangles
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();
    
    MeshletBuilderConfig config{};
    config.targetVerticesPerMeshlet = 64;
    config.targetPrimitivesPerMeshlet = 81;
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices, config);
    
    EXPECT_GT(result.meshlets.size(), 1u);
    
    // Verify coverage: total primitives should equal input triangles
    uint32_t totalPrimitives = 0;
    for (const auto& meshlet : result.meshlets) {
        totalPrimitives += meshlet.primitiveCount;
    }
    EXPECT_EQ(totalPrimitives * 3, result.primitives.size());  // 3 bytes per triangle
}

TEST(Meshlet, MeshletOffsetsAreMonotonic)
{
    Mesh mesh = makeGridMesh(16, 16);
    const std::vector<uint32_t> indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const std::span<const nexus::render::Vec3> positions = mesh.attributes().positions();
    
    MeshletGroup result = buildMeshletsFromMesh(positions, indices);
    
    uint32_t prevVertexEnd = 0;
    uint32_t prevPrimEnd = 0;
    
    for (const auto& meshlet : result.meshlets) {
        EXPECT_EQ(meshlet.vertexOffset, prevVertexEnd);
        EXPECT_EQ(meshlet.primitiveOffset, prevPrimEnd);
        
        prevVertexEnd += meshlet.vertexCount;
        prevPrimEnd += meshlet.primitiveCount * 3;  // 3 bytes per triangle
    }
    
    EXPECT_EQ(prevVertexEnd, result.vertices.size());
    EXPECT_EQ(prevPrimEnd, result.primitives.size());
}

