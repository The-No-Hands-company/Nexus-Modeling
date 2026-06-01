#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/neural/NeuralRenderer.h>
#include <nexus/render/Camera.h>
#include <nexus/render/FrameTimingLayer.h>
#include <nexus/render/GaussianSplatPass.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

// Internal Vulkan headers — only included when the Vulkan backend is compiled in.
#ifdef NEXUS_BACKEND_VULKAN
#include "../../src/kernel/src/backend/vulkan/VulkanDevice.h"
#include "../../src/kernel/src/backend/vulkan/VulkanSwapchain.h"
#include "../../src/kernel/src/backend/vulkan/VulkanFrameScheduler.h"
#endif

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

using namespace nexus::gfx;
using namespace nexus::geometry;
using namespace nexus::render;

namespace {

struct Args {
    uint32_t frames = 120;
    uint32_t determinismRuns = 1;
    std::string outputPath;
    Backend backend = Backend::Null;
    bool enableRT = false;
    nexus::neural::NeuralBackend neuralBackend = nexus::neural::NeuralBackend::Bilinear;
    bool attachNeural = false;
    double gpuCeilingMs = 0.0;           // 0 = no ceiling check
    double neuralOverheadCeilingMs = 0.0; // 0 = no overhead check
};

static const char* backendName(Backend b)
{
    switch (b) {
        case Backend::Null:   return "null";
        case Backend::Vulkan: return "vulkan";
        default:              return "unknown";
    }
}

static nexus::neural::NeuralBackend parseNeuralBackend(std::string_view s)
{
    if (s == "dlss"  || s == "dlss4")      return nexus::neural::NeuralBackend::DLSS4;
    if (s == "dlss-rr" || s == "dlss_rr") return nexus::neural::NeuralBackend::DLSS_RR;
    if (s == "xess")                       return nexus::neural::NeuralBackend::XeSS;
    if (s == "oidn"  || s == "oidn_cpu")   return nexus::neural::NeuralBackend::OIDN_CPU;
    if (s == "fsr3"  || s == "fsr")        return nexus::neural::NeuralBackend::FSR3;
    if (s == "fsr4")                       return nexus::neural::NeuralBackend::FSR4;
    if (s == "bilinear")                   return nexus::neural::NeuralBackend::Bilinear;
    return nexus::neural::NeuralBackend::Auto;
}

Args parseArgs(int argc, char** argv)
{
    Args args{};
    for (int i = 1; i < argc; ++i) {
        const std::string_view token = argv[i];
        if (token == "--frames" && i + 1 < argc) {
            args.frames = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (token == "--determinism-runs" && i + 1 < argc) {
            args.determinismRuns = static_cast<uint32_t>(std::stoul(argv[++i]));
            if (args.determinismRuns == 0) args.determinismRuns = 1;
        } else if (token == "--output" && i + 1 < argc) {
            args.outputPath = argv[++i];
        } else if (token == "--backend" && i + 1 < argc) {
            const std::string_view val = argv[++i];
            if (val == "vulkan") args.backend = Backend::Vulkan;
            else                  args.backend = Backend::Null;
        } else if (token == "--rt") {
            args.enableRT = true;
        } else if (token == "--neural-backend" && i + 1 < argc) {
            args.neuralBackend = parseNeuralBackend(argv[++i]);
            args.attachNeural = true;
        } else if (token == "--gpu-ceiling-ms" && i + 1 < argc) {
            args.gpuCeilingMs = std::stod(argv[++i]);
        } else if (token == "--neural-overhead-ceiling-ms" && i + 1 < argc) {
            args.neuralOverheadCeilingMs = std::stod(argv[++i]);
        }
    }
    return args;
}

struct ScenarioResult {
    bool valid = false;
    bool skipped = false;
    std::string skipReason;
    std::string error;
    double totalMs = 0.0;
    double averageMs = 0.0;
    double medianMs = 0.0;
    uint32_t drawCalls = 0;
    uint32_t splatDrawCalls = 0;
    double gpuTimeMs = 0.0;
    double cpuCullTimeMs = 0.0;
    bool rtReflectionsActive = false;
    bool upscalingActive = false;
    bool denoisingActive = false;
};

// Options forwarded into a single scenario run.
struct RunOptions {
    uint32_t frames      = 120;
    Backend  backend     = Backend::Null;
    bool     enableRT    = false;
    bool     attachNeural = false;
    nexus::neural::NeuralBackend neuralBackend = nexus::neural::NeuralBackend::Bilinear;
};

ScenarioResult runScenario(const RunOptions& opts)
{
    ScenarioResult result{};

    RenderContextDesc desc{};
    desc.preferredBackend = opts.backend;
    desc.validation = ValidationLevel::Off;
    desc.enableRayTracing = opts.enableRT;

    std::unique_ptr<RenderContext> ctx;
    try {
        ctx = RenderContext::create(desc);
    } catch (const std::exception& e) {
        if (opts.backend == Backend::Vulkan) {
            result.skipped = true;
            result.skipReason = std::string("Vulkan unavailable: ") + e.what();
            return result;
        }
        result.error = std::string("failed to create render context: ") + e.what();
        return result;
    }

    if (!ctx) {
        if (opts.backend == Backend::Vulkan) {
            result.skipped = true;
            result.skipReason = "Vulkan context returned null";
            return result;
        }
        result.error = "failed to create render context for backend=" + std::string(backendName(opts.backend));
        return result;
    }

    SwapchainDesc sd{};
    sd.extent = {1280, 720};
    auto sc = ctx->createSwapchain(sd);
    if (!sc) {
        result.error = "failed to create swapchain";
        return result;
    }

    Renderer renderer(*ctx, *sc);

    // On the Vulkan backend the renderer drives frames through a frame scheduler;
    // without one the headless path stalls waiting on swapchain semaphores.
#ifdef NEXUS_BACKEND_VULKAN
    std::unique_ptr<VulkanFrameScheduler> vkScheduler;
    VulkanDevice* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    VulkanSwapchain* vkSc = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (opts.backend == Backend::Vulkan) {
        if (!vkDev || !vkSc) {
            result.error = "vulkan backend selected but concrete device/swapchain unavailable";
            return result;
        }
        vkScheduler = std::make_unique<VulkanFrameScheduler>(*vkDev, *vkSc, 1);
        renderer.setFrameScheduler(vkScheduler.get());
    }
#else
    if (opts.backend == Backend::Vulkan) {
        result.skipped = true;
        result.skipReason = "vulkan backend not compiled in";
        return result;
    }
    void* vkDev = nullptr;
#endif

    PipelineHandle geomPipe{};
    geomPipe.id = 1001;
    PipelineHandle lightPipe{};
    lightPipe.id = 1002;
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

    // Apply RT settings when requested. The RT dispatch fires only when all four
    // conditions hold (enableRTReflect + mode + caps + valid pipeline). We enable
    // the settings here; a real RT pipeline would be set by the caller if needed.
    if (opts.enableRT) {
        RendererSettings settings{};
        settings.enableRTReflect = true;
        settings.mode = RenderMode::HybridRT;
        renderer.applySettings(settings);
    }

    // Attach neural renderer when requested.
    std::unique_ptr<nexus::neural::INeuralRenderer> neuralRenderer;
    if (opts.attachNeural) {
        neuralRenderer = nexus::neural::NeuralRendererFactory::create(
            opts.neuralBackend, ctx->device());
        renderer.setNeuralRenderer(neuralRenderer.get());

        RendererSettings settings{};
        settings.enableUpscaling = true;
        settings.enableDenoising = true;
        if (opts.enableRT) {
            settings.enableRTReflect = true;
            settings.mode = RenderMode::HybridRT;
        }
        renderer.applySettings(settings);
    }

    auto& device = ctx->device();
    SamplerHandle sampler = device.createSampler({});
    if (!sampler.valid()) {
        result.error = "failed to create sampler";
        return result;
    }

    BufferDesc materialTableDesc{};
    materialTableDesc.sizeBytes = 64;
    materialTableDesc.usage = BufferUsage::StorageBuffer;
    materialTableDesc.memory = MemoryHint::GpuOnly;
    materialTableDesc.debugName = "perf.smoke.materialTable";
    BufferHandle materialTable = device.createBuffer(materialTableDesc);
    if (!materialTable.valid()) {
        device.destroySampler(sampler);
        result.error = "failed to create material table";
        return result;
    }

    CompositeMaterialBindings bindings{};
    bindings.albedoMaterialSampler = sampler;
    bindings.normalSampler = sampler;
    bindings.velocitySampler = sampler;
    bindings.depthSampler = sampler;
    bindings.materialTable = materialTable;
    renderer.setCompositeMaterialBindings(bindings);

    Mesh mesh = primitives::makeTriangle(1.f);
    if (!mesh.computeVertexNormals()) {
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        result.error = "failed to compute vertex normals";
        return result;
    }
    mesh.attributes().setUVs({
        {0.f, 0.f},
        {1.f, 0.f},
        {0.f, 1.f},
    });

    const auto indices = MeshUploadContract::buildTriangulatedIndexBuffer(mesh);
    const auto sections = MeshUploadContract::makeSingleSection(indices, kInvalidMaterial);
    const MeshUploadView view = MeshUploadContract::buildView(mesh, indices, sections);
    const PackedVertexLayout layout = MeshUploadContract::buildPackedVertexLayout(view);

    UploadedGeometryMesh uploaded{};
    const auto uploadReport = GeometryRenderBridge::uploadToDevice(device, view, layout, sections, uploaded);
    if (!uploadReport.valid) {
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        result.error = "failed to upload geometry";
        return result;
    }

    SceneGraph scene;
    Node* node = scene.createNode("perf-smoke-triangle");
    if (!node) {
        GeometryRenderBridge::destroyUploadedMesh(device, uploaded);
        device.destroyBuffer(materialTable);
        device.destroySampler(sampler);
        result.error = "failed to create scene node";
        return result;
    }
    GeometryRenderBridge::assignUploadedMeshToNode(uploaded, *node);
    node->localTransform().translation = {0.f, 0.f, -5.f};

    Camera cam;
    cam.setPerspective(70.f, 1280.f / 720.f, 0.1f, 1000.f);
    cam.lookAt({0.f, 0.f, 5.f}, {0.f, 0.f, 0.f});

    nexus::gfx::GaussianSplatCloud splatCloud{};
    for (int i = 0; i < 8; ++i) {
        nexus::gfx::GaussianSplat s{};
        s.position = { static_cast<float>(i) * 0.25f, 0.f, -3.f };
        s.opacity  = 0.f;
        splatCloud.splats.push_back(s);
    }
    nexus::gfx::GaussianSplatSceneNode splatNode{ std::move(splatCloud) };
    nexus::render::GaussianSplatPass splatPass;
    splatPass.addCloud(&splatNode);
    renderer.setGaussianSplatPass(&splatPass);

    (void)renderer.enableFrameTiming(2);

    std::vector<double> frameTimesMs;
    frameTimesMs.reserve(opts.frames);
    for (uint32_t frame = 0; frame < opts.frames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();
#ifdef NEXUS_BACKEND_VULKAN
        if (vkDev) vkDev->waitIdle();
#endif
        const auto end = std::chrono::steady_clock::now();
        frameTimesMs.push_back(
            std::chrono::duration<double, std::milli>(end - start).count());
    }

    std::sort(frameTimesMs.begin(), frameTimesMs.end());
    result.totalMs   = std::accumulate(frameTimesMs.begin(), frameTimesMs.end(), 0.0);
    result.averageMs = frameTimesMs.empty() ? 0.0
                     : result.totalMs / static_cast<double>(frameTimesMs.size());
    result.medianMs  = frameTimesMs.empty() ? 0.0
                     : frameTimesMs[frameTimesMs.size() / 2];

    const auto& lastStats = renderer.lastFrameStats();
    result.drawCalls          = lastStats.drawCalls;
    result.splatDrawCalls     = lastStats.splatDrawCalls;
    result.gpuTimeMs          = renderer.lastFrameTimingResult().totalGpuMs;
    result.cpuCullTimeMs      = lastStats.cpuCullTimeMs;
    result.rtReflectionsActive = lastStats.rtReflectionsActive;
    result.upscalingActive    = lastStats.upscalingActive;
    result.denoisingActive    = lastStats.denoisingActive;

    GeometryRenderBridge::destroyNodeMeshPayload(device, *node);
#ifdef NEXUS_BACKEND_VULKAN
    if (vkDev) vkDev->waitIdle();
    vkScheduler.reset();
#endif
    renderer.setNeuralRenderer(nullptr);
    device.destroyBuffer(materialTable);
    device.destroySampler(sampler);
    result.valid = true;
    return result;
}

std::string buildReport(const Args& args,
                        uint32_t determinismRuns,
                        bool determinismConsistent,
                        const ScenarioResult& r,
                        double neuralOverheadMs,
                        const std::string& deviceName)
{
    std::string report;
    report += std::string("backend=") + backendName(args.backend) + "\n";
    if (!deviceName.empty()) report += "device_name=" + deviceName + "\n";
    report += "scenario=single_triangle_direct_path\n";
    report += "frames=" + std::to_string(args.frames) + "\n";
    report += "determinism_runs=" + std::to_string(determinismRuns) + "\n";
    report += "determinism_consistent=" + std::string(determinismConsistent ? "true" : "false") + "\n";
    report += "total_ms=" + std::to_string(r.totalMs) + "\n";
    report += "average_ms=" + std::to_string(r.averageMs) + "\n";
    report += "median_ms=" + std::to_string(r.medianMs) + "\n";
    report += "final_frame_draw_calls=" + std::to_string(r.drawCalls) + "\n";
    report += "splat_draw_calls=" + std::to_string(r.splatDrawCalls) + "\n";
    report += "gpu_time_ms=" + std::to_string(r.gpuTimeMs) + "\n";
    report += "cpu_cull_time_ms=" + std::to_string(r.cpuCullTimeMs) + "\n";
    report += "rt_reflections_active=" + std::string(r.rtReflectionsActive ? "true" : "false") + "\n";
    report += "upscaling_active=" + std::string(r.upscalingActive ? "true" : "false") + "\n";
    report += "denoising_active=" + std::string(r.denoisingActive ? "true" : "false") + "\n";
    if (args.neuralOverheadCeilingMs > 0.0)
        report += "neural_overhead_ms=" + std::to_string(neuralOverheadMs) + "\n";
    return report;
}

} // namespace

int main(int argc, char** argv)
{
    const Args args = parseArgs(argc, argv);

    // ── Primary scenario run ───────────────────────────────────────────────────
    RunOptions opts{};
    opts.frames       = args.frames;
    opts.backend      = args.backend;
    opts.enableRT     = args.enableRT;
    opts.attachNeural = args.attachNeural;
    opts.neuralBackend = args.neuralBackend;

    ScenarioResult baseline{};
    bool determinismConsistent = true;

    for (uint32_t run = 0; run < args.determinismRuns; ++run) {
        ScenarioResult current = runScenario(opts);

        if (current.skipped) {
            std::cout << "SKIPPED: " << current.skipReason << "\n";
            return 0;
        }
        if (!current.valid) {
            std::cerr << current.error << "\n";
            return 1;
        }

        if (run == 0) {
            baseline = current;
        } else {
            if (current.drawCalls != baseline.drawCalls)
                determinismConsistent = false;
        }
    }

    // ── Neural overhead measurement (two-phase) ────────────────────────────────
    double neuralOverheadMs = 0.0;
    if (args.neuralOverheadCeilingMs > 0.0) {
        // Run without neural to get baseline frame time.
        RunOptions baseOpts = opts;
        baseOpts.attachNeural = false;
        ScenarioResult baseResult = runScenario(baseOpts);
        if (baseResult.skipped) {
            std::cout << "SKIPPED: " << baseResult.skipReason << "\n";
            return 0;
        }
        if (!baseResult.valid) {
            std::cerr << "neural overhead baseline run failed: " << baseResult.error << "\n";
            return 1;
        }
        neuralOverheadMs = baseline.averageMs - baseResult.averageMs;
    }

    // ── Device name (best-effort for Vulkan) ───────────────────────────────────
    std::string deviceName;
#ifdef NEXUS_BACKEND_VULKAN
    if (args.backend == Backend::Vulkan) {
        RenderContextDesc probeDesc{};
        probeDesc.preferredBackend = Backend::Vulkan;
        probeDesc.validation = ValidationLevel::Off;
        try {
            if (auto probe = RenderContext::create(probeDesc))
                deviceName = std::string(probe->device().deviceName());
        } catch (...) {}
    }
#endif

    const std::string report = buildReport(args,
        args.determinismRuns, determinismConsistent,
        baseline, neuralOverheadMs, deviceName);

    std::cout << report;
    if (!args.outputPath.empty()) {
        std::ofstream out(args.outputPath, std::ios::trunc);
        out << report;
    }

    // ── Ceiling assertions ─────────────────────────────────────────────────────
    // GPU ceiling: only enforce when gpuTimeMs is non-zero (real GPU timing available).
    if (args.gpuCeilingMs > 0.0 && baseline.gpuTimeMs > 0.0) {
        if (baseline.gpuTimeMs > args.gpuCeilingMs) {
            std::cerr << "PERF GATE FAILED: gpu_time_ms=" << baseline.gpuTimeMs
                      << " exceeds ceiling=" << args.gpuCeilingMs << " ms\n";
            return 3;
        }
    }

    // Neural overhead ceiling: only enforce when baseline has meaningful timing.
    if (args.neuralOverheadCeilingMs > 0.0 && neuralOverheadMs > args.neuralOverheadCeilingMs) {
        std::cerr << "PERF GATE FAILED: neural_overhead_ms=" << neuralOverheadMs
                  << " exceeds ceiling=" << args.neuralOverheadCeilingMs << " ms\n";
        return 3;
    }

    return determinismConsistent ? 0 : 2;
}
