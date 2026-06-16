#include <gtest/gtest.h>

#include <nexus/cad/CadAssembly.h>
#include <nexus/cad/CadRenderBridge.h>
#include <nexus/cad/CadAutoConstraintSketch.h>
#include <nexus/render/SceneGraph.h>

using namespace nexus::cad;
using namespace nexus::parametric;

TEST(CadAssembly, CreateEmptyAssembly)
{
    CadAssembly assembly;
    EXPECT_EQ(assembly.partCount(), 0u);
    EXPECT_TRUE(assembly.constraints().empty());
}

TEST(CadAssembly, AddAndRemovePart)
{
    CadAssembly assembly;
    auto part = std::make_shared<CadPart>("TestPart");
    uint32_t id = assembly.addPart(part);
    EXPECT_EQ(assembly.partCount(), 1u);
    ASSERT_NE(assembly.part(id), nullptr);
    EXPECT_EQ(assembly.part(id)->name(), "TestPart");

    EXPECT_TRUE(assembly.removePart(id));
    EXPECT_EQ(assembly.partCount(), 0u);
}

TEST(CadAssembly, AddConstraint)
{
    CadAssembly assembly;
    assembly.addConstraint({0, 1, AssemblyConstraint::Mate, 0.0});
    EXPECT_EQ(assembly.constraints().size(), 1u);
}

TEST(CadPart, NamedPart)
{
    CadPart part("Bracket");
    EXPECT_EQ(part.name(), "Bracket");
    EXPECT_TRUE(part.isValid());

    part.setName("Updated");
    EXPECT_EQ(part.name(), "Updated");
}

TEST(CadAutoConstraintSketch, BeginEndSketchLifecycle)
{
    // Create a minimal mock document for testing.
    CadDocument doc;
    CadAutoConstraintSketch sketcher(doc);

    sketcher.beginSketch();
    sketcher.addPoint(0.0, 0.0);
    sketcher.addPoint(10.0, 0.0);
    sketcher.endSketch();

    // After endSketch, the sketch should be added to the document.
    EXPECT_GT(doc.history().featureCount(), 0u);
}

TEST(CadAutoConstraintSketch, AutoSnapToExistingPoint)
{
    CadDocument doc;
    CadAutoConstraintSketch sketcher(doc);

    sketcher.beginSketch();
    auto p1 = sketcher.addPoint(5.0, 5.0);
    auto p2 = sketcher.addPoint(5.001, 5.001); // within snap radius
    EXPECT_EQ(p1, p2); // should snap to existing point
}

TEST(CadAutoConstraintSketch, AddRectangleReturnsFourPoints)
{
    CadDocument doc;
    CadAutoConstraintSketch sketcher(doc);
    sketcher.beginSketch();
    auto rect = sketcher.addRectangle(0, 0, 8, 6);
    for (auto id : rect) EXPECT_NE(id, kInvalidEntityId);
}

TEST(CadMeasure, StubReturnsValid)
{
    CadPart part("test");
    auto r = CadMeasure::distanceBetween(part, 0, 1);
    EXPECT_TRUE(r.valid);
}

TEST(CadRenderBridge, PopulateEmptyDocument)
{
    CadDocument doc;
    nexus::render::SceneGraph scene;
    uint32_t count = CadRenderBridge::populateScene(doc, scene);
    EXPECT_EQ(count, 0u);
}
