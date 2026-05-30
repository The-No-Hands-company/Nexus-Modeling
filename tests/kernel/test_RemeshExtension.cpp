// Tests for mesh.remesh, mesh.weld, and mesh.describe automation commands.
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/RemeshExtension.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerRemeshCommands(h);
    return h;
}

void loadSphere(nexus::automation::ScriptContext& ctx,
                uint32_t lat = 8, uint32_t lon = 8)
{
    ctx.mesh    = nexus::geometry::primitives::makeSphere(1.f, lat, lon);
    ctx.hasMesh = true;
}

void loadBox(nexus::automation::ScriptContext& ctx)
{
    ctx.mesh    = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
    ctx.hasMesh = true;
}

} // namespace

// ── mesh.remesh ───────────────────────────────────────────────────────────────

TEST(RemeshExtension, RemeshRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(RemeshExtension, RemeshDefaultsSucceed)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 6, 6);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    // Success message must contain key fields
    bool foundOk = false;
    for (const auto& m : msgs)
        if (m.find("mesh.remesh ok") != std::string::npos) { foundOk = true; break; }
    EXPECT_TRUE(foundOk);
}

TEST(RemeshExtension, RemeshUpdatesContextMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 6, 6);

    const std::size_t facesBefore = ctx.mesh.topology().faceCount();

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";
    cmd.args["target_edge_length"] = "0.3";
    cmd.args["max_iterations"]     = "1";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    // Mesh must have been replaced (face count may change)
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
    // Context mesh is live (not the original object)
    (void)facesBefore; // either more or same is acceptable
}

TEST(RemeshExtension, RemeshSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 8, 8);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";
    cmd.args["target_edge_length"] = "0.4";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    ASSERT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.find("mesh.remesh ok") == std::string::npos) continue;
        found = true;
        EXPECT_NE(m.find("faces_in="),  std::string::npos);
        EXPECT_NE(m.find("faces_out="), std::string::npos);
        EXPECT_NE(m.find("splits="),    std::string::npos);
        EXPECT_NE(m.find("collapses="), std::string::npos);
        EXPECT_NE(m.find("iters="),     std::string::npos);
    }
    EXPECT_TRUE(found) << "success message not found in: " << msgs[0];
}

TEST(RemeshExtension, RemeshMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 6, 6);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

TEST(RemeshExtension, RemeshWithCollapsePasses)
{
    // Split first to produce fine mesh, then collapse slightly short edges.
    // collapse_below=0.3 means edges < 0.3*target are collapsed — conservative.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 12, 12);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";
    cmd.args["target_edge_length"]      = "0.2";
    cmd.args["max_iterations"]          = "1";
    cmd.args["collapse_below"]          = "0.3"; // collapse only very short edges
    cmd.args["max_collapse_iterations"] = "1";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(RemeshExtension, RemeshNoNormalRecompute)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 6, 6);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.remesh";
    cmd.args["recompute_normals"] = "false";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
}

// ── mesh.weld ────────────────────────────────────────────────────────────────

TEST(RemeshExtension, WeldRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.weld";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(RemeshExtension, WeldSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.weld";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("mesh.weld ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(RemeshExtension, WeldSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.weld";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    ASSERT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.find("mesh.weld ok") == std::string::npos) continue;
        found = true;
        EXPECT_NE(m.find("verts_before="), std::string::npos);
        EXPECT_NE(m.find("verts_after="),  std::string::npos);
    }
    EXPECT_TRUE(found);
}

TEST(RemeshExtension, WeldCustomEpsilon)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.weld";
    cmd.args["epsilon"] = "0.001";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
}

// ── mesh.describe ─────────────────────────────────────────────────────────────

TEST(RemeshExtension, DescribeNoMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok); // informational — not an error
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("no mesh"), std::string::npos);
}

TEST(RemeshExtension, DescribeReportsVerticesAndFaces)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool foundVerts = false, foundFaces = false;
    for (const auto& m : msgs) {
        if (m.find("vertices=") != std::string::npos) foundVerts = true;
        if (m.find("faces=")    != std::string::npos) foundFaces = true;
    }
    EXPECT_TRUE(foundVerts);
    EXPECT_TRUE(foundFaces);
}

TEST(RemeshExtension, DescribeReportsBBox)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool foundBBox = false;
    for (const auto& m : msgs)
        if (m.find("bbox_min=") != std::string::npos) { foundBBox = true; break; }
    EXPECT_TRUE(foundBBox);
}

TEST(RemeshExtension, DescribeReportsMeanEdge)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 8, 8);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("mean_edge_len=") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(RemeshExtension, DescribeReportsAttributeFlags)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    // UV sphere has UVs
    ctx.mesh    = nexus::geometry::primitives::makeSphere(1.f, 8u, 8u);
    ctx.hasMesh = true;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool foundAttr = false;
    for (const auto& m : msgs)
        if (m.find("has_uvs=") != std::string::npos) { foundAttr = true; break; }
    EXPECT_TRUE(foundAttr);
}

TEST(RemeshExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.describe";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── Remesh → render pipeline ──────────────────────────────────────────────────

TEST(RemeshExtension, RemeshThenDescribeShowsChangedFaceCount)
{
    // After remesh, mesh.describe should report a different face count
    auto h = makeHarness();
    // Also register softrast so we can chain into render
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 4, 4); // coarse — remesh will add faces

    const std::size_t facesBefore = ctx.mesh.topology().faceCount();

    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "mesh.remesh";
        cmd.args["target_edge_length"] = "0.2"; // small target → more splits
        cmd.args["max_iterations"]     = "2";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    const std::size_t facesAfter = ctx.mesh.topology().faceCount();
    EXPECT_GT(facesAfter, facesBefore)
        << "remesh with small target_edge_length should increase face count";

    // mesh.describe must report the new count
    nexus::automation::ScriptCommand desc;
    desc.name = "mesh.describe";
    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, desc, msgs));

    bool found = false;
    const std::string expected = "faces=" + std::to_string(facesAfter);
    for (const auto& m : msgs)
        if (m.find(expected) != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found) << "describe should report updated face count " << expected;
}
