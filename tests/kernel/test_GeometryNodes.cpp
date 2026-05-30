// Tests for nexus::eval geometry eval nodes (Slice 6).
//
// Covers GeometryBooleanNode, GeometryRemeshNode, GeometryBevelNode,
// GeometryInsetNode — and confirms existing Transform/Merge kind values.

#include <nexus/eval/GeometryNodes.h>
#include <nexus/geometry/BevelChamfer.h>
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/InsetFacesOperation.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/RemeshOperation.h>

#include <gtest/gtest.h>

using namespace nexus::eval;
using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

// ── Kind and name contracts ───────────────────────────────────────────────────

TEST(GeometryNodes, KindValuesAreDistinct)
{
    EXPECT_NE(static_cast<uint32_t>(GeometryNodeKind::Transform),
              static_cast<uint32_t>(GeometryNodeKind::Merge));
    EXPECT_NE(static_cast<uint32_t>(GeometryNodeKind::Merge),
              static_cast<uint32_t>(GeometryNodeKind::Boolean));
    EXPECT_NE(static_cast<uint32_t>(GeometryNodeKind::Boolean),
              static_cast<uint32_t>(GeometryNodeKind::Remesh));
    EXPECT_NE(static_cast<uint32_t>(GeometryNodeKind::Remesh),
              static_cast<uint32_t>(GeometryNodeKind::Bevel));
    EXPECT_NE(static_cast<uint32_t>(GeometryNodeKind::Bevel),
              static_cast<uint32_t>(GeometryNodeKind::Inset));
}

TEST(GeometryNodes, NameStringsAreCorrect)
{
    EXPECT_STREQ(GeometryTransformNode::name(), "GeometryTransform");
    EXPECT_STREQ(GeometryMergeNode::name(),     "GeometryMerge");
    EXPECT_STREQ(GeometryBooleanNode::name(),   "GeometryBoolean");
    EXPECT_STREQ(GeometryRemeshNode::name(),    "GeometryRemesh");
    EXPECT_STREQ(GeometryBevelNode::name(),     "GeometryBevel");
    EXPECT_STREQ(GeometryInsetNode::name(),     "GeometryInset");
}

// ── GeometryBooleanNode ───────────────────────────────────────────────────────

TEST(GeometryNodes, BooleanUnionProducesOutput)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeBox(2.f, 2.f, 2.f);
    Mesh out;
    EXPECT_TRUE(GeometryBooleanNode::compute(a, b, BooleanOperationType::Union, out));
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(GeometryNodes, BooleanDifferenceProducesOutput)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    EXPECT_TRUE(GeometryBooleanNode::compute(a, b, BooleanOperationType::Difference, out));
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(GeometryNodes, BooleanIntersectionSucceeds)
{
    // The intersection result may be empty (OutputEmpty is a valid outcome);
    // what matters is that the node returns true (report.valid).
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    EXPECT_TRUE(GeometryBooleanNode::compute(a, b, BooleanOperationType::Intersection, out));
}

TEST(GeometryNodes, BooleanEmptyMeshReturnsFalse)
{
    Mesh empty;
    Mesh b = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    EXPECT_FALSE(GeometryBooleanNode::compute(empty, b, BooleanOperationType::Union, out));
}

TEST(GeometryNodes, BooleanIsDeterministic)
{
    Mesh a1 = makeBox(2.f, 2.f, 2.f);
    Mesh b1 = makeBox(1.f, 1.f, 1.f);
    Mesh a2 = makeBox(2.f, 2.f, 2.f);
    Mesh b2 = makeBox(1.f, 1.f, 1.f);
    Mesh out1, out2;
    ASSERT_TRUE(GeometryBooleanNode::compute(a1, b1, BooleanOperationType::Difference, out1));
    ASSERT_TRUE(GeometryBooleanNode::compute(a2, b2, BooleanOperationType::Difference, out2));
    EXPECT_EQ(out1.topology().faceCount(), out2.topology().faceCount());
    EXPECT_EQ(out1.attributes().vertexCount(), out2.attributes().vertexCount());
}

// ── GeometryRemeshNode ────────────────────────────────────────────────────────

TEST(GeometryNodes, RemeshDefaultDescSucceeds)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    RemeshDesc desc;
    EXPECT_TRUE(GeometryRemeshNode::compute(box, desc, out));
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(GeometryNodes, RemeshIsDeterministic)
{
    RemeshDesc desc;
    desc.targetEdgeLength = 0.5f;
    Mesh out1, out2;
    ASSERT_TRUE(GeometryRemeshNode::compute(makeBox(1.f, 1.f, 1.f), desc, out1));
    ASSERT_TRUE(GeometryRemeshNode::compute(makeBox(1.f, 1.f, 1.f), desc, out2));
    EXPECT_EQ(out1.topology().faceCount(), out2.topology().faceCount());
}

TEST(GeometryNodes, RemeshEmptyMeshReturnsFalse)
{
    Mesh empty;
    Mesh out;
    EXPECT_FALSE(GeometryRemeshNode::compute(empty, RemeshDesc{}, out));
}

// ── GeometryBevelNode ─────────────────────────────────────────────────────────

TEST(GeometryNodes, BevelDefaultDescSucceeds)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    BevelChamferDesc desc;
    EXPECT_TRUE(GeometryBevelNode::compute(box, desc, out));
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(GeometryNodes, BevelChamferModeSucceeds)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    BevelChamferDesc desc;
    desc.mode = BevelChamferMode::Chamfer;
    EXPECT_TRUE(GeometryBevelNode::compute(box, desc, out));
}

TEST(GeometryNodes, BevelIsDeterministic)
{
    BevelChamferDesc desc;
    desc.distance = 0.05f;
    Mesh out1, out2;
    ASSERT_TRUE(GeometryBevelNode::compute(makeBox(1.f, 1.f, 1.f), desc, out1));
    ASSERT_TRUE(GeometryBevelNode::compute(makeBox(1.f, 1.f, 1.f), desc, out2));
    EXPECT_EQ(out1.topology().faceCount(), out2.topology().faceCount());
    EXPECT_EQ(out1.attributes().vertexCount(), out2.attributes().vertexCount());
}

TEST(GeometryNodes, BevelEmptyMeshReturnsFalse)
{
    Mesh empty;
    Mesh out;
    EXPECT_FALSE(GeometryBevelNode::compute(empty, BevelChamferDesc{}, out));
}

// ── GeometryInsetNode ─────────────────────────────────────────────────────────

TEST(GeometryNodes, InsetDefaultDescSucceeds)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    Mesh out;
    InsetDesc desc;
    EXPECT_TRUE(GeometryInsetNode::compute(box, desc, out));
    EXPECT_GT(out.topology().faceCount(), 0u);
}

TEST(GeometryNodes, InsetAddsGeometry)
{
    Mesh box = makeBox(1.f, 1.f, 1.f);
    const uint32_t origFaces = box.topology().faceCount();
    Mesh out;
    InsetDesc desc;
    desc.replaceOriginal = false;
    ASSERT_TRUE(GeometryInsetNode::compute(box, desc, out));
    EXPECT_GT(out.topology().faceCount(), origFaces);
}

TEST(GeometryNodes, InsetIsDeterministic)
{
    InsetDesc desc;
    desc.amount = 0.05f;
    Mesh out1, out2;
    ASSERT_TRUE(GeometryInsetNode::compute(makeBox(1.f, 1.f, 1.f), desc, out1));
    ASSERT_TRUE(GeometryInsetNode::compute(makeBox(1.f, 1.f, 1.f), desc, out2));
    EXPECT_EQ(out1.topology().faceCount(), out2.topology().faceCount());
    EXPECT_EQ(out1.attributes().vertexCount(), out2.attributes().vertexCount());
}

TEST(GeometryNodes, InsetEmptyMeshReturnsFalse)
{
    Mesh empty;
    Mesh out;
    EXPECT_FALSE(GeometryInsetNode::compute(empty, InsetDesc{}, out));
}

// ── Pipeline: Boolean → Remesh → Bevel ───────────────────────────────────────

TEST(GeometryNodes, PipelineBooleanThenRemeshThenBevel)
{
    Mesh a = makeBox(2.f, 2.f, 2.f);
    Mesh b = makeBox(1.f, 1.f, 1.f);

    Mesh afterBool;
    ASSERT_TRUE(GeometryBooleanNode::compute(a, b, BooleanOperationType::Difference, afterBool));

    Mesh afterRemesh;
    RemeshDesc rd; rd.targetEdgeLength = 0.5f;
    ASSERT_TRUE(GeometryRemeshNode::compute(afterBool, rd, afterRemesh));

    Mesh afterBevel;
    BevelChamferDesc bd; bd.distance = 0.04f;
    ASSERT_TRUE(GeometryBevelNode::compute(afterRemesh, bd, afterBevel));

    EXPECT_GT(afterBevel.topology().faceCount(), 0u);
}
