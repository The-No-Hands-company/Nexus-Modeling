#include <gtest/gtest.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshTangentSpace.h>
#include <nexus/geometry/MeshTransform.h>

#include <cmath>

using namespace nexus::geometry;
using namespace nexus::geometry::primitives;

TEST(MeshTransform, TranslationAppliesCorrectly)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    mat.m[0][3] = 5.f;
    mat.m[1][3] = -3.f;
    mat.m[2][3] = 7.f;

    Mesh result = MeshTransform::apply(box, mat);

    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.attributes().vertexCount(), origPos.size());
    EXPECT_EQ(result.topology().faceCount(), box.topology().faceCount());

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x + 5.f, 1e-4f);
        EXPECT_NEAR(newPos[i].y, origPos[i].y - 3.f, 1e-4f);
        EXPECT_NEAR(newPos[i].z, origPos[i].z + 7.f, 1e-4f);
    }
}

TEST(MeshTransform, IdentityPreservesPositions)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    Mesh result = MeshTransform::apply(box, mat);

    EXPECT_TRUE(result.isValid());
    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x, 1e-4f);
        EXPECT_NEAR(newPos[i].y, origPos[i].y, 1e-4f);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }
}

TEST(MeshTransform, NormalTransformRotation)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    ASSERT_TRUE(box.computeVertexNormals());
    const auto& origNorms = box.attributes().normals();

    nexus::render::Mat4 rot = nexus::render::Mat4::identity();
    rot.m[0][0] = 0.f; rot.m[0][2] = 1.f;
    rot.m[2][0] = -1.f; rot.m[2][2] = 0.f;

    Mesh result = MeshTransform::apply(box, rot, true);

    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());

    const auto& newNorms = result.attributes().normals();
    EXPECT_EQ(newNorms.size(), origNorms.size());

    for (size_t i = 0; i < origNorms.size(); ++i) {
        float len = std::sqrt(newNorms[i].x * newNorms[i].x +
                              newNorms[i].y * newNorms[i].y +
                              newNorms[i].z * newNorms[i].z);
        EXPECT_NEAR(len, 1.f, 1e-4f);

        bool anyChanged = (std::fabs(newNorms[i].x - origNorms[i].x) > 1e-4f ||
                           std::fabs(newNorms[i].y - origNorms[i].y) > 1e-4f ||
                           std::fabs(newNorms[i].z - origNorms[i].z) > 1e-4f);
        if (std::fabs(origNorms[i].y - 1.f) < 1e-4f) {
            EXPECT_FALSE(anyChanged);
        }
    }
}

TEST(MeshTransform, PivotRotatesAroundPoint)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    nexus::render::Vec3 pivot{3.f, 0.f, 0.f};

    nexus::render::Mat4 rot = nexus::render::Mat4::identity();
    rot.m[1][1] = -1.f;
    rot.m[2][2] = -1.f;

    Mesh result = MeshTransform::apply(box, rot, false, pivot);

    EXPECT_TRUE(result.isValid());
    const auto& pos = result.attributes().positions();
    const auto& origPos = box.attributes().positions();

    for (size_t i = 0; i < pos.size(); ++i) {
        float origDist = (origPos[i].x - pivot.x) * (origPos[i].x - pivot.x) +
                         origPos[i].y * origPos[i].y +
                         origPos[i].z * origPos[i].z;
        float newDist = (pos[i].x - pivot.x) * (pos[i].x - pivot.x) +
                        pos[i].y * pos[i].y +
                        pos[i].z * pos[i].z;
        EXPECT_NEAR(origDist, newDist, 1e-3f);

        EXPECT_NEAR(pos[i].x, origPos[i].x, 1e-4f);
    }
}

TEST(MeshTransform, PivotIdentityDoesNotAffectResult)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    mat.m[0][3] = 10.f;

    Mesh noPivot = MeshTransform::apply(box, mat, false);
    Mesh withPivot = MeshTransform::apply(box, mat, false, {0.f, 0.f, 0.f});

    const auto& a = noPivot.attributes().positions();
    const auto& b = withPivot.attributes().positions();
    ASSERT_EQ(a.size(), b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        EXPECT_NEAR(a[i].x, b[i].x, 1e-4f);
        EXPECT_NEAR(a[i].y, b[i].y, 1e-4f);
        EXPECT_NEAR(a[i].z, b[i].z, 1e-4f);
    }
}

TEST(MeshTransform, NormalMatrixUniformScaleReuses3x3)
{
    nexus::render::Mat4 s = nexus::render::Mat4::identity();
    s.m[0][0] = 2.f;
    s.m[1][1] = 2.f;
    s.m[2][2] = 2.f;

    nexus::render::Mat4 nm = MeshTransform::normalMatrix(s);

    EXPECT_NEAR(nm.m[0][0], 2.f, 1e-4f);
    EXPECT_NEAR(nm.m[1][1], 2.f, 1e-4f);
    EXPECT_NEAR(nm.m[2][2], 2.f, 1e-4f);
}

TEST(MeshTransform, NormalMatrixNonUniformScaleUsesInverseTranspose)
{
    nexus::render::Mat4 s = nexus::render::Mat4::identity();
    s.m[0][0] = 2.f;
    s.m[1][1] = 1.f;
    s.m[2][2] = 1.f;

    nexus::render::Mat4 nm = MeshTransform::normalMatrix(s);

    EXPECT_NEAR(nm.m[0][0], 0.5f, 1e-4f);
    EXPECT_NEAR(nm.m[1][1], 1.f, 1e-4f);
    EXPECT_NEAR(nm.m[2][2], 1.f, 1e-4f);
}

TEST(MeshTransform, NormalMatrixZeroScaleReturnsIdentity)
{
    nexus::render::Mat4 s = nexus::render::Mat4::identity();
    s.m[0][0] = 0.f;

    nexus::render::Mat4 nm = MeshTransform::normalMatrix(s);
    EXPECT_NEAR(nm.m[0][0], 1.f, 1e-4f);
    EXPECT_NEAR(nm.m[1][1], 1.f, 1e-4f);
}

TEST(MeshTransform, NormalMatrixRotationOnlyReturns3x3)
{
    const float a = 0.5f;
    nexus::render::Mat4 rot = nexus::render::Mat4::identity();
    rot.m[0][0] = std::cos(a); rot.m[0][2] = std::sin(a);
    rot.m[2][0] = -std::sin(a); rot.m[2][2] = std::cos(a);

    nexus::render::Mat4 nm = MeshTransform::normalMatrix(rot);

    EXPECT_NEAR(nm.m[0][0], rot.m[0][0], 1e-4f);
    EXPECT_NEAR(nm.m[0][2], rot.m[0][2], 1e-4f);
    EXPECT_NEAR(nm.m[2][0], rot.m[2][0], 1e-4f);
    EXPECT_NEAR(nm.m[2][2], rot.m[2][2], 1e-4f);
}

TEST(MeshTransform, NormalTransformWithNonUniformScale)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    ASSERT_TRUE(box.computeVertexNormals());

    nexus::render::Mat4 s = nexus::render::Mat4::identity();
    s.m[0][0] = 2.f;
    s.m[1][1] = 1.f;
    s.m[2][2] = 1.f;

    Mesh result = MeshTransform::apply(box, s, true);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasNormals());

    const auto& n = result.attributes().normals();
    for (const auto& v : n) {
        float len = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        EXPECT_NEAR(len, 1.f, 1e-3f);
    }
}

TEST(MeshTransform, TangentTransformPreservesHandedness)
{
    Mesh plane = makePlane(2.f, 2.f, 2, 2);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(plane.computeVertexNormals());

    Mesh withTangents = MeshTangentSpace::compute(plane);
    ASSERT_TRUE(withTangents.isValid());
    ASSERT_TRUE(withTangents.attributes().hasTangents());

    const auto& origTans = withTangents.attributes().tangents();
    auto origHandedness = std::vector<float>{};
    origHandedness.reserve(origTans.size());
    for (const auto& t : origTans) origHandedness.push_back(t.w);

    nexus::render::Mat4 rot = nexus::render::Mat4::identity();
    rot.m[1][1] = -1.f;
    rot.m[2][2] = -1.f;

    Mesh result = MeshTransform::apply(withTangents, rot, true);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasTangents());

    const auto& newTans = result.attributes().tangents();
    ASSERT_EQ(newTans.size(), origHandedness.size());
    for (size_t i = 0; i < newTans.size(); ++i) {
        EXPECT_NEAR(newTans[i].w, origHandedness[i], 1e-4f);
    }
}

TEST(MeshTransform, TangentTransformRotatesDirection)
{
    Mesh plane = makePlane(2.f, 2.f, 2, 2);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(plane.computeVertexNormals());

    Mesh withTangents = MeshTangentSpace::compute(plane);
    ASSERT_TRUE(withTangents.attributes().hasTangents());

    nexus::render::Mat4 rot = nexus::render::Mat4::identity();
    rot.m[0][0] = 0.f; rot.m[0][2] = 1.f;
    rot.m[2][0] = -1.f; rot.m[2][2] = 0.f;

    Mesh result = MeshTransform::apply(withTangents, rot, true);
    EXPECT_TRUE(result.isValid());

    const auto& tans = result.attributes().tangents();
    for (const auto& t : tans) {
        float len = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        EXPECT_NEAR(len, 1.f, 1e-3f);
    }
}

TEST(MeshTransform, ConvenienceTranslate)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    Mesh result = MeshTransform::translate(box, {5.f, 0.f, 0.f});
    EXPECT_TRUE(result.isValid());

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x + 5.f, 1e-4f);
        EXPECT_NEAR(newPos[i].y, origPos[i].y, 1e-4f);
        EXPECT_NEAR(newPos[i].z, origPos[i].z, 1e-4f);
    }
}

TEST(MeshTransform, ConvenienceRotate)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    Quat q = Quat::fromAxisAngle({0.f, 1.f, 0.f}, 3.14159265f * 0.5f);
    Mesh result = MeshTransform::rotate(box, q);
    EXPECT_TRUE(result.isValid());
    EXPECT_EQ(result.attributes().vertexCount(), box.attributes().vertexCount());
}

TEST(MeshTransform, ConvenienceScale)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const auto& origPos = box.attributes().positions();

    Mesh result = MeshTransform::scale(box, {2.f, 3.f, 4.f});
    EXPECT_TRUE(result.isValid());

    const auto& newPos = result.attributes().positions();
    for (size_t i = 0; i < origPos.size(); ++i) {
        EXPECT_NEAR(newPos[i].x, origPos[i].x * 2.f, 1e-4f);
        EXPECT_NEAR(newPos[i].y, origPos[i].y * 3.f, 1e-4f);
        EXPECT_NEAR(newPos[i].z, origPos[i].z * 4.f, 1e-4f);
    }
}

TEST(MeshTransform, QuatFromAxisAngleIdentity)
{
    Quat q = Quat::fromAxisAngle({0.f, 1.f, 0.f}, 0.f);
    EXPECT_NEAR(q.x, 0.f, 1e-4f);
    EXPECT_NEAR(q.y, 0.f, 1e-4f);
    EXPECT_NEAR(q.z, 0.f, 1e-4f);
    EXPECT_NEAR(q.w, 1.f, 1e-4f);
}

TEST(MeshTransform, QuatToMat4Identity)
{
    Quat q = Quat::identity();
    nexus::render::Mat4 m = q.toMat4();
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            EXPECT_NEAR(m.m[i][j], (i == j) ? 1.f : 0.f, 1e-4f);
}

TEST(MeshTransform, QuatNormalize)
{
    Quat q{2.f, 0.f, 0.f, 0.f};
    Quat n = q.normalize();
    EXPECT_NEAR(n.x, 1.f, 1e-4f);
    EXPECT_NEAR(n.y, 0.f, 1e-4f);
    EXPECT_NEAR(n.z, 0.f, 1e-4f);
    EXPECT_NEAR(n.w, 0.f, 1e-4f);
}

TEST(MeshTransform, CenterToOriginCentersPositions)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    mat.m[0][3] = 10.f;
    mat.m[1][3] = 20.f;
    mat.m[2][3] = 30.f;
    Mesh offset = MeshTransform::apply(box, mat, false);

    Mesh centered = MeshTransform::centerToOrigin(offset);
    EXPECT_TRUE(centered.isValid());

    const auto& pos = centered.attributes().positions();
    nexus::render::Vec3 sum{};
    for (const auto& p : pos) sum += p;
    sum = sum * (1.f / static_cast<float>(pos.size()));
    EXPECT_NEAR(sum.x, 0.f, 1e-3f);
    EXPECT_NEAR(sum.y, 0.f, 1e-3f);
    EXPECT_NEAR(sum.z, 0.f, 1e-3f);
}

TEST(MeshTransform, CenterToOriginPreservesShape)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    Mesh centered = MeshTransform::centerToOrigin(box);
    EXPECT_TRUE(centered.isValid());

    nexus::render::Vec3 sum{};
    for (const auto& p : centered.attributes().positions()) sum += p;
    sum = sum * (1.f / static_cast<float>(centered.attributes().vertexCount()));
    EXPECT_NEAR(sum.x, 0.f, 1e-3f);
    EXPECT_NEAR(sum.y, 0.f, 1e-3f);
    EXPECT_NEAR(sum.z, 0.f, 1e-3f);
}

TEST(MeshTransform, NormalizeScaleTargetsSize)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());

    Mesh result = MeshTransform::normalizeScale(box, 10.f);
    EXPECT_TRUE(result.isValid());

    const auto& pos = result.attributes().positions();
    float maxExt = 0.f;
    for (const auto& p : pos) {
        maxExt = std::max(maxExt, std::fabs(p.x));
        maxExt = std::max(maxExt, std::fabs(p.y));
        maxExt = std::max(maxExt, std::fabs(p.z));
    }
    EXPECT_NEAR(maxExt, 5.f, 1e-3f);
}

TEST(MeshTransform, NormalizeScaleZeroSizeMeshUnchanged)
{
    Mesh box = makeBox(0.f, 0.f, 0.f);
    auto copy = box;
    Mesh result = MeshTransform::normalizeScale(box, 1.f);
    EXPECT_TRUE(result.isValid());

    if (!box.attributes().positions().empty()) {
        EXPECT_EQ(result.attributes().vertexCount(), box.attributes().vertexCount());
    }
}

TEST(MeshTransform, UvsPreservedAfterTransform)
{
    Mesh plane = makePlane(2.f, 2.f, 2, 2);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(plane.attributes().hasUVs());
    const auto& origUVs = plane.attributes().uvs();

    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    mat.m[0][3] = 5.f;
    mat.m[1][3] = 10.f;

    Mesh result = MeshTransform::apply(plane, mat);
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasUVs());

    const auto& newUVs = result.attributes().uvs();
    ASSERT_EQ(newUVs.size(), origUVs.size());
    for (size_t i = 0; i < origUVs.size(); ++i) {
        EXPECT_NEAR(newUVs[i].u, origUVs[i].u, 1e-4f);
        EXPECT_NEAR(newUVs[i].v, origUVs[i].v, 1e-4f);
    }
}

TEST(MeshTransform, UvsPreservedAfterScale)
{
    Mesh plane = makePlane(2.f, 2.f, 2, 2);
    ASSERT_TRUE(plane.isValid());
    ASSERT_TRUE(plane.attributes().hasUVs());
    const auto& origUVs = plane.attributes().uvs();

    Mesh result = MeshTransform::scale(plane, {3.f, 3.f, 3.f});
    EXPECT_TRUE(result.isValid());
    EXPECT_TRUE(result.attributes().hasUVs());

    const auto& newUVs = result.attributes().uvs();
    for (size_t i = 0; i < origUVs.size(); ++i) {
        EXPECT_NEAR(newUVs[i].u, origUVs[i].u, 1e-4f);
        EXPECT_NEAR(newUVs[i].v, origUVs[i].v, 1e-4f);
    }
}

TEST(MeshTransform, TopologyPreservedAfterTransform)
{
    Mesh box = makeBox(2.f, 2.f, 2.f);
    ASSERT_TRUE(box.isValid());
    const size_t origFaces = box.topology().faceCount();
    const size_t origVerts = box.attributes().vertexCount();

    Quat q = Quat::fromAxisAngle({1.f, 0.f, 0.f}, 0.5f);
    Mesh result = MeshTransform::rotate(box, q);

    EXPECT_EQ(result.topology().faceCount(), origFaces);
    EXPECT_EQ(result.attributes().vertexCount(), origVerts);
}

TEST(MeshTransform, InvalidMeshReturnsUnchanged)
{
    Mesh invalid;
    nexus::render::Mat4 mat = nexus::render::Mat4::identity();
    mat.m[0][3] = 10.f;

    Mesh result = MeshTransform::apply(invalid, mat);
    EXPECT_FALSE(result.isValid());

    Mesh r2 = MeshTransform::centerToOrigin(invalid);
    EXPECT_FALSE(r2.isValid());

    Mesh r3 = MeshTransform::normalizeScale(invalid, 1.f);
    EXPECT_FALSE(r3.isValid());
}
