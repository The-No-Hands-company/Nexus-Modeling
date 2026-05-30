// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Viewer — interactive CPU-rasterized geometry preview
//
//  Usage:
//    nexus_viewer                  — opens an 800×600 window with built-in box
//    nexus_viewer --mesh path.obj  — load OBJ from disk
//    nexus_viewer --offscreen      — headless (SDL_VIDEODRIVER=offscreen, for CI)
//    nexus_viewer --frames <N>     — render N frames then exit (for CI)
//
//  Controls (windowed mode):
//    Arrow keys — orbit camera
//    W          — cycle shading: flat → gouraud → wireframe
//    S          — save screenshot as nexus_viewer_NNNN.ppm
//    Q / Escape — quit
//    (idle)     — camera auto-orbits slowly
// ─────────────────────────────────────────────────────────────────────────────
#include <softrast/ImageWriter.h>
#include <softrast/PixelBuffer.h>
#include <softrast/SoftwareRasterizer.h>

#include <nexus/geometry/Mesh.h>
#include <nexus/geometry/MeshIO.h>
#include <nexus/render/Camera.h>

#include <SDL2/SDL.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Scene setup helpers
// ─────────────────────────────────────────────────────────────────────────────

static nexus::geometry::Mesh makeBox() {
    nexus::geometry::Mesh m;
    m.attributes().setPositions({
        {-0.5f, -0.5f,  0.5f}, {0.5f, -0.5f,  0.5f},
        { 0.5f,  0.5f,  0.5f}, {-0.5f,  0.5f,  0.5f},
        {-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f},
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

// ─────────────────────────────────────────────────────────────────────────────
//  Orbit camera helper
// ─────────────────────────────────────────────────────────────────────────────

struct OrbitCamera {
    float azimuth   = 45.f;   // degrees, horizontal
    float elevation = 30.f;   // degrees, vertical
    float distance  = 4.f;
    float fovY      = 45.f;
    uint32_t width  = 800;
    uint32_t height = 600;

    [[nodiscard]] nexus::render::Camera build() const {
        const float az  = azimuth  * static_cast<float>(std::numbers::pi) / 180.f;
        const float el  = elevation * static_cast<float>(std::numbers::pi) / 180.f;
        const float cosEl = std::cos(el), sinEl = std::sin(el);
        nexus::render::Vec3 eye{
            distance * cosEl * std::sin(az),
            distance * sinEl,
            distance * cosEl * std::cos(az),
        };
        nexus::render::Camera cam;
        cam.setPerspective(fovY, static_cast<float>(width) / static_cast<float>(height),
                           0.1f, 100.f);
        cam.lookAt(eye, {0.f, 0.f, 0.f});
        return cam;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
//  Main
// ─────────────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    bool offscreen = false;
    int  maxFrames = 0; // 0 = run until quit
    nexus::softrast::ShadingMode shadingMode = nexus::softrast::ShadingMode::Flat;
    std::string meshPath;
    int screenshotCounter = 0;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--offscreen") == 0) {
            offscreen = true;
        } else if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            maxFrames = std::atoi(argv[++i]);
        } else if (std::strcmp(argv[i], "--wireframe") == 0) {
            shadingMode = nexus::softrast::ShadingMode::Wireframe;
        } else if (std::strcmp(argv[i], "--gouraud") == 0) {
            shadingMode = nexus::softrast::ShadingMode::Gouraud;
        } else if (std::strcmp(argv[i], "--mesh") == 0 && i + 1 < argc) {
            meshPath = argv[++i];
        }
    }

    if (offscreen) {
        SDL_setenv("SDL_VIDEODRIVER", "offscreen", /*overwrite=*/1);
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init error: %s", SDL_GetError());
        return 1;
    }

    constexpr uint32_t W = 800, H = 600;
    SDL_Window* window = SDL_CreateWindow(
        "Nexus Viewer",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        static_cast<int>(W), static_cast<int>(H),
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        SDL_Log("SDL_CreateWindow error: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        // Fall back to software renderer
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        SDL_Log("SDL_CreateRenderer error: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        static_cast<int>(W), static_cast<int>(H));
    if (!texture) {
        SDL_Log("SDL_CreateTexture error: %s", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // Scene — load from disk or fall back to built-in box
    nexus::geometry::Mesh box;
    if (!meshPath.empty()) {
        nexus::geometry::MeshImportOptions opts;
        auto report = nexus::geometry::MeshIO::importMesh(meshPath, box, opts);
        if (!report.valid) {
            SDL_Log("Failed to load mesh '%s': %s", meshPath.c_str(), report.messages.empty() ? "unknown error" : report.messages[0].c_str());
            SDL_DestroyTexture(texture);
            SDL_DestroyRenderer(renderer);
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        SDL_Log("Loaded mesh '%s': %zu vertices, %zu faces",
            meshPath.c_str(),
            box.attributes().positions().size(),
            box.topology().faceCount());
    } else {
        box = makeBox();
    }
    nexus::softrast::SoftwareRasterizer sr;
    nexus::softrast::PixelBuffer buf(W, H);
    OrbitCamera orbit;
    orbit.width = W; orbit.height = H;

    int frameCount = 0;
    bool running = true;
    bool userInteracted = false; // suppress auto-orbit once user presses a key
    uint32_t lastTick = SDL_GetTicks();
    float frameMs = 0.f;

    while (running) {
        // Event handling
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                userInteracted = true;
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE: case SDLK_q:
                    running = false;
                    break;
                case SDLK_LEFT:  orbit.azimuth -= 5.f; break;
                case SDLK_RIGHT: orbit.azimuth += 5.f; break;
                case SDLK_UP:    orbit.elevation = std::min(89.f, orbit.elevation + 5.f); break;
                case SDLK_DOWN:  orbit.elevation = std::max(-89.f, orbit.elevation - 5.f); break;
                case SDLK_w:
                    // Cycle: Flat → Gouraud → Wireframe → Flat
                    if (shadingMode == nexus::softrast::ShadingMode::Flat)
                        shadingMode = nexus::softrast::ShadingMode::Gouraud;
                    else if (shadingMode == nexus::softrast::ShadingMode::Gouraud)
                        shadingMode = nexus::softrast::ShadingMode::Wireframe;
                    else
                        shadingMode = nexus::softrast::ShadingMode::Flat;
                    break;
                case SDLK_s: {
                    char name[64] = {};
                    auto [ptr, ec] = std::to_chars(name, name+sizeof(name), screenshotCounter++);
                    std::string path = "nexus_viewer_";
                    path += std::string(name, ptr);
                    path += ".ppm";
                    if (nexus::softrast::writePPM(path, buf))
                        SDL_Log("Screenshot saved: %s", path.c_str());
                    else
                        SDL_Log("Screenshot failed: %s", path.c_str());
                    break;
                }
                default: break;
                }
            }
        }

        // Auto-orbit: slowly rotate when user hasn't touched the keyboard
        if (!userInteracted && !offscreen) {
            orbit.azimuth += 0.3f; // ~18°/s at 60 fps
        }

        // Render frame
        nexus::softrast::RasterizerConfig cfg;
        cfg.mode       = shadingMode;
        cfg.background = {20, 20, 30, 255};
        cfg.wireColor  = {220, 220, 180, 255};

        const auto cam = orbit.build();
        sr.render(buf, box, cam, cfg);

        // Upload pixels to texture (RGBA8888: R in highest byte)
        void* pixels = nullptr; int pitch = 0;
        if (SDL_LockTexture(texture, nullptr, &pixels, &pitch) == 0) {
            auto* dst = static_cast<uint8_t*>(pixels);
            for (uint32_t y = 0; y < H; ++y) {
                for (uint32_t x = 0; x < W; ++x) {
                    const auto& px = buf.getPixel(x, y);
                    const std::size_t off = static_cast<std::size_t>(y) * static_cast<std::size_t>(pitch)
                                         + static_cast<std::size_t>(x) * 4;
                    dst[off + 0] = px.r;
                    dst[off + 1] = px.g;
                    dst[off + 2] = px.b;
                    dst[off + 3] = 255;
                }
            }
            SDL_UnlockTexture(texture);
        }

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, nullptr, nullptr);
        SDL_RenderPresent(renderer);

        // Frame timing
        const uint32_t now = SDL_GetTicks();
        frameMs = static_cast<float>(now - lastTick);
        lastTick = now;

        // Update window title with mesh info + mode + frame time
        if (!offscreen) {
            const char* modeName = (shadingMode == nexus::softrast::ShadingMode::Gouraud) ? "Gouraud"
                                 : (shadingMode == nexus::softrast::ShadingMode::Wireframe) ? "Wireframe"
                                 : "Flat";
            const std::string meshLabel = meshPath.empty() ? "box"
                : meshPath.substr(meshPath.find_last_of("/\\") + 1);
            char title[256];
            std::snprintf(title, sizeof(title),
                "Nexus Viewer — %s  [%s]  %.1f ms/frame  (W=cycle  S=screenshot  Q=quit)",
                meshLabel.c_str(), modeName, frameMs);
            SDL_SetWindowTitle(window, title);
        }

        ++frameCount;
        if (maxFrames > 0 && frameCount >= maxFrames) running = false;

        // ~60 fps cap
        const uint32_t elapsed = SDL_GetTicks() - now;
        if (elapsed < 16) SDL_Delay(16 - elapsed);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
