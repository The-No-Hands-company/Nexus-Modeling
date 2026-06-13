#include <gtest/gtest.h>

#include <nexus/geometry/HardSurfaceWorkflow.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/geometry/Mesh.h>

namespace {

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(HardSurfaceWorkflow, CreateBoxProducesValidMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    EXPECT_TRUE(wf.isValid());
    EXPECT_GT(wf.mesh().attributes().vertexCount(), 0u);
    EXPECT_GT(wf.mesh().topology().faceCount(), 0u);

    const auto reports = wf.stepReports();
    EXPECT_FALSE(reports.empty());
}

TEST(HardSurfaceWorkflow, ApplyBevelReturnsValidResult)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    BevelChamferDesc desc;
    desc.mode              = BevelChamferMode::Bevel;
    desc.distance          = 0.05f;
    desc.sharpAngleDegrees = 40.0f;

    wf.bevelEdges(desc);

    EXPECT_TRUE(wf.isValid());

    const auto reports = wf.stepReports();
    ASSERT_GE(reports.size(), 2u);

    bool hasBevelReport = false;
    for (const auto& r : reports) {
        if (r.kind == HardSurfaceStepKind::BevelChamfer) {
            hasBevelReport = true;
            break;
        }
    }
    EXPECT_TRUE(hasBevelReport);
}

TEST(HardSurfaceWorkflow, WorkflowProducesValidMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));
    wf.rebuildNormals();

    BevelChamferDesc bevelDesc;
    bevelDesc.mode              = BevelChamferMode::Bevel;
    bevelDesc.distance          = 0.05f;
    bevelDesc.sharpAngleDegrees = 40.0f;
    wf.bevelEdges(bevelDesc);

    RemeshDesc remeshDesc;
    remeshDesc.targetEdgeLength = 0.5f;
    remeshDesc.maxIterations    = 1;
    wf.remesh(remeshDesc);

    EXPECT_TRUE(wf.isValid());
    EXPECT_TRUE(wf.mesh().isValid());
    EXPECT_GT(wf.mesh().attributes().vertexCount(), 0u);
    EXPECT_GT(wf.mesh().topology().faceCount(), 0u);
}

TEST(HardSurfaceWorkflow, RebuildReplaysOperations)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    BevelChamferDesc desc;
    desc.mode              = BevelChamferMode::Bevel;
    desc.distance          = 0.05f;
    desc.sharpAngleDegrees = 40.0f;

    size_t origVerts = wf.mesh().attributes().vertexCount();

    wf.bevelEdges(desc);
    const Mesh& result = wf.rebuild();

    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.attributes().vertexCount(), origVerts);
    EXPECT_EQ(wf.operationCount(), 2u);
}

TEST(HardSurfaceWorkflow, OperationCountTracksAllOps)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    EXPECT_EQ(wf.operationCount(), 1u);

    wf.rebuildNormals();
    EXPECT_EQ(wf.operationCount(), 2u);

    wf.bevelSelectedEdges(0.05f, 1);
    EXPECT_EQ(wf.operationCount(), 3u);

    wf.extrudeSelectedFaces(0.1f);
    EXPECT_EQ(wf.operationCount(), 4u);

    wf.insetSelectedFaces(0.05f);
    EXPECT_EQ(wf.operationCount(), 5u);

    RemeshDesc remeshDesc;
    remeshDesc.targetEdgeLength = 0.5f;
    wf.remesh(remeshDesc);
    EXPECT_EQ(wf.operationCount(), 6u);
}

TEST(HardSurfaceWorkflow, EdgeSelection)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    EXPECT_FALSE(wf.hasSelectedEdges());

    wf.selectEdge(5);
    EXPECT_TRUE(wf.hasSelectedEdges());
    EXPECT_TRUE(wf.selectedEdges().count(5));

    wf.clearEdgeSelection();
    EXPECT_FALSE(wf.hasSelectedEdges());

    wf.selectEdges({1, 2, 3, 4});
    EXPECT_TRUE(wf.hasSelectedEdges());
    EXPECT_EQ(wf.selectedEdges().size(), 4u);
}

TEST(HardSurfaceWorkflow, FaceSelection)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    EXPECT_FALSE(wf.hasSelectedFaces());

    wf.selectFace(0);
    EXPECT_TRUE(wf.hasSelectedFaces());
    EXPECT_TRUE(wf.selectedFaces().count(0));

    wf.clearFaceSelection();
    EXPECT_FALSE(wf.hasSelectedFaces());

    wf.selectFaces({0, 1, 2});
    EXPECT_EQ(wf.selectedFaces().size(), 3u);
}

TEST(HardSurfaceWorkflow, EdgeLoopSelection)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    auto heOpt = HalfEdgeMesh::fromMesh(wf.mesh());
    ASSERT_TRUE(heOpt.has_value());
    const auto& hem = *heOpt;
    ASSERT_GT(hem.edgeCount(), 0u);

    wf.selectEdgeLoop(0);
    // On a box, an edge loop should select multiple edges.
    EXPECT_GE(wf.selectedEdges().size(), 2u);
}

TEST(HardSurfaceWorkflow, EdgeRingSelection)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    auto heOpt = HalfEdgeMesh::fromMesh(wf.mesh());
    ASSERT_TRUE(heOpt.has_value());
    const auto& hem = *heOpt;
    ASSERT_GT(hem.edgeCount(), 0u);

    wf.selectEdgeRing(0);
    EXPECT_GE(wf.selectedEdges().size(), 2u);
}

TEST(HardSurfaceWorkflow, ExtrudeSelectedFacesModifiesMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    size_t origVerts = wf.mesh().attributes().vertexCount();
    size_t origFaces = wf.mesh().topology().faceCount();

    wf.selectFace(0);
    wf.extrudeSelectedFaces(0.5f);

    const Mesh& result = wf.rebuild();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.topology().faceCount(), origFaces);
    EXPECT_GT(result.attributes().vertexCount(), origVerts);
    EXPECT_EQ(wf.operationCount(), 2u);
}

TEST(HardSurfaceWorkflow, InsetSelectedFacesModifiesMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    size_t origVerts = wf.mesh().attributes().vertexCount();

    wf.selectFace(0);
    wf.insetSelectedFaces(0.1f);

    const Mesh& result = wf.rebuild();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.attributes().vertexCount(), origVerts);
    EXPECT_EQ(wf.operationCount(), 2u);
}

TEST(HardSurfaceWorkflow, BevelSelectedEdgesProducesValidMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    wf.selectEdge(0);
    wf.bevelSelectedEdges(0.05f, 1);

    const Mesh& result = wf.rebuild();
    EXPECT_TRUE(result.isValid());
    EXPECT_GT(result.attributes().vertexCount(), 0u);
}

TEST(HardSurfaceWorkflow, RebuildMultipleOpsChain)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    wf.selectFace(0);
    wf.extrudeSelectedFaces(0.2f);
    wf.insetSelectedFaces(0.05f);

    RemeshDesc remeshDesc;
    remeshDesc.targetEdgeLength = 0.5f;
    remeshDesc.maxIterations    = 1;
    wf.remesh(remeshDesc);

    const Mesh& result = wf.rebuild();
    EXPECT_TRUE(result.isValid());

    EXPECT_EQ(wf.operationCount(), 4u);

    const auto& ops = wf.operations();
    EXPECT_EQ(ops.size(), 4u);
    EXPECT_EQ(ops[0].kind, HardSurfaceOpKind::Init);
    EXPECT_TRUE(ops[0].success);

    const auto reports = wf.stepReports();
    EXPECT_GE(reports.size(), 4u);
}

TEST(HardSurfaceWorkflow, PerStepSuccessFailureInReport)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(1.0f, 1.0f, 1.0f));

    wf.bevelEdges(BevelChamferDesc{});
    wf.rebuild();

    const auto reports = wf.stepReports();
    ASSERT_GE(reports.size(), 2u);

    bool sawSuccess = false;
    for (const auto& r : reports) {
        if (r.success) sawSuccess = true;
    }
    EXPECT_TRUE(sawSuccess);
}

TEST(HardSurfaceWorkflow, EmptySelectionExtrudeDoesNoHarm)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    size_t origVerts = wf.mesh().attributes().vertexCount();

    wf.extrudeSelectedFaces(0.5f);
    // No faces selected — extrude should not modify mesh
    const Mesh& result = wf.rebuild();
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.attributes().vertexCount(), origVerts);
}

TEST(HardSurfaceWorkflow, ReportsIncludeOperationCount)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    wf.rebuildNormals();
    wf.bevelSelectedEdges(0.03f, 1);

    EXPECT_EQ(wf.operationCount(), 3u);

    wf.rebuild();

    const auto reports = wf.stepReports();
    EXPECT_GE(reports.size(), 3u);
}

TEST(HardSurfaceWorkflow, OperationsVectorMatchesCount)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    wf.rebuildNormals();
    wf.bevelSelectedEdges(0.05f, 2);
    wf.extrudeSelectedFaces(0.3f);

    const auto& ops = wf.operations();
    EXPECT_EQ(ops.size(), 4u);
    EXPECT_EQ(wf.operationCount(), 4u);
}

TEST(HardSurfaceWorkflow, RebuildIsIdempotent)
{
    HardSurfaceWorkflow wf;
    wf.init(makeBox(2.0f, 2.0f, 2.0f));

    wf.bevelEdges(BevelChamferDesc{});

    const Mesh& result1 = wf.rebuild();
    size_t vertCount1 = result1.attributes().vertexCount();

    const Mesh& result2 = wf.rebuild();
    EXPECT_EQ(result2.attributes().vertexCount(), vertCount1);
}

} // namespace
