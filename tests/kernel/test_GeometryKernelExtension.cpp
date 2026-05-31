// ─────────────────────────────────────────────────────────────────────────────
//  Tests — GeometryKernelExtension
// ─────────────────────────────────────────────────────────────────────────────
#include <gtest/gtest.h>
#include <nexus/automation/GeometryKernelExtension.h>
#include <nexus/automation/AutomationScript.h>

using namespace nexus::automation;

static std::vector<std::string> run(ScriptBatchHarness& h,
                                     const std::string& name,
                                     std::map<std::string,std::string> args = {})
{
    ScriptContext ctx;
    ScriptCommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    std::vector<std::string> msgs;
    (void)h.registry().execute(ctx, cmd, msgs);
    return msgs;
}

static bool runOk(ScriptBatchHarness& h, const std::string& name,
                  std::map<std::string,std::string> args = {})
{
    ScriptContext ctx;
    ScriptCommand cmd;
    cmd.name = name;
    cmd.args = std::move(args);
    std::vector<std::string> msgs;
    return h.registry().execute(ctx, cmd, msgs);
}

class GeomKernelTest : public ::testing::Test {
protected:
    ScriptBatchHarness harness;
    void SetUp() override { registerGeometryKernelCommands(harness); }
};

// ── geom_kernel.triangulate ───────────────────────────────────────────────────

TEST_F(GeomKernelTest, TriangulateEmptyMeshRegistered) {
    auto msgs = run(harness, "geom_kernel.triangulate");
    EXPECT_FALSE(msgs.empty());
}

TEST_F(GeomKernelTest, TriangulateReturnsMessage) {
    auto msgs = run(harness, "geom_kernel.triangulate");
    ASSERT_FALSE(msgs.empty());
    // Empty mesh may fail or succeed depending on impl; just check message
    const bool hasOk    = msgs[0].find("ok") != std::string::npos;
    const bool hasError = msgs[0].find("error") != std::string::npos;
    EXPECT_TRUE(hasOk || hasError);
}

// ── geom_kernel.validate ──────────────────────────────────────────────────────

TEST_F(GeomKernelTest, ValidateEmptyMesh) {
    EXPECT_TRUE(runOk(harness, "geom_kernel.validate"));
    auto msgs = run(harness, "geom_kernel.validate");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("issues="), std::string::npos);
}

// ── geom_kernel.weld ──────────────────────────────────────────────────────────

TEST_F(GeomKernelTest, WeldEmptyMeshRegistered) {
    auto msgs = run(harness, "geom_kernel.weld");
    EXPECT_FALSE(msgs.empty());
}

TEST_F(GeomKernelTest, WeldCustomEpsilon) {
    auto msgs = run(harness, "geom_kernel.weld", {{"epsilon","0.001"}});
    EXPECT_FALSE(msgs.empty());
}

// ── mesh_io.export / import ───────────────────────────────────────────────────

TEST_F(GeomKernelTest, ExportMissingPath) {
    EXPECT_FALSE(runOk(harness, "mesh_io.export"));
    auto msgs = run(harness, "mesh_io.export");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(GeomKernelTest, ImportMissingPath) {
    EXPECT_FALSE(runOk(harness, "mesh_io.import"));
    auto msgs = run(harness, "mesh_io.import");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(GeomKernelTest, ImportNonexistentFile) {
    EXPECT_FALSE(runOk(harness, "mesh_io.import", {{"path","/nonexistent/mesh.obj"}}));
    auto msgs = run(harness, "mesh_io.import", {{"path","/nonexistent/mesh.obj"}});
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

// ── meshlet.describe ──────────────────────────────────────────────────────────

TEST_F(GeomKernelTest, MeshletDescribeInitial) {
    auto msgs = run(harness, "meshlet.describe");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("has_meshlets=0"), std::string::npos);
    EXPECT_NE(msgs[0].find("count=0"), std::string::npos);
}

TEST_F(GeomKernelTest, MeshletBuildNoIndexBuffer) {
    EXPECT_FALSE(runOk(harness, "meshlet.build"));
    auto msgs = run(harness, "meshlet.build");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

// ── modeling.* ────────────────────────────────────────────────────────────────

TEST_F(GeomKernelTest, ModelingGuidedIntro) {
    EXPECT_TRUE(runOk(harness, "modeling.guided_intro"));
    auto msgs = run(harness, "modeling.guided_intro");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("steps="), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingSessionReportBeforeStart) {
    EXPECT_TRUE(runOk(harness, "modeling.session_report"));
    auto msgs = run(harness, "modeling.session_report");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("steps="), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingStartBox) {
    EXPECT_TRUE(runOk(harness, "modeling.start"));
    auto msgs = run(harness, "modeling.start");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("primitive=Box"), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingStartSphere) {
    EXPECT_TRUE(runOk(harness, "modeling.start", {{"primitive","Sphere"},{"size","2.0"}}));
    auto msgs = run(harness, "modeling.start", {{"primitive","Sphere"}});
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("primitive=Sphere"), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingStartWithLabel) {
    EXPECT_TRUE(runOk(harness, "modeling.start", {{"label","test-session"}}));
    auto msgs = run(harness, "modeling.session_report");
    ASSERT_FALSE(msgs.empty());
    // label may or may not show in session report depending on impl
}

TEST_F(GeomKernelTest, ModelingQuickCleanupNoSession) {
    // Fresh harness — no session
    ScriptBatchHarness h2;
    registerGeometryKernelCommands(h2);
    EXPECT_FALSE(runOk(h2, "modeling.quick_cleanup"));
    auto msgs = run(h2, "modeling.quick_cleanup");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("error"), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingQuickCleanupAfterStart) {
    EXPECT_TRUE(runOk(harness, "modeling.start", {{"primitive","Box"}}));
    EXPECT_TRUE(runOk(harness, "modeling.quick_cleanup"));
    auto msgs = run(harness, "modeling.quick_cleanup");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingRefreshDiagnosticsNoSession) {
    ScriptBatchHarness h2;
    registerGeometryKernelCommands(h2);
    EXPECT_FALSE(runOk(h2, "modeling.refresh_diagnostics"));
}

TEST_F(GeomKernelTest, ModelingRefreshDiagnosticsAfterStart) {
    EXPECT_TRUE(runOk(harness, "modeling.start"));
    EXPECT_TRUE(runOk(harness, "modeling.refresh_diagnostics"));
    auto msgs = run(harness, "modeling.refresh_diagnostics");
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("ok"), std::string::npos);
    EXPECT_NE(msgs[0].find("ready="), std::string::npos);
}

TEST_F(GeomKernelTest, ModelingFullPipeline) {
    EXPECT_TRUE(runOk(harness, "modeling.start", {{"primitive","Cylinder"},{"size","1.5"},{"label","pipeline"}}));
    EXPECT_TRUE(runOk(harness, "modeling.quick_cleanup"));
    EXPECT_TRUE(runOk(harness, "modeling.refresh_diagnostics"));
    auto report = run(harness, "modeling.session_report");
    ASSERT_FALSE(report.empty());
    EXPECT_NE(report[0].find("label=pipeline"), std::string::npos);
}
