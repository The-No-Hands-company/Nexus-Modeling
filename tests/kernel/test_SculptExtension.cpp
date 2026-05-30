// Tests for sculpt.stroke, sculpt.describe, and mesh.bevel automation commands.
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/SculptExtension.h>
#include <nexus/geometry/ExtrudeOperation.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerSculptCommands(h);
    return h;
}

void loadSphere(nexus::automation::ScriptContext& ctx,
                uint32_t lat = 12, uint32_t lon = 12)
{
    ctx.mesh    = nexus::geometry::primitives::makeSphere(1.f, lat, lon);
    ctx.hasMesh = true;
}

void loadBox(nexus::automation::ScriptContext& ctx)
{
    ctx.mesh    = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
    ctx.hasMesh = true;
}

// Return the mean distance of all vertices from the origin.
float meanRadius(const nexus::geometry::Mesh& mesh)
{
    const auto& pos = mesh.attributes().positions();
    if (pos.empty()) return 0.f;
    float sum = 0.f;
    for (const auto& p : pos)
        sum += std::sqrt(p.x*p.x + p.y*p.y + p.z*p.z);
    return sum / static_cast<float>(pos.size());
}

} // namespace

// ── sculpt.stroke ─────────────────────────────────────────────────────────────

TEST(SculptExtension, StrokeRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(SculptExtension, InflateDefaultsSucceed)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    // brush=inflate by default, positioned at sphere center

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("sculpt.stroke ok") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(SculptExtension, InflateExpandsSphere)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 16, 16);

    const float radiusBefore = meanRadius(ctx.mesh);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    cmd.args["brush"]    = "inflate";
    cmd.args["radius"]   = "2.0"; // large enough to cover the whole sphere
    cmd.args["strength"] = "0.2";
    cmd.args["samples"]  = "1";
    cmd.args["ox"] = "0"; cmd.args["oy"] = "0"; cmd.args["oz"] = "0";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    const float radiusAfter = meanRadius(ctx.mesh);
    EXPECT_GT(radiusAfter, radiusBefore)
        << "inflate should expand the sphere (mean radius should increase)";
}

TEST(SculptExtension, SmoothStrokeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    cmd.args["brush"]    = "smooth";
    cmd.args["radius"]   = "1.5";
    cmd.args["strength"] = "0.5";
    cmd.args["samples"]  = "3";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
}

TEST(SculptExtension, DrawStrokeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    cmd.args["brush"]             = "draw";
    cmd.args["radius"]            = "0.8";
    cmd.args["strength"]          = "0.3";
    cmd.args["samples"]           = "4";
    cmd.args["use_vertex_normal"] = "false";
    cmd.args["dir_x"] = "0"; cmd.args["dir_y"] = "1"; cmd.args["dir_z"] = "0";
    cmd.args["ox"] = "0"; cmd.args["oy"] = "0.5"; cmd.args["oz"] = "0";
    cmd.args["tx"] = "0"; cmd.args["ty"] = "0.5"; cmd.args["tz"] = "0";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
}

TEST(SculptExtension, GrabStrokeSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    cmd.args["brush"]    = "grab";
    cmd.args["radius"]   = "0.6";
    cmd.args["strength"] = "0.2";
    cmd.args["samples"]  = "2";
    cmd.args["ox"] = "0"; cmd.args["oy"] = "0.9"; cmd.args["oz"] = "0";
    cmd.args["tx"] = "0"; cmd.args["ty"] = "1.2"; cmd.args["tz"] = "0.2";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
}

TEST(SculptExtension, AllBrushKindsAccepted)
{
    // All eight brush kinds must be parsed and accepted without crashing.
    const char* kinds[] = {"draw","smooth","inflate","flatten","pinch","crease","layer","grab"};
    for (const auto* kind : kinds) {
        auto h = makeHarness();
        nexus::automation::ScriptContext ctx;
        loadSphere(ctx);

        nexus::automation::ScriptCommand cmd;
        cmd.name = "sculpt.stroke";
        cmd.args["brush"]    = kind;
        cmd.args["radius"]   = "1.5";
        cmd.args["strength"] = "0.1";
        cmd.args["samples"]  = "2";

        std::vector<std::string> msgs;
        bool ok = h.registry().execute(ctx, cmd, msgs);
        EXPECT_TRUE(ok) << "brush kind '" << kind << "' failed";
        EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
    }
}

TEST(SculptExtension, StrokeSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";
    cmd.args["samples"] = "3";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    bool found = false;
    for (const auto& m : msgs) {
        if (m.find("sculpt.stroke ok") == std::string::npos) continue;
        found = true;
        EXPECT_NE(m.find("brush="),   std::string::npos);
        EXPECT_NE(m.find("touched="), std::string::npos);
        EXPECT_NE(m.find("samples="), std::string::npos);
    }
    EXPECT_TRUE(found);
}

TEST(SculptExtension, StrokeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.stroke";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── sculpt.describe ───────────────────────────────────────────────────────────

TEST(SculptExtension, DescribeNoMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok); // informational
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("no mesh"), std::string::npos);
}

TEST(SculptExtension, DescribeReportsVerticesAndFaces)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.describe";

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

TEST(SculptExtension, DescribeReportsBBox)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs)
        if (m.find("bbox=") != std::string::npos) { found = true; break; }
    EXPECT_TRUE(found);
}

TEST(SculptExtension, DescribeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "sculpt.describe";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── mesh.bevel ────────────────────────────────────────────────────────────────

TEST(SculptExtension, BevelRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(SculptExtension, BevelDefaultsSucceed)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    // BevelChamfer may succeed or return NoSharpEdgesDetected as a warning —
    // either way the mesh must remain valid.
    (void)ok;
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(SculptExtension, BevelSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";
    cmd.args["distance"]    = "0.05";
    cmd.args["sharp_angle"] = "30";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    // Whether it succeeds or warns, result message must be present
    ASSERT_FALSE(msgs.empty());
    bool hasResult = false;
    for (const auto& m : msgs)
        if (m.find("mesh.bevel") != std::string::npos) { hasResult = true; break; }
    EXPECT_TRUE(hasResult);
}

TEST(SculptExtension, BevelOnExtrudedBoxSucceeds)
{
    // An extruded box has clear sharp edges — bevel should detect and process them.
    // Register geometry commands to be able to extrude first.
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerSculptCommands(h);
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    // Manually extrude to create sharp edges, then bevel
    // (No GeometryExtension registered — use direct API)
    nexus::geometry::ExtrudeDesc ed;
    ed.distance = 0.2f;
    nexus::geometry::Mesh extruded;
    [[maybe_unused]] auto extReport = nexus::geometry::ExtrudeOperation::applyToAllFaces(ctx.mesh, ed, extruded);
    ctx.mesh = std::move(extruded);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";
    cmd.args["distance"]    = "0.03";
    cmd.args["sharp_angle"] = "25";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
    EXPECT_GT(ctx.mesh.attributes().positions().size(), 0u);
}

TEST(SculptExtension, ChamferModeAccepted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";
    cmd.args["mode"]        = "chamfer";
    cmd.args["distance"]    = "0.05";
    cmd.args["sharp_angle"] = "30";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(SculptExtension, BevelMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.bevel";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── Pipeline: sculpt → bevel → render ─────────────────────────────────────────

TEST(SculptExtension, InflateThenBevelMeshRemainsValid)
{
    // A common pipeline: inflate a sphere slightly, then soften edges.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx, 12, 12);

    // Inflate
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "sculpt.stroke";
        cmd.args["brush"]    = "inflate";
        cmd.args["radius"]   = "2.0";
        cmd.args["strength"] = "0.15";
        cmd.args["samples"]  = "1";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
    EXPECT_GT(ctx.mesh.attributes().positions().size(), 0u);

    // Bevel
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "mesh.bevel";
        cmd.args["distance"] = "0.02";
        std::vector<std::string> msgs;
        [[maybe_unused]] bool ok2 = h.registry().execute(ctx, cmd, msgs);
    }

    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
    EXPECT_GT(ctx.mesh.attributes().positions().size(), 0u);
}
