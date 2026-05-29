#include <nexus/sculpt/SculptSession.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>

using namespace nexus::sculpt;
using nexus::render::Vec3;
using nexus::geometry::Face;

// ── Mesh helpers ──────────────────────────────────────────────────────────────

static nexus::geometry::Mesh makeLineMesh(int n, float spacing = 1.f)
{
    // n vertices along X, connected as a single degenerate polygon for adjacency.
    nexus::geometry::Mesh m;
    std::vector<Vec3> pos;
    for (int i = 0; i < n; ++i) {
        pos.push_back({i * spacing, 0.f, 0.f});
    }
    m.attributes().setPositions(pos);
    std::vector<Vec3> nrm(n, {0.f, 1.f, 0.f});
    m.attributes().setNormals(nrm);
    // Build a simple strip of faces so adjacency is non-trivial.
    for (int i = 0; i < n - 1; ++i) {
        m.topology().addFace(Face{{static_cast<uint32_t>(i), static_cast<uint32_t>(i + 1)}});
    }
    return m;
}

static nexus::geometry::Mesh makeSymmetricMesh()
{
    // 4 vertices: two on +X side and two on -X side.
    // v0=(-1,0,0), v1=(-2,0,0), v2=(1,0,0), v3=(2,0,0)
    nexus::geometry::Mesh m;
    std::vector<Vec3> pos = {{-1.f,0.f,0.f},{-2.f,0.f,0.f},{1.f,0.f,0.f},{2.f,0.f,0.f}};
    m.attributes().setPositions(pos);
    std::vector<Vec3> nrm(4, {0.f, 1.f, 0.f});
    m.attributes().setNormals(nrm);
    m.topology().addFace(Face{{0u, 1u}});
    m.topology().addFace(Face{{2u, 3u}});
    return m;
}

// ── Mask: default is unmasked ─────────────────────────────────────────────────

TEST(SculptSlice3, MaskDefaultIsUnmasked)
{
    auto mesh = makeLineMesh(5);
    SculptSession session(mesh);
    for (uint32_t i = 0; i < 5u; ++i) {
        EXPECT_FLOAT_EQ(session.getMask(i), 1.f);
    }
}

TEST(SculptSlice3, SetMaskOutOfRangeReturnsFalse)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    EXPECT_FALSE(session.setMask(10, 0.f));
}

TEST(SculptSlice3, SetMaskInRangeRoundTrips)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    EXPECT_TRUE(session.setMask(1, 0.5f));
    EXPECT_FLOAT_EQ(session.getMask(1), 0.5f);
}

TEST(SculptSlice3, SetMaskClampsBelowZero)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    EXPECT_TRUE(session.setMask(0, -99.f));
    EXPECT_FLOAT_EQ(session.getMask(0), 0.f);
}

TEST(SculptSlice3, SetMaskClampsAboveOne)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    EXPECT_TRUE(session.setMask(0, 5.f));
    EXPECT_FLOAT_EQ(session.getMask(0), 1.f);
}

TEST(SculptSlice3, FloodFillSetsAll)
{
    auto mesh = makeLineMesh(4);
    SculptSession session(mesh);
    session.floodFillMask(0.3f);
    for (uint32_t i = 0; i < 4u; ++i) {
        EXPECT_FLOAT_EQ(session.getMask(i), 0.3f);
    }
}

TEST(SculptSlice3, ClearMaskRestoresUnmasked)
{
    auto mesh = makeLineMesh(4);
    SculptSession session(mesh);
    session.floodFillMask(0.f);
    session.clearMask();
    for (uint32_t i = 0; i < 4u; ++i) {
        EXPECT_FLOAT_EQ(session.getMask(i), 1.f);
    }
}

TEST(SculptSlice3, InvertMaskFlipsValues)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    session.setMask(0, 0.f);
    session.setMask(1, 0.25f);
    session.setMask(2, 1.f);
    session.invertMask();
    EXPECT_FLOAT_EQ(session.getMask(0), 1.f);
    EXPECT_FLOAT_EQ(session.getMask(1), 0.75f);
    EXPECT_FLOAT_EQ(session.getMask(2), 0.f);
}

// ── Mask: effect on displacement ──────────────────────────────────────────────

TEST(SculptSlice3, FullyMaskedVertexIsNotDisplaced)
{
    auto mesh = makeLineMesh(3, 0.1f); // close together so brush hits all
    SculptSession session(mesh);

    // Mask vertex 1 fully.
    EXPECT_TRUE(session.setMask(1, 0.f));

    const Vec3 before = session.workingPositions()[1];

    BrushParams params;
    params.radius = 2.f;
    params.strength = 5.f;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto sid = session.beginStroke(BrushKind::Draw, params);
    ASSERT_NE(sid, kInvalidStrokeId);
    BrushSample s;
    s.position = {0.1f, 0.f, 0.f}; // center of mesh
    s.pressure = 1.f;
    s.sequence = 1;
    session.applySample(sid, s);
    [[maybe_unused]] auto _d = session.endStroke(sid);

    // Vertex 1 must not have moved.
    const Vec3 after = session.workingPositions()[1];
    EXPECT_FLOAT_EQ(after.x, before.x);
    EXPECT_FLOAT_EQ(after.y, before.y);
    EXPECT_FLOAT_EQ(after.z, before.z);
}

TEST(SculptSlice3, PartialMaskReducesDisplacement)
{
    // Two identical sessions; one has mask=0.5 on a vertex.
    auto mesh1 = makeLineMesh(1);
    auto mesh2 = makeLineMesh(1);
    SculptSession s1(mesh1);
    SculptSession s2(mesh2);
    s2.setMask(0, 0.5f);

    BrushParams params;
    params.radius = 5.f;
    params.strength = 1.f;
    params.falloff = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto applyOnce = [&](SculptSession& sess) {
        auto sid = sess.beginStroke(BrushKind::Draw, params);
        BrushSample s;
        s.position = {0.f, 0.f, 0.f};
        s.pressure = 1.f;
        s.sequence = 1;
        sess.applySample(sid, s);
        [[maybe_unused]] auto _d = sess.endStroke(sid);
    };
    applyOnce(s1);
    applyOnce(s2);

    const float dy1 = s1.workingPositions()[0].y;
    const float dy2 = s2.workingPositions()[0].y;
    EXPECT_GT(dy1, 0.f);
    EXPECT_GT(dy2, 0.f);
    EXPECT_NEAR(dy2, dy1 * 0.5f, 1e-4f);
}

TEST(SculptSlice3, MaskedVertexNotInDelta)
{
    auto mesh = makeLineMesh(2, 0.1f);
    SculptSession session(mesh);
    session.setMask(0, 0.f); // protect vertex 0

    BrushParams params;
    params.radius = 2.f;
    params.strength = 1.f;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};
    params.falloff = FalloffShape::Constant;

    auto sid = session.beginStroke(BrushKind::Draw, params);
    BrushSample s;
    s.position = {0.05f, 0.f, 0.f};
    s.pressure = 1.f;
    s.sequence = 1;
    session.applySample(sid, s);
    auto delta = session.endStroke(sid);

    // Vertex 0 should not appear in the delta.
    for (const auto& [idx, d] : delta.vertexDeltas) {
        EXPECT_NE(idx, 0u);
    }
}

// ── Symmetry API ──────────────────────────────────────────────────────────────

TEST(SculptSlice3, SymmetryDefaultIsNone)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    EXPECT_EQ(session.symmetry(), SymmetryAxes::None);
}

TEST(SculptSlice3, SetSymmetryRoundTrips)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    session.setSymmetry(SymmetryAxes::X);
    EXPECT_EQ(session.symmetry(), SymmetryAxes::X);
    session.setSymmetry(SymmetryAxes::None);
    EXPECT_EQ(session.symmetry(), SymmetryAxes::None);
}

// ── Symmetry: effect on displacement ─────────────────────────────────────────

TEST(SculptSlice3, XSymmetryMirrorsStampAcrossOrigin)
{
    // Mesh: v0=(-1,0,0), v1=(-2,0,0), v2=(1,0,0), v3=(2,0,0)
    auto mesh = makeSymmetricMesh();
    SculptSession session(mesh);
    session.setSymmetry(SymmetryAxes::X);

    BrushParams params;
    params.radius   = 1.5f;  // hits v2=(1,0,0) and v3=(2,0,0) from primary stamp at (2,0,0)
    params.strength = 1.f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto sid = session.beginStroke(BrushKind::Draw, params);
    BrushSample s;
    s.position  = {2.f, 0.f, 0.f}; // primary stamp at +X side
    s.pressure  = 1.f;
    s.sequence  = 1;
    session.applySample(sid, s);
    [[maybe_unused]] auto _d = session.endStroke(sid);

    // Primary stamp displaced vertices at +X side (v2, v3).
    // Mirror stamp at (-2,0,0) should have displaced vertices at -X side (v0, v1).
    const float dy_pos = session.workingPositions()[2].y; // v2 at (1,0,0), within 1.5 of (2,0,0)
    const float dy_neg = session.workingPositions()[0].y; // v0 at (-1,0,0), within 1.5 of (-2,0,0)

    EXPECT_GT(dy_pos, 0.f);
    EXPECT_GT(dy_neg, 0.f);
    EXPECT_NEAR(dy_pos, dy_neg, 1e-4f); // symmetric displacement
}

TEST(SculptSlice3, NoSymmetryDoesNotMirror)
{
    auto mesh = makeSymmetricMesh();
    SculptSession session(mesh); // symmetry=None by default

    BrushParams params;
    params.radius   = 1.5f;
    params.strength = 1.f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto sid = session.beginStroke(BrushKind::Draw, params);
    BrushSample s;
    s.position  = {2.f, 0.f, 0.f};
    s.pressure  = 1.f;
    s.sequence  = 1;
    session.applySample(sid, s);
    [[maybe_unused]] auto _d = session.endStroke(sid);

    // -X side should be untouched.
    EXPECT_FLOAT_EQ(session.workingPositions()[0].y, 0.f);
    EXPECT_FLOAT_EQ(session.workingPositions()[1].y, 0.f);
}

TEST(SculptSlice3, SymmetryAxesCombination)
{
    // Mesh with 4 vertices in a cross pattern.
    nexus::geometry::Mesh mesh;
    std::vector<Vec3> pos = {
        { 1.f, 0.f, 0.f}, // v0: +X
        {-1.f, 0.f, 0.f}, // v1: -X
        {0.f,  1.f, 0.f}, // v2: +Y
        {0.f, -1.f, 0.f}, // v3: -Y
    };
    mesh.attributes().setPositions(pos);
    std::vector<Vec3> nrm(4, {0.f, 0.f, 1.f});
    mesh.attributes().setNormals(nrm);
    mesh.topology().addFace(Face{{0u, 1u}});
    mesh.topology().addFace(Face{{2u, 3u}});

    SculptSession session(mesh);
    session.setSymmetry(SymmetryAxes::X | SymmetryAxes::Y);

    BrushParams params;
    params.radius   = 1.5f;
    params.strength = 1.f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 0.f, 1.f}; // displace along Z

    auto sid = session.beginStroke(BrushKind::Draw, params);
    BrushSample s;
    s.position  = {1.f, 0.f, 0.f}; // stamp at +X
    s.pressure  = 1.f;
    s.sequence  = 1;
    session.applySample(sid, s);
    [[maybe_unused]] auto _d = session.endStroke(sid);

    // v0 (+X) hit by primary stamp; v1 (-X) hit by X-mirror stamp.
    EXPECT_GT(session.workingPositions()[0].z, 0.f);
    EXPECT_GT(session.workingPositions()[1].z, 0.f);
}

TEST(SculptSlice3, SymmetryIsDeterministicAcrossSessions)
{
    auto mesh1 = makeSymmetricMesh();
    auto mesh2 = makeSymmetricMesh();
    SculptSession s1(mesh1);
    SculptSession s2(mesh2);
    s1.setSymmetry(SymmetryAxes::X);
    s2.setSymmetry(SymmetryAxes::X);

    BrushParams params;
    params.radius   = 1.5f;
    params.strength = 0.7f;
    params.falloff  = FalloffShape::Smooth;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto applyOnce = [&](SculptSession& sess) {
        auto sid = sess.beginStroke(BrushKind::Draw, params);
        BrushSample s;
        s.position = {1.5f, 0.f, 0.f};
        s.pressure = 1.f;
        s.sequence = 1;
        sess.applySample(sid, s);
        [[maybe_unused]] auto _d = sess.endStroke(sid);
    };
    applyOnce(s1);
    applyOnce(s2);

    const auto& p1 = s1.workingPositions();
    const auto& p2 = s2.workingPositions();
    ASSERT_EQ(p1.size(), p2.size());
    for (size_t i = 0; i < p1.size(); ++i) {
        EXPECT_FLOAT_EQ(p1[i].x, p2[i].x);
        EXPECT_FLOAT_EQ(p1[i].y, p2[i].y);
        EXPECT_FLOAT_EQ(p1[i].z, p2[i].z);
    }
}

// ── Mask + Symmetry interaction ───────────────────────────────────────────────

TEST(SculptSlice3, MaskAppliesOnMirroredStampToo)
{
    // v0=(-1,0,0), v1=(-2,0,0), v2=(1,0,0), v3=(2,0,0)
    auto mesh = makeSymmetricMesh();
    SculptSession session(mesh);
    session.setSymmetry(SymmetryAxes::X);

    // Mask v0 fully.
    session.setMask(0, 0.f);

    BrushParams params;
    params.radius   = 1.5f;
    params.strength = 1.f;
    params.falloff  = FalloffShape::Constant;
    params.useVertexNormal = false;
    params.direction = {0.f, 1.f, 0.f};

    auto sid = session.beginStroke(BrushKind::Draw, params);
    BrushSample s;
    s.position  = {2.f, 0.f, 0.f};
    s.pressure  = 1.f;
    s.sequence  = 1;
    session.applySample(sid, s);
    [[maybe_unused]] auto _d = session.endStroke(sid);

    // v0 at (-1,0,0) is within range of the mirror stamp at (-2,0,0) but masked.
    EXPECT_FLOAT_EQ(session.workingPositions()[0].y, 0.f);
    // v2 at (1,0,0) is within primary stamp and unmasked.
    EXPECT_GT(session.workingPositions()[2].y, 0.f);
}

// ── resync preserves mask size ────────────────────────────────────────────────

TEST(SculptSlice3, ResyncPreservesMaskValues)
{
    auto mesh = makeLineMesh(3);
    SculptSession session(mesh);
    session.setMask(0, 0.25f);
    session.setMask(1, 0.75f);
    // resync() must not clobber existing mask values.
    session.resync();
    EXPECT_FLOAT_EQ(session.getMask(0), 0.25f);
    EXPECT_FLOAT_EQ(session.getMask(1), 0.75f);
}
