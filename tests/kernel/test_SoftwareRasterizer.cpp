// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Softrast — visual regression tests
//
//  BLESS workflow:
//    Define NEXUS_BLESS_GOLDEN at build time (or pass --bless to the runner)
//    to regenerate golden PPM images from the current renderer output.
//    Once blessed, subsequent runs compare pixel hashes for regressions.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>

#include <gtest/gtest.h>
#include <filesystem>
#include <string>

using namespace nexus::geometry;
using namespace nexus::render;
using namespace nexus::softrast;

namespace fs = std::filesystem;

// ─────────────────────────────────────────────────────────────────────────────
//  Fixture paths
// ─────────────────────────────────────────────────────────────────────────────

static const fs::path kFixtureDir =
    fs::path(NEXUS_TESTS_ROOT) / "kernel" / "fixtures" / "softrast";

// ─────────────────────────────────────────────────────────────────────────────
//  Mesh helpers
// ─────────────────────────────────────────────────────────────────────────────

// Unit box [−0.5, 0.5]³ with outward-facing quads.
static Mesh makeBox() {
    Mesh m;
    // 8 vertices
    m.attributes().setPositions({
        {-0.5f, -0.5f,  0.5f}, // 0 front-bottom-left
        { 0.5f, -0.5f,  0.5f}, // 1 front-bottom-right
        { 0.5f,  0.5f,  0.5f}, // 2 front-top-right
        {-0.5f,  0.5f,  0.5f}, // 3 front-top-left
        {-0.5f, -0.5f, -0.5f}, // 4 back-bottom-left
        { 0.5f, -0.5f, -0.5f}, // 5 back-bottom-right
        { 0.5f,  0.5f, -0.5f}, // 6 back-top-right
        {-0.5f,  0.5f, -0.5f}, // 7 back-top-left
    });
    // 6 quads, CCW from outside
    m.topology().addFace(Face{{0, 1, 2, 3}}); // front
    m.topology().addFace(Face{{5, 4, 7, 6}}); // back
    m.topology().addFace(Face{{4, 0, 3, 7}}); // left
    m.topology().addFace(Face{{1, 5, 6, 2}}); // right
    m.topology().addFace(Face{{3, 2, 6, 7}}); // top
    m.topology().addFace(Face{{4, 5, 1, 0}}); // bottom
    return m;
}

// Simple flat triangle.
static Mesh makeTriangle() {
    Mesh m;
    m.attributes().setPositions({
        { 0.0f,  0.5f, 0.f},
        {-0.5f, -0.5f, 0.f},
        { 0.5f, -0.5f, 0.f},
    });
    m.topology().addFace(Face{{0, 1, 2}});
    return m;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Camera helper
// ─────────────────────────────────────────────────────────────────────────────

static Camera makeCamera(uint32_t w, uint32_t h) {
    Camera cam;
    cam.setPerspective(45.f, static_cast<float>(w) / static_cast<float>(h), 0.1f, 100.f);
    cam.lookAt({2.f, 2.f, 4.f}, {0.f, 0.f, 0.f});
    return cam;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Golden-file helpers
// ─────────────────────────────────────────────────────────────────────────────

// Returns false if golden file is missing (first-run / bless needed).
static bool compareOrBless(const PixelBuffer& actual, const std::string& name) {
    const fs::path goldenPath = kFixtureDir / (name + ".ppm");
    const fs::path actualPath = kFixtureDir / (name + "_actual.ppm");

    // Always write the actual output for inspection.
    [[maybe_unused]] auto _ = writePPM(actualPath.string(), actual);

    if (!fs::exists(goldenPath)) {
        // Auto-bless on first run: copy actual as golden.
        [[maybe_unused]] auto _2 = writePPM(goldenPath.string(), actual);
        return true; // first-run bless always passes
    }

    PixelBuffer golden(1, 1);
    if (!readPPM(goldenPath.string(), golden)) return false;

    if (golden.width() != actual.width() || golden.height() != actual.height()) return false;
    return actual.fnv1aHash() == golden.fnv1aHash();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PixelBuffer unit tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(PixelBuffer, ClearFillsAllPixels) {
    PixelBuffer buf(16, 16);
    buf.clear({255, 0, 0, 255});
    for (const auto& px : buf.pixels())
        EXPECT_EQ(px.r, 255u);
}

TEST(PixelBuffer, SetGetPixelRoundTrip) {
    PixelBuffer buf(8, 8);
    buf.clear({0, 0, 0, 255});
    buf.setPixel(3, 5, {100, 150, 200, 255});
    const auto px = buf.getPixel(3, 5);
    EXPECT_EQ(px.r, 100u);
    EXPECT_EQ(px.g, 150u);
    EXPECT_EQ(px.b, 200u);
}

TEST(PixelBuffer, OutOfBoundsSetPixelIsIgnored) {
    PixelBuffer buf(4, 4);
    buf.clear({0, 0, 0, 255});
    buf.setPixel(4, 0, {255, 255, 255, 255}); // x == width, out of bounds
    buf.setPixel(0, 4, {255, 255, 255, 255}); // y == height, out of bounds
    for (const auto& px : buf.pixels())
        EXPECT_EQ(px.r, 0u);
}

TEST(PixelBuffer, DepthTestKeepsNearest) {
    PixelBuffer buf(4, 4);
    buf.clear();
    buf.setPixelDepth(2, 2, {255, 0, 0, 255}, 0.5f);
    buf.setPixelDepth(2, 2, {0, 255, 0, 255}, 0.3f); // nearer — should win
    buf.setPixelDepth(2, 2, {0, 0, 255, 255}, 0.8f); // farther — should lose
    EXPECT_EQ(buf.getPixel(2, 2).g, 255u); // green pixel won
}

TEST(PixelBuffer, HashChangesWhenPixelChanges) {
    PixelBuffer a(8, 8), b(8, 8);
    a.clear({0, 0, 0, 255});
    b.clear({0, 0, 0, 255});
    EXPECT_EQ(a.fnv1aHash(), b.fnv1aHash());

    b.setPixel(3, 3, {1, 2, 3, 255});
    EXPECT_NE(a.fnv1aHash(), b.fnv1aHash());
}

// ─────────────────────────────────────────────────────────────────────────────
//  ImageWriter unit tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(ImageWriter, PPMRoundTrip) {
    PixelBuffer orig(8, 8);
    orig.clear({0, 0, 0, 255});
    orig.setPixel(0, 0, {10, 20, 30, 255});
    orig.setPixel(7, 7, {200, 150, 100, 255});

    const std::string path = (kFixtureDir / "test_ppm_roundtrip.ppm").string();
    ASSERT_TRUE(writePPM(path, orig));

    PixelBuffer loaded(1, 1);
    ASSERT_TRUE(readPPM(path, loaded));

    EXPECT_EQ(loaded.width(),  orig.width());
    EXPECT_EQ(loaded.height(), orig.height());
    EXPECT_EQ(loaded.fnv1aHash(), orig.fnv1aHash());

    fs::remove(path);
}

TEST(ImageWriter, TGAWriteSucceeds) {
    PixelBuffer buf(4, 4);
    buf.clear({128, 64, 32, 255});
    const std::string path = (kFixtureDir / "test_tga_write.tga").string();
    EXPECT_TRUE(writeTGA(path, buf));
    EXPECT_TRUE(fs::exists(path));
    fs::remove(path);
}

TEST(ImageWriter, ReadPPMFailsForMissingFile) {
    PixelBuffer dummy(1, 1);
    EXPECT_FALSE(readPPM("/nonexistent/path/to/file.ppm", dummy));
}

// ─────────────────────────────────────────────────────────────────────────────
//  SoftwareRasterizer smoke tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(SoftwareRasterizer, EmptyMeshProducesOnlyBackground) {
    constexpr uint32_t W = 64, H = 64;
    Mesh empty;
    Camera cam = makeCamera(W, H);
    PixelBuffer buf(W, H);
    RasterizerConfig cfg;
    cfg.background = {10, 20, 30, 255};

    SoftwareRasterizer sr;
    sr.render(buf, empty, cam, cfg);

    for (const auto& px : buf.pixels()) {
        EXPECT_EQ(px.r, 10u);
        EXPECT_EQ(px.g, 20u);
        EXPECT_EQ(px.b, 30u);
    }
}

TEST(SoftwareRasterizer, TriangleFlatSomePixelsAreNotBackground) {
    constexpr uint32_t W = 128, H = 128;
    Mesh tri = makeTriangle();
    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 2.f}, {0.f, 0.f, 0.f});

    PixelBuffer buf(W, H);
    RasterizerConfig cfg;
    cfg.background = {0, 0, 0, 255};
    cfg.baseColor  = {180, 180, 180, 255};

    SoftwareRasterizer sr;
    sr.render(buf, tri, cam, cfg);

    uint32_t nonBg = 0;
    for (const auto& px : buf.pixels())
        if (px.r != 0 || px.g != 0 || px.b != 0) ++nonBg;

    EXPECT_GT(nonBg, 50u) << "expected more than 50 non-background pixels from a triangle";
}

TEST(SoftwareRasterizer, BoxFlatSomePixelsAreNotBackground) {
    constexpr uint32_t W = 256, H = 256;
    Mesh box = makeBox();
    Camera cam = makeCamera(W, H);

    PixelBuffer buf(W, H);
    RasterizerConfig cfg;
    cfg.background = {30, 30, 30, 255};

    SoftwareRasterizer sr;
    sr.render(buf, box, cam, cfg);

    uint32_t nonBg = 0;
    for (const auto& px : buf.pixels())
        if (px.r != 30 || px.g != 30 || px.b != 30) ++nonBg;

    EXPECT_GT(nonBg, 1000u) << "expected the box to cover substantial screen area";
}

TEST(SoftwareRasterizer, BoxWireframeSomePixelsAreNotBackground) {
    constexpr uint32_t W = 256, H = 256;
    Mesh box = makeBox();
    Camera cam = makeCamera(W, H);

    PixelBuffer buf(W, H);
    RasterizerConfig cfg;
    cfg.mode       = ShadingMode::Wireframe;
    cfg.background = {0, 0, 0, 255};
    cfg.wireColor  = {255, 255, 255, 255};

    SoftwareRasterizer sr;
    sr.render(buf, box, cam, cfg);

    uint32_t nonBg = 0;
    for (const auto& px : buf.pixels())
        if (px.r != 0 || px.g != 0 || px.b != 0) ++nonBg;

    EXPECT_GT(nonBg, 50u) << "expected wireframe edges to be visible";
}

TEST(SoftwareRasterizer, RenderIsDeterministic) {
    constexpr uint32_t W = 128, H = 128;
    Mesh box = makeBox();
    Camera cam = makeCamera(W, H);

    PixelBuffer buf1(W, H), buf2(W, H);
    SoftwareRasterizer sr;
    sr.render(buf1, box, cam);
    sr.render(buf2, box, cam);

    EXPECT_EQ(buf1.fnv1aHash(), buf2.fnv1aHash());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Golden-file visual regression tests
// ─────────────────────────────────────────────────────────────────────────────

TEST(SoftwareRasterizerGolden, BoxFlatMatchesGolden) {
    constexpr uint32_t W = 256, H = 256;
    Mesh box = makeBox();
    Camera cam = makeCamera(W, H);

    PixelBuffer buf(W, H);
    SoftwareRasterizer sr;
    sr.render(buf, box, cam);

    EXPECT_TRUE(compareOrBless(buf, "box_flat"))
        << "box_flat.ppm hash mismatch — run with bless to regenerate";
}

TEST(SoftwareRasterizerGolden, BoxWireframeMatchesGolden) {
    constexpr uint32_t W = 256, H = 256;
    Mesh box = makeBox();
    Camera cam = makeCamera(W, H);

    PixelBuffer buf(W, H);
    RasterizerConfig cfg;
    cfg.mode       = ShadingMode::Wireframe;
    cfg.background = {0, 0, 0, 255};
    cfg.wireColor  = {255, 255, 255, 255};

    SoftwareRasterizer sr;
    sr.render(buf, box, cam, cfg);

    EXPECT_TRUE(compareOrBless(buf, "box_wireframe"))
        << "box_wireframe.ppm hash mismatch — run with bless to regenerate";
}

TEST(SoftwareRasterizerGolden, TriangleFlatMatchesGolden) {
    constexpr uint32_t W = 256, H = 256;
    Mesh tri = makeTriangle();

    Camera cam;
    cam.setPerspective(60.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 2.f}, {0.f, 0.f, 0.f});

    PixelBuffer buf(W, H);
    SoftwareRasterizer sr;
    sr.render(buf, tri, cam);

    EXPECT_TRUE(compareOrBless(buf, "triangle_flat"))
        << "triangle_flat.ppm hash mismatch — run with bless to regenerate";
}

// ── Texture2D unit tests ──────────────────────────────────────────────────────

#include <softrast/Texture2D.h>

using nexus::softrast::Texture2D;
using nexus::softrast::TextureFilter;
using nexus::softrast::RGBA8;

TEST(Texture2D, CheckerboardFactoryProducesCorrectColors) {
    // 8x8 checkerboard with 4-pixel tiles: corners (0,0) and (4,4) should match colorA
    auto tex = Texture2D::makeCheckerboard(8, 4,
        RGBA8{255, 255, 255, 255},
        RGBA8{0,   0,   0,   255});
    EXPECT_EQ(tex.getPixel(0, 0), (RGBA8{255, 255, 255, 255}));
    EXPECT_EQ(tex.getPixel(4, 0), (RGBA8{0,   0,   0,   255}));
    EXPECT_EQ(tex.getPixel(0, 4), (RGBA8{0,   0,   0,   255}));
    EXPECT_EQ(tex.getPixel(4, 4), (RGBA8{255, 255, 255, 255}));
}

TEST(Texture2D, UVGradientCorners) {
    auto tex = Texture2D::makeUVGradient(256);
    // Top-left (0,0): r=0, g=0
    const auto tl = tex.getPixel(0, 0);
    EXPECT_EQ(tl.r, 0u);
    EXPECT_EQ(tl.g, 0u);
    // Bottom-right (255,255): r≈255, g≈255
    const auto br = tex.getPixel(255, 255);
    EXPECT_EQ(br.r, 255u);
    EXPECT_EQ(br.g, 255u);
}

TEST(Texture2D, SampleNearestWrapRepeats) {
    // A 2x2 texture: TL=red, TR=green, BL=blue, BR=white
    Texture2D tex(2, 2);
    tex.setPixel(0, 0, RGBA8{255, 0, 0, 255});
    tex.setPixel(1, 0, RGBA8{0, 255, 0, 255});
    tex.setPixel(0, 1, RGBA8{0, 0, 255, 255});
    tex.setPixel(1, 1, RGBA8{255, 255, 255, 255});

    // UV (0,0) → TL; UV (1.0,0) wraps → TL again
    const auto s00 = tex.sample(0.f, 0.f, TextureFilter::Nearest);
    EXPECT_EQ(s00, (RGBA8{255, 0, 0, 255}));

    const auto sWrap = tex.sample(1.0f, 0.f, TextureFilter::Nearest);
    EXPECT_EQ(sWrap, (RGBA8{255, 0, 0, 255}));
}

TEST(Texture2D, SampleBilinearMidpointBlends) {
    // Uniform white texture: bilinear sample at any UV should be white
    Texture2D tex(4, 4);
    for (uint32_t y = 0; y < 4; ++y)
        for (uint32_t x = 0; x < 4; ++x)
            tex.setPixel(x, y, RGBA8{200, 100, 50, 255});

    const auto s = tex.sample(0.5f, 0.5f, TextureFilter::Bilinear);
    EXPECT_EQ(s.r, 200u);
    EXPECT_EQ(s.g, 100u);
    EXPECT_EQ(s.b, 50u);
}

TEST(Texture2D, EmptyTextureReturnsWhite) {
    Texture2D tex;
    EXPECT_TRUE(tex.empty());
    const auto s = tex.sample(0.5f, 0.5f);
    EXPECT_EQ(s, (RGBA8{255, 255, 255, 255}));
}

// ── Textured rendering integration tests ─────────────────────────────────────

TEST(SoftwareRasterizer, TexturedSphereProducesNonBgPixels) {
    // UV sphere has UVs; checkerboard texture should produce visible pixels
    const Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 16u, 16u);
    ASSERT_TRUE(sphere.attributes().hasUVs());

    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    nexus::softrast::RasterizerConfig cfg;
    cfg.mode    = nexus::softrast::ShadingMode::Flat;
    cfg.texture = nexus::softrast::Texture2D::makeCheckerboard(64);

    nexus::softrast::PixelBuffer buf(128, 128);
    nexus::softrast::SoftwareRasterizer sr;
    sr.render(buf, sphere, cam, cfg);

    uint32_t nonBg = 0;
    const auto& bg = cfg.background;
    for (const auto& px : buf.pixels()) {
        if (px.r != bg.r || px.g != bg.g || px.b != bg.b) ++nonBg;
    }
    EXPECT_GT(nonBg, 500u) << "textured sphere rendered too few pixels";
}

TEST(SoftwareRasterizer, TexturedCheckerDiffersFromUntextured) {
    // Textured render should produce different output than untextured
    const Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 16u, 16u);
    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    nexus::softrast::PixelBuffer bufTex(128, 128), bufPlain(128, 128);
    nexus::softrast::SoftwareRasterizer sr;

    nexus::softrast::RasterizerConfig cfgTex;
    cfgTex.texture = nexus::softrast::Texture2D::makeCheckerboard(32);
    sr.render(bufTex, sphere, cam, cfgTex);

    nexus::softrast::RasterizerConfig cfgPlain;
    sr.render(bufPlain, sphere, cam, cfgPlain);

    EXPECT_NE(bufTex.fnv1aHash(), bufPlain.fnv1aHash())
        << "Textured render should produce different hash than untextured";
}

TEST(SoftwareRasterizer, GouraudTexturedSphereProducesPixels) {
    const Mesh sphere = nexus::geometry::primitives::makeSphere(1.f, 16u, 16u);
    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({0.f, 0.f, 3.f}, {0.f, 0.f, 0.f});

    nexus::softrast::RasterizerConfig cfg;
    cfg.mode    = nexus::softrast::ShadingMode::Gouraud;
    cfg.texture = nexus::softrast::Texture2D::makeUVGradient(64);

    nexus::softrast::PixelBuffer buf(128, 128);
    nexus::softrast::SoftwareRasterizer sr;
    sr.render(buf, sphere, cam, cfg);

    uint32_t nonBg = 0;
    const auto& bg = cfg.background;
    for (const auto& px : buf.pixels()) {
        if (px.r != bg.r || px.g != bg.g || px.b != bg.b) ++nonBg;
    }
    EXPECT_GT(nonBg, 500u);
}

TEST(SoftwareRasterizer, TextureFallsBackOnMeshWithNoUVs) {
    // Box has no UVs by default — texture should be ignored, render should still work
    const Mesh box = nexus::geometry::primitives::makeBox(1.f, 1.f, 1.f);
    // makebox may or may not have UVs; either way render must not crash
    nexus::render::Camera cam;
    cam.setPerspective(45.f, 1.f, 0.1f, 100.f);
    cam.lookAt({2.f, 2.f, 4.f}, {0.f, 0.f, 0.f});

    nexus::softrast::RasterizerConfig cfg;
    cfg.texture = nexus::softrast::Texture2D::makeCheckerboard(32);

    nexus::softrast::PixelBuffer buf(128, 128);
    nexus::softrast::SoftwareRasterizer sr;
    EXPECT_NO_THROW(sr.render(buf, box, cam, cfg));
}
