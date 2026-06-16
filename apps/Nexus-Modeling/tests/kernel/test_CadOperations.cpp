#include <gtest/gtest.h>

#include <nexus/cad/CadOperations.h>
#include <nexus/cad/CadFeatureTree.h>
#include <nexus/cad/CadDocument.h>

using namespace nexus::cad;
using namespace nexus::parametric;

TEST(CadOperations, FilletCommandHasDescription)
{
    FilletCommand cmd(FeatureId(1), {0, 1, 2}, 2.5f);
    EXPECT_NE(cmd.description().find("Fillet"), std::string::npos);
    EXPECT_FALSE(cmd.wasExecuted());
}

TEST(CadOperations, ChamferCommandHasDescription)
{
    ChamferCommand cmd(FeatureId(2), {3, 4}, 1.0f);
    EXPECT_NE(cmd.description().find("Chamfer"), std::string::npos);
}

TEST(CadFeatureTree, BuildFromEmptyDocument)
{
    CadDocument doc;
    auto tree = CadFeatureTree::build(doc);
    EXPECT_TRUE(tree.empty());
}

TEST(CadFeatureTree, BuildFromDocumentWithSketch)
{
    CadDocument doc;
    auto sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0, 0);
    ParametricSketchFactory::addSketchPoint(sk, 10, 10);
    doc.addSketch(sk);

    auto tree = CadFeatureTree::build(doc);
    EXPECT_EQ(tree.size(), 1u);
    EXPECT_EQ(tree[0].kind, FeatureKind::Sketch);
}

TEST(CadWorkplane, StandardPlanes)
{
    auto xy = Workplane::XY();
    EXPECT_FLOAT_EQ(xy.normal.z, 1.0f);

    auto xz = Workplane::XZ();
    EXPECT_FLOAT_EQ(xz.normal.y, 1.0f);

    auto yz = Workplane::YZ();
    EXPECT_FLOAT_EQ(yz.normal.x, 1.0f);
}

TEST(CadWorkplane, FromOriginNormal)
{
    Vec3 origin{1, 2, 3};
    Vec3 normal{0, 0, 1};
    auto wp = Workplane::fromOriginNormal(origin, normal);
    EXPECT_FLOAT_EQ(wp.origin.x, 1.0f);
    EXPECT_FLOAT_EQ(wp.normal.z, 1.0f);
}

TEST(CadWorkplaneManager, SetAndGetActive)
{
    CadWorkplaneManager mgr;
    mgr.setActive(Workplane::XZ());
    EXPECT_FLOAT_EQ(mgr.active().normal.y, 1.0f);
    mgr.setActive(Workplane::XY());
    EXPECT_FLOAT_EQ(mgr.active().normal.z, 1.0f);
}

TEST(CadGridSnap, SnapToGrid)
{
    CadGridSnap snap;
    CadDocument doc;
    Workplane wp = Workplane::XY();
    Vec3 pt{3.3f, 4.7f, 0.0f};
    auto snapped = snap.snap(pt, wp, doc);
    EXPECT_FLOAT_EQ(snapped.x, 3.0f);
    EXPECT_FLOAT_EQ(snapped.y, 5.0f);
}

TEST(CadFeatureEditor, SetHeightMarksModified)
{
    CadDocument doc;
    auto sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0, 0);
    ParametricSketchFactory::addSketchPoint(sk, 5, 0);
    auto skId = doc.addSketch(sk);
    ASSERT_NE(skId, kInvalidFeatureId);

    auto exId = doc.addExtrude(skId, {{0,0,1}, 10.0f});
    ASSERT_NE(exId, kInvalidFeatureId);

    bool ok = CadFeatureEditor::setHeight(doc, exId, 20.0f);
    EXPECT_TRUE(ok);
    EXPECT_TRUE(doc.isModified());
}
