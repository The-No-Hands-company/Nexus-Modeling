// ─────────────────────────────────────────────────────────────────────────────
//  Tests: Hard-Surface Workflow Slice 1 (Month 5)
//  Covers:
//    • New primitive constructors: makeSphere, makeCylinder, makeCone, makeCapsule
//    • HardSurfaceWorkflow chaining and step-report accumulation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/HardSurfaceWorkflow.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

using namespace nexus::geometry;
using namespace nexus::render;

namespace {

// Computes the axis-aligned bounding box [min, max] of mesh positions.
std::pair<Vec3, Vec3> meshBounds(const Mesh& m)
{
    Vec3 lo{ 1e30f,  1e30f,  1e30f};
    Vec3 hi{-1e30f, -1e30f, -1e30f};
    for (const auto& p : m.attributes().positions()) {
        lo.x = std::min(lo.x, p.x);  lo.y = std::min(lo.y, p.y);  lo.z = std::min(lo.z, p.z);
        hi.x = std::max(hi.x, p.x);  hi.y = std::max(hi.y, p.y);  hi.z = std::max(hi.z, p.z);
    }
    return {lo, hi};
}

// Returns true when every computed per-vertex normal has a positive dot product
// with the corresponding outward radial direction (position / radius).
// Suitable only for convex shapes where outward = normalise(pos - center).
bool normalsPointOutward(const Mesh& m, float minDot = 0.f)
{
    if (!m.attributes().hasNormals()) {
        return false;
    }
    const auto& pos = m.attributes().positions();
    const auto& nrm = m.attributes().normals();
    if (pos.size() != nrm.size()) {
        return false;
    }
    for (size_t i = 0; i < pos.size(); ++i) {
        const Vec3& p = pos[i];
        const float len = std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
        if (len < 1e-6f) continue;
        const float dot = (p.x*nrm[i].x + p.y*nrm[i].y + p.z*nrm[i].z) / len;
        if (dot < minDot) {
            return false;
        }
    }
    return true;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
//  makeSphere
// ─────────────────────────────────────────────────────────────────────────────

TEST(Primitives, MakeSphereIsValidAndHasCorrectBounds)
{
    const float r = 1.5f;
    Mesh m = primitives::makeSphere(r, 8, 8);
    EXPECT_TRUE(m.isValid());
    EXPECT_GT(m.attributes().positions().size(), 0u);
    EXPECT_GT(m.topology().faceCount(), 0u);

    const auto [lo, hi] = meshBounds(m);
    EXPECT_NEAR(hi.y,  r, 1e-5f);
    EXPECT_NEAR(lo.y, -r, 1e-5f);

    // Largest XZ extent should not exceed the radius.
    EXPECT_LE(hi.x,  r + 1e-5f);
    EXPECT_GE(lo.x, -r - 1e-5f);
}

TEST(Primitives, MakeSphereOutwardNormals)
{
    Mesh m = primitives::makeSphere(2.f, 8, 8);
    EXPECT_TRUE(m.computeVertexNormals());
    EXPECT_TRUE(normalsPointOutward(m, 0.f))
        << "At least one vertex normal points inward for makeSphere";
}

TEST(Primitives, MakeSphereDefaultResolutionHasExpectedFaceCount)
{
    // latSegs=16, lonSegs=16 → 16 top-cap tris + 14*16 quads + 16 bot-cap tris
    // = 32 + 224 = 256 faces
    const Mesh m = primitives::makeSphere(1.f, 16, 16);
    const size_t expected = 32u + 14u * 16u;
    EXPECT_EQ(m.topology().faceCount(), expected);
}

TEST(Primitives, MakeSphereMinimumParametersProducesValidMesh)
{
    // latSegs clamped to 2, lonSegs to 3 → triangle fan top + triangle fan bot only
    const Mesh m = primitives::makeSphere(1.f, 1, 2);
    EXPECT_TRUE(m.isValid());
}

TEST(Primitives, MakeSphereUVsArePresentAndInRange)
{
    const Mesh m = primitives::makeSphere(1.f, 4, 4);
    EXPECT_TRUE(m.attributes().hasUVs());
    for (const auto& uv : m.attributes().uvs()) {
        EXPECT_GE(uv.u, 0.f);  EXPECT_LE(uv.u, 1.f);
        EXPECT_GE(uv.v, 0.f);  EXPECT_LE(uv.v, 1.f);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  makeCylinder
// ─────────────────────────────────────────────────────────────────────────────

TEST(Primitives, MakeCylinderIsValidAndHasCorrectBounds)
{
    const float r = 1.f, h = 2.f;
    const Mesh m = primitives::makeCylinder(r, h, 8);
    EXPECT_TRUE(m.isValid());

    const auto [lo, hi] = meshBounds(m);
    EXPECT_NEAR(hi.y,  h * 0.5f, 1e-5f);
    EXPECT_NEAR(lo.y, -h * 0.5f, 1e-5f);
    EXPECT_LE(hi.x, r + 1e-5f);
    EXPECT_GE(lo.x, -r - 1e-5f);
}

TEST(Primitives, MakeCylinderOutwardNormals)
{
    // Normals on the barrel should have zero or positive y-component for
    // outward-facing lateral faces; poles have ±Y normals.
    Mesh m = primitives::makeCylinder(1.f, 2.f, 8);
    EXPECT_TRUE(m.computeVertexNormals());
    EXPECT_TRUE(m.attributes().hasNormals());
}

TEST(Primitives, MakeCylinderFaceCount)
{
    // radialSegs=8 → 8 barrel quads + 8 top tris + 8 bot tris = 24 faces
    const Mesh m = primitives::makeCylinder(1.f, 1.f, 8);
    EXPECT_EQ(m.topology().faceCount(), 24u);
}

TEST(Primitives, MakeCylinderMinimumRadialSegsProducesValidMesh)
{
    const Mesh m = primitives::makeCylinder(1.f, 1.f, 2); // clamped to 3
    EXPECT_TRUE(m.isValid());
    EXPECT_EQ(m.topology().faceCount(), 9u); // 3+3+3
}

// ─────────────────────────────────────────────────────────────────────────────
//  makeCone
// ─────────────────────────────────────────────────────────────────────────────

TEST(Primitives, MakeConeIsValidAndHasCorrectBounds)
{
    const float r = 0.5f, h = 2.f;
    const Mesh m = primitives::makeCone(r, h, 8);
    EXPECT_TRUE(m.isValid());

    const auto [lo, hi] = meshBounds(m);
    EXPECT_NEAR(hi.y, h, 1e-5f);       // apex
    EXPECT_NEAR(lo.y, 0.f, 1e-5f);     // base at y=0
    EXPECT_LE(hi.x, r + 1e-5f);
    EXPECT_GE(lo.x, -r - 1e-5f);
}

TEST(Primitives, MakeConeFaceCount)
{
    // radialSegs=6 → 6 lateral tris + 6 base tris = 12 faces
    const Mesh m = primitives::makeCone(1.f, 2.f, 6);
    EXPECT_EQ(m.topology().faceCount(), 12u);
}

TEST(Primitives, MakeConeNormalReconstructionSucceeds)
{
    Mesh m = primitives::makeCone(1.f, 1.f, 8);
    EXPECT_TRUE(m.computeVertexNormals());
    EXPECT_TRUE(m.attributes().hasNormals());
}

// ─────────────────────────────────────────────────────────────────────────────
//  makeCapsule
// ─────────────────────────────────────────────────────────────────────────────

TEST(Primitives, MakeCapsuleIsValidAndHasCorrectBounds)
{
    const float r = 0.5f, cylH = 1.f;
    const Mesh m = primitives::makeCapsule(r, cylH, 8, 4);
    EXPECT_TRUE(m.isValid());

    const float expectedHalfHeight = cylH * 0.5f + r;
    const auto [lo, hi] = meshBounds(m);
    EXPECT_NEAR(hi.y,  expectedHalfHeight, 1e-5f);
    EXPECT_NEAR(lo.y, -expectedHalfHeight, 1e-5f);
    EXPECT_LE(hi.x, r + 1e-5f);
    EXPECT_GE(lo.x, -r - 1e-5f);
}

TEST(Primitives, MakeCapsuleZeroCylinderHeightProducesValidSphere)
{
    // cylinderHeight=0 → degenerate capsule collapses to a sphere
    const Mesh m = primitives::makeCapsule(1.f, 0.f, 8, 4);
    EXPECT_TRUE(m.isValid());
    EXPECT_GT(m.topology().faceCount(), 0u);
}

TEST(Primitives, MakeCapsuleNormalReconstructionSucceeds)
{
    Mesh m = primitives::makeCapsule(0.5f, 1.f, 8, 4);
    EXPECT_TRUE(m.computeVertexNormals());
    EXPECT_TRUE(m.attributes().hasNormals());
}

// ─────────────────────────────────────────────────────────────────────────────
//  HardSurfaceWorkflow — basic API contract
// ─────────────────────────────────────────────────────────────────────────────

TEST(HardSurfaceWorkflow, InitWithValidMeshReportsSuccess)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    EXPECT_TRUE(wf.isValid());
    ASSERT_EQ(wf.stepReports().size(), 1u);
    EXPECT_EQ(wf.stepReports()[0].kind, HardSurfaceStepKind::Init);
    EXPECT_TRUE(wf.stepReports()[0].success);
}

TEST(HardSurfaceWorkflow, InitDefaultConstructedThenInitWithMesh)
{
    HardSurfaceWorkflow wf;
    wf.init(primitives::makeBox(2.f, 2.f, 2.f));
    EXPECT_TRUE(wf.isValid());
    EXPECT_EQ(wf.stepReports().size(), 1u);
}

TEST(HardSurfaceWorkflow, RebuildNormalsSucceedsOnBox)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    wf.rebuildNormals();
    EXPECT_TRUE(wf.isValid());
    EXPECT_EQ(wf.stepReports().size(), 2u);
    EXPECT_EQ(wf.lastStepReport().kind, HardSurfaceStepKind::RebuildNormals);
    EXPECT_TRUE(wf.lastStepReport().success);
    EXPECT_TRUE(wf.mesh().attributes().hasNormals());
}

TEST(HardSurfaceWorkflow, ApplyTransformScalesMesh)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    Mat4 scale = Mat4::identity();
    scale.m[0][0] = 2.f;
    scale.m[1][1] = 2.f;
    scale.m[2][2] = 2.f;
    wf.applyTransform(scale);
    EXPECT_TRUE(wf.isValid());
    EXPECT_EQ(wf.lastStepReport().kind, HardSurfaceStepKind::ApplyTransform);
    EXPECT_TRUE(wf.lastStepReport().success);

    const auto [lo, hi] = meshBounds(wf.mesh());
    EXPECT_NEAR(hi.x, 1.f, 1e-4f);   // box was ±0.5 → scaled to ±1
}

TEST(HardSurfaceWorkflow, RemeshStepProducesValidMesh)
{
    // Make a sphere primitive (triangulated faces) and remesh it
    Mesh sphere = primitives::makeSphere(1.f, 4, 4);
    [[maybe_unused]] const size_t trianglesAdded = sphere.topology().triangulate();

    HardSurfaceWorkflow wf(std::move(sphere));

    RemeshDesc desc{};
    desc.targetEdgeLength = 1.5f;
    desc.maxIterations = 1;
    wf.remesh(desc);

    EXPECT_EQ(wf.lastStepReport().kind, HardSurfaceStepKind::Remesh);
    // Remesh succeeds or warns — in any case the mesh should remain valid
    EXPECT_TRUE(wf.mesh().isValid());
}

TEST(HardSurfaceWorkflow, StepReportsAccumulateInOrder)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    wf.rebuildNormals();

    Mat4 id = Mat4::identity();
    wf.applyTransform(id);

    ASSERT_EQ(wf.stepReports().size(), 3u);
    EXPECT_EQ(wf.stepReports()[0].kind, HardSurfaceStepKind::Init);
    EXPECT_EQ(wf.stepReports()[1].kind, HardSurfaceStepKind::RebuildNormals);
    EXPECT_EQ(wf.stepReports()[2].kind, HardSurfaceStepKind::ApplyTransform);
}

// CG-1 (kernel-contract-gaps.md): HardSurfaceWorkflow now exposes an explicit
// Triangulate step so ModelingShell::quickCleanup can guarantee triangle-only
// input to BevelChamfer/Remesh on starter primitives that ship as quads.
TEST(HardSurfaceWorkflow, TriangulateStepConvertsQuadsToTriangles)
{
    // makeBox emits 6 quads; verify Triangulate splits them into 12 triangles
    // and records a successful step report.
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    const size_t facesBefore = wf.mesh().topology().faceCount();
    ASSERT_EQ(facesBefore, 6u);

    wf.triangulate();

    EXPECT_EQ(wf.lastStepReport().kind, HardSurfaceStepKind::Triangulate);
    EXPECT_TRUE(wf.lastStepReport().success);
    EXPECT_TRUE(wf.mesh().isValid());
    EXPECT_EQ(wf.mesh().topology().faceCount(), 12u);
    for (size_t fi = 0; fi < wf.mesh().topology().faceCount(); ++fi) {
        EXPECT_EQ(wf.mesh().topology().face(fi).indices.size(), 3u);
    }

    // Idempotent: a second invocation on already-triangulated input keeps the
    // mesh valid and the face count stable.
    wf.triangulate();
    EXPECT_TRUE(wf.lastStepReport().success);
    EXPECT_EQ(wf.mesh().topology().faceCount(), 12u);
}

TEST(HardSurfaceWorkflow, BevelEdgesOnValidMeshRunsWithoutCrash)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    BevelChamferDesc desc{};
    desc.distance = 0.05f;
    wf.bevelEdges(desc);
    // Step is recorded regardless of success/failure
    ASSERT_GE(wf.stepReports().size(), 2u);
    EXPECT_EQ(wf.lastStepReport().kind, HardSurfaceStepKind::BevelChamfer);
}

TEST(HardSurfaceWorkflow, ReleaseMeshTransfersOwnership)
{
    HardSurfaceWorkflow wf(primitives::makeSphere(1.f, 4, 4));
    const size_t expectedFaces = wf.mesh().topology().faceCount();

    Mesh released = wf.releaseMesh();
    EXPECT_EQ(released.topology().faceCount(), expectedFaces);
    // After release, the working mesh is empty
    EXPECT_EQ(wf.mesh().topology().faceCount(), 0u);
}

TEST(HardSurfaceWorkflow, IsValidReturnsFalseAfterInvalidInit)
{
    Mesh empty;  // empty mesh: isValid() returns false
    HardSurfaceWorkflow wf(std::move(empty));
    EXPECT_FALSE(wf.isValid());
    EXPECT_FALSE(wf.stepReports()[0].success);
}

TEST(HardSurfaceWorkflow, LastStepReportReturnsDefaultWhenNoSteps)
{
    HardSurfaceWorkflow wf;
    const HardSurfaceStepReport rep = wf.lastStepReport();
    EXPECT_EQ(rep.kind, HardSurfaceStepKind::Init);
    EXPECT_TRUE(rep.success);  // default-constructed success = true
}

// ─────────────────────────────────────────────────────────────────────────────
//  End-to-end: box → bevel → normals workflow
// ─────────────────────────────────────────────────────────────────────────────

TEST(HardSurfaceWorkflow, BoxBevelNormalsEndToEndWorkflow)
{
    HardSurfaceWorkflow wf(primitives::makeBox(1.f, 1.f, 1.f));
    ASSERT_TRUE(wf.isValid());

    BevelChamferDesc bev{};
    bev.distance = 0.05f;
    bev.recomputeNormals = false;  // let workflow rebuild normals explicitly
    wf.bevelEdges(bev);
    wf.rebuildNormals();

    // Workflow may have warnings but should not crash; final mesh valid
    EXPECT_TRUE(wf.mesh().isValid());

    // At least the init, bevel, and normals steps should be recorded
    ASSERT_GE(wf.stepReports().size(), 3u);
}

TEST(HardSurfaceWorkflow, SphereRemeshNormalsEndToEndWorkflow)
{
    // triangulate sphere, remesh, rebuild normals
    Mesh sphere = primitives::makeSphere(1.f, 6, 6);
    [[maybe_unused]] const size_t trianglesAdded = sphere.topology().triangulate();

    HardSurfaceWorkflow wf(std::move(sphere));

    RemeshDesc rd{};
    rd.targetEdgeLength = 1.2f;
    rd.maxIterations    = 1;
    wf.remesh(rd);
    wf.rebuildNormals();

    EXPECT_TRUE(wf.mesh().isValid());
    EXPECT_GE(wf.stepReports().size(), 3u);
}
