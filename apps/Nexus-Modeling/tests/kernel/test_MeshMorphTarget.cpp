#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshMorphTarget.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

namespace {

Mesh makeTestMesh()
{
    return makeBox(2.f, 2.f, 2.f);
}

MorphTarget makeShiftTarget(const Mesh& base, const nexus::render::Vec3& offset)
{
    MorphTarget t;
    const auto& pos = base.attributes().positions();
    t.deltas.resize(pos.size());
    for (size_t i = 0; i < pos.size(); ++i) {
        t.deltas[i] = offset;
    }
    return t;
}

bool positionsEqual(const Mesh& a, const Mesh& b)
{
    const auto& pa = a.attributes().positions();
    const auto& pb = b.attributes().positions();
    if (pa.size() != pb.size()) return false;
    for (size_t i = 0; i < pa.size(); ++i) {
        if (std::fabs(pa[i].x - pb[i].x) > 1e-6f ||
            std::fabs(pa[i].y - pb[i].y) > 1e-6f ||
            std::fabs(pa[i].z - pb[i].z) > 1e-6f)
            return false;
    }
    return true;
}

} // namespace

TEST(MeshMorphTarget, IdentityBlendPreservesPositions)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    MorphTarget t = makeShiftTarget(base, {1.f, 0.f, 0.f});
    Mesh result = MeshMorphTarget::blend(base, {t}, {0.f});

    ASSERT_TRUE(result.isValid());
    ASSERT_EQ(result.attributes().positions().size(), base.attributes().positions().size());
    const auto& orig = base.attributes().positions();
    const auto& res = result.attributes().positions();
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x, 1e-5f);
        EXPECT_NEAR(res[i].y, orig[i].y, 1e-5f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-5f);
    }
}

TEST(MeshMorphTarget, SingleTargetShiftAppliesCorrectly)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    nexus::render::Vec3 offset{0.f, 1.f, 0.f};
    MorphTarget t = makeShiftTarget(base, offset);
    Mesh result = MeshMorphTarget::blend(base, {t}, {1.f});

    ASSERT_TRUE(result.isValid());
    const auto& orig = base.attributes().positions();
    const auto& res = result.attributes().positions();
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x, 1e-5f);
        EXPECT_NEAR(res[i].y, orig[i].y + 1.f, 1e-5f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-5f);
    }
}

TEST(MeshMorphTarget, ScaledWeightIsHalfShift)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    nexus::render::Vec3 offset{2.f, 0.f, 0.f};
    MorphTarget t = makeShiftTarget(base, offset);
    Mesh result = MeshMorphTarget::blend(base, {t}, {0.5f});

    ASSERT_TRUE(result.isValid());
    const auto& orig = base.attributes().positions();
    const auto& res = result.attributes().positions();
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x + 1.f, 1e-5f);
        EXPECT_NEAR(res[i].y, orig[i].y, 1e-5f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-5f);
    }
}

TEST(MeshMorphTarget, MismatchedTargetWeightCountLeavesPositionsUnchanged)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    MorphTarget t = makeShiftTarget(base, {1.f, 0.f, 0.f});
    Mesh result = MeshMorphTarget::blend(base, {t, t}, {1.f});

    ASSERT_TRUE(result.isValid());
    EXPECT_TRUE(positionsEqual(result, base));
}

TEST(MeshMorphTarget, WrongDeltaCountLeavesPositionsUnchanged)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    MorphTarget t;
    t.deltas = {{0.f, 0.f, 0.f}};

    Mesh result = MeshMorphTarget::blend(base, {t}, {1.f});

    ASSERT_TRUE(result.isValid());
    EXPECT_TRUE(positionsEqual(result, base));
}

TEST(MeshMorphTarget, MultipleTargetsAccumulate)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    MorphTarget t1 = makeShiftTarget(base, {1.f, 0.f, 0.f});
    MorphTarget t2 = makeShiftTarget(base, {0.f, 2.f, 0.f});
    Mesh result = MeshMorphTarget::blend(base, {t1, t2}, {1.f, 1.f});

    ASSERT_TRUE(result.isValid());
    const auto& orig = base.attributes().positions();
    const auto& res = result.attributes().positions();
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x + 1.f, 1e-5f);
        EXPECT_NEAR(res[i].y, orig[i].y + 2.f, 1e-5f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-5f);
    }
}

TEST(MeshMorphTarget, NormalsRecomputeWhenRequested)
{
    Mesh base = makeTestMesh();
    ASSERT_TRUE(base.isValid());

    MorphTarget t = makeShiftTarget(base, {1.f, 0.f, 0.f});
    BlendOptions opts;
    opts.recomputeNormals = true;
    Mesh result = MeshMorphTarget::blend(base, {t}, {1.f}, opts);

    ASSERT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());
}
