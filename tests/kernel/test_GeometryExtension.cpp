// Tests for mesh.extrude and mesh.inset automation commands.
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/GeometryExtension.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

nexus::automation::ScriptBatchHarness makeHarness()
{
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerGeometryCommands(h);
    return h;
}

void loadBox(nexus::automation::ScriptContext& ctx)
{
    ctx.mesh    = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
    ctx.hasMesh = true;
}

void loadSphere(nexus::automation::ScriptContext& ctx)
{
    ctx.mesh    = nexus::geometry::primitives::makeSphere(1.f, 8u, 8u);
    ctx.hasMesh = true;
}

} // namespace

// ── mesh.extrude ──────────────────────────────────────────────────────────────

TEST(GeometryExtension, ExtrudeRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(GeometryExtension, ExtrudeDefaultsSucceed)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(GeometryExtension, ExtrudeSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";
    cmd.args["distance"] = "0.2";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    ASSERT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.find("mesh.extrude ok") == std::string::npos) continue;
        found = true;
        EXPECT_NE(m.find("extruded="),    std::string::npos);
        EXPECT_NE(m.find("added_faces="), std::string::npos);
        EXPECT_NE(m.find("added_verts="), std::string::npos);
    }
    EXPECT_TRUE(found);
}

TEST(GeometryExtension, ExtrudeIncreasesGeometry)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);
    const std::size_t facesBefore = ctx.mesh.topology().faceCount();
    const std::size_t vertsBefore = ctx.mesh.attributes().positions().size();

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";
    cmd.args["distance"] = "0.15";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    // Extrusion should add new cap + wall faces and new vertices
    EXPECT_GT(ctx.mesh.topology().faceCount(),          facesBefore);
    EXPECT_GT(ctx.mesh.attributes().positions().size(), vertsBefore);
}

TEST(GeometryExtension, ExtrudeKeepOriginalFaces)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);
    const std::size_t facesBefore = ctx.mesh.topology().faceCount();

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";
    cmd.args["distance"]      = "0.1";
    cmd.args["keep_original"] = "true";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    // Keeping originals means even more faces than without
    EXPECT_GT(ctx.mesh.topology().faceCount(), facesBefore);
}

TEST(GeometryExtension, ExtrudeNegativeDistance)
{
    // Negative distance extrudes inward — operation should still succeed
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";
    cmd.args["distance"] = "-0.05";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    // Success or documented failure, but must not crash
    (void)ok;
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(GeometryExtension, ExtrudeMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.extrude";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

// ── mesh.inset ────────────────────────────────────────────────────────────────

TEST(GeometryExtension, InsetRequiresMesh)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(GeometryExtension, InsetDefaultsSucceed)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(GeometryExtension, InsetSuccessMessageFormat)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";
    cmd.args["amount"] = "0.2";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    ASSERT_TRUE(ok);
    bool found = false;
    for (const auto& m : msgs) {
        if (m.find("mesh.inset ok") == std::string::npos) continue;
        found = true;
        EXPECT_NE(m.find("inset="),       std::string::npos);
        EXPECT_NE(m.find("added_faces="), std::string::npos);
        EXPECT_NE(m.find("added_verts="), std::string::npos);
    }
    EXPECT_TRUE(found);
}

TEST(GeometryExtension, InsetIncreasesGeometry)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);
    const std::size_t facesBefore = ctx.mesh.topology().faceCount();

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";
    cmd.args["amount"] = "0.2";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    // Inset creates inner face + border ring quads — more faces
    EXPECT_GT(ctx.mesh.topology().faceCount(), facesBefore);
}

TEST(GeometryExtension, InsetDistanceMode)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";
    cmd.args["amount"] = "0.1";
    cmd.args["mode"]   = "distance";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

TEST(GeometryExtension, InsetKeepOriginal)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);
    const std::size_t facesBefore = ctx.mesh.topology().faceCount();

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";
    cmd.args["amount"]           = "0.2";
    cmd.args["replace_original"] = "false";

    std::vector<std::string> msgs;
    ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));

    // Keeping originals adds even more faces
    EXPECT_GT(ctx.mesh.topology().faceCount(), facesBefore);
}

TEST(GeometryExtension, InsetMessagesAreSorted)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

TEST(GeometryExtension, InsetOnSphereSucceeds)
{
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadSphere(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "mesh.inset";
    cmd.args["amount"] = "0.15";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_GT(ctx.mesh.topology().faceCount(), 0u);
}

// ── Pipeline: inset → extrude ─────────────────────────────────────────────────

TEST(GeometryExtension, InsetThenExtrudePipeline)
{
    // Classic hard-surface loop: inset face borders, then extrude inward
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::size_t facesOriginal = ctx.mesh.topology().faceCount();

    // Step 1: inset
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "mesh.inset";
        cmd.args["amount"] = "0.2";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    const std::size_t facesAfterInset = ctx.mesh.topology().faceCount();
    EXPECT_GT(facesAfterInset, facesOriginal);

    // Step 2: extrude
    {
        nexus::automation::ScriptCommand cmd;
        cmd.name = "mesh.extrude";
        cmd.args["distance"] = "0.1";
        std::vector<std::string> msgs;
        ASSERT_TRUE(h.registry().execute(ctx, cmd, msgs));
    }

    const std::size_t facesAfterExtrude = ctx.mesh.topology().faceCount();
    EXPECT_GT(facesAfterExtrude, facesAfterInset);
    EXPECT_GT(ctx.mesh.attributes().positions().size(), 0u);
}
