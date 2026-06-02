#include <nexus/geometry/MeshRepair.h>
#include <nexus/geometry/HalfEdgeMesh.h>
#include <nexus/geometry/Mesh.h>
#include <gtest/gtest.h>

using namespace nexus::geometry;

// ─── Helpers ──────────────────────────────────────────────────────────────────

// Single triangle — open mesh (1 boundary loop, 3 boundary edges).
static Mesh makeOpenTriangle()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({{0,0,0},{1,0,0},{0,1,0}});
    m.attributes() = std::move(attrs);
    Face f; f.indices = {0,1,2};
    m.topology().addFace(std::move(f));
    return m;
}

// Closed tetrahedron: 4 faces, all edges paired.
static Mesh makeTetrahedron()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f,  0.f,  0.f},
        {1.f,  0.f,  0.f},
        {0.5f, 1.f,  0.f},
        {0.5f, 0.5f, 1.f},
    });
    m.attributes() = std::move(attrs);
    Face f0; f0.indices = {0, 2, 1};
    Face f1; f1.indices = {0, 1, 3};
    Face f2; f2.indices = {1, 2, 3};
    Face f3; f3.indices = {0, 3, 2};
    m.topology().addFace(std::move(f0));
    m.topology().addFace(std::move(f1));
    m.topology().addFace(std::move(f2));
    m.topology().addFace(std::move(f3));
    return m;
}

// Two triangles sharing one edge — open mesh (1 boundary loop, 4 boundary edges).
static Mesh makeTwoTris()
{
    Mesh m;
    MeshAttributes attrs;
    attrs.setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.5f, 1.f, 0.f},
        {1.5f, 0.f, 0.f},
    });
    m.attributes() = std::move(attrs);
    Face f0; f0.indices = {0, 1, 2};
    Face f1; f1.indices = {1, 3, 2};
    m.topology().addFace(std::move(f0));
    m.topology().addFace(std::move(f1));
    return m;
}

// ─── Tests ────────────────────────────────────────────────────────────────────

TEST(MeshRepair, ClosedMeshNoHolesFilled)
{
    auto [repaired, report] = MeshRepair::repair(makeTetrahedron());
    EXPECT_EQ(report.holesFilled, 0u);
    EXPECT_TRUE(report.inputWasClosed);
    EXPECT_TRUE(report.outputIsClosed);
}

TEST(MeshRepair, OpenTriangleFillsOneHole)
{
    auto [repaired, report] = MeshRepair::repair(makeOpenTriangle());
    EXPECT_EQ(report.holesFilled, 1u);
    EXPECT_EQ(report.holeBoundaryEdgeCount, 3u);
    EXPECT_TRUE(report.outputIsClosed);
}

TEST(MeshRepair, FilledMeshHasMoreFaces)
{
    const auto original = makeOpenTriangle();
    auto [repaired, report] = MeshRepair::repair(original);
    EXPECT_GT(repaired.topology().faceCount(),
              original.topology().faceCount());
}

TEST(MeshRepair, FilledMeshVertexCountIncreasedByCentroid)
{
    const auto original = makeOpenTriangle();
    auto [repaired, report] = MeshRepair::repair(original);
    // Each hole gets one centroid vertex.
    EXPECT_EQ(repaired.attributes().positions().size(),
              original.attributes().positions().size() + report.holesFilled);
}

TEST(MeshRepair, TwoTriangleMeshFillsHole)
{
    auto [repaired, report] = MeshRepair::repair(makeTwoTris());
    EXPECT_EQ(report.holesFilled, 1u);
}

TEST(MeshRepair, FillDisabledLeavesOpenMeshOpen)
{
    MeshRepairOptions opts;
    opts.fillHoles = false;
    auto [repaired, report] = MeshRepair::repair(makeOpenTriangle(), opts);
    EXPECT_EQ(report.holesFilled, 0u);
}

TEST(MeshRepair, MaxHoleEdgesSkipsLargeHoles)
{
    // Triangle has 3 boundary edges; maxHoleEdges = 2 should skip it.
    MeshRepairOptions opts;
    opts.maxHoleEdges = 2;
    auto [repaired, report] = MeshRepair::repair(makeOpenTriangle(), opts);
    EXPECT_EQ(report.holesFilled, 0u);
    EXPECT_FALSE(report.warnings.empty()); // warning about skipped hole
}

TEST(MeshRepair, ReportInputWasClosedCorrect)
{
    auto [r1, rep1] = MeshRepair::repair(makeTetrahedron());
    auto [r2, rep2] = MeshRepair::repair(makeOpenTriangle());
    EXPECT_TRUE(rep1.inputWasClosed);
    EXPECT_FALSE(rep2.inputWasClosed);
}

TEST(MeshRepair, ReportNonManifoldCountZeroForCleanMesh)
{
    auto [repaired, report] = MeshRepair::repair(makeTetrahedron());
    EXPECT_EQ(report.nonManifoldEdgesFound, 0u);
}
