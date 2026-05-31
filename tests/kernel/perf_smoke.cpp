#include <nexus/gfx/GaussianSplatting.h>
#include <nexus/gfx/RenderContext.h>
#include <nexus/geometry/GeometryKernel.h>
#include <nexus/geometry/Mesh.h>
#include <nexus/render/Camera.h>
#include <nexus/render/FrameTimingLayer.h>
#include <nexus/render/GaussianSplatPass.h>
#include <nexus/render/Renderer.h>
#include <nexus/render/SceneGraph.h>

// Internal Vulkan headers required to drive the frame scheduler when --backend vulkan
// is selected. The headless renderer path needs an explicit IFrameScheduler.
#include "../../src/kernel/src/backend/vulkan/VulkanDevice.h"
#include "../../src/kernel/src/backend/vulkan/VulkanSwapchain.h"
#include "../../src/kernel/src/backend/vulkan/VulkanFrameScheduler.h"

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
};

static const char* backendName(Backend b)
{
    switch (b) {
        case Backend::Null:   return "null";
        case Backend::Vulkan: return "vulkan";
        default:              return "unknown";
    }
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
            if (args.determinismRuns == 0) {
                args.determinismRuns = 1;
            }
        } else if (token == "--output" && i + 1 < argc) {
            args.outputPath = argv[++i];
        } else if (token == "--backend" && i + 1 < argc) {
            const std::string_view val = argv[++i];
            if (val == "vulkan") args.backend = Backend::Vulkan;
            else                  args.backend = Backend::Null;
        }
    }
    return args;
}

struct ScenarioResult {
    bool valid = false;
    std::string error;
    double totalMs = 0.0;
    double averageMs = 0.0;
    double medianMs = 0.0;
    uint32_t drawCalls = 0;
    uint32_t splatDrawCalls = 0;
    double gpuTimeMs = 0.0;
    double cpuCullTimeMs = 0.0;
};

ScenarioResult runScenario(uint32_t frames, Backend backend)
{
    ScenarioResult result{};

    RenderContextDesc desc{};
    desc.preferredBackend = backend;
    desc.validation = ValidationLevel::Off;

    auto ctx = RenderContext::create(desc);
    if (!ctx) {
        result.error = std::string("failed to create render context for backend=") + backendName(backend);
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
    std::unique_ptr<VulkanFrameScheduler> vkScheduler;
    VulkanDevice* vkDev = dynamic_cast<VulkanDevice*>(&ctx->device());
    VulkanSwapchain* vkSc = dynamic_cast<VulkanSwapchain*>(sc.get());
    if (backend == Backend::Vulkan) {
        if (!vkDev || !vkSc) {
            result.error = "vulkan backend selected but concrete device/swapchain unavailable";
            return result;
        }
        vkScheduler = std::make_unique<VulkanFrameScheduler>(*vkDev, *vkSc, 1);
        renderer.setFrameScheduler(vkScheduler.get());
    }

    PipelineHandle geomPipe{};
    geomPipe.id = 1001;
    PipelineHandle lightPipe{};
    lightPipe.id = 1002;
    renderer.setFallbackGeometryPipeline(geomPipe);
    renderer.setLightingCompositePipeline(lightPipe);

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

    // Attach a splat cloud so perf_smoke reports splat_draw_calls.
    nexus::gfx::GaussianSplatCloud splatCloud{};
    for (int i = 0; i < 8; ++i) {
        nexus::gfx::GaussianSplat s{};
        s.position = { static_cast<float>(i) * 0.25f, 0.f, -3.f };
        s.opacity  = 0.f; // sigmoid(0) ≈ 0.5 — above opacity cutoff
        splatCloud.splats.push_back(s);
    }
    nexus::gfx::GaussianSplatSceneNode splatNode{ std::move(splatCloud) };
    nexus::render::GaussianSplatPass splatPass;
    splatPass.addCloud(&splatNode);
    renderer.setGaussianSplatPass(&splatPass);

    renderer.enableFrameTiming(2);

    std::vector<double> frameTimesMs;
    frameTimesMs.reserve(frames);
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const auto start = std::chrono::steady_clock::now();
        renderer.beginFrame();
        renderer.render(cam, scene);
        renderer.endFrame();
        if (vkDev) vkDev->waitIdle();
        const auto end = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
        frameTimesMs.push_back(elapsed);
    }

    std::sort(frameTimesMs.begin(), frameTimesMs.end());
    result.totalMs = std::accumulate(frameTimesMs.begin(), frameTimesMs.end(), 0.0);
    result.averageMs = frameTimesMs.empty() ? 0.0 : result.totalMs / static_cast<double>(frameTimesMs.size());
    result.medianMs = frameTimesMs.empty() ? 0.0 : frameTimesMs[frameTimesMs.size() / 2];
    result.drawCalls      = renderer.lastFrameStats().drawCalls;
    result.splatDrawCalls = renderer.lastFrameStats().splatDrawCalls;
    result.gpuTimeMs      = renderer.lastFrameTimingResult().totalGpuMs;
    result.cpuCullTimeMs  = renderer.lastFrameStats().cpuCullTimeMs;

    GeometryRenderBridge::destroyNodeMeshPayload(device, *node);
    if (vkDev) vkDev->waitIdle();
    vkScheduler.reset();
    device.destroyBuffer(materialTable);
    device.destroySampler(sampler);
    result.valid = true;
    return result;
}

std::string buildReport(uint32_t frames,
                        uint32_t determinismRuns,
                        bool determinismConsistent,
                        double totalMs,
                        double averageMs,
                        double medianMs,
                        uint32_t drawCalls,
                        uint32_t splatDrawCalls,
                        double gpuTimeMs,
                        double cpuCullTimeMs,
                        Backend backend,
                        const std::string& deviceName)
{
    std::string report;
    report += std::string("backend=") + backendName(backend) + "\n";
    if (!deviceName.empty()) report += "device_name=" + deviceName + "\n";
    report += "scenario=single_triangle_direct_path\n";
    report += "frames=" + std::to_string(frames) + "\n";
    report += "determinism_runs=" + std::to_string(determinismRuns) + "\n";
    report += "determinism_consistent=" + std::string(determinismConsistent ? "true" : "false") + "\n";
    report += "total_ms=" + std::to_string(totalMs) + "\n";
    report += "average_ms=" + std::to_string(averageMs) + "\n";
    report += "median_ms=" + std::to_string(medianMs) + "\n";
    report += "final_frame_draw_calls=" + std::to_string(drawCalls) + "\n";
    report += "splat_draw_calls=" + std::to_string(splatDrawCalls) + "\n";
    report += "gpu_time_ms=" + std::to_string(gpuTimeMs) + "\n";
    report += "cpu_cull_time_ms=" + std::to_string(cpuCullTimeMs) + "\n";
    return report;
}

} // namespace

int main(int argc, char** argv)
{
    const Args args = parseArgs(argc, argv);

    ScenarioResult baseline{};
    bool determinismConsistent = true;
    std::string deviceName;

    for (uint32_t run = 0; run < args.determinismRuns; ++run) {
        ScenarioResult current = runScenario(args.frames, args.backend);
        if (!current.valid) {
            std::cerr << current.error << "\n";
            return 1;
        }

        if (run == 0) {
            baseline = current;
        } else {
            if (current.drawCalls != baseline.drawCalls) {
                determinismConsistent = false;
            }
        }
    }

    // Best-effort device name capture for Vulkan baselines (Null backend reports empty).
    if (args.backend == Backend::Vulkan) {
        RenderContextDesc probeDesc{};
        probeDesc.preferredBackend = Backend::Vulkan;
        probeDesc.validation = ValidationLevel::Off;
        if (auto probe = RenderContext::create(probeDesc)) {
            deviceName = std::string(probe->device().deviceName());
        }
    }

    const std::string report = buildReport(
        args.frames,
        args.determinismRuns,
        determinismConsistent,
        baseline.totalMs,
        baseline.averageMs,
        baseline.medianMs,
        baseline.drawCalls,
        baseline.splatDrawCalls,
        baseline.gpuTimeMs,
        baseline.cpuCullTimeMs,
        args.backend,
        deviceName);

    std::cout << report;
    if (!args.outputPath.empty()) {
        std::ofstream out(args.outputPath, std::ios::trunc);
        out << report;
    }

    return determinismConsistent ? 0 : 2;
}