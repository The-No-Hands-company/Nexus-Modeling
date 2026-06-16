#include <gtest/gtest.h>

#include <nexus/parametric/FeatureHistory.h>
#include <nexus/parametric/ParametricSketchProfile.h>

using namespace nexus::parametric;
using namespace nexus::geometry;

// ── Basic add / rebuild / query ──────────────────────────────────────────

TEST(FeatureHistory, AddExtrudeAndRebuild)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0.0, 0.0, 3.0, 4.0);

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);
    ASSERT_NE(id, kInvalidFeatureId);

    history.setDirection(id, {0.f, 0.f, 1.f});
    history.setHeight(id, 10.f);
    history.setCapEnds(id, true);

    FeatureHistoryReport report = history.rebuild();
    EXPECT_TRUE(report.converged);
    EXPECT_TRUE(report.errors.empty());

    const Mesh* m = history.mesh(id);
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->attributes().vertexCount(), 0u);
    EXPECT_GT(m->topology().faceCount(), 0u);
}

TEST(FeatureHistory, AddRevolveAndRebuild)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 3.0, 0.0);
    ParametricSketchFactory::addSketchPoint(sk, 3.0, 5.0);

    FeatureHistory history;
    FeatureId id = history.addRevolve(sk);
    ASSERT_NE(id, kInvalidFeatureId);

    history.setAxis(id, {0.f, 0.f, 0.f}, {0.f, 0.f, 1.f});
    history.setCapEnds(id, true);

    FeatureHistoryReport report = history.rebuild();
    EXPECT_TRUE(report.converged);

    const Mesh* m = history.mesh(id);
    ASSERT_NE(m, nullptr);
    EXPECT_GT(m->attributes().vertexCount(), 0u);
}

TEST(FeatureHistory, RebuildOnlyDirtyFeatures)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0, 0, 2, 2);

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);
    history.setDirection(id, {0, 0, 1});
    history.setHeight(id, 5);

    ASSERT_TRUE(history.rebuild().converged);

    // Nothing changed — no features dirty.
    EXPECT_FALSE(history.isDirty(id));

    FeatureHistoryReport r2 = history.rebuild();
    EXPECT_TRUE(r2.converged);
}

TEST(FeatureHistory, ParameterChangeMarksDirty)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0, 0, 2, 2);

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);
    history.setDirection(id, {0, 0, 1});
    history.setHeight(id, 5);

    ASSERT_TRUE(history.rebuild().converged);
    EXPECT_FALSE(history.isDirty(id));

    history.setHeight(id, 10);
    EXPECT_TRUE(history.isDirty(id));

    ASSERT_TRUE(history.rebuild().converged);
    EXPECT_FALSE(history.isDirty(id));
}

TEST(FeatureHistory, MeshReturnsNullForDirtyFeature)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 1, 0);
    ParametricSketchFactory::addSketchPoint(sk, 2, 0);

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);
    history.setHeight(id, 5);

    // Not rebuilt yet — mesh should be null.
    EXPECT_EQ(history.mesh(id), nullptr);
    EXPECT_TRUE(history.isDirty(id));

    ASSERT_TRUE(history.rebuild().converged);
    EXPECT_NE(history.mesh(id), nullptr);
    EXPECT_FALSE(history.isDirty(id));
}

TEST(FeatureHistory, MultipleFeaturesDontInterfere)
{
    FeatureHistory history;

    // Feature 1: short extrude
    SketchProfile sk1 = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk1, 0, 0, 3, 2);
    FeatureId id1 = history.addExtrude(sk1);
    history.setDirection(id1, {0, 0, 1});
    history.setHeight(id1, 5);

    // Feature 2: tall extrude
    SketchProfile sk2 = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk2, 0, 0, 4, 3);
    FeatureId id2 = history.addExtrude(sk2);
    history.setDirection(id2, {0, 0, 1});
    history.setHeight(id2, 15);

    ASSERT_TRUE(history.rebuild().converged);

    ASSERT_NE(history.mesh(id1), nullptr);
    ASSERT_NE(history.mesh(id2), nullptr);

    // Only change feature 1.
    history.setHeight(id1, 8);
    EXPECT_TRUE(history.isDirty(id1));
    EXPECT_FALSE(history.isDirty(id2));

    ASSERT_TRUE(history.rebuild().converged);
    EXPECT_FALSE(history.isDirty(id1));
}

TEST(FeatureHistory, BadFeatureIdReturnsNull)
{
    FeatureHistory history;
    EXPECT_EQ(history.node(kInvalidFeatureId), nullptr);
    EXPECT_EQ(history.node(9999), nullptr);
    EXPECT_FALSE(history.setHeight(kInvalidFeatureId, 5));
    EXPECT_FALSE(history.setDirection(9999, {0, 0, 1}));
    EXPECT_FALSE(history.setAxis(0, {0, 0, 0}, {0, 0, 1}));
}

TEST(FeatureHistory, RebuildFailureOnEmptyProfile)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    // No sketch points — cannot build profile.

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);
    history.setHeight(id, 5);

    FeatureHistoryReport report = history.rebuild();
    EXPECT_FALSE(report.converged);
    EXPECT_FALSE(report.errors.empty());
    EXPECT_EQ(history.mesh(id), nullptr);
}

TEST(FeatureHistory, NodeMetadata)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0, 0, 2, 2);

    FeatureHistory history;
    FeatureId id = history.addRevolve(sk);
    history.setName(id, "MyRevolve");

    const FeatureNode* n = history.node(id);
    ASSERT_NE(n, nullptr);
    EXPECT_EQ(n->id, id);
    EXPECT_EQ(n->kind, FeatureKind::Revolve);
    EXPECT_EQ(n->name, "MyRevolve");
    EXPECT_EQ(history.parent(id), kInvalidFeatureId);
    EXPECT_EQ(history.kind(id), FeatureKind::Revolve);
}

TEST(FeatureHistory, ReviveAngleSettings)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addSketchPoint(sk, 2, 0);
    ParametricSketchFactory::addSketchPoint(sk, 2, 3);

    FeatureHistory history;
    FeatureId id = history.addRevolve(sk);
    ASSERT_TRUE(history.setStartAngle(id, 30));
    ASSERT_TRUE(history.setEndAngle(id, 330));
    ASSERT_TRUE(history.setAngularSamples(id, 48));
    ASSERT_TRUE(history.setAxis(id, {0, 0, 0}, {0, 0, 1}));

    ASSERT_TRUE(history.rebuild().converged);
    EXPECT_NE(history.mesh(id), nullptr);
}

TEST(FeatureHistory, SetterRejectsWrongFeatureType)
{
    SketchProfile sk = ParametricSketchFactory::createSketch();
    ParametricSketchFactory::addRectangle(sk, 0, 0, 2, 2);

    FeatureHistory history;
    FeatureId id = history.addExtrude(sk);

    // Extrude setter on extrude feature should work.
    EXPECT_TRUE(history.setHeight(id, 5));
    EXPECT_TRUE(history.setDraftAngle(id, 3));

    // Revolve setters on extrude feature should fail.
    EXPECT_FALSE(history.setAxis(id, {0, 0, 0}, {0, 0, 1}));
    EXPECT_FALSE(history.setStartAngle(id, 45));
}
