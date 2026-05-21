// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Sculpt — SculptSession + Brush behavior tests (Slice 1)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/Mesh.h>
#include <nexus/sculpt/Brush.h>
#include <nexus/sculpt/SculptSession.h>

#include <gtest/gtest.h>

#include <cmath>

namespace nexus::sculpt::testing {

using nexus::geometry::Mesh;
using nexus::geometry::primitives::makePlane;
using nexus::geometry::primitives::makeSphere;
using nexus::render::Vec3;

namespace {

float distSq(const Vec3& a, const Vec3& b) noexcept
{
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

Mesh makePlaneWithNormals(float size, uint32_t segs)
{
    Mesh m = makePlane(size, size, segs, segs);
    EXPECT_TRUE(m.computeVertexNormals());
    return m;
}

uint32_t findClosestVertexIn(const std::vector<Vec3>& pos, const Vec3& target)
{
    uint32_t best  = 0;
    float    bestD = distSq(pos[0], target);
    for (uint32_t i = 1; i < pos.size(); ++i) {
        const float d = distSq(pos[i], target);
        if (d < bestD) {
            best  = i;
            bestD = d;
        }
    }
    return best;
}

} // namespace

// ── Falloff helper ──────────────────────────────────────────────────────────

TEST(SculptFalloff, MonotonicAndBounded)
{
    for (auto shape : {FalloffShape::Constant, FalloffShape::Linear,
                       FalloffShape::Smooth, FalloffShape::Sharp}) {
        EXPECT_FLOAT_EQ(evaluateFalloff(shape, 0.f), 1.f) << "shape=" << int(shape);
        if (shape == FalloffShape::Constant) {
            EXPECT_FLOAT_EQ(evaluateFalloff(shape, 1.f), 1.f);
        } else {
            EXPECT_NEAR(evaluateFalloff(shape, 1.f), 0.f, 1e-6f) << "shape=" << int(shape);
        }
        EXPECT_FLOAT_EQ(evaluateFalloff(shape, -0.5f), evaluateFalloff(shape, 0.f));
        EXPECT_FLOAT_EQ(evaluateFalloff(shape,  1.5f), evaluateFalloff(shape, 1.f));
        float prev = evaluateFalloff(shape, 0.f);
        for (int i = 1; i <= 10; ++i) {
            const float v = evaluateFalloff(shape, i / 10.f);
            EXPECT_LE(v, prev + 1e-6f) << "shape=" << int(shape) << " i=" << i;
            EXPECT_GE(v, 0.f);
            EXPECT_LE(v, 1.f);
            prev = v;
        }
    }
}

// ── beginStroke validation ──────────────────────────────────────────────────

TEST(SculptSession, BeginStrokeRejectsInvalidParams)
{
    Mesh plane = makePlaneWithNormals(2.f, 4);
    SculptSession session(plane);

    BrushParams good{};
    good.radius = 0.5f;
    good.useVertexNormal = true;

    BrushParams badRadius = good;
    badRadius.radius = 0.f;
    EXPECT_EQ(session.beginStroke(BrushKind::Draw, badRadius), kInvalidStrokeId);

    BrushParams badDir{};
    badDir.radius = 0.5f;
    badDir.useVertexNormal = false;
    badDir.direction = {0.f, 0.f, 0.f};
    EXPECT_EQ(session.beginStroke(BrushKind::Draw, badDir), kInvalidStrokeId);

    const StrokeId id = session.beginStroke(BrushKind::Draw, good);
    EXPECT_NE(id, kInvalidStrokeId);
    EXPECT_TRUE(session.strokeInProgress());

    // Second begin while one is open must fail.
    EXPECT_EQ(session.beginStroke(BrushKind::Draw, good), kInvalidStrokeId);

    (void)session.endStroke(id);
    EXPECT_FALSE(session.strokeInProgress());
}

// ── Draw brush along vertex normal ──────────────────────────────────────────

TEST(SculptSession, DrawAlongVertexNormalDisplacesUpward)
{
    Mesh plane = makePlaneWithNormals(4.f, 8);
    const auto baseline = plane.attributes().positions();
    SculptSession session(plane);

    BrushParams params{};
    params.radius   = 1.0f;
    params.strength = 1.0f;
    params.falloff  = FalloffShape::Smooth;
    params.useVertexNormal = true;

    const StrokeId id = session.beginStroke(BrushKind::Draw, params);
    ASSERT_NE(id, kInvalidStrokeId);

    BrushSample s{};
    s.position = {0.f, 0.f, 0.f};
    s.pressure = 1.f;
    s.sequence = 1;
    ASSERT_TRUE(session.applySample(id, s));

    const StrokeDelta delta = session.endStroke(id);
    EXPECT_EQ(delta.kind, BrushKind::Draw);
    EXPECT_GT(delta.touchedVertexCount(), 0u);

    // makePlane is the XZ plane with +Y normals. Center vertex must move upward
    // by more than any edge vertex (smooth falloff).
    const uint32_t center = findClosestVertexIn(baseline, {0.f, 0.f, 0.f});
    const auto& after = plane.attributes().positions();
    EXPECT_GT(after[center].y, baseline[center].y);

    // All non-center, in-radius vertices moved less than center.
    const float dyCenter = after[center].y - baseline[center].y;
    for (uint32_t i = 0; i < after.size(); ++i) {
        if (i == center) continue;
        const Vec3& bp = baseline[i];
        const float d = std::sqrt(bp.x * bp.x + bp.z * bp.z);
        if (d < params.radius) {
            EXPECT_LE(after[i].y - bp.y, dyCenter + 1e-6f);
        } else {
            EXPECT_NEAR(after[i].y, bp.y, 1e-6f);
            EXPECT_NEAR(after[i].x, bp.x, 1e-6f);
            EXPECT_NEAR(after[i].z, bp.z, 1e-6f);
        }
    }
}

// ── Inflate brush ───────────────────────────────────────────────────────────

TEST(SculptSession, InflateMovesAlongVertexNormal)
{
    Mesh sphere = makeSphere(1.f, 12, 16);
    ASSERT_TRUE(sphere.computeVertexNormals());
    const auto baseline = sphere.attributes().positions();
    const auto baseNormals = sphere.attributes().normals();

    SculptSession session(sphere);
    BrushParams params{};
    params.radius   = 2.5f;  // covers the whole sphere
    params.strength = 0.25f;
    params.falloff  = FalloffShape::Constant;

    const StrokeId id = session.beginStroke(BrushKind::Inflate, params);
    ASSERT_NE(id, kInvalidStrokeId);

    BrushSample s{};
    s.position = {0.f, 0.f, 0.f};
    s.pressure = 1.f;
    s.sequence = 1;
    ASSERT_TRUE(session.applySample(id, s));

    const StrokeDelta delta = session.endStroke(id);
    EXPECT_GT(delta.touchedVertexCount(), 0u);

    const auto& after = sphere.attributes().positions();
    // Every touched vertex moved approximately along its baseline normal.
    for (const auto& [v, d] : delta.vertexDeltas) {
        const Vec3& n = baseNormals[v];
        const Vec3  moved = after[v] - baseline[v];
        const float ml = std::sqrt(moved.x * moved.x + moved.y * moved.y + moved.z * moved.z);
        if (ml < 1e-6f) continue;
        const Vec3 movedN = {moved.x / ml, moved.y / ml, moved.z / ml};
        const float cosA = movedN.x * n.x + movedN.y * n.y + movedN.z * n.z;
        EXPECT_GT(cosA, 0.99f) << "vertex " << v;
    }
}

// ── Smooth brush ────────────────────────────────────────────────────────────

TEST(SculptSession, SmoothBrushReducesRoughness)
{
    Mesh plane = makePlaneWithNormals(4.f, 8);
    // Manually perturb every other vertex along +Y to create roughness.
    {
        auto positions = plane.attributes().positions();
        for (size_t i = 0; i < positions.size(); ++i) {
            positions[i].y += (i % 2 == 0) ? 0.2f : -0.2f;
        }
        plane.attributes().setPositions(std::move(positions));
    }
    const auto noisy = plane.attributes().positions();
    auto computeRoughnessSq = [](const std::vector<Vec3>& p) {
        float s = 0.f;
        for (const auto& v : p) s += v.y * v.y;
        return s;
    };
    const float r0 = computeRoughnessSq(noisy);

    SculptSession session(plane);
    BrushParams params{};
    params.radius   = 5.f;  // cover everything
    params.strength = 0.8f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    const StrokeId id = session.beginStroke(BrushKind::Smooth, params);
    ASSERT_NE(id, kInvalidStrokeId);
    BrushSample s{};
    s.position = {0.f, 0.f, 0.f};
    s.pressure = 1.f;
    s.sequence = 1;
    ASSERT_TRUE(session.applySample(id, s));
    (void)session.endStroke(id);

    const float r1 = computeRoughnessSq(plane.attributes().positions());
    EXPECT_LT(r1, r0 * 0.95f);
}

// ── Flatten brush ───────────────────────────────────────────────────────────

TEST(SculptSession, FlattenReducesDeviationFromPlane)
{
    Mesh plane = makePlaneWithNormals(4.f, 8);
    // Bumpy y-values.
    {
        auto positions = plane.attributes().positions();
        for (size_t i = 0; i < positions.size(); ++i) {
            positions[i].y = std::sin(static_cast<float>(i)) * 0.3f;
        }
        plane.attributes().setPositions(std::move(positions));
    }
    auto maxAbsY = [](const std::vector<Vec3>& p) {
        float m = 0.f;
        for (const auto& v : p) m = std::max(m, std::abs(v.y));
        return m;
    };
    const float before = maxAbsY(plane.attributes().positions());

    SculptSession session(plane);
    BrushParams params{};
    params.radius   = 5.f;
    params.strength = 1.0f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    const StrokeId id = session.beginStroke(BrushKind::Flatten, params);
    ASSERT_NE(id, kInvalidStrokeId);
    BrushSample s{};
    s.position = {0.f, 0.f, 0.f};  // y=0 plane
    s.sequence = 1;
    s.pressure = 1.f;
    ASSERT_TRUE(session.applySample(id, s));
    (void)session.endStroke(id);

    const float after = maxAbsY(plane.attributes().positions());
    EXPECT_LT(after, before * 0.5f);
}

// ── Pinch brush ─────────────────────────────────────────────────────────────

TEST(SculptSession, PinchDrawsVerticesTowardCenter)
{
    Mesh plane = makePlaneWithNormals(4.f, 8);
    const auto baseline = plane.attributes().positions();
    SculptSession session(plane);

    BrushParams params{};
    params.radius   = 1.5f;
    params.strength = 0.5f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};  // unused by Pinch but must be non-zero

    const StrokeId id = session.beginStroke(BrushKind::Pinch, params);
    ASSERT_NE(id, kInvalidStrokeId);
    BrushSample s{};
    s.position = {0.f, 0.f, 0.f};
    s.sequence = 1;
    s.pressure = 1.f;
    ASSERT_TRUE(session.applySample(id, s));
    const StrokeDelta delta = session.endStroke(id);

    const auto& after = plane.attributes().positions();
    for (const auto& [v, d] : delta.vertexDeltas) {
        const float dBefore = std::sqrt(distSq(baseline[v], {0.f, 0.f, 0.f}));
        const float dAfter  = std::sqrt(distSq(after[v],    {0.f, 0.f, 0.f}));
        EXPECT_LE(dAfter, dBefore + 1e-6f) << "vertex " << v;
    }
}

// ── Sample ordering ─────────────────────────────────────────────────────────

TEST(SculptSession, ApplySampleRejectsNonIncreasingSequence)
{
    Mesh plane = makePlaneWithNormals(2.f, 4);
    SculptSession session(plane);

    BrushParams params{};
    params.radius = 1.f;
    params.strength = 0.5f;
    const StrokeId id = session.beginStroke(BrushKind::Draw, params);
    ASSERT_NE(id, kInvalidStrokeId);

    BrushSample s1{}; s1.sequence = 5; s1.position = {0.f, 0.f, 0.f}; s1.pressure = 1.f;
    BrushSample s2 = s1; s2.sequence = 5;  // equal, must reject
    BrushSample s3 = s1; s3.sequence = 3;  // backwards, must reject
    BrushSample s4 = s1; s4.sequence = 6;  // increases, must accept

    EXPECT_TRUE(session.applySample(id, s1));
    EXPECT_FALSE(session.applySample(id, s2));
    EXPECT_FALSE(session.applySample(id, s3));
    EXPECT_TRUE(session.applySample(id, s4));

    (void)session.endStroke(id);
}

// ── Undo / Redo round-trip ──────────────────────────────────────────────────

TEST(SculptSession, UndoRestoresOriginalPositions)
{
    Mesh plane = makePlaneWithNormals(4.f, 6);
    const auto original = plane.attributes().positions();

    SculptSession session(plane);
    BrushParams params{};
    params.radius   = 1.5f;
    params.strength = 0.5f;
    params.useVertexNormal = true;

    StrokeDelta d1, d2;
    {
        const StrokeId id = session.beginStroke(BrushKind::Draw, params);
        BrushSample s{}; s.position = {0.f, 0.f, 0.f}; s.pressure = 1.f; s.sequence = 1;
        ASSERT_TRUE(session.applySample(id, s));
        d1 = session.endStroke(id);
    }
    {
        const StrokeId id = session.beginStroke(BrushKind::Inflate, params);
        BrushSample s{}; s.position = {0.5f, 0.f, 0.5f}; s.pressure = 1.f; s.sequence = 1;
        ASSERT_TRUE(session.applySample(id, s));
        d2 = session.endStroke(id);
    }

    ASSERT_TRUE(session.undoStroke(d2));
    ASSERT_TRUE(session.undoStroke(d1));

    const auto& restored = plane.attributes().positions();
    ASSERT_EQ(restored.size(), original.size());
    for (size_t i = 0; i < restored.size(); ++i) {
        EXPECT_NEAR(restored[i].x, original[i].x, 1e-5f) << "v=" << i;
        EXPECT_NEAR(restored[i].y, original[i].y, 1e-5f) << "v=" << i;
        EXPECT_NEAR(restored[i].z, original[i].z, 1e-5f) << "v=" << i;
    }
}

TEST(SculptSession, RedoReproducesPostStrokeState)
{
    Mesh plane = makePlaneWithNormals(4.f, 6);
    SculptSession session(plane);

    BrushParams params{};
    params.radius   = 1.2f;
    params.strength = 0.7f;
    params.useVertexNormal = true;

    const StrokeId id = session.beginStroke(BrushKind::Draw, params);
    BrushSample s{}; s.position = {0.f, 0.f, 0.f}; s.pressure = 1.f; s.sequence = 1;
    ASSERT_TRUE(session.applySample(id, s));
    const StrokeDelta delta = session.endStroke(id);

    const auto afterStroke = plane.attributes().positions();
    ASSERT_TRUE(session.undoStroke(delta));
    ASSERT_TRUE(session.redoStroke(delta));
    const auto afterRedo = plane.attributes().positions();

    ASSERT_EQ(afterStroke.size(), afterRedo.size());
    for (size_t i = 0; i < afterStroke.size(); ++i) {
        EXPECT_FLOAT_EQ(afterStroke[i].x, afterRedo[i].x);
        EXPECT_FLOAT_EQ(afterStroke[i].y, afterRedo[i].y);
        EXPECT_FLOAT_EQ(afterStroke[i].z, afterRedo[i].z);
    }
}

// ── Determinism: identical strokes on identical meshes ──────────────────────

TEST(SculptSession, DeterministicReplayAcrossSessions)
{
    Mesh a = makePlaneWithNormals(4.f, 8);
    Mesh b = makePlaneWithNormals(4.f, 8);
    SculptSession sa(a);
    SculptSession sb(b);

    BrushParams params{};
    params.radius   = 1.2f;
    params.strength = 0.6f;
    params.useVertexNormal = true;
    params.falloff  = FalloffShape::Smooth;

    auto runStroke = [&](SculptSession& s) -> StrokeDelta {
        const StrokeId id = s.beginStroke(BrushKind::Draw, params);
        for (int i = 1; i <= 5; ++i) {
            BrushSample bs{};
            bs.position = {static_cast<float>(i) * 0.1f, 0.f, -0.2f};
            bs.pressure = 0.8f;
            bs.sequence = static_cast<uint64_t>(i);
            (void)s.applySample(id, bs);
        }
        return s.endStroke(id);
    };

    const StrokeDelta da = runStroke(sa);
    const StrokeDelta db = runStroke(sb);

    ASSERT_EQ(da.vertexDeltas.size(), db.vertexDeltas.size());
    for (size_t i = 0; i < da.vertexDeltas.size(); ++i) {
        EXPECT_EQ(da.vertexDeltas[i].first, db.vertexDeltas[i].first);
        EXPECT_FLOAT_EQ(da.vertexDeltas[i].second.x, db.vertexDeltas[i].second.x);
        EXPECT_FLOAT_EQ(da.vertexDeltas[i].second.y, db.vertexDeltas[i].second.y);
        EXPECT_FLOAT_EQ(da.vertexDeltas[i].second.z, db.vertexDeltas[i].second.z);
    }
    const auto& pa = a.attributes().positions();
    const auto& pb = b.attributes().positions();
    ASSERT_EQ(pa.size(), pb.size());
    for (size_t i = 0; i < pa.size(); ++i) {
        EXPECT_FLOAT_EQ(pa[i].x, pb[i].x);
        EXPECT_FLOAT_EQ(pa[i].y, pb[i].y);
        EXPECT_FLOAT_EQ(pa[i].z, pb[i].z);
    }
}

// ── Empty stroke ────────────────────────────────────────────────────────────

TEST(SculptSession, EmptyStrokeProducesNoChanges)
{
    Mesh plane = makePlaneWithNormals(2.f, 4);
    const auto before = plane.attributes().positions();
    SculptSession session(plane);

    BrushParams params{};
    params.radius = 0.5f;
    const StrokeId id = session.beginStroke(BrushKind::Draw, params);
    ASSERT_NE(id, kInvalidStrokeId);
    const StrokeDelta delta = session.endStroke(id);
    EXPECT_TRUE(delta.isValid());
    EXPECT_EQ(delta.touchedVertexCount(), 0u);

    const auto& after = plane.attributes().positions();
    ASSERT_EQ(before.size(), after.size());
    for (size_t i = 0; i < before.size(); ++i) {
        EXPECT_FLOAT_EQ(before[i].x, after[i].x);
        EXPECT_FLOAT_EQ(before[i].y, after[i].y);
        EXPECT_FLOAT_EQ(before[i].z, after[i].z);
    }
}

// ── Stats accumulation ──────────────────────────────────────────────────────

TEST(SculptSession, StatsAccumulateAcrossStrokes)
{
    Mesh plane = makePlaneWithNormals(2.f, 4);
    SculptSession session(plane);

    BrushParams params{};
    params.radius = 0.6f;
    params.strength = 0.2f;
    params.useVertexNormal = true;

    for (int k = 0; k < 3; ++k) {
        const StrokeId id = session.beginStroke(BrushKind::Draw, params);
        BrushSample s{}; s.position = {0.f, 0.f, 0.f}; s.pressure = 1.f; s.sequence = 1;
        (void)session.applySample(id, s);
        (void)session.endStroke(id);
    }
    EXPECT_EQ(session.stats().strokesStarted, 3u);
    EXPECT_EQ(session.stats().strokesCommitted, 3u);
    EXPECT_EQ(session.stats().samplesProcessed, 3u);
    EXPECT_GT(session.stats().verticesTouched, 0u);

    session.resetStats();
    EXPECT_EQ(session.stats().strokesStarted, 0u);
    EXPECT_EQ(session.stats().verticesTouched, 0u);
}

} // namespace nexus::sculpt::testing
