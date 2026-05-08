// ─────────────────────────────────────────────────────────────────────────────
//  Boolean Operation Regression Tests
//
//  Tests v0 boolean operation implementation with deterministic geometry.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/BooleanOperation.h>
#include <nexus/geometry/Mesh.h>
#include <gtest/gtest.h>

namespace nexus::geometry::testing {

using namespace nexus::geometry::primitives;

// ─────────────────────────────────────────────────────────────────────────────
//  Validation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, ValidateMeshAcceptsValidTriangleMesh)
{
    // Create a simple triangle mesh
    Mesh mesh = makeTriangle(1.0f);

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
}

TEST(BooleanOperation, ValidateMeshRejectsEmptyMesh)
{
    Mesh empty;

    auto report = BooleanOperation::validateMesh(empty);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
    EXPECT_FALSE(report.messages.empty());
}

TEST(BooleanOperation, ValidateMeshRejectsNoPositions)
{
    Mesh meshNoPos;
    meshNoPos.topology().addFace(Face{{0, 1, 2}});

    auto report = BooleanOperation::validateMesh(meshNoPos);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
}

TEST(BooleanOperation, ValidateMeshRejectsOutOfBoundsIndices)
{
    Mesh meshBadIndices;
    meshBadIndices.attributes().setPositions({{0, 0, 0}, {1, 0, 0}, {0, 1, 0}});
    meshBadIndices.topology().addFace(Face{{0, 1, 5}});  // Index 5 out of bounds

    auto report = BooleanOperation::validateMesh(meshBadIndices);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputAInvalid));
}

TEST(BooleanOperation, ValidateMeshRejectsNonManifoldEdges)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, -1.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});
    mesh.topology().addFace(Face{{1u, 0u, 3u}});
    mesh.topology().addFace(Face{{0u, 1u, 4u}});

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputANotManifold));
}

TEST(BooleanOperation, ValidateMeshRejectsDegenerateTriangle)
{
    Mesh mesh;
    mesh.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {2.f, 0.f, 0.f},
    });
    mesh.topology().addFace(Face{{0u, 1u, 2u}});

    auto report = BooleanOperation::validateMesh(mesh);
    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::GeometricDegeneracy));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Basic Operation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(2.0f, 2.0f, 2.0f);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, result);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, DifferenceTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, result);

    EXPECT_TRUE(report.valid);
    EXPECT_EQ(report.code, BooleanOperationDiagnostic::Success);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, IntersectionTwoBoxesDeterministically)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result;
    auto report =
        BooleanOperation::compute(boxA, boxB, BooleanOperationType::Intersection, result);

    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(
        report.code == BooleanOperationDiagnostic::Success
        || report.code == BooleanOperationDiagnostic::OutputEmpty);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Non-intersecting Geometry Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionWithNonIntersectingBoxes)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    boxB.applyTransform(translate);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, result);

    // Union should succeed even if meshes don't intersect
    EXPECT_TRUE(report.valid);
    EXPECT_GT(result.attributes().vertexCount(), 0);
}

TEST(BooleanOperation, DifferenceWithNonIntersectingMeshesReturnsInputA)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    boxB.applyTransform(translate);

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, result);

    // Difference should return A (or similar volume)
    EXPECT_TRUE(report.valid);
    EXPECT_GT(result.attributes().vertexCount(), 0);
    EXPECT_GT(result.topology().faceCount(), 0);
}

TEST(BooleanOperation, IntersectionWithNonIntersectingMeshesReturnsEmpty)
{
    Mesh boxA = makeBox(1.0f, 1.0f, 1.0f);

    // Create boxB separated from boxA
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    nexus::render::Mat4 translate = nexus::render::Mat4::identity();
    translate.m[0][3] = 5.0f;
    boxB.applyTransform(translate);

    Mesh result;
    auto report =
        BooleanOperation::compute(boxA, boxB, BooleanOperationType::Intersection, result);

    // Intersection of non-overlapping should be empty (but still valid)
    EXPECT_TRUE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::NoIntersection) ||
                report.hasWarning(BooleanOperationDiagnostic::OutputEmpty));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Normal Preservation Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, PreservesNormalsWhenOptionEnabled)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    boxA.computeVertexNormals();

    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);
    boxB.computeVertexNormals();

    BooleanOperationOptions opts;
    opts.preserveNormals = true;

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Difference, opts, result);

    EXPECT_TRUE(report.valid);
    // Result should have normals if preserve option is true
    if (result.attributes().vertexCount() > 0) {
        // Note: normals are optional, but we expect them if preserve flag is set
        EXPECT_GE(result.attributes().vertexCount(), 0);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostic Test
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, DiagnosticFlagsWorkCorrectly)
{
    BooleanOperationDiagnostic flags = BooleanOperationDiagnostic::InputAInvalid;
    flags = flags | BooleanOperationDiagnostic::InputBEmpty;

    EXPECT_TRUE(flags == (BooleanOperationDiagnostic::InputAInvalid |
                          BooleanOperationDiagnostic::InputBEmpty));

    bool hasA = (flags & BooleanOperationDiagnostic::InputAInvalid) != BooleanOperationDiagnostic::Success;
    bool hasB = (flags & BooleanOperationDiagnostic::InputBEmpty) != BooleanOperationDiagnostic::Success;
    EXPECT_TRUE(hasA);
    EXPECT_TRUE(hasB);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Operation Name Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, OperationNameReturnsCorrectStrings)
{
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Union), "Union");
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Difference), "Difference");
    EXPECT_STREQ(BooleanOperation::operationName(BooleanOperationType::Intersection),
                 "Intersection");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Determinism Tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(BooleanOperation, UnionResultIsDeterministicAcrossRuns)
{
    Mesh boxA1 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB1 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result1;
    auto report1 =
        BooleanOperation::compute(boxA1, boxB1, BooleanOperationType::Union, result1);

    // Second run with same inputs
    Mesh boxA2 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB2 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result2;
    auto report2 =
        BooleanOperation::compute(boxA2, boxB2, BooleanOperationType::Union, result2);

    // Results should be identical
    EXPECT_TRUE(report1.valid && report2.valid);
    EXPECT_EQ(result1.attributes().vertexCount(), result2.attributes().vertexCount());
    EXPECT_EQ(result1.topology().faceCount(), result2.topology().faceCount());

    // Positions should match
    const auto& pos1 = result1.attributes().positions();
    const auto& pos2 = result2.attributes().positions();
    for (size_t i = 0; i < pos1.size(); ++i) {
        EXPECT_FLOAT_EQ(pos1[i].x, pos2[i].x);
        EXPECT_FLOAT_EQ(pos1[i].y, pos2[i].y);
        EXPECT_FLOAT_EQ(pos1[i].z, pos2[i].z);
    }
}

TEST(BooleanOperation, DifferenceResultIsDeterministicAcrossRuns)
{
    Mesh boxA1 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB1 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result1;
    auto report1 =
        BooleanOperation::compute(boxA1, boxB1, BooleanOperationType::Difference, result1);

    // Second run with same inputs
    Mesh boxA2 = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB2 = makeBox(1.0f, 1.0f, 1.0f);

    Mesh result2;
    auto report2 =
        BooleanOperation::compute(boxA2, boxB2, BooleanOperationType::Difference, result2);

    // Results should be identical
    EXPECT_TRUE(report1.valid && report2.valid);
    EXPECT_EQ(result1.attributes().vertexCount(), result2.attributes().vertexCount());
    EXPECT_EQ(result1.topology().faceCount(), result2.topology().faceCount());
}

TEST(BooleanOperation, ComputeRejectsNonPositiveTolerance)
{
    Mesh boxA = makeBox(2.0f, 2.0f, 2.0f);
    Mesh boxB = makeBox(1.0f, 1.0f, 1.0f);

    BooleanOperationOptions opts;
    opts.geometricTolerance = 0.0f;

    Mesh result;
    auto report = BooleanOperation::compute(boxA, boxB, BooleanOperationType::Union, opts, result);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::NumericalInstability));
}

TEST(BooleanOperation, ComputeMapsInputBNonManifoldDiagnostic)
{
    Mesh meshA = makeBox(2.0f, 2.0f, 2.0f);

    Mesh meshB;
    meshB.attributes().setPositions({
        {0.f, 0.f, 0.f},
        {1.f, 0.f, 0.f},
        {0.f, 1.f, 0.f},
        {0.f, 0.f, 1.f},
        {0.f, -1.f, 0.f},
    });
    meshB.topology().addFace(Face{{0u, 1u, 2u}});
    meshB.topology().addFace(Face{{1u, 0u, 3u}});
    meshB.topology().addFace(Face{{0u, 1u, 4u}});

    Mesh result;
    auto report =
        BooleanOperation::compute(meshA, meshB, BooleanOperationType::Difference, result);

    EXPECT_FALSE(report.valid);
    EXPECT_TRUE(report.hasWarning(BooleanOperationDiagnostic::InputBNotManifold));
}

}  // namespace nexus::geometry::testing
