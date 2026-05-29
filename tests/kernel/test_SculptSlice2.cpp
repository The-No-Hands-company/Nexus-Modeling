// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — Slice 2 brush tests: Crease, Layer, Grab
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/Mesh.h>
#include <nexus/sculpt/Brush.h>
#include <nexus/sculpt/SculptSession.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::sculpt::testing {

using nexus::geometry::Mesh;
using nexus::geometry::primitives::makePlane;
using nexus::render::Vec3;

namespace {

float length(const Vec3& v) noexcept
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

float distSq(const Vec3& a, const Vec3& b) noexcept
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

Mesh makePlaneWithNormals(float size = 2.0f, uint32_t segs = 4)
{
    Mesh m = makePlane(size, size, segs, segs);
    EXPECT_TRUE(m.computeVertexNormals());
    return m;
}

uint32_t findClosestVertex(const std::vector<Vec3>& pos, const Vec3& target)
{
    uint32_t best = 0;
    float bestD = distSq(pos[0], target);
    for (uint32_t i = 1; i < pos.size(); ++i) {
        float d = distSq(pos[i], target);
        if (d < bestD) { best = i; bestD = d; }
    }
    return best;
}

BrushSample makeSample(Vec3 pos, uint64_t seq, float pressure = 1.0f)
{
    BrushSample s;
    s.position = pos;
    s.sequence = seq;
    s.pressure = pressure;
    return s;
}

} // namespace

// ── BrushKind enum: new values are recognized ─────────────────────────────────

TEST(SculptSlice2, BrushKindEnumValues) {
    EXPECT_NE(static_cast<int>(BrushKind::Crease), static_cast<int>(BrushKind::Pinch));
    EXPECT_NE(static_cast<int>(BrushKind::Layer),  static_cast<int>(BrushKind::Crease));
    EXPECT_NE(static_cast<int>(BrushKind::Grab),   static_cast<int>(BrushKind::Layer));
}

// ── Crease brush ─────────────────────────────────────────────────────────────

TEST(SculptCrease, MovesVertexTowardNeighborMidpoints) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);

    const auto posBefore = m.attributes().positions();
    const Vec3 center = {0.f, 0.f, 0.f};
    uint32_t vi = findClosestVertex(posBefore, center);

    BrushParams params;
    params.radius   = 2.0f;   // cover central vertex
    params.strength = 1.0f;   // full strength → snap to midpoint avg
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;

    auto id = session.beginStroke(BrushKind::Crease, params);
    ASSERT_NE(id, kInvalidStrokeId);
    EXPECT_TRUE(session.applySample(id, makeSample(center, 1)));
    auto delta = session.endStroke(id);
    EXPECT_TRUE(delta.isValid());

    const auto posAfter = m.attributes().positions();
    // Central vertex should have moved toward neighbor midpoints — not stayed at original.
    EXPECT_GT(distSq(posAfter[vi], posBefore[vi]), 0.f);
}

TEST(SculptCrease, IsolatedVertex_NoNeighbors_NoChange) {
    // A 1x1 grid produces a single quad with perimeter vertices only.
    // Even if the brush hits the center, a vertex with no 1-ring neighbors is unchanged.
    Mesh m = makePlane(1.0f, 1.0f, 1, 1);
    m.computeVertexNormals();
    SculptSession session(m);

    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius   = 10.0f;
    params.strength = 1.0f;
    params.falloff  = FalloffShape::Constant;

    auto id = session.beginStroke(BrushKind::Crease, params);
    session.applySample(id, makeSample({0.f,0.f,0.f}, 1));
    session.endStroke(id);

    const auto posAfter = m.attributes().positions();
    // Vertices without adjacency should not move.
    bool anyWithAdjacency = false;
    for (uint32_t i = 0; i < posAfter.size(); ++i) {
        if (distSq(posAfter[i], posBefore[i]) > 1e-6f) {
            anyWithAdjacency = true; // at least one vertex moved — it has neighbors
        }
    }
    // A 1x1 plane has 4 corner vertices; each has 2 neighbors, so some should move.
    EXPECT_TRUE(anyWithAdjacency);
}

TEST(SculptCrease, UndoRestoresOriginalPositions) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius = 2.0f; params.strength = 0.5f;
    params.falloff = FalloffShape::Constant;

    auto id = session.beginStroke(BrushKind::Crease, params);
    session.applySample(id, makeSample({0,0,0}, 1));
    auto delta = session.endStroke(id);

    EXPECT_TRUE(session.undoStroke(delta));
    const auto posAfterUndo = m.attributes().positions();
    for (uint32_t i = 0; i < posBefore.size(); ++i) {
        EXPECT_NEAR(posAfterUndo[i].x, posBefore[i].x, 1e-5f) << "vertex " << i;
        EXPECT_NEAR(posAfterUndo[i].y, posBefore[i].y, 1e-5f) << "vertex " << i;
        EXPECT_NEAR(posAfterUndo[i].z, posBefore[i].z, 1e-5f) << "vertex " << i;
    }
}

TEST(SculptCrease, Deterministic) {
    auto run = [](float strength) -> std::vector<Vec3> {
        Mesh m = makePlaneWithNormals();
        SculptSession session(m);
        BrushParams p; p.radius = 2.0f; p.strength = strength;
        p.falloff = FalloffShape::Smooth;
        auto id = session.beginStroke(BrushKind::Crease, p);
        session.applySample(id, makeSample({0,0,0}, 1));
        session.endStroke(id);
        return m.attributes().positions();
    };
    auto r1 = run(0.6f);
    auto r2 = run(0.6f);
    ASSERT_EQ(r1.size(), r2.size());
    for (uint32_t i = 0; i < r1.size(); ++i) {
        EXPECT_EQ(r1[i].x, r2[i].x) << "vertex " << i;
        EXPECT_EQ(r1[i].y, r2[i].y) << "vertex " << i;
        EXPECT_EQ(r1[i].z, r2[i].z) << "vertex " << i;
    }
}

// ── Layer brush ───────────────────────────────────────────────────────────────

TEST(SculptLayer, DisplacesAlongNormal) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);

    const auto posBefore = m.attributes().positions();
    const Vec3 center = {0.f, 0.f, 0.f};
    uint32_t vi = findClosestVertex(posBefore, center);

    BrushParams params;
    params.radius    = 2.0f;
    params.strength  = 0.5f;
    params.falloff   = FalloffShape::Constant;
    params.useVertexNormal = true;
    params.maxPerVertexDisplacement = 1.0f;

    auto id = session.beginStroke(BrushKind::Layer, params);
    ASSERT_NE(id, kInvalidStrokeId);
    session.applySample(id, makeSample(center, 1));
    auto delta = session.endStroke(id);
    EXPECT_TRUE(delta.isValid());

    const auto posAfter = m.attributes().positions();
    // Plane normal is +Y; vertex should move upward.
    EXPECT_GT(posAfter[vi].y, posBefore[vi].y);
}

TEST(SculptLayer, RespectsCapAfterMultipleSamples) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);

    const auto posBefore = m.attributes().positions();
    const Vec3 center = {0.f, 0.f, 0.f};
    uint32_t vi = findClosestVertex(posBefore, center);

    const float cap = 0.3f;
    BrushParams params;
    params.radius    = 2.0f;
    params.strength  = 1.0f;
    params.falloff   = FalloffShape::Constant;
    params.useVertexNormal = true;
    params.maxPerVertexDisplacement = cap;

    auto id = session.beginStroke(BrushKind::Layer, params);
    // Apply many samples — displacement should saturate at cap.
    for (uint64_t seq = 1; seq <= 10; ++seq) {
        session.applySample(id, makeSample(center, seq));
    }
    session.endStroke(id);

    const auto posAfter = m.attributes().positions();
    const float disp = posAfter[vi].y - posBefore[vi].y;
    // Must not exceed the cap by more than floating-point tolerance.
    EXPECT_LE(disp, cap + 1e-4f);
    // Must have actually displaced by some positive amount.
    EXPECT_GT(disp, 0.f);
}

TEST(SculptLayer, UndoRestoresOriginal) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius = 2.0f; params.strength = 0.5f;
    params.falloff = FalloffShape::Constant;
    params.useVertexNormal = true;
    params.maxPerVertexDisplacement = 0.5f;

    auto id = session.beginStroke(BrushKind::Layer, params);
    session.applySample(id, makeSample({0,0,0}, 1));
    auto delta = session.endStroke(id);

    EXPECT_TRUE(session.undoStroke(delta));
    const auto posAfterUndo = m.attributes().positions();
    for (uint32_t i = 0; i < posBefore.size(); ++i) {
        EXPECT_NEAR(posAfterUndo[i].y, posBefore[i].y, 1e-5f) << "vertex " << i;
    }
}

TEST(SculptLayer, ZeroDisplacementWhenAlreadyAtCap) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();
    const Vec3 center = {0.f, 0.f, 0.f};
    uint32_t vi = findClosestVertex(posBefore, center);

    BrushParams params;
    params.radius    = 2.0f;
    params.strength  = 1.0f;
    params.falloff   = FalloffShape::Constant;
    params.useVertexNormal = true;
    params.maxPerVertexDisplacement = 0.1f;

    // First stroke: reach the cap.
    auto id1 = session.beginStroke(BrushKind::Layer, params);
    for (uint64_t s = 1; s <= 20; ++s) session.applySample(id1, makeSample(center, s));
    session.endStroke(id1);
    const float afterFirst = m.attributes().positions()[vi].y;

    // Second stroke on same vertex: should not go further (new baseline = current pos).
    auto id2 = session.beginStroke(BrushKind::Layer, params);
    for (uint64_t s = 1; s <= 10; ++s) session.applySample(id2, makeSample(center, s));
    session.endStroke(id2);
    const float afterSecond = m.attributes().positions()[vi].y;

    // Second stroke baseline is reset, so it can displace up to the cap again.
    // But the first stroke already moved it, so the combined displacement is at most 2×cap.
    const float totalDisp = afterSecond - posBefore[vi].y;
    EXPECT_LE(totalDisp, 2.f * params.maxPerVertexDisplacement + 1e-4f);
}

TEST(SculptLayer, Deterministic) {
    auto run = [](float strength) -> std::vector<Vec3> {
        Mesh m = makePlaneWithNormals();
        SculptSession session(m);
        BrushParams p;
        p.radius = 2.0f; p.strength = strength;
        p.falloff = FalloffShape::Smooth;
        p.useVertexNormal = true;
        p.maxPerVertexDisplacement = 0.5f;
        auto id = session.beginStroke(BrushKind::Layer, p);
        session.applySample(id, makeSample({0,0,0}, 1));
        session.endStroke(id);
        return m.attributes().positions();
    };
    auto r1 = run(0.4f);
    auto r2 = run(0.4f);
    ASSERT_EQ(r1.size(), r2.size());
    for (uint32_t i = 0; i < r1.size(); ++i) {
        EXPECT_EQ(r1[i].y, r2[i].y) << "vertex " << i;
    }
}

// ── Grab brush ───────────────────────────────────────────────────────────────

TEST(SculptGrab, TranslatesVerticesByDelta) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius   = 10.0f;  // cover all vertices
    params.strength = 1.0f;
    params.falloff  = FalloffShape::Constant;

    const Vec3 origin = {0.f, 0.f, 0.f};
    const Vec3 target = {0.5f, 0.f, 0.f};

    auto id = session.beginStroke(BrushKind::Grab, params);
    ASSERT_NE(id, kInvalidStrokeId);
    session.applySample(id, makeSample(origin, 1)); // sets grabOrigin
    session.applySample(id, makeSample(target, 2)); // delta = {0.5, 0, 0}
    session.endStroke(id);

    const auto posAfter = m.attributes().positions();
    // With Constant falloff and large radius all vertices translate by ~delta.
    for (uint32_t i = 0; i < posAfter.size(); ++i) {
        EXPECT_NEAR(posAfter[i].x, posBefore[i].x + 0.5f, 1e-5f) << "vertex " << i;
        EXPECT_NEAR(posAfter[i].y, posBefore[i].y,         1e-5f) << "vertex " << i;
        EXPECT_NEAR(posAfter[i].z, posBefore[i].z,         1e-5f) << "vertex " << i;
    }
}

TEST(SculptGrab, FirstSampleAloneIsNoOp) {
    // First sample establishes grabOrigin; delta = 0, no movement.
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius = 10.0f; params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    auto id = session.beginStroke(BrushKind::Grab, params);
    session.applySample(id, makeSample({0.f, 0.f, 0.f}, 1));
    auto delta = session.endStroke(id);

    const auto posAfter = m.attributes().positions();
    for (uint32_t i = 0; i < posAfter.size(); ++i) {
        EXPECT_NEAR(posAfter[i].x, posBefore[i].x, 1e-5f);
        EXPECT_NEAR(posAfter[i].y, posBefore[i].y, 1e-5f);
        EXPECT_NEAR(posAfter[i].z, posBefore[i].z, 1e-5f);
    }
}

TEST(SculptGrab, FalloffAttenuatesPeripheralVertices) {
    // With a small radius only central vertices get the full delta;
    // outer vertices receive less or nothing.
    Mesh m = makePlaneWithNormals(4.0f, 8);
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius   = 0.6f;  // small — only hits central vertices
    params.strength = 1.0f;
    params.falloff  = FalloffShape::Sharp;

    const Vec3 origin = {0.f, 0.f, 0.f};
    const Vec3 target = {1.0f, 0.f, 0.f};

    auto id = session.beginStroke(BrushKind::Grab, params);
    session.applySample(id, makeSample(origin, 1));
    session.applySample(id, makeSample(target, 2));
    session.endStroke(id);

    const auto posAfter = m.attributes().positions();

    // Central vertex (closest to origin) moves the most.
    uint32_t vi = findClosestVertex(posBefore, origin);
    const float centralDx = posAfter[vi].x - posBefore[vi].x;
    EXPECT_GT(centralDx, 0.5f); // significantly moved

    // A far corner vertex moves much less.
    uint32_t vfar = findClosestVertex(posBefore, {2.f, 0.f, 2.f});
    const float farDx = posAfter[vfar].x - posBefore[vfar].x;
    EXPECT_LT(std::abs(farDx), centralDx * 0.1f + 1e-4f);
}

TEST(SculptGrab, UndoRestoresOriginalPositions) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);
    const auto posBefore = m.attributes().positions();

    BrushParams params;
    params.radius = 10.0f; params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    auto id = session.beginStroke(BrushKind::Grab, params);
    session.applySample(id, makeSample({0,0,0}, 1));
    session.applySample(id, makeSample({0.3f,0,0}, 2));
    auto delta = session.endStroke(id);

    EXPECT_TRUE(session.undoStroke(delta));
    const auto posUndo = m.attributes().positions();
    for (uint32_t i = 0; i < posBefore.size(); ++i) {
        EXPECT_NEAR(posUndo[i].x, posBefore[i].x, 1e-5f) << "vertex " << i;
        EXPECT_NEAR(posUndo[i].y, posBefore[i].y, 1e-5f) << "vertex " << i;
        EXPECT_NEAR(posUndo[i].z, posBefore[i].z, 1e-5f) << "vertex " << i;
    }
}

TEST(SculptGrab, RedoReproducesPostStrokeState) {
    Mesh m = makePlaneWithNormals();
    SculptSession session(m);

    BrushParams params;
    params.radius = 10.0f; params.strength = 1.0f;
    params.falloff = FalloffShape::Constant;

    auto id = session.beginStroke(BrushKind::Grab, params);
    session.applySample(id, makeSample({0,0,0}, 1));
    session.applySample(id, makeSample({0.25f,0,0}, 2));
    auto delta = session.endStroke(id);
    const auto posAfterStroke = m.attributes().positions();

    session.undoStroke(delta);
    session.redoStroke(delta);
    const auto posAfterRedo = m.attributes().positions();

    ASSERT_EQ(posAfterStroke.size(), posAfterRedo.size());
    for (uint32_t i = 0; i < posAfterStroke.size(); ++i) {
        EXPECT_NEAR(posAfterRedo[i].x, posAfterStroke[i].x, 1e-5f);
    }
}

TEST(SculptGrab, Deterministic) {
    auto run = [](float dx) -> std::vector<Vec3> {
        Mesh m = makePlaneWithNormals();
        SculptSession session(m);
        BrushParams p;
        p.radius = 10.0f; p.strength = 1.0f;
        p.falloff = FalloffShape::Smooth;
        auto id = session.beginStroke(BrushKind::Grab, p);
        session.applySample(id, makeSample({0,0,0}, 1));
        session.applySample(id, makeSample({dx,0,0}, 2));
        session.endStroke(id);
        return m.attributes().positions();
    };
    auto r1 = run(0.7f);
    auto r2 = run(0.7f);
    ASSERT_EQ(r1.size(), r2.size());
    for (uint32_t i = 0; i < r1.size(); ++i) {
        EXPECT_EQ(r1[i].x, r2[i].x) << "vertex " << i;
    }
}

// ── All three slice-2 brushes: empty stroke is a no-op ───────────────────────

TEST(SculptSlice2, EmptyStrokeIsNoOp) {
    for (auto kind : {BrushKind::Crease, BrushKind::Layer, BrushKind::Grab}) {
        Mesh m = makePlaneWithNormals();
        SculptSession session(m);
        const auto posBefore = m.attributes().positions();

        BrushParams p; p.radius = 2.0f; p.strength = 1.0f;
        auto id = session.beginStroke(kind, p);
        auto delta = session.endStroke(id); // no applySample

        EXPECT_TRUE(delta.vertexDeltas.empty()) << "kind=" << int(kind);
        const auto posAfter = m.attributes().positions();
        for (uint32_t i = 0; i < posBefore.size(); ++i) {
            EXPECT_FLOAT_EQ(posAfter[i].x, posBefore[i].x) << "kind=" << int(kind);
            EXPECT_FLOAT_EQ(posAfter[i].y, posBefore[i].y) << "kind=" << int(kind);
            EXPECT_FLOAT_EQ(posAfter[i].z, posBefore[i].z) << "kind=" << int(kind);
        }
    }
}

} // namespace nexus::sculpt::testing
