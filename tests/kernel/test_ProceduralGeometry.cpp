#include <nexus/eval/GeometryNodes.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/SceneGraph.h>

#include <gtest/gtest.h>

using namespace nexus;
using namespace nexus::eval;
using namespace nexus::geometry;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

static Mesh makeSimpleMesh(float translation)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f + translation, 0.f, 0.f},
        {1.f + translation, 0.f, 0.f},
        {0.f + translation, 1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0, 1, 2}});
    return mesh;
}

// ─────────────────────────────────────────────────────────────────────────────
//  GeometryTransformNode Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralGeometry, GeometryTransformNodeAppliesTranslation)
{
    const Mesh inputMesh = makeSimpleMesh(0.f);

    nexus::render::Transform transform;
    transform.translation = {5.f, 2.f, 0.f};
    transform.rotation = {0.f, 0.f, 0.f, 1.f};
    transform.scale = {1.f, 1.f, 1.f};

    Mesh outputMesh;
    ASSERT_TRUE(GeometryTransformNode::compute(inputMesh, transform, outputMesh));

    const auto& positions = outputMesh.attributes().positions();
    ASSERT_EQ(positions.size(), 3u);
    EXPECT_NEAR(positions[0].x, 5.f, 1e-5f);
    EXPECT_NEAR(positions[0].y, 2.f, 1e-5f);
    EXPECT_NEAR(positions[1].x, 6.f, 1e-5f);
    EXPECT_NEAR(positions[2].y, 3.f, 1e-5f);
}

TEST(ProceduralGeometry, GeometryTransformNodeAppliesScale)
{
    const Mesh inputMesh = makeSimpleMesh(0.f);

    nexus::render::Transform transform;
    transform.translation = {0.f, 0.f, 0.f};
    transform.rotation = {0.f, 0.f, 0.f, 1.f};
    transform.scale = {2.f, 3.f, 1.f};

    Mesh outputMesh;
    ASSERT_TRUE(GeometryTransformNode::compute(inputMesh, transform, outputMesh));

    const auto& positions = outputMesh.attributes().positions();
    ASSERT_EQ(positions.size(), 3u);
    EXPECT_NEAR(positions[0].x, 0.f, 1e-5f);
    EXPECT_NEAR(positions[1].x, 2.f, 1e-5f);
    EXPECT_NEAR(positions[2].y, 3.f, 1e-5f);
}

TEST(ProceduralGeometry, GeometryTransformNodePreserveTopology)
{
    const Mesh inputMesh = makeSimpleMesh(0.f);

    nexus::render::Transform transform;
    transform.translation = {1.f, 0.f, 0.f};

    Mesh outputMesh;
    ASSERT_TRUE(GeometryTransformNode::compute(inputMesh, transform, outputMesh));

    EXPECT_EQ(outputMesh.topology().faceCount(), inputMesh.topology().faceCount());
}

TEST(ProceduralGeometry, GeometryTransformIsDeterministicAcrossRuns)
{
    const Mesh inputMesh = makeSimpleMesh(0.f);

    nexus::render::Transform transform;
    transform.translation = {3.5f, -2.1f, 1.5f};
    transform.rotation = {0.f, 0.f, 0.f, 1.f};
    transform.scale = {1.2f, 0.8f, 1.f};

    Mesh run1, run2;
    ASSERT_TRUE(GeometryTransformNode::compute(inputMesh, transform, run1));
    ASSERT_TRUE(GeometryTransformNode::compute(inputMesh, transform, run2));

    const auto& pos1 = run1.attributes().positions();
    const auto& pos2 = run2.attributes().positions();
    
    EXPECT_EQ(pos1.size(), pos2.size());
    for (size_t i = 0; i < pos1.size(); ++i) {
        EXPECT_FLOAT_EQ(pos1[i].x, pos2[i].x);
        EXPECT_FLOAT_EQ(pos1[i].y, pos2[i].y);
        EXPECT_FLOAT_EQ(pos1[i].z, pos2[i].z);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  GeometryMergeNode Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ProceduralGeometry, GeometryMergeNodeCombinesVertices)
{
    const Mesh meshA = makeSimpleMesh(0.f);
    const Mesh meshB = makeSimpleMesh(10.f);

    Mesh outputMesh;
    ASSERT_TRUE(GeometryMergeNode::compute(meshA, meshB, outputMesh));

    const auto& positions = outputMesh.attributes().positions();
    EXPECT_EQ(positions.size(), 6u);
    // First 3 from A
    EXPECT_NEAR(positions[0].x, 0.f, 1e-5f);
    EXPECT_NEAR(positions[1].x, 1.f, 1e-5f);
    // Next 3 from B
    EXPECT_NEAR(positions[3].x, 10.f, 1e-5f);
    EXPECT_NEAR(positions[4].x, 11.f, 1e-5f);
}

TEST(ProceduralGeometry, GeometryMergeNodePreservesTopologyCount)
{
    const Mesh meshA = makeSimpleMesh(0.f);
    const Mesh meshB = makeSimpleMesh(10.f);

    Mesh outputMesh;
    ASSERT_TRUE(GeometryMergeNode::compute(meshA, meshB, outputMesh));

    EXPECT_EQ(outputMesh.topology().faceCount(), 
              meshA.topology().faceCount() + meshB.topology().faceCount());
}

TEST(ProceduralGeometry, GeometryMergeIsDeterministicAcrossRuns)
{
    const Mesh meshA = makeSimpleMesh(1.5f);
    const Mesh meshB = makeSimpleMesh(3.2f);

    Mesh run1, run2;
    ASSERT_TRUE(GeometryMergeNode::compute(meshA, meshB, run1));
    ASSERT_TRUE(GeometryMergeNode::compute(meshA, meshB, run2));

    const auto& pos1 = run1.attributes().positions();
    const auto& pos2 = run2.attributes().positions();
    
    EXPECT_EQ(pos1.size(), pos2.size());
    for (size_t i = 0; i < pos1.size(); ++i) {
        EXPECT_FLOAT_EQ(pos1[i].x, pos2[i].x);
        EXPECT_FLOAT_EQ(pos1[i].y, pos2[i].y);
        EXPECT_FLOAT_EQ(pos1[i].z, pos2[i].z);
    }
}

TEST(ProceduralGeometry, GeometryMergeCommutativityProducesDistinctResults)
{
    const Mesh meshA = makeSimpleMesh(0.f);
    const Mesh meshB = makeSimpleMesh(5.f);

    Mesh ab, ba;
    ASSERT_TRUE(GeometryMergeNode::compute(meshA, meshB, ab));
    ASSERT_TRUE(GeometryMergeNode::compute(meshB, meshA, ba));

    const auto& pos_ab = ab.attributes().positions();
    const auto& pos_ba = ba.attributes().positions();
    
    // Merge is not commutative: order matters.
    EXPECT_NE(pos_ab[0].x, pos_ba[0].x);
}

TEST(ProceduralGeometry, GeometryNodeKindsAreDefined)
{
    EXPECT_EQ(GeometryTransformNode::kind(), GeometryNodeKind::Transform);
    EXPECT_EQ(GeometryMergeNode::kind(), GeometryNodeKind::Merge);
}

TEST(ProceduralGeometry, GeometryNodeNamesAreDefined)
{
    EXPECT_EQ(std::string(GeometryTransformNode::name()), "GeometryTransform");
    EXPECT_EQ(std::string(GeometryMergeNode::name()), "GeometryMerge");
}
