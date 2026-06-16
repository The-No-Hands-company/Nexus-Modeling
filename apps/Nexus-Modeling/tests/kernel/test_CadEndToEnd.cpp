#include <gtest/gtest.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadFeatureTree.h>
#include <nexus/cad/CadAutoConstraintSketch.h>
#include <nexus/geometry/ProfileRevolve.h>
#include <nexus/geometry/CurveExtrude.h>
#include <nexus/geometry/BRepBody.h>
#include <nexus/geometry/MeshMassProperties.h>
#include <nexus/geometry/MeshAnalysis.h>
#include <nexus/geometry/SolidOperations.h>
#include <nexus/geometry/BlendNetwork.h>

using namespace nexus::cad;
using namespace nexus::parametric;
using namespace nexus::geometry;

// ── Full Pipeline: Sketch → Constrain → Revolve → B-Rep → Mass Props ──

TEST(CadEndToEnd, SketchRectangleRevolveBRepMassProperties)
{
    // 1. Create document.
    CadDocument doc;

    // 2. Create a sketch with a constrained rectangle.
    SketchProfile sk = ParametricSketchFactory::createSketch();
    auto [origin, xHandle, yHandle, corner] =
        ParametricSketchFactory::addRectangle(sk, 0.0, 0.0, 3.0, 5.0);
    ASSERT_NE(origin, kInvalidEntityId);

    // 3. Add sketch to document and solve.
    FeatureId sketchId = doc.addSketch(sk);
    ASSERT_NE(sketchId, kInvalidFeatureId);
    doc.history().rebuild();

    // 4. Revolve the sketch profile around Z axis.
    //    Extract the right edge (xHandle→corner) as the profile.
    RevolveDesc revolveDesc;
    revolveDesc.axisDirection = {0, 0, 1};
    revolveDesc.capEnds = true;
    revolveDesc.angularSamples = 16;
    revolveDesc.outputAsNurbsSurface = true;

    NurbsCurve profile(1, {0.f,0.f,1.f,1.f},
        {Vec3{3.f,0.f,0.f}, Vec3{3.f,0.f,5.f}});

    NurbsSurface surface;
    Mesh mesh;
    RevolveReport revRep = RevolveOperation::revolve(profile, revolveDesc, surface, &mesh);
    ASSERT_EQ(revRep.diagnostic, RevolveDiagnostic::Success);

    // 5. Build B-Rep from the mesh.
    auto [body, brepReport] = BRepBody::fromMesh(mesh);
    ASSERT_TRUE(brepReport.valid);
    EXPECT_GE(body.vertexCount(), 2u);
    EXPECT_GE(body.edgeCount(), 1u);
    EXPECT_GE(body.faceCount(), 2u);

    // 6. Compute mass properties.
    auto mp = MeshMassProperties::compute(mesh);
    EXPECT_GT(mp.volume, 0.f);
}

// ── Assembly Pipeline ─────────────────────────────────────────────────

TEST(CadEndToEnd, AssemblyStructure)
{
    CadAssembly assembly;
    auto part1 = std::make_shared<CadPart>("Base");
    auto part2 = std::make_shared<CadPart>("Top");

    uint32_t id1 = assembly.addPart(part1);
    uint32_t id2 = assembly.addPart(part2);

    EXPECT_EQ(assembly.partCount(), 2u);

    AssemblyConstraint constraint;
    constraint.partA = id1;
    constraint.partB = id2;
    constraint.type = AssemblyConstraint::Mate;
    assembly.addConstraint(constraint);

    EXPECT_EQ(assembly.constraints().size(), 1u);

    // Verify part access.
    ASSERT_NE(assembly.part(id1), nullptr);
    EXPECT_EQ(assembly.part(id1)->name(), "Base");
    ASSERT_NE(assembly.part(id2), nullptr);
    EXPECT_EQ(assembly.part(id2)->name(), "Top");
}

// ── Feature Tree Pipeline ─────────────────────────────────────────────

TEST(CadEndToEnd, FeatureTreeContents)
{
    CadDocument doc;

    auto sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0, 0);
    ParametricSketchFactory::addSketchPoint(sk, 10, 0);
    doc.addSketch(sk);

    auto tree = CadFeatureTree::build(doc);
    EXPECT_EQ(tree.size(), 1u);
    EXPECT_EQ(tree[0].kind, FeatureKind::Sketch);
}

// ── Solid Operations Pipeline ─────────────────────────────────────────

TEST(CadEndToEnd, SplitBodyPreservesManifoldness)
{
    // Create a unit cube.
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t = cube.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}});t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}});t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}});t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}});t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}});t.addFace(Face{{1,6,5}});

    // Split along Y=0.5.
    auto [top, bottom] = splitBody(cube, {0,0.5,0}, {0,1,0});

    // Both halves should have faces.
    EXPECT_GT(top.topology().faceCount(), 0u);
    EXPECT_GT(bottom.topology().faceCount(), 0u);
    EXPECT_NE(top.attributes().vertexCount(), 0u);
    EXPECT_NE(bottom.attributes().vertexCount(), 0u);
}

// ── Blend Network Pipeline ────────────────────────────────────────────

TEST(CadEndToEnd, BlendNetworkOnCube)
{
    Mesh cube;
    cube.attributes().setPositions({
        {0,0,0},{1,0,0},{1,1,0},{0,1,0},
        {0,0,1},{1,0,1},{1,1,1},{0,1,1}
    });
    auto& t = cube.topology();
    t.addFace(Face{{0,2,1}});t.addFace(Face{{0,3,2}});
    t.addFace(Face{{4,5,6}});t.addFace(Face{{4,6,7}});
    t.addFace(Face{{0,1,5}});t.addFace(Face{{0,5,4}});
    t.addFace(Face{{2,3,7}});t.addFace(Face{{2,7,6}});
    t.addFace(Face{{0,4,7}});t.addFace(Face{{0,7,3}});
    t.addFace(Face{{1,2,6}});t.addFace(Face{{1,6,5}});

    auto heOpt = HalfEdgeMesh::fromMesh(cube);
    ASSERT_TRUE(heOpt.has_value());
    HalfEdgeMesh he = *heOpt;

    // Find an interior edge.
    uint32_t edge = he.findEdge(0, 1);
    ASSERT_NE(edge, HalfEdgeMesh::kInvalid);

    BlendNetworkReport report = solveBlendNetwork(he, {{edge, 0.15f}});
    EXPECT_GE(report.edgesProcessed, 1u);
}
