#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshCageDeform.h>

#include <cmath>

using namespace nexus::geometry;

namespace {

FFDGrid makeUnitGrid()
{
    FFDGrid grid;
    grid.resolution = {2, 2, 2};
    grid.origin = {0.f, 0.f, 0.f};
    grid.cellSize = {1.f, 1.f, 1.f};
    grid.controlPoints = {
        {0.f, 0.f, 0.f}, {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f}, {1.f, 1.f, 0.f},
        {0.f, 0.f, 1.f}, {1.f, 0.f, 1.f},
        {0.f, 1.f, 1.f}, {1.f, 1.f, 1.f},
    };
    return grid;
}

Mesh makeTestMesh()
{
    std::vector<nexus::render::Vec3> positions = {
        { 0.1f, 0.1f, 0.1f},
        { 0.9f, 0.1f, 0.1f},
        { 0.1f, 0.9f, 0.1f},
        { 0.9f, 0.9f, 0.1f},
        { 0.1f, 0.1f, 0.9f},
        { 0.9f, 0.1f, 0.9f},
        { 0.1f, 0.9f, 0.9f},
        { 0.9f, 0.9f, 0.9f},
    };
    Mesh mesh;
    mesh.attributes().setPositions(std::move(positions));
    {
        Face f{};
        f.indices = {0, 1, 3, 2};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {4, 5, 7, 6};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {0, 4, 6, 2};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {1, 5, 7, 3};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {0, 1, 5, 4};
        mesh.topology().addFace(std::move(f));
    }
    {
        Face f{};
        f.indices = {2, 3, 7, 6};
        mesh.topology().addFace(std::move(f));
    }
    return mesh;
}

} // namespace

TEST(MeshCageDeform, BindSucceeds)
{
    Mesh mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());
    FFDGrid grid = makeUnitGrid();

    MeshCageDeform deformer;
    Mesh bound = deformer.bind(mesh, grid);

    ASSERT_TRUE(bound.isValid());
}

TEST(MeshCageDeform, IdentityDeformPreservesPositions)
{
    Mesh mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());
    FFDGrid grid = makeUnitGrid();

    MeshCageDeform deformer;
    Mesh bound = deformer.bind(mesh, grid);
    ASSERT_TRUE(bound.isValid());

    Mesh result = deformer.deform(bound, grid.controlPoints);
    ASSERT_TRUE(result.isValid());

    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    ASSERT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z, 1e-4f);
    }
}

TEST(MeshCageDeform, NotBoundFails)
{
    Mesh mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());
    FFDGrid grid = makeUnitGrid();

    MeshCageDeform deformer;
    Mesh result = deformer.deform(mesh, grid.controlPoints);

    ASSERT_TRUE(result.isValid());
    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    ASSERT_EQ(res.size(), orig.size());
    bool allSame = true;
    for (size_t i = 0; i < orig.size(); ++i) {
        if (std::fabs(res[i].x - orig[i].x) > 1e-6f ||
            std::fabs(res[i].y - orig[i].y) > 1e-6f ||
            std::fabs(res[i].z - orig[i].z) > 1e-6f) {
            allSame = false;
            break;
        }
    }
    EXPECT_TRUE(allSame);
}

TEST(MeshCageDeform, TranslationDeformWorks)
{
    Mesh mesh = makeTestMesh();
    ASSERT_TRUE(mesh.isValid());
    FFDGrid grid = makeUnitGrid();

    MeshCageDeform deformer;
    Mesh bound = deformer.bind(mesh, grid);
    ASSERT_TRUE(bound.isValid());

    std::vector<nexus::render::Vec3> translatedCage = grid.controlPoints;
    for (auto& cp : translatedCage) {
        cp.x += 2.f;
        cp.y += 3.f;
        cp.z += 4.f;
    }

    Mesh result = deformer.deform(bound, translatedCage);
    ASSERT_TRUE(result.isValid());

    const auto& orig = mesh.attributes().positions();
    const auto& res = result.attributes().positions();
    ASSERT_EQ(res.size(), orig.size());
    for (size_t i = 0; i < orig.size(); ++i) {
        EXPECT_NEAR(res[i].x, orig[i].x + 2.f, 1e-4f);
        EXPECT_NEAR(res[i].y, orig[i].y + 3.f, 1e-4f);
        EXPECT_NEAR(res[i].z, orig[i].z + 4.f, 1e-4f);
    }
}
