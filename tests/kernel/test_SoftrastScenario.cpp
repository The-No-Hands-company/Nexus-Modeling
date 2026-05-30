// Softrast scenario render test.
//
// When SOFTRAST_ARTIFACT_DIR is set (by the scenario runner), renders a box
// from several angles and writes PPMs + a frame_hash.txt into that directory.
// Without the env var the test is a no-op (skipped), so it never pollutes normal runs.
#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/render/Camera.h>

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
