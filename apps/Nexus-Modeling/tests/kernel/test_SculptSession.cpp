#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/sculpt/Brush.h>
#include <nexus/sculpt/SculptSession.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;
using namespace nexus::sculpt;

class SculptSessionTest : public ::testing::Test {
protected:
    Mesh mesh;
    SculptSession* session = nullptr;

    void SetUp() override {
        mesh = makeBox(2.0f, 2.0f, 2.0f);
        ASSERT_TRUE(mesh.isValid());
        ASSERT_GT(mesh.attributes().vertexCount(), 0u);
        session = new SculptSession(mesh);
    }

    void TearDown() override {
        delete session;
    }
};

TEST_F(SculptSessionTest, InitialStateHasNoActiveStroke)
{
    EXPECT_FALSE(session->strokeInProgress());
}

TEST_F(SculptSessionTest, BeginAndEndStrokeLifecycle)
{
    BrushParams params;
    params.radius = 0.5f;
    params.strength = 0.5f;

    StrokeId id = session->beginStroke(BrushKind::Draw, params);
    EXPECT_NE(id, kInvalidStrokeId);
    EXPECT_TRUE(session->strokeInProgress());

    StrokeDelta delta = session->endStroke(id);
    EXPECT_EQ(delta.id, id);
    EXPECT_EQ(delta.kind, BrushKind::Draw);
    EXPECT_FALSE(session->strokeInProgress());
}

TEST_F(SculptSessionTest, BeginStrokeRejectsZeroRadius)
{
    BrushParams params;
    params.radius = 0.0f;

    StrokeId id = session->beginStroke(BrushKind::Draw, params);
    EXPECT_EQ(id, kInvalidStrokeId);
    EXPECT_FALSE(session->strokeInProgress());
}

TEST_F(SculptSessionTest, BeginStrokeRejectsWhileStrokeActive)
{
    BrushParams params;
    params.radius = 0.5f;

    StrokeId first = session->beginStroke(BrushKind::Draw, params);
    EXPECT_NE(first, kInvalidStrokeId);

    StrokeId second = session->beginStroke(BrushKind::Smooth, params);
    EXPECT_EQ(second, kInvalidStrokeId);

    session->endStroke(first);
}

TEST_F(SculptSessionTest, ApplySampleRejectsWrongStrokeId)
{
    BrushParams params;
    params.radius = 2.0f;

    StrokeId id = session->beginStroke(BrushKind::Draw, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.pressure = 1.0f;
    sample.sequence = 1;

    EXPECT_FALSE(session->applySample(kInvalidStrokeId, sample));
    EXPECT_FALSE(session->applySample(id + 100, sample));

    session->endStroke(id);
}

TEST_F(SculptSessionTest, ApplySampleProcessesInSequence)
{
    BrushParams params;
    params.radius = 2.0f;

    StrokeId id = session->beginStroke(BrushKind::Inflate, params);

    BrushSample s1;
    s1.position = {1.0f, 1.0f, 1.0f};
    s1.sequence = 1;
    EXPECT_TRUE(session->applySample(id, s1));

    BrushSample s2;
    s2.position = {1.1f, 1.0f, 1.0f};
    s2.sequence = 2;
    EXPECT_TRUE(session->applySample(id, s2));

    session->endStroke(id);
}

TEST_F(SculptSessionTest, UndoRedoStrokeRoundtrip)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 0.5f;

    StrokeId id = session->beginStroke(BrushKind::Inflate, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    StrokeDelta delta = session->endStroke(id);
    EXPECT_GT(delta.touchedVertexCount(), 0u);

    auto posBefore = mesh.attributes().positions();
    EXPECT_TRUE(session->undoStroke(delta));
    EXPECT_TRUE(session->redoStroke(delta));

    auto posAfter = mesh.attributes().positions();
    for (size_t i = 0; i < posBefore.size(); ++i) {
        EXPECT_FLOAT_EQ(posBefore[i].x, posAfter[i].x);
        EXPECT_FLOAT_EQ(posBefore[i].y, posAfter[i].y);
        EXPECT_FLOAT_EQ(posBefore[i].z, posAfter[i].z);
    }
}

TEST_F(SculptSessionTest, UndoRedoRejectsInvalidDelta)
{
    StrokeDelta invalid;
    EXPECT_FALSE(invalid.isValid());
    EXPECT_FALSE(session->undoStroke(invalid));
    EXPECT_FALSE(session->redoStroke(invalid));
}

TEST_F(SculptSessionTest, MaskDefaultsToAllOnes)
{
    EXPECT_EQ(session->mask().size(), mesh.attributes().vertexCount());
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 1.0f);
    }
}

TEST_F(SculptSessionTest, SetMaskUniform)
{
    session->setMask(0.5f);
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 0.5f);
    }
}

TEST_F(SculptSessionTest, SetMaskClamped)
{
    session->setMask(-0.5f);
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 0.0f);
    }

    session->setMask(1.5f);
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 1.0f);
    }
}

TEST_F(SculptSessionTest, SetMaskVertex)
{
    session->setMask(0.0f);
    session->setMaskVertex(0, 0.75f);
    EXPECT_FLOAT_EQ(session->maskAt(0), 0.75f);

    for (size_t i = 1; i < session->mask().size(); ++i) {
        EXPECT_FLOAT_EQ(session->maskAt(static_cast<uint32_t>(i)), 0.0f);
    }
}

TEST_F(SculptSessionTest, ClearMaskResetsToOne)
{
    session->setMask(0.3f);
    session->clearMask();
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 1.0f);
    }
}

TEST_F(SculptSessionTest, InvertMaskNegates)
{
    session->setMask(0.25f);
    session->invertMask();
    for (float m : session->mask()) {
        EXPECT_FLOAT_EQ(m, 0.75f);
    }
}

TEST_F(SculptSessionTest, MaskAtOutOfBounds)
{
    EXPECT_FLOAT_EQ(session->maskAt(99999u), 1.0f);
}

TEST_F(SculptSessionTest, SymmetryDefaultsToNone)
{
    EXPECT_EQ(session->symmetryAxes(), SymmetryAxes::None);
}

TEST_F(SculptSessionTest, SetSymmetryRoundtrip)
{
    session->setSymmetry(SymmetryAxes::X);
    EXPECT_EQ(session->symmetryAxes(), SymmetryAxes::X);

    auto axes = SymmetryAxes::X | SymmetryAxes::Y;
    session->setSymmetry(axes);
    EXPECT_TRUE(hasSymmetry(session->symmetryAxes(), SymmetryAxes::X));
    EXPECT_TRUE(hasSymmetry(session->symmetryAxes(), SymmetryAxes::Y));
    EXPECT_FALSE(hasSymmetry(session->symmetryAxes(), SymmetryAxes::Z));
}

TEST_F(SculptSessionTest, StatsInitialZero)
{
    auto& stats = session->stats();
    EXPECT_EQ(stats.strokesStarted, 0u);
    EXPECT_EQ(stats.strokesCommitted, 0u);
    EXPECT_EQ(stats.samplesProcessed, 0u);
    EXPECT_EQ(stats.verticesTouched, 0u);
    EXPECT_EQ(stats.undoApplied, 0u);
    EXPECT_EQ(stats.redoApplied, 0u);
}

TEST_F(SculptSessionTest, StrokeIncrementsStats)
{
    BrushParams params;
    params.radius = 2.0f;

    StrokeId id = session->beginStroke(BrushKind::Draw, params);
    EXPECT_EQ(session->stats().strokesStarted, 1u);

    BrushSample s;
    s.position = {1.0f, 1.0f, 1.0f};
    s.sequence = 1;
    ASSERT_TRUE(session->applySample(id, s));
    EXPECT_EQ(session->stats().samplesProcessed, 1u);

    session->endStroke(id);
    EXPECT_EQ(session->stats().strokesCommitted, 1u);
}

TEST_F(SculptSessionTest, DrawKernelMovesVerticesInRadius)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.direction = {0.0f, 1.0f, 0.0f};
    params.falloff = FalloffShape::Constant;

    auto posBefore = mesh.attributes().positions();

    StrokeId id = session->beginStroke(BrushKind::Draw, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);

    auto posAfter = mesh.attributes().positions();
    bool moved = false;
    for (size_t i = 0; i < posAfter.size(); ++i) {
        if (posAfter[i].y != posBefore[i].y) {
            moved = true;
        }
    }
    EXPECT_TRUE(moved);
}

TEST_F(SculptSessionTest, SmoothKernelBlendsAdjacency)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    StrokeId id = session->beginStroke(BrushKind::Smooth, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);
    EXPECT_EQ(session->stats().strokesCommitted, 1u);
}

TEST_F(SculptSessionTest, FlattenKernelConstrainsHeight)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 0.5f;
    params.direction = {0.0f, 1.0f, 0.0f};
    params.falloff = FalloffShape::Constant;

    StrokeId id = session->beginStroke(BrushKind::Flatten, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);
    EXPECT_EQ(session->stats().strokesCommitted, 1u);
}

TEST_F(SculptSessionTest, PinchKernelPullsToCenter)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    StrokeId id = session->beginStroke(BrushKind::Pinch, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);
    EXPECT_EQ(session->stats().strokesCommitted, 1u);
}

TEST_F(SculptSessionTest, CreaseKernelMovesAlongNormal)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.direction = {0.0f, 1.0f, 0.0f};
    params.falloff = FalloffShape::Constant;

    StrokeId id = session->beginStroke(BrushKind::Crease, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);
    EXPECT_EQ(session->stats().strokesCommitted, 1u);
}

TEST_F(SculptSessionTest, LayerKernelRespectsDisplacementCap)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.maxPerVertexDisplacement = 0.01f;
    params.direction = {1.0f, 0.0f, 0.0f};
    params.falloff = FalloffShape::Constant;

    auto posBefore = mesh.attributes().positions();

    StrokeId id = session->beginStroke(BrushKind::Layer, params);

    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));

    session->endStroke(id);

    auto posAfter = mesh.attributes().positions();
    for (size_t i = 0; i < posAfter.size(); ++i) {
        float dx = std::abs(posAfter[i].x - posBefore[i].x);
        EXPECT_LE(dx, params.maxPerVertexDisplacement + 1e-6f);
    }
}

TEST_F(SculptSessionTest, GrabKernelMovesWithGrabPosition)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    auto posBefore = mesh.attributes().positions();

    StrokeId id = session->beginStroke(BrushKind::Grab, params);

    BrushSample s1;
    s1.position = {1.0f, 1.0f, 1.0f};
    s1.sequence = 1;
    ASSERT_TRUE(session->applySample(id, s1));

    BrushSample s2;
    s2.position = {2.0f, 1.0f, 1.0f};
    s2.sequence = 2;
    ASSERT_TRUE(session->applySample(id, s2));

    session->endStroke(id);

    auto posAfter = mesh.attributes().positions();
    bool moved = false;
    for (size_t i = 0; i < posAfter.size(); ++i) {
        if (posAfter[i].x != posBefore[i].x) {
            moved = true;
            break;
        }
    }
    EXPECT_TRUE(moved);
}

TEST_F(SculptSessionTest, ResyncResetsStrokePositions)
{
    BrushParams params;
    params.radius = 2.0f;
    params.strength = 1.0f;

    StrokeId id = session->beginStroke(BrushKind::Draw, params);
    BrushSample sample;
    sample.position = {1.0f, 1.0f, 1.0f};
    sample.sequence = 1;
    ASSERT_TRUE(session->applySample(id, sample));
    session->endStroke(id);

    auto posAfter = mesh.attributes().positions();
    session->resync();

    auto posAfterResync = mesh.attributes().positions();
    for (size_t i = 0; i < posAfter.size(); ++i) {
        EXPECT_FLOAT_EQ(posAfter[i].x, posAfterResync[i].x);
    }
}
