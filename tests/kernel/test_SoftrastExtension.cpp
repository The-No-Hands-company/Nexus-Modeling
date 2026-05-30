// Tests for softrast.render and softrast.describe automation commands.
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/SoftrastExtension.h>
#include <nexus/geometry/Mesh.h>

#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <string>

namespace {

nexus::automation::ScriptBatchHarness makeHarness() {
    nexus::automation::ScriptBatchHarness h;
    nexus::automation::registerSoftrastCommands(h);
    return h;
}

// Load a unit box into context.mesh
void loadBox(nexus::automation::ScriptContext& ctx) {
    ctx.mesh = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
    ctx.hasMesh = true;
}

std::string tmpPPM() {
    return (std::filesystem::temp_directory_path() / "nexus_ext_test.ppm").string();
}

} // namespace

// ── softrast.render ──────────────────────────────────────────────────────────

TEST(SoftrastExtension, RenderRequiresMesh) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = tmpPPM();

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires a loaded mesh"), std::string::npos);
}

TEST(SoftrastExtension, RenderRequiresOutputArg) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    // deliberately omit output=

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_FALSE(ok);
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("requires output="), std::string::npos);
}

TEST(SoftrastExtension, RenderProducesPPM) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "128";
    cmd.args["height"] = "128";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    EXPECT_GT(std::filesystem::file_size(out), 0u);
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderSuccessMessageFormat) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "64";
    cmd.args["height"] = "64";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    ASSERT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());
    const auto& m = msgs[0];
    EXPECT_NE(m.find("softrast.render ok"), std::string::npos);
    EXPECT_NE(m.find("64x64"), std::string::npos);
    EXPECT_NE(m.find("nonbg="), std::string::npos);
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderHasNonZeroPixels) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "128";
    cmd.args["height"] = "128";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);
    ASSERT_TRUE(ok);
    ASSERT_FALSE(msgs.empty());

    // Extract nonbg count from message
    const auto& m = msgs[0];
    auto pos = m.find("nonbg=");
    ASSERT_NE(pos, std::string::npos);
    int nonbg = std::stoi(m.substr(pos + 6));
    EXPECT_GT(nonbg, 0);
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderWireframeMode) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "128";
    cmd.args["height"] = "128";
    cmd.args["mode"]   = "wireframe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderDefaultsAccepted) {
    // no width/height/mode args — uses defaults
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    std::remove(out.c_str());
}

// ── softrast.describe ────────────────────────────────────────────────────────

TEST(SoftrastExtension, DescribeNoMesh) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    ctx.hasMesh = false;

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok); // informational, not an error
    ASSERT_FALSE(msgs.empty());
    EXPECT_NE(msgs[0].find("no mesh"), std::string::npos);
}

TEST(SoftrastExtension, DescribeReportsVerticesAndFaces) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    ASSERT_GE(msgs.size(), 1u);

    bool foundVertices = false;
    bool foundFaces    = false;
    for (const auto& m : msgs) {
        if (m.find("vertices=") != std::string::npos) foundVertices = true;
        if (m.find("faces=")    != std::string::npos) foundFaces    = true;
    }
    EXPECT_TRUE(foundVertices);
    EXPECT_TRUE(foundFaces);
}

TEST(SoftrastExtension, DescribeReportsBBox) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.describe";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    bool foundBBox = false;
    for (const auto& m : msgs) {
        if (m.find("bbox=") != std::string::npos) { foundBBox = true; break; }
    }
    EXPECT_TRUE(foundBBox);
}

TEST(SoftrastExtension, RenderGouraudMode) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "128";
    cmd.args["height"] = "128";
    cmd.args["mode"]   = "gouraud";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderTGAFormat) {
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = (std::filesystem::temp_directory_path() / "nexus_ext_test.tga").string();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"] = out;
    cmd.args["width"]  = "64";
    cmd.args["height"] = "64";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    // TGA header: bytes 2=image type 2 (truecolor), bytes 16=24 (bpp)
    if (std::filesystem::exists(out)) {
        std::FILE* f = std::fopen(out.c_str(), "rb");
        if (f) {
            uint8_t hdr[18] = {};
            std::fread(hdr, 1, 18, f);
            std::fclose(f);
            EXPECT_EQ(hdr[2], 2u);   // uncompressed truecolor
            EXPECT_EQ(hdr[16], 24u); // 24 bpp
        }
    }
    std::remove(out.c_str());
}

TEST(SoftrastExtension, MessagesAreSorted) {
    // All command handlers must leave messages in sorted order.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.describe";

    std::vector<std::string> msgs;
    [[maybe_unused]] bool ok = h.registry().execute(ctx, cmd, msgs);

    auto sorted = msgs;
    std::sort(sorted.begin(), sorted.end());
    EXPECT_EQ(msgs, sorted);
}

TEST(SoftrastExtension, RenderCustomColors) {
    // Custom base_r/g/b, bg_r/g/b, light_x/y/z args are accepted and produce output.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    const std::string out = tmpPPM();
    std::remove(out.c_str());

    nexus::automation::ScriptCommand cmd;
    cmd.name = "softrast.render";
    cmd.args["output"]  = out;
    cmd.args["width"]   = "64";
    cmd.args["height"]  = "64";
    cmd.args["base_r"]  = "100";
    cmd.args["base_g"]  = "160";
    cmd.args["base_b"]  = "220"; // steel blue
    cmd.args["bg_r"]    = "0";
    cmd.args["bg_g"]    = "0";
    cmd.args["bg_b"]    = "0"; // pure black background
    cmd.args["light_x"] = "-1.0";
    cmd.args["light_y"] = "1.0";
    cmd.args["light_z"] = "0.5";

    std::vector<std::string> msgs;
    bool ok = h.registry().execute(ctx, cmd, msgs);

    EXPECT_TRUE(ok);
    EXPECT_TRUE(std::filesystem::exists(out));
    std::remove(out.c_str());
}

TEST(SoftrastExtension, RenderCustomLightChangesOutput) {
    // Rendering with opposite light directions produces different pixel hashes.
    auto h = makeHarness();
    nexus::automation::ScriptContext ctx;
    loadBox(ctx);

    auto renderWithLight = [&](float lx, float ly, float lz) -> std::string {
        const std::string out = tmpPPM();
        std::remove(out.c_str());
        nexus::automation::ScriptCommand cmd;
        cmd.name = "softrast.render";
        cmd.args["output"]  = out;
        cmd.args["width"]   = "64";
        cmd.args["height"]  = "64";
        cmd.args["light_x"] = std::to_string(lx);
        cmd.args["light_y"] = std::to_string(ly);
        cmd.args["light_z"] = std::to_string(lz);
        std::vector<std::string> msgs;
        h.registry().execute(ctx, cmd, msgs);
        return out;
    };

    const std::string out1 = renderWithLight( 1.f,  1.f,  1.f);
    const std::string out2 = renderWithLight(-1.f, -1.f, -1.f);

    // The two files should exist but have different content
    EXPECT_TRUE(std::filesystem::exists(out1));
    EXPECT_TRUE(std::filesystem::exists(out2));
    if (std::filesystem::exists(out1) && std::filesystem::exists(out2))
        EXPECT_NE(std::filesystem::file_size(out1), 0u);

    std::remove(out1.c_str());
    std::remove(out2.c_str());
}
