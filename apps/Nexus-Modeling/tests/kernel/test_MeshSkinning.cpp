#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshSkinning.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

namespace {

struct Bone {
    nexus::render::Mat4 transform = nexus::render::Mat4::identity();
};

Mesh makeSkinnedTestMesh()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f, 0.f, 0.f},
        { 1.f, 0.f, 0.f},
        { 0.f, 1.f, 0.f},
    };
    Face f{};
    f.indices = {0, 1, 2};
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    mesh.topology().addFace(std::move(f));

    std::vector<JointIndex4> indices(mesh.attributes().vertexCount());
    std::vector<JointWeight4> weights(mesh.attributes().vertexCount());
    for (auto& idx : indices) { idx = {0, 1, 2, 3}; }
    for (auto& w : weights) {
        w = {1.f, 0.f, 0.f, 0.f};
    }
    mesh.attributes().setSkinning(std::move(indices), std::move(weights));
    return mesh;
}

Mesh makeMeshWithoutSkinning()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.f, 0.f, 0.f},
    };
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    return mesh;
}

} // namespace

TEST(MeshSkinning, IdentityPreservesPositions)
{
    Mesh mesh = makeSkinnedTestMesh();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    bones[0] = nexus::render::Mat4::identity();
    bones[1] = nexus::render::Mat4::identity();
    bones[2] = nexus::render::Mat4::identity();
    bones[3] = nexus::render::Mat4::identity();

    Mesh result = MeshSkinning::deform(mesh, bones);

    ASSERT_TRUE(result.isValid());
    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    EXPECT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-4f);
    }
}

TEST(MeshSkinning, TranslationAppliesCorrectly)
{
    Mesh mesh = makeSkinnedTestMesh();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    bones[0] = nexus::render::Mat4::identity();
    bones[0].m[0][3] = 5.f;
    bones[0].m[1][3] = 2.f;
    bones[0].m[2][3] = -1.f;

    bones[1] = nexus::render::Mat4::identity();
    bones[2] = nexus::render::Mat4::identity();
    bones[3] = nexus::render::Mat4::identity();

    Mesh result = MeshSkinning::deform(mesh, bones);

    ASSERT_TRUE(result.isValid());
    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    EXPECT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x + 5.f, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y + 2.f, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z - 1.f, 1e-4f);
    }
}

TEST(MeshSkinning, NoSkinningDataFails)
{
    Mesh mesh = makeMeshWithoutSkinning();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_FALSE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    for (auto& b : bones) {
        b = nexus::render::Mat4::identity();
    }

    Mesh result = MeshSkinning::deform(mesh, bones);

    EXPECT_FALSE(result.isValid());
}

TEST(MeshSkinning, DQS_IdentityPreservesPositions)
{
    Mesh mesh = makeSkinnedTestMesh();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    for (auto& b : bones) {
        b = nexus::render::Mat4::identity();
    }

    SkinningOptions opts;
    opts.method = SkinningMethod::DQS;
    Mesh result = MeshSkinning::deform(mesh, bones, opts);

    ASSERT_TRUE(result.isValid());
    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    EXPECT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-4f);
    }
}

TEST(MeshSkinning, DQS_TranslationAppliesCorrectly)
{
    Mesh mesh = makeSkinnedTestMesh();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    bones[0] = nexus::render::Mat4::identity();
    bones[0].m[0][3] = 5.f;
    bones[0].m[1][3] = 2.f;
    bones[0].m[2][3] = -1.f;
    for (size_t i = 1; i < 4; ++i) {
        bones[i] = nexus::render::Mat4::identity();
    }

    SkinningOptions opts;
    opts.method = SkinningMethod::DQS;
    Mesh result = MeshSkinning::deform(mesh, bones, opts);

    ASSERT_TRUE(result.isValid());
    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    EXPECT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x + 5.f, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y + 2.f, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z - 1.f, 1e-4f);
    }
}

TEST(MeshSkinning, DQS_RotationAppliesCorrectly)
{
    Mesh mesh = makeSkinnedTestMesh();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_TRUE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    for (auto& b : bones) {
        b = nexus::render::Mat4::identity();
    }
    bones[0].m[0][0] = -1.f;
    bones[0].m[2][2] = -1.f;

    SkinningOptions opts;
    opts.method = SkinningMethod::DQS;
    Mesh result = MeshSkinning::deform(mesh, bones, opts);

    ASSERT_TRUE(result.isValid());
    const auto& res = result.attributes().positions();
    EXPECT_EQ(res.size(), 3u);
    EXPECT_NEAR(res[0].x,  0.f, 1e-4f);
    EXPECT_NEAR(res[0].y,  0.f, 1e-4f);
    EXPECT_NEAR(res[0].z,  0.f, 1e-4f);
    EXPECT_NEAR(res[1].x, -1.f, 1e-4f);
    EXPECT_NEAR(res[1].y,  0.f, 1e-4f);
    EXPECT_NEAR(res[1].z,  0.f, 1e-4f);
    EXPECT_NEAR(res[2].x,  0.f, 1e-4f);
    EXPECT_NEAR(res[2].y,  1.f, 1e-4f);
    EXPECT_NEAR(res[2].z,  0.f, 1e-4f);
}

TEST(MeshSkinning, DQS_NoSkinningDataFails)
{
    Mesh mesh = makeMeshWithoutSkinning();
    ASSERT_TRUE(mesh.isValid());
    ASSERT_FALSE(mesh.attributes().hasSkinning());

    std::vector<nexus::render::Mat4> bones(4);
    for (auto& b : bones) {
        b = nexus::render::Mat4::identity();
    }

    SkinningOptions opts;
    opts.method = SkinningMethod::DQS;
    Mesh result = MeshSkinning::deform(mesh, bones, opts);

    EXPECT_FALSE(result.isValid());
}
