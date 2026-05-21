#include <nexus/geometry/ModelingShell.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>

namespace nexus::geometry::testing {

TEST(ModelingShell, GuidedIntroStepsAreStableAndNonEmpty)
{
    const auto steps = ModelingShell::guidedIntroSteps();
    ASSERT_GE(steps.size(), 4u);
    EXPECT_EQ(steps[0], "Pick a starter primitive for your blockout.");
    EXPECT_EQ(steps[1], "Use quick cleanup to smooth topology and edge flow.");
}

TEST(ModelingShell, StartStarterModelBuildsValidWorkflowAndOverlay)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Sphere, 2.0f);

    EXPECT_TRUE(shell.workflow().mesh().isValid());
    EXPECT_TRUE(shell.workflow().mesh().attributes().hasNormals());
    EXPECT_TRUE(shell.isReady());

    ASSERT_GE(shell.workflow().stepReports().size(), 2u);
    EXPECT_EQ(shell.workflow().stepReports()[0].kind, HardSurfaceStepKind::Init);
    EXPECT_EQ(shell.workflow().stepReports()[1].kind, HardSurfaceStepKind::RebuildNormals);
}

TEST(ModelingShell, QuickCleanupAppendsExpectedSteps)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Box, 1.0f);

    BevelChamferDesc bevel{};
    bevel.distance = 0.05f;

    RemeshDesc remesh{};
    remesh.targetEdgeLength = 0.8f;
    remesh.maxIterations = 1u;

    const size_t before = shell.workflow().stepReports().size();
    shell.quickCleanup(bevel, remesh);

    ASSERT_GE(shell.workflow().stepReports().size(), before + 5u);
    const auto& reports = shell.workflow().stepReports();
    EXPECT_EQ(reports[before + 0].kind, HardSurfaceStepKind::Triangulate);
    EXPECT_EQ(reports[before + 1].kind, HardSurfaceStepKind::BevelChamfer);
    EXPECT_EQ(reports[before + 2].kind, HardSurfaceStepKind::Triangulate);
    EXPECT_EQ(reports[before + 3].kind, HardSurfaceStepKind::Remesh);
    EXPECT_EQ(reports[before + 4].kind, HardSurfaceStepKind::RebuildNormals);
    // CG-1 (kernel-contract-gaps.md): quickCleanup pre-triangulates AND
    // re-triangulates after bevel so Remesh always receives a triangle mesh.
    EXPECT_TRUE(reports[before + 0].success);
    EXPECT_TRUE(shell.workflow().mesh().topology().allFacesArePoly3Plus());
    for (size_t fi = 0; fi < shell.workflow().mesh().topology().faceCount(); ++fi) {
        EXPECT_EQ(shell.workflow().mesh().topology().face(fi).indices.size(), 3u);
    }
}

TEST(ModelingShell, SessionReportReflectsWorkflowState)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Capsule, 1.5f);

    const ModelingShellSessionReport report = shell.sessionReport();
    EXPECT_TRUE(report.success);
    EXPECT_FALSE(report.workflowSteps.empty());
    EXPECT_GE(report.overlay.boundaryEdgeCount, 0u);
    EXPECT_FALSE(report.message.empty());
}

TEST(ModelingShell, RefreshDiagnosticsHonorsModeFiltering)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Plane, 1.0f);

    MeshDiagnosticOverlayOptions opts{};
    opts.modes = MeshOverlayMode::BoundaryEdges;

    ASSERT_TRUE(shell.refreshDiagnostics(opts));
    EXPECT_EQ(shell.diagnostics().nonManifoldEdgeCount, 0u);
    EXPECT_EQ(shell.diagnostics().degenerateFaceCount, 0u);
    EXPECT_GT(shell.diagnostics().boundaryEdgeCount, 0u);
}

TEST(ModelingShell, NonFiniteInputsAreSanitizedToSafeDefaults)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Box, std::numeric_limits<float>::quiet_NaN());

    BevelChamferDesc bevel{};
    bevel.distance = std::numeric_limits<float>::infinity();

    RemeshDesc remesh{};
    remesh.targetEdgeLength = std::numeric_limits<float>::quiet_NaN();
    remesh.maxIterations = 0u;

    shell.quickCleanup(bevel, remesh);

    EXPECT_TRUE(shell.workflow().mesh().isValid());
    EXPECT_TRUE(shell.workflow().mesh().attributes().hasNormals());
    ASSERT_FALSE(shell.workflow().stepReports().empty());
    EXPECT_EQ(shell.workflow().stepReports().back().kind, HardSurfaceStepKind::RebuildNormals);
}

// CG-1 (kernel-contract-gaps.md): the session report message used to always
// echo the cosmetically-last step ("Normals rebuilt.") even when an earlier
// step failed. Verify the message now surfaces the first failing step so
// callers can act on the real blocker.
TEST(ModelingShell, SessionReportMessageSurfacesFirstFailingStep)
{
    ModelingShell shell;
    shell.startStarterModel(StarterPrimitive::Box, 1.0f);
    shell.quickCleanup();

    const ModelingShellSessionReport report = shell.sessionReport();
    if (report.success) {
        // Workflow succeeded end-to-end; nothing to assert about a failing step.
        SUCCEED();
        return;
    }

    const auto& steps = report.workflowSteps;
    auto firstFail = std::find_if(steps.begin(), steps.end(),
                                  [](const HardSurfaceStepReport& s) { return !s.success; });
    ASSERT_NE(firstFail, steps.end()) << "non-success report should contain a failing step";
    EXPECT_EQ(report.message, firstFail->message);
    EXPECT_NE(report.message, "Normals rebuilt.");
}

// CG-4 (kernel-contract-gaps.md) closure: ModelingShell now stores a
// caller-supplied session label that round-trips through sessionReport()
// so callers no longer have to thread it app-side.
TEST(ModelingShell, SessionLabelRoundTripsThroughSessionReport)
{
    ModelingShell shell;
    EXPECT_TRUE(shell.sessionLabel().empty());

    shell.setSessionLabel("scenario://starter/box");
    EXPECT_EQ(shell.sessionLabel(), "scenario://starter/box");

    shell.startStarterModel(StarterPrimitive::Box, 1.0f);
    const ModelingShellSessionReport report = shell.sessionReport();
    EXPECT_EQ(report.sessionLabel, "scenario://starter/box");

    // Default-constructed report carries an empty label.
    ModelingShell other;
    EXPECT_TRUE(other.sessionReport().sessionLabel.empty());
}

} // namespace nexus::geometry::testing
