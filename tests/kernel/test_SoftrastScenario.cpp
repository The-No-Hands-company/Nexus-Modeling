// Softrast scenario render test.
//
// When SOFTRAST_ARTIFACT_DIR is set (by the scenario runner), renders a box
// from several angles and writes PPMs + a frame_hash.txt into that directory.
// Without the env var the test is a no-op (skipped), so it never pollutes normal runs.
#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>

#include <nexus/asset/SceneAsset.h>
#include <nexus/automation/AutomationScript.h>
#include <nexus/automation/RemeshExtension.h>
#include <nexus/automation/SoftrastExtension.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/geometry/RemeshOperation.h>
#include <nexus/render/Camera.h>
#include <softrast/Texture2D.h>

#include <gtest/gtest.h>

#include <charconv>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

nexus::geometry::Mesh makeSceneBox() {
    nexus::geometry::Mesh m;
    m.attributes().setPositions({
        {-0.5f, -0.5f,  0.5f}, { 0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, { 0.5f, -0.5f, -0.5f},
        { 0.5f,  0.5f, -0.5f}, {-0.5f,  0.5f, -0.5f},
    });
    m.topology().addFace(nexus::geometry::Face{{0,1,2,3}});
    m.topology().addFace(nexus::geometry::Face{{5,4,7,6}});
    m.topology().addFace(nexus::geometry::Face{{4,0,3,7}});
    m.topology().addFace(nexus::geometry::Face{{1,5,6,2}});
    m.topology().addFace(nexus::geometry::Face{{3,2,6,7}});
    m.topology().addFace(nexus::geometry::Face{{4,5,1,0}});
    return m;
}

nexus::render::Camera makeCamera(float eyeX, float eyeY, float eyeZ,
                                 uint32_t w, uint32_t h) {
    nexus::render::Camera cam;
    cam.setPerspective(45.f, static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.f);
    cam.lookAt({eyeX, eyeY, eyeZ}, {0.f, 0.f, 0.f});
    return cam;
}

} // namespace

TEST(SoftrastScenario, RenderBoxToArtifactDir) {
    const char* dirEnv = std::getenv("SOFTRAST_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_ARTIFACT_DIR not set — skipping scenario render";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec) << "could not create artifact dir: " << ec.message();

    const nexus::geometry::Mesh box = makeSceneBox();
    nexus::softrast::SoftwareRasterizer sr;

    constexpr uint32_t W = 256, H = 256;
    nexus::softrast::PixelBuffer buf(W, H);

    struct View { const char* name; float ex, ey, ez; nexus::softrast::ShadingMode mode; };
    const View views[] = {
        { "flat_front",     2.f,  2.f,  4.f, nexus::softrast::ShadingMode::Flat      },
        { "flat_top",       0.f,  5.f,  0.5f,nexus::softrast::ShadingMode::Flat      },
        { "wireframe_iso",  3.f,  3.f,  3.f, nexus::softrast::ShadingMode::Wireframe },
    };

    uint64_t combinedHash = 14695981039346656037ULL; // FNV-1a offset basis
    bool allOk = true;

    for (const auto& v : views) {
        nexus::softrast::RasterizerConfig cfg;
        cfg.mode = v.mode;
        cfg.background = {20, 20, 30, 255};

        const auto cam = makeCamera(v.ex, v.ey, v.ez, W, H);
        sr.render(buf, box, cam, cfg);

        const std::string ppmName = std::string(v.name) + ".ppm";
        const bool wrote = nexus::softrast::writePPM((artifactDir / ppmName).string(), buf);
        EXPECT_TRUE(wrote) << "failed to write " << ppmName;
        if (!wrote) { allOk = false; continue; }

        // Mix this view's hash into the combined hash (FNV-1a extend)
        const uint64_t viewHash = buf.fnv1aHash();
        const uint8_t* vhBytes = reinterpret_cast<const uint8_t*>(&viewHash);
        for (std::size_t i = 0; i < sizeof(viewHash); ++i) {
            combinedHash ^= vhBytes[i];
            combinedHash *= 1099511628211ULL;
        }
    }

    // Write frame_hash.txt
    {
        std::ofstream fh(artifactDir / "frame_hash.txt");
        char buf64[20]; // enough for uint64 hex
        auto [ptr, ec2] = std::to_chars(buf64, buf64 + sizeof(buf64), combinedHash, 16);
        fh << std::string(buf64, ptr) << "\n";
    }

    EXPECT_TRUE(allOk);
}

// ── SoftrastScenario.RenderSphereGouraud ──────────────────────────────────────
//
// Renders a UV sphere in Gouraud mode from two viewpoints (front + 45° iso),
// demonstrating the full pipeline:
//   mesh.make_sphere → per-vertex normals → gouraud → PPM artifact + frame hash
//
// Activated only when SOFTRAST_SPHERE_ARTIFACT_DIR is set (by the scenario runner).

TEST(SoftrastScenario, RenderSphereGouraud) {
    const char* dirEnv = std::getenv("SOFTRAST_SPHERE_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_SPHERE_ARTIFACT_DIR not set — skipping sphere scenario";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec) << "could not create artifact dir: " << ec.message();

    // Build a smooth UV sphere (32 lat × 32 lon)
    const nexus::geometry::Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 32u, 32u);
    ASSERT_GT(sphere.attributes().positions().size(), 0u);

    nexus::softrast::SoftwareRasterizer sr;
    constexpr uint32_t W = 256, H = 256;
    nexus::softrast::PixelBuffer buf(W, H);

    nexus::softrast::RasterizerConfig cfg;
    cfg.mode       = nexus::softrast::ShadingMode::Gouraud;
    cfg.background = {15, 15, 25, 255};
    cfg.baseColor  = {100, 160, 220, 255}; // steel blue
    cfg.ambientMin = 0.10f;

    struct View { const char* name; float ex, ey, ez; };
    const View views[] = {
        { "sphere_gouraud_front", 0.f, 0.f, 3.f },
        { "sphere_gouraud_iso",   2.f, 1.5f, 2.f },
    };

    uint64_t combinedHash = 14695981039346656037ULL;
    bool allOk = true;

    for (const auto& v : views) {
        nexus::render::Camera cam;
        cam.setPerspective(45.f, static_cast<float>(W) / static_cast<float>(H), 0.1f, 100.f);
        cam.lookAt({v.ex, v.ey, v.ez}, {0.f, 0.f, 0.f});

        sr.render(buf, sphere, cam, cfg);

        const bool wrote = nexus::softrast::writePPM(
            (artifactDir / (std::string(v.name) + ".ppm")).string(), buf);
        EXPECT_TRUE(wrote) << "failed to write " << v.name;
        if (!wrote) { allOk = false; continue; }

        const uint64_t viewHash = buf.fnv1aHash();
        const uint8_t* vhBytes = reinterpret_cast<const uint8_t*>(&viewHash);
        for (std::size_t i = 0; i < sizeof(viewHash); ++i) {
            combinedHash ^= vhBytes[i];
            combinedHash *= 1099511628211ULL;
        }
    }

    // Sanity: Gouraud sphere must have many non-background pixels
    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});
    sr.render(buf, sphere, cam, cfg);

    uint32_t nonBg = 0;
    const auto& bg = cfg.background;
    for (const auto& px : buf.pixels()) {
        if (px.r != bg.r || px.g != bg.g || px.b != bg.b) ++nonBg;
    }
    EXPECT_GT(nonBg, 1000u) << "Gouraud sphere rendered too few pixels — shading broken?";

    // Write frame_hash.txt
    {
        std::ofstream fh(artifactDir / "frame_hash.txt");
        char buf64[20];
        auto [ptr, ec2] = std::to_chars(buf64, buf64 + sizeof(buf64), combinedHash, 16);
        fh << std::string(buf64, ptr) << "\n";
    }

    EXPECT_TRUE(allOk);
}

// ── SoftrastScenario.RenderSphereSpecular ─────────────────────────────────────
//
// Renders a UV sphere in Gouraud + Blinn-Phong specular from three viewpoints,
// verifying that specular render is measurably brighter than diffuse-only.
// Activated only when SOFTRAST_SPECULAR_ARTIFACT_DIR is set.

TEST(SoftrastScenario, RenderSphereSpecular) {
    const char* dirEnv = std::getenv("SOFTRAST_SPECULAR_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_SPECULAR_ARTIFACT_DIR not set — skipping specular scenario";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec);

    const nexus::geometry::Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 48u, 48u);
    ASSERT_GT(sphere.attributes().positions().size(), 0u);

    nexus::softrast::SoftwareRasterizer sr;
    constexpr uint32_t W = 256, H = 256;
    nexus::softrast::PixelBuffer buf(W, H);

    nexus::softrast::RasterizerConfig cfg;
    cfg.mode         = nexus::softrast::ShadingMode::Gouraud;
    cfg.background   = {10, 10, 20, 255};
    cfg.baseColor    = {60, 100, 200, 255};
    cfg.specColor    = {255, 240, 200, 255};
    cfg.specStrength = 0.7f;
    cfg.shininess    = 64.f;
    cfg.ambientMin   = 0.08f;
    cfg.lightDir     = nexus::render::Vec3{0.6f, 0.8f, 0.5f}.normalize();

    struct View { const char* name; float ex, ey, ez; };
    const View views[] = {
        { "spec_front",  0.f, 0.f,  3.f },
        { "spec_top",    0.f, 3.f,  0.1f },
        { "spec_iso",    2.f, 1.5f, 2.f  },
    };

    uint64_t combinedHash = 14695981039346656037ULL;
    bool allOk = true;

    for (const auto& v : views) {
        nexus::render::Camera cam;
        cam.setPerspective(45.f, static_cast<float>(W) / static_cast<float>(H), 0.1f, 100.f);
        cam.lookAt({v.ex, v.ey, v.ez}, {0.f, 0.f, 0.f});
        sr.render(buf, sphere, cam, cfg);

        const bool wrote = nexus::softrast::writePPM(
            (artifactDir / (std::string(v.name) + ".ppm")).string(), buf);
        EXPECT_TRUE(wrote) << "failed to write " << v.name;
        if (!wrote) { allOk = false; continue; }

        const uint64_t vh = buf.fnv1aHash();
        const uint8_t* vhb = reinterpret_cast<const uint8_t*>(&vh);
        for (std::size_t i = 0; i < sizeof(vh); ++i) {
            combinedHash ^= vhb[i];
            combinedHash *= 1099511628211ULL;
        }
    }

    // Specular render must be measurably brighter than diffuse-only.
    nexus::softrast::RasterizerConfig cfgNoSpec = cfg;
    cfgNoSpec.specStrength = 0.f;

    nexus::render::Camera camFront;
    camFront.setPerspective(45.f, 1.f, 0.1f, 100.f);
    camFront.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    nexus::softrast::PixelBuffer bufSpec(W, H), bufDiff(W, H);
    sr.render(bufSpec, sphere, camFront, cfg);
    sr.render(bufDiff, sphere, camFront, cfgNoSpec);

    uint64_t brightnessSpec = 0, brightnessDiff = 0;
    for (const auto& px : bufSpec.pixels()) brightnessSpec += px.r + px.g + px.b;
    for (const auto& px : bufDiff.pixels())  brightnessDiff += px.r + px.g + px.b;
    EXPECT_GT(brightnessSpec, brightnessDiff)
        << "Specular render should be brighter than diffuse-only";

    std::ofstream fh(artifactDir / "frame_hash.txt");
    char hbuf[20];
    auto [ptr, ec2] = std::to_chars(hbuf, hbuf + sizeof(hbuf), combinedHash, 16);
    fh << std::string(hbuf, ptr) << "\n";

    EXPECT_TRUE(allOk);
}

// ── SoftrastScenario.RenderMultiObjectScene ───────────────────────────────────
//
// Builds a scene with three mesh entries (sphere, box, sphere) placed at
// (-2,0,0), (0,0,0), (2,0,0) and renders them via softrast.render_scene.
// Activated only when SOFTRAST_MULTI_ARTIFACT_DIR is set.

TEST(SoftrastScenario, RenderMultiObjectScene) {
    const char* dirEnv = std::getenv("SOFTRAST_MULTI_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_MULTI_ARTIFACT_DIR not set — skipping multi-object scenario";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec) << "could not create artifact dir: " << ec.message();

    nexus::automation::ScriptBatchHarness harness;
    nexus::automation::registerSoftrastCommands(harness);

    // Build scene: sphere | box | sphere at x = -2, 0, +2
    nexus::automation::ScriptContext ctx;
    {
        nexus::asset::SceneMeshEntry e1;
        e1.name = "sphere_left";
        e1.mesh = nexus::geometry::primitives::makeSphere(0.8f, 24u, 24u);
        e1.transform.translation = {-2.f, 0.f, 0.f};
        e1.visible = true;
        ctx.scene.addEntry(std::move(e1));

        nexus::asset::SceneMeshEntry e2;
        e2.name = "box_center";
        e2.mesh = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
        e2.transform.translation = {0.f, 0.f, 0.f};
        e2.visible = true;
        ctx.scene.addEntry(std::move(e2));

        nexus::asset::SceneMeshEntry e3;
        e3.name = "sphere_right";
        e3.mesh = nexus::geometry::primitives::makeSphere(0.8f, 24u, 24u);
        e3.transform.translation = {2.f, 0.f, 0.f};
        e3.visible = true;
        ctx.scene.addEntry(std::move(e3));
    }
    ctx.hasScene = true;

    constexpr int W = 512, H = 256;

    struct View { const char* name; float ex, ey, ez; };
    const View views[] = {
        { "multi_front", 0.f, 0.f, 6.f  },
        { "multi_iso",   4.f, 3.f, 4.f  },
        { "multi_top",   0.f, 6.f, 0.5f },
    };

    uint64_t combinedHash = 14695981039346656037ULL;
    bool allOk = true;

    for (const auto& v : views) {
        const fs::path outPath = artifactDir / (std::string(v.name) + ".ppm");

        nexus::automation::ScriptCommand cmd;
        cmd.name = "softrast.render_scene";
        cmd.args["output"]  = outPath.string();
        cmd.args["width"]   = std::to_string(W);
        cmd.args["height"]  = std::to_string(H);
        cmd.args["mode"]    = "gouraud";
        cmd.args["eye_x"]   = std::to_string(v.ex);
        cmd.args["eye_y"]   = std::to_string(v.ey);
        cmd.args["eye_z"]   = std::to_string(v.ez);
        cmd.args["base_r"]  = "140";
        cmd.args["base_g"]  = "180";
        cmd.args["base_b"]  = "230";

        std::vector<std::string> msgs;
        bool ok = harness.registry().execute(ctx, cmd, msgs);
        EXPECT_TRUE(ok) << "render_scene failed for " << v.name;
        if (!ok) { allOk = false; continue; }
        EXPECT_TRUE(fs::exists(outPath)) << "output file missing: " << outPath;

        // Extract nonbg from message and verify mesh is visible
        ASSERT_FALSE(msgs.empty());
        auto pos = msgs[0].find("nonbg=");
        if (pos != std::string::npos) {
            int nonbg = std::stoi(msgs[0].substr(pos + 6));
            EXPECT_GT(nonbg, 0) << "no mesh pixels rendered for " << v.name;
        }

        // Fold the nonbg count into hash (fast proxy for pixel content)
        auto pos2 = msgs[0].find("nonbg=");
        if (pos2 != std::string::npos) {
            uint64_t val = static_cast<uint64_t>(std::stoi(msgs[0].substr(pos2 + 6)));
            const uint8_t* b = reinterpret_cast<const uint8_t*>(&val);
            for (std::size_t i = 0; i < sizeof(val); ++i) {
                combinedHash ^= b[i];
                combinedHash *= 1099511628211ULL;
            }
        }
    }

    std::ofstream fh(artifactDir / "frame_hash.txt");
    char hbuf[20];
    auto [ptr, ec2] = std::to_chars(hbuf, hbuf + sizeof(hbuf), combinedHash, 16);
    fh << std::string(hbuf, ptr) << "\n";

    EXPECT_TRUE(allOk);
}

// ── SoftrastScenario.RenderTexturedSphere ─────────────────────────────────────
//
// Renders a UV sphere with two procedural textures (checker + uvgrad) from two
// viewpoints, verifying textured output differs from untextured.
// Activated only when SOFTRAST_TEXTURED_ARTIFACT_DIR is set.

TEST(SoftrastScenario, RenderTexturedSphere) {
    const char* dirEnv = std::getenv("SOFTRAST_TEXTURED_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_TEXTURED_ARTIFACT_DIR not set — skipping textured sphere scenario";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec) << "could not create artifact dir: " << ec.message();

    const nexus::geometry::Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 32u, 32u);
    ASSERT_TRUE(sphere.attributes().hasUVs()) << "UV sphere must carry UV coordinates";

    nexus::softrast::SoftwareRasterizer sr;
    constexpr uint32_t W = 256, H = 256;
    nexus::softrast::PixelBuffer buf(W, H);

    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    uint64_t combinedHash = 14695981039346656037ULL;
    bool allOk = true;

    // Render: checker flat, checker gouraud, uvgrad flat, uvgrad gouraud
    struct Variant {
        const char*                   name;
        nexus::softrast::ShadingMode  mode;
        nexus::softrast::Texture2D    tex;
    };
    std::vector<Variant> variants;
    variants.push_back({"checker_flat",    nexus::softrast::ShadingMode::Flat,
                        nexus::softrast::Texture2D::makeCheckerboard(64)});
    variants.push_back({"checker_gouraud", nexus::softrast::ShadingMode::Gouraud,
                        nexus::softrast::Texture2D::makeCheckerboard(64)});
    variants.push_back({"uvgrad_flat",     nexus::softrast::ShadingMode::Flat,
                        nexus::softrast::Texture2D::makeUVGradient(128)});
    variants.push_back({"uvgrad_gouraud",  nexus::softrast::ShadingMode::Gouraud,
                        nexus::softrast::Texture2D::makeUVGradient(128)});

    for (auto& v : variants) {
        nexus::softrast::RasterizerConfig cfg;
        cfg.mode       = v.mode;
        cfg.background = {10, 10, 20, 255};
        cfg.ambientMin = 0.12f;
        cfg.texture    = std::move(v.tex);

        sr.render(buf, sphere, cam, cfg);

        const bool wrote = nexus::softrast::writePPM(
            (artifactDir / (std::string(v.name) + ".ppm")).string(), buf);
        EXPECT_TRUE(wrote) << "failed to write " << v.name;
        if (!wrote) { allOk = false; continue; }

        const uint64_t vh = buf.fnv1aHash();
        const uint8_t* vhb = reinterpret_cast<const uint8_t*>(&vh);
        for (std::size_t i = 0; i < sizeof(vh); ++i) {
            combinedHash ^= vhb[i];
            combinedHash *= 1099511628211ULL;
        }

        // Sanity: must have non-background pixels
        uint32_t nonBg = 0;
        const auto& bg = cfg.background;
        for (const auto& px : buf.pixels()) {
            if (px.r != bg.r || px.g != bg.g || px.b != bg.b) ++nonBg;
        }
        EXPECT_GT(nonBg, 500u) << "variant " << v.name << " rendered too few pixels";
    }

    // Checker and uvgrad renders must differ from each other
    nexus::softrast::RasterizerConfig cfgChecker;
    cfgChecker.texture = nexus::softrast::Texture2D::makeCheckerboard(64);
    nexus::softrast::PixelBuffer bufChecker(W, H), bufUVGrad(W, H);
    sr.render(bufChecker, sphere, cam, cfgChecker);

    nexus::softrast::RasterizerConfig cfgGrad;
    cfgGrad.texture = nexus::softrast::Texture2D::makeUVGradient(64);
    sr.render(bufUVGrad, sphere, cam, cfgGrad);

    EXPECT_NE(bufChecker.fnv1aHash(), bufUVGrad.fnv1aHash())
        << "checker and uvgrad textures should produce different renders";

    std::ofstream fh(artifactDir / "frame_hash.txt");
    char hbuf[20];
    auto [ptr, ec2] = std::to_chars(hbuf, hbuf + sizeof(hbuf), combinedHash, 16);
    fh << std::string(hbuf, ptr) << "\n";

    EXPECT_TRUE(allOk);
}

// ── SoftrastScenario.RemeshAndRender ─────────────────────────────────────────
//
// Demonstrates the full remesh → render pipeline:
//   1. Build a coarse UV sphere
//   2. Apply RemeshOperation to subdivide it isotropically
//   3. Render with Gouraud + checkerboard texture
// Verifies that the remeshed sphere produces more faces than the original and
// that the textured render has non-background pixels.
// Activated only when SOFTRAST_REMESH_ARTIFACT_DIR is set.

TEST(SoftrastScenario, RemeshAndRender) {
    const char* dirEnv = std::getenv("SOFTRAST_REMESH_ARTIFACT_DIR");
    if (!dirEnv || *dirEnv == '\0') {
        GTEST_SKIP() << "SOFTRAST_REMESH_ARTIFACT_DIR not set — skipping remesh scenario";
    }

    namespace fs = std::filesystem;
    const fs::path artifactDir = dirEnv;
    std::error_code ec;
    fs::create_directories(artifactDir, ec);
    ASSERT_FALSE(ec);

    // 1. Coarse sphere
    nexus::geometry::Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 6u, 6u);
    const std::size_t facesBefore = sphere.topology().faceCount();

    // 2. Remesh
    nexus::geometry::RemeshDesc desc;
    desc.targetEdgeLength = 0.25f;
    desc.maxIterations    = 2;
    nexus::geometry::Mesh remeshed;
    const auto report = nexus::geometry::RemeshOperation::apply(sphere, desc, remeshed);
    ASSERT_TRUE(report.isSuccess()) << "RemeshOperation failed";
    EXPECT_GT(remeshed.topology().faceCount(), facesBefore)
        << "remesh should increase face count";

    // Write face counts to metadata file
    {
        std::ofstream f(artifactDir / "remesh_stats.txt");
        f << "faces_before=" << facesBefore << "\n"
          << "faces_after="  << remeshed.topology().faceCount() << "\n"
          << "splits="       << report.splitCount << "\n"
          << "collapses="    << report.collapseCount << "\n";
    }

    // 3. Render original and remeshed, compare hashes
    nexus::softrast::SoftwareRasterizer sr;
    constexpr uint32_t W = 256, H = 256;
    nexus::softrast::PixelBuffer bufOrig(W, H), bufRemeshed(W, H);

    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    nexus::softrast::RasterizerConfig cfg;
    cfg.mode       = nexus::softrast::ShadingMode::Gouraud;
    cfg.background = {10, 10, 20, 255};
    cfg.texture    = nexus::softrast::Texture2D::makeCheckerboard(64);

    sr.render(bufOrig,     sphere,   cam, cfg);
    sr.render(bufRemeshed, remeshed, cam, cfg);

    EXPECT_TRUE(nexus::softrast::writePPM((artifactDir / "coarse.ppm").string(),    bufOrig));
    EXPECT_TRUE(nexus::softrast::writePPM((artifactDir / "remeshed.ppm").string(),  bufRemeshed));

    // Remeshed should have more non-background pixels (smoother silhouette)
    uint32_t nonBgRemesh = 0;
    const auto& bg = cfg.background;
    for (const auto& px : bufRemeshed.pixels())
        if (px.r != bg.r || px.g != bg.g || px.b != bg.b) ++nonBgRemesh;
    EXPECT_GT(nonBgRemesh, 500u);

    // Frame hash
    uint64_t combinedHash = 14695981039346656037ULL;
    for (const auto* buf : {&bufOrig, &bufRemeshed}) {
        const uint64_t vh = buf->fnv1aHash();
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&vh);
        for (std::size_t i = 0; i < sizeof(vh); ++i) {
            combinedHash ^= b[i];
            combinedHash *= 1099511628211ULL;
        }
    }
    std::ofstream fh(artifactDir / "frame_hash.txt");
    char hbuf[20];
    auto [ptr, ec2] = std::to_chars(hbuf, hbuf + sizeof(hbuf), combinedHash, 16);
    fh << std::string(hbuf, ptr) << "\n";
}
