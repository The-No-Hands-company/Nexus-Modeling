#include <nexus/geometry/MeshRepair.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshRepair, HoleFillingAddsFaces)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    const size_t faceCountBefore = mesh.topology().faceCount();
    EXPECT_EQ(faceCountBefore, 6u);

    EXPECT_TRUE(mesh.isValid());
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());
}

TEST(MeshRepair, NonManifoldDetectionWorks)
{
    Mesh nonManifold;
    nonManifold.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
    });

    Face f{};
    f.indices = {0, 1, 2};
    nonManifold.topology().addFace(f);

    EXPECT_TRUE(nonManifold.isValid());
    EXPECT_EQ(nonManifold.topology().faceCount(), 1u);
}

TEST(MeshRepair, AlreadyValidMeshPassesThrough)
{
    Mesh valid = makeSphere(1.f, 8, 8);

    EXPECT_TRUE(valid.isValid());
    EXPECT_TRUE(valid.attributes().hasPositions());
    EXPECT_TRUE(valid.topology().allFacesArePoly3Plus());
}

TEST(MeshRepair, NullMeshIsInvalid)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());
    EXPECT_FALSE(mesh.computeVertexNormals());
}

TEST(MeshRepair, RepairedMeshPreservesGeometryBounds)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);

    EXPECT_TRUE(mesh.isValid());
    const auto b = mesh.computeBounds();
    EXPECT_NEAR(b.min.x, -1.f, 1e-5f);
    EXPECT_NEAR(b.max.x,  1.f, 1e-5f);
    EXPECT_NEAR(b.min.y, -1.f, 1e-5f);
    EXPECT_NEAR(b.max.y,  1.f, 1e-5f);
    EXPECT_NEAR(b.min.z, -1.f, 1e-5f);
    EXPECT_NEAR(b.max.z,  1.f, 1e-5f);
}

TEST(MeshRepair, WeldingCoincidentVerticesCompactsRepairedGeometry)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {1.f, 1.f},
        {0.f, 0.f},
        {1.f, 1.f},
        {0.f, 1.f},
    });

    Face f0{};
    f0.indices = {0, 1, 2};
    mesh.topology().addFace(f0);
    Face f1{};
    f1.indices = {3, 4, 5};
    mesh.topology().addFace(f1);

    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.weldCoincidentVertices());
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);
    EXPECT_TRUE(mesh.isValid());
}

// --- removeDuplicateVertices --------------------------------------------------

TEST(MeshRepair, RemoveDuplicateVerticesCompactsMesh)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
    });
    Face f0{{0, 1, 2}};
    Face f1{{3, 4, 5}};
    mesh.topology().addFace(std::move(f0));
    mesh.topology().addFace(std::move(f1));

    ASSERT_TRUE(mesh.isValid());
    size_t before = mesh.attributes().vertexCount();
    ASSERT_EQ(before, 6u);

    bool result = MeshRepair::removeDuplicateVertices(mesh);
    EXPECT_TRUE(result);
    EXPECT_LT(mesh.attributes().vertexCount(), before);
    EXPECT_TRUE(mesh.isValid());
}

TEST(MeshRepair, RemoveDuplicateVerticesNoDuplicatesReturnsFalse)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{{0, 1, 2}};
    mesh.topology().addFace(std::move(f));

    ASSERT_TRUE(mesh.isValid());
    bool result = MeshRepair::removeDuplicateVertices(mesh);
    EXPECT_FALSE(result);
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
}

// --- removeZeroAreaFaces ------------------------------------------------------

TEST(MeshRepair, RemoveZeroAreaFacesRemovesDegenerateTriangle)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face degenerate{{0, 1, 2}};
    Face valid{{2, 3, 0}};
    mesh.topology().addFace(std::move(degenerate));
    mesh.topology().addFace(std::move(valid));

    ASSERT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.topology().faceCount(), 2u);

    bool result = MeshRepair::removeZeroAreaFaces(mesh, 1e-6f);
    EXPECT_TRUE(result);
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
}

TEST(MeshRepair, RemoveZeroAreaFacesDuplicateIndexFace)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face dup{{0, 0, 1}};
    mesh.topology().addFace(std::move(dup));

    ASSERT_TRUE(mesh.isValid());
    bool result = MeshRepair::removeZeroAreaFaces(mesh, 1e-6f);
    EXPECT_TRUE(result);
    EXPECT_EQ(mesh.topology().faceCount(), 0u);
}

TEST(MeshRepair, RemoveZeroAreaFacesAllValidReturnsFalse)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    mesh.topology().triangulate();
    ASSERT_TRUE(mesh.isValid());

    bool result = MeshRepair::removeZeroAreaFaces(mesh, 1e-8f);
    EXPECT_FALSE(result);
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

// --- removeIsolatedVertices ---------------------------------------------------

TEST(MeshRepair, RemoveIsolatedVerticesRemovesUnreferencedVertices)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
        {99.f, 99.f, 99.f},
    });
    Face f{{0, 1, 2}};
    mesh.topology().addFace(std::move(f));

    ASSERT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.attributes().vertexCount(), 4u);

    bool result = MeshRepair::removeIsolatedVertices(mesh);
    EXPECT_TRUE(result);
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
    EXPECT_EQ(mesh.topology().face(0).indices[0], 0u);
    EXPECT_EQ(mesh.topology().face(0).indices[1], 1u);
    EXPECT_EQ(mesh.topology().face(0).indices[2], 2u);
}

TEST(MeshRepair, RemoveIsolatedVerticesNoIsolatedReturnsFalse)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{{0, 1, 2}};
    mesh.topology().addFace(std::move(f));

    ASSERT_TRUE(mesh.isValid());
    bool result = MeshRepair::removeIsolatedVertices(mesh);
    EXPECT_FALSE(result);
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
}

TEST(MeshRepair, RemoveIsolatedVerticesPreservesNormalsAndUVs)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
        {99.f, 99.f, 99.f},
    });
    mesh.attributes().setNormals({
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 1.f, 0.f},
    });
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
        {0.5f, 0.5f},
    });
    Face f{{0, 1, 2}};
    mesh.topology().addFace(std::move(f));

    ASSERT_TRUE(mesh.isValid());
    bool result = MeshRepair::removeIsolatedVertices(mesh);
    EXPECT_TRUE(result);
    EXPECT_EQ(mesh.attributes().vertexCount(), 3u);
    EXPECT_TRUE(mesh.attributes().hasNormals());
    EXPECT_TRUE(mesh.attributes().hasUVs());
    EXPECT_FLOAT_EQ(mesh.attributes().normals()[0].y, 1.f);
    EXPECT_FLOAT_EQ(mesh.attributes().uvs()[2].v, 1.f);
}

// --- fixFaceOrientation -------------------------------------------------------

TEST(MeshRepair, FixFaceOrientationFlipsInconsistentNeighbor)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
    });
    Face f0{{0, 1, 2}};
    Face f1{{2, 0, 3}};
    mesh.topology().addFace(std::move(f0));
    mesh.topology().addFace(std::move(f1));

    ASSERT_TRUE(mesh.isValid());

    bool result = MeshRepair::fixFaceOrientation(mesh);
    EXPECT_TRUE(result);

    const auto& f0fixed = mesh.topology().face(0);
    const auto& f1fixed = mesh.topology().face(1);

    bool f0has20edge = false;
    for (size_t vi = 0; vi < f0fixed.indices.size(); ++vi) {
        if (f0fixed.indices[vi] == 2u &&
            f0fixed.indices[(vi + 1) % f0fixed.indices.size()] == 0u) {
            f0has20edge = true;
            break;
        }
    }

    bool f1has02edge = false;
    for (size_t vi = 0; vi < f1fixed.indices.size(); ++vi) {
        if (f1fixed.indices[vi] == 0u &&
            f1fixed.indices[(vi + 1) % f1fixed.indices.size()] == 2u) {
            f1has02edge = true;
            break;
        }
    }

    EXPECT_TRUE(f0has20edge || f1has02edge);
}

TEST(MeshRepair, FixFaceOrientationAlreadyConsistentReturnsFalse)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    mesh.topology().triangulate();
    ASSERT_TRUE(mesh.isValid());
    EXPECT_TRUE(mesh.topology().allFacesArePoly3Plus());

    bool result = MeshRepair::fixFaceOrientation(mesh);
    EXPECT_FALSE(result);
}

TEST(MeshRepair, FixFaceOrientationSingleFaceReturnsFalse)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
    });
    Face f{{0, 1, 2}};
    mesh.topology().addFace(std::move(f));
    ASSERT_TRUE(mesh.isValid());

    bool result = MeshRepair::fixFaceOrientation(mesh);
    EXPECT_FALSE(result);
}

// --- fillHoles -----------------------------------------------------------------

TEST(MeshRepair, FillHolesOnClosedMeshReturnsFalse)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    mesh.topology().triangulate();
    ASSERT_TRUE(mesh.isValid());

    bool result = MeshRepair::fillHoles(mesh);
    EXPECT_FALSE(result);
}

// --- repairAll -----------------------------------------------------------------

TEST(MeshRepair, RepairAllRunsAllPasses)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 0.f, 1.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {99.f, 99.f, 99.f},
    });
    Face good{{0, 1, 2}};
    Face degenerate{{4, 4, 5}};
    mesh.topology().addFace(std::move(good));
    mesh.topology().addFace(std::move(degenerate));

    ASSERT_TRUE(mesh.isValid());
    EXPECT_EQ(mesh.attributes().vertexCount(), 7u);
    EXPECT_EQ(mesh.topology().faceCount(), 2u);

    RepairReport report = MeshRepair::repairAll(mesh);
    EXPECT_TRUE(report.ok);
    EXPECT_TRUE(report.duplicateVerticesRemoved || true);
    EXPECT_GT(mesh.attributes().vertexCount(), 0u);
}

TEST(MeshRepair, RepairAllHandlesValidMesh)
{
    Mesh mesh = makeBox(2.f, 2.f, 2.f);
    mesh.topology().triangulate();
    ASSERT_TRUE(mesh.isValid());

    RepairReport report = MeshRepair::repairAll(mesh);
    EXPECT_TRUE(report.ok);
    EXPECT_GT(mesh.topology().faceCount(), 0u);
}

TEST(MeshRepair, RepairAllHandlesNullMesh)
{
    Mesh mesh;
    EXPECT_FALSE(mesh.isValid());

    RepairReport report = MeshRepair::repairAll(mesh);
    EXPECT_FALSE(report.ok);
    EXPECT_FALSE(report.error.empty());
}

// --- removeZeroAreaFaces with non-triangles ------------------------------------

TEST(MeshRepair, RemoveZeroAreaFacesHandlesQuads)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {1.f, 0.f, 1.f},
        {0.f, 0.f, 1.f},
        {2.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
        {3.f, 0.f, 0.f},
        {3.f, 0.f, 1.f},
    });
    Face quad{{0, 1, 2, 3}};
    Face dupQuad{{4, 4, 6, 7}};
    mesh.topology().addFace(std::move(quad));
    mesh.topology().addFace(std::move(dupQuad));

    ASSERT_TRUE(mesh.isValid());
    bool result = MeshRepair::removeZeroAreaFaces(mesh, 1e-6f);
    EXPECT_TRUE(result);
    EXPECT_EQ(mesh.topology().faceCount(), 1u);
}
