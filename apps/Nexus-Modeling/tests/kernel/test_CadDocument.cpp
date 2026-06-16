#include <gtest/gtest.h>

#include <nexus/cad/CadDocument.h>
#include <nexus/cad/CadCommand.h>
#include <nexus/cad/CadSelection.h>
#include <nexus/parametric/ParametricSketchProfile.h>

using namespace nexus::cad;
using namespace nexus::parametric;
using namespace nexus::geometry;

TEST(CadDocument, CreateEmptyDocument)
{
    CadDocument doc;
    EXPECT_EQ(doc.history().featureCount(), 0u);
    EXPECT_TRUE(doc.selection().empty());
    EXPECT_FALSE(doc.isModified());
    EXPECT_FALSE(doc.canUndo());
    EXPECT_FALSE(doc.canRedo());
}

TEST(CadDocument, AddSketchCreatesFeature)
{
    CadDocument doc;
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 5.0, 5.0);

    FeatureId id = doc.addSketch(sk);
    EXPECT_NE(id, kInvalidFeatureId);
    EXPECT_GT(id, kInvalidFeatureId);
    EXPECT_GT(doc.history().featureCount(), 0u);
    EXPECT_TRUE(doc.canUndo());
    EXPECT_TRUE(doc.isModified());
}

TEST(CadDocument, UndoRedoCycle)
{
    CadDocument doc;
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 0.0, 0.0);

    size_t before = doc.history().featureCount();
    doc.addSketch(sk);
    size_t after = doc.history().featureCount();
    EXPECT_GT(after, before);

    EXPECT_TRUE(doc.canUndo());
    EXPECT_TRUE(doc.undo());
    EXPECT_TRUE(doc.canRedo());
    EXPECT_TRUE(doc.redo());
}

TEST(CadDocument, SelectionState)
{
    CadDocument doc;
    doc.selection().addFace(42);
    EXPECT_FALSE(doc.selection().empty());
    EXPECT_EQ(doc.selection().count(), 1u);

    doc.selection().clear();
    EXPECT_TRUE(doc.selection().empty());
}

TEST(CadDocument, ToolSwitchingClearsSelection)
{
    CadDocument doc;
    doc.selection().addFace(10);
    EXPECT_FALSE(doc.selection().empty());

    doc.setActiveTool(CadTool::Sketch);
    EXPECT_TRUE(doc.selection().empty());
    EXPECT_EQ(doc.activeTool(), CadTool::Sketch);
}

TEST(CadDocument, SerializationRoundTrip)
{
    CadDocument doc;
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 1.0, 2.0);
    doc.addSketch(sk);

    auto data = doc.serialize();
    EXPECT_FALSE(data.empty());

    CadDocument restored;
    EXPECT_TRUE(restored.deserialize(data.data(), data.size()));
}

TEST(CadCommand, AddSketchCommandDescribesItself)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    AddSketchCommand cmd(sk);
    EXPECT_EQ(cmd.description(), "Add Sketch");
    EXPECT_FALSE(cmd.wasExecuted());
}

TEST(CadSelection, PickFaceReturnsNegativeOnEmpty)
{
    Mesh empty;
    CadSelection sel;
    Vec3 origin{0, 0, 10}, dir{0, 0, -1};
    EXPECT_EQ(sel.pickFace(empty, origin, dir), -1);
}
