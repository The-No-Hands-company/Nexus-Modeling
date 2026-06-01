// ─────────────────────────────────────────────────────────────────────────────
//  Renderer — frame orchestration
// ────────────────────────────────────────────────────────────────────────────────
#include <nexus/render/Renderer.h>
#include <nexus/gfx/CommandBuffer.h>
#include <nexus/render/RenderGraphValidator.h>
#include <nexus/render/FrameCaptureExporter.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <vector>
#include <optional>
#include <unordered_map>

namespace nexus::render {

struct Renderer::Impl {
    std::vector<nexus::gfx::CmdBufHandle>   cmdBufs;      // graphics[0], compute[1]
    nexus::gfx::FenceHandle                 frameFence;
    nexus::gfx::SemaphoreHandle             imageAvailable;
    nexus::gfx::SemaphoreHandle             renderFinished;
    nexus::gfx::IFrameScheduler*            scheduler     = nullptr;
    std::optional<nexus::gfx::FrameContext> currentFrame;
    uint32_t                                currentImageIndex = 0;
    struct GBuffer {
        nexus::gfx::TextureHandle albedoMaterial;
        nexus::gfx::TextureHandle normal;
        nexus::gfx::TextureHandle velocity;
        nexus::gfx::TextureHandle depth;
        nexus::gfx::Extent2D      extent{};
    } gbuffer;
    struct ShadowTargets {
        nexus::gfx::TextureHandle depthAtlas;
        nexus::gfx::Extent2D      atlasExtent{};
        nexus::gfx::Extent2D      cascadeExtent{};
        uint32_t                  cascadeCount = 0;
        uint32_t                  atlasColumns = 0;
        uint32_t                  atlasRows = 0;
        std::array<nexus::gfx::Rect2D, ShadowLightingContract::kMaxCascades> cascadeViewports{};
        std::array<uint32_t, ShadowLightingContract::kMaxCascades> cascadeDrawCalls{};
        std::array<uint32_t, ShadowLightingContract::kMaxCascades> cascadeTriangles{};
    } shadow;
    nexus::gfx::TextureLayout                albedoLayout   = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                normalLayout   = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                velocityLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                depthLayout    = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
    nexus::gfx::TextureLayout                swapchainLayout= nexus::gfx::TextureLayout::Present;
    nexus::gfx::PipelineHandle               fallbackGeometryPipeline;
    nexus::gfx::PipelineHandle               fallbackMeshPipeline;
    nexus::gfx::PipelineHandle               rayTracingPipeline;
    nexus::gfx::AccelStructHandle            sceneTLAS;             // non-owning; set by caller
    nexus::gfx::SBTHandle                    sbt;                   // non-owning; set by caller
    nexus::render::VolumetricSettings        volumetric;            // froxel pass configuration
    nexus::render::AOSettings                ao;                    // SSAO pass configuration
    nexus::render::SSRSettings               ssr;                   // SSR pass configuration
    nexus::render::BloomSettings             bloom;                 // bloom post-process configuration
    nexus::render::DoFSettings               dof;                   // depth-of-field configuration
    nexus::render::MotionBlurSettings        motionBlur;            // motion blur configuration
    nexus::render::ToneMappingSettings       toneMapping;           // tone mapping configuration
    nexus::render::IBLSettings               ibl;                   // image-based lighting configuration
    nexus::render::OITSettings               oit;                   // OIT resolve configuration
    nexus::render::SSSSettings               sss;                   // subsurface scattering configuration
    nexus::render::ContactShadowSettings     contactShadow;         // contact shadow configuration
    nexus::render::TiledLightingSettings     tiledLighting;         // tiled deferred lighting configuration
    nexus::render::ClusteredLightingSettings clusteredLighting;     // 3-D clustered lighting configuration
    nexus::render::SSGISettings              ssgi;                  // SSGI gather configuration
    nexus::render::DecalSettings             decals;                // decal projection configuration
    nexus::gfx::PipelineHandle               shadowPipeline;
    nexus::gfx::PipelineHandle               shadowMeshPipeline;
    nexus::gfx::PipelineHandle               lightingCompositePipeline;
    std::unordered_map<MaterialID, nexus::gfx::PipelineHandle> materialPipelines;
    std::unordered_map<MaterialID, nexus::gfx::PipelineHandle> materialMeshPipelines;
    std::vector<std::vector<nexus::gfx::DescriptorSetHandle>> deferredDescriptorSetsByFrame;
    CompositeMaterialBindings                compositeBindings;
    ShadowLightingContract                   shadowLightingContract{};
    nexus::gfx::BufferHandle                 shadowLightingBuffer;
    bool                                     shadowLightingDirty = false;
    uint32_t                                frameIndex    = 0;

    // Render graph validation
    bool                        renderGraphValidationEnabled = false;
    RenderGraphValidationReport lastRenderGraphReport;

    // Frame capture exporter
    IFrameCaptureExporter*      captureExporter = nullptr;

    // Gaussian splat pass (Month 13 first slice). Not owned; nullptr disables.
    GaussianSplatPass*          gaussianSplatPass = nullptr;
};

namespace {

struct SceneDrawPacket {
    nexus::gfx::BufferHandle vertexBuffer;
    nexus::gfx::BufferHandle indexBuffer;
    nexus::gfx::BufferHandle meshletBuffer;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
    uint32_t meshletCount = 0;
    Vec3 worldCenter{}; // world-space node center for light-space frustum culling
    MaterialID material = kInvalidMaterial;
    bool castShadow = true;
};

std::vector<SceneDrawPacket> buildSceneDrawPackets(std::span<Node* const> visible)
{
    std::vector<SceneDrawPacket> packets;
    packets.reserve(visible.size());

    for (Node* node : visible) {
        if (!node) {
            continue;
        }

        const auto& mesh = node->mesh;
        if (!mesh.vertexBuffer.valid() || !mesh.indexBuffer.valid() || mesh.indexCount == 0) {
            continue;
        }
        const Mat4 world = node->worldMatrix();
        const Vec3 worldCenter{world.m[0][3], world.m[1][3], world.m[2][3]};

        if (!node->sectionDrawPackets.empty()) {
            for (const auto& sec : node->sectionDrawPackets) {
                if (sec.indexCount == 0) {
                    continue;
                }
                const uint64_t begin = sec.firstIndex;
                const uint64_t end = static_cast<uint64_t>(sec.firstIndex) + sec.indexCount;
                if (begin >= mesh.indexCount || end > mesh.indexCount) {
                    continue;
                }

                SceneDrawPacket packet{};
                packet.vertexBuffer = mesh.vertexBuffer;
                packet.indexBuffer = mesh.indexBuffer;
                packet.firstIndex = sec.firstIndex;
                packet.indexCount = sec.indexCount;
                packet.meshletBuffer = mesh.meshletBuffer;
                packet.meshletCount = mesh.meshletCount;
                packet.worldCenter = worldCenter;
                packet.material = sec.material;
                packet.castShadow = node->castShadow;
                packets.push_back(packet);
            }
            continue;
        }

        SceneDrawPacket packet{};
        packet.vertexBuffer = mesh.vertexBuffer;
        packet.indexBuffer = mesh.indexBuffer;
        packet.firstIndex = 0;
        packet.indexCount = mesh.indexCount;
        packet.meshletBuffer = mesh.meshletBuffer;
        packet.meshletCount = mesh.meshletCount;
        packet.worldCenter = worldCenter;
        packet.material = node->material;
        packet.castShadow = node->castShadow;
        packets.push_back(packet);
    }

    return packets;
}

// submitShadowMeshPackets: per-packet selects mesh-task path when the packet
// carries meshlet data and shadowMeshPipeline is valid; falls back to indexed
// draw via shadowPipeline otherwise.  Either pipeline may be invalid — packets
// requiring an unavailable pipeline are silently skipped.
uint32_t submitShadowMeshPackets(nexus::gfx::ICommandBuffer& cmd,
                                  std::span<const SceneDrawPacket> packets,
                                  nexus::gfx::PipelineHandle shadowPipeline,
                                  nexus::gfx::PipelineHandle shadowMeshPipeline,
                                  bool useMeshShaderPath,
                                  uint32_t& outMeshlets,
                                  uint32_t& outMeshShaderDraws)
{
    uint32_t submitted = 0;
    outMeshlets = 0;
    outMeshShaderDraws = 0;
    nexus::gfx::PipelineHandle boundPipeline{};

    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) continue;

        const bool canUseMeshTasks = useMeshShaderPath
                                  && packet.meshletBuffer.valid()
                                  && packet.meshletCount > 0
                                  && shadowMeshPipeline.valid();
        const nexus::gfx::PipelineHandle activePipeline =
            canUseMeshTasks ? shadowMeshPipeline : shadowPipeline;
        if (!activePipeline.valid()) continue;

        if (!(activePipeline == boundPipeline)) {
            cmd.bindPipeline(activePipeline);
            boundPipeline = activePipeline;
        }
        cmd.bindVertexBuffer(packet.vertexBuffer, 0, 0);
        if (canUseMeshTasks) {
            cmd.drawMeshTasks(packet.meshletCount, 1, 1);
            outMeshlets += packet.meshletCount;
            ++outMeshShaderDraws;
        } else {
            cmd.bindIndexBuffer(packet.indexBuffer, 0, false);
            cmd.drawIndexed(packet.indexCount, 1, packet.firstIndex, 0, 0);
        }
        ++submitted;
    }
    return submitted;
}

uint32_t shadowTriangleCount(std::span<const SceneDrawPacket> packets)
{
    uint32_t triangles = 0;
    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) {
            continue;
        }
        triangles += packet.indexCount / 3;
    }
    return triangles;
}

// NDC slack applied to the point-based frustum test to conservatively include
// nodes whose mesh geometry extends slightly beyond the node's world center.
static constexpr float kCascadeFrustumMargin = 0.1f;

// Projects worldCenter into light clip space and tests inclusion within the
// cascade frustum, with a small margin for mesh-extent conservatism.
// Returns false for degenerate projections (w <= 0) and for points clearly
// outside the light NDC cube.
bool isNodeVisibleInCascadeFrustum(const Vec3& worldCenter, const Mat4& lightViewProj)
{
    const Vec4 clip = lightViewProj * Vec4{worldCenter.x, worldCenter.y, worldCenter.z, 1.0f};
    if (clip.w < 1e-6f) {
        return false;
    }
    const float rcpW = 1.0f / clip.w;
    const float ndcX = clip.x * rcpW;
    const float ndcY = clip.y * rcpW;
    const float ndcZ = clip.z * rcpW;
    const float m = kCascadeFrustumMargin;
    return ndcX >= -(1.0f + m) && ndcX <= (1.0f + m)
        && ndcY >= -(1.0f + m) && ndcY <= (1.0f + m)
        && ndcZ >= -m           && ndcZ <= (1.0f + m);
}

std::vector<SceneDrawPacket> buildCascadeShadowPackets(std::span<const SceneDrawPacket> packets,
                                                       uint32_t cascadeIndex,
                                                       const ShadowLightingContract& contract)
{
    std::vector<SceneDrawPacket> result;
    if (cascadeIndex >= contract.cascadeCount || cascadeIndex >= ShadowLightingContract::kMaxCascades) {
        return result;
    }

    const Mat4& lvp = contract.lightViewProj[cascadeIndex];
    result.reserve(packets.size());
    for (const auto& packet : packets) {
        if (!packet.castShadow || packet.indexCount == 0) {
            continue;
        }
        if (isNodeVisibleInCascadeFrustum(packet.worldCenter, lvp)) {
            result.push_back(packet);
        }
    }

    return result;
}

struct ShadowAtlasLayout {
    nexus::gfx::Extent2D atlasExtent{};
    uint32_t cascadeCount = 0;
    uint32_t atlasColumns = 0;
    uint32_t atlasRows = 0;
    std::array<nexus::gfx::Rect2D, ShadowLightingContract::kMaxCascades> cascadeViewports{};
};

ShadowAtlasLayout buildShadowAtlasLayout(nexus::gfx::Extent2D cascadeExtent,
                                         uint32_t cascadeCount)
{
    ShadowAtlasLayout layout{};
    if (cascadeExtent.width == 0 || cascadeExtent.height == 0 || cascadeCount == 0) {
        return layout;
    }

    const uint32_t clampedCount = cascadeCount <= ShadowLightingContract::kMaxCascades
        ? cascadeCount
        : ShadowLightingContract::kMaxCascades;
    const uint32_t columns = clampedCount > 2 ? 2u : clampedCount;
    const uint32_t rows = (clampedCount + columns - 1) / columns;

    layout.cascadeCount = clampedCount;
    layout.atlasColumns = columns;
    layout.atlasRows = rows;
    layout.atlasExtent = {
        cascadeExtent.width * columns,
        cascadeExtent.height * rows
    };

    for (uint32_t cascadeIndex = 0; cascadeIndex < clampedCount; ++cascadeIndex) {
        const uint32_t col = cascadeIndex % columns;
        const uint32_t row = cascadeIndex / columns;
        layout.cascadeViewports[cascadeIndex] = {
            {
                static_cast<int32_t>(col * cascadeExtent.width),
                static_cast<int32_t>(row * cascadeExtent.height)
            },
            cascadeExtent
        };
    }

    return layout;
}

uint32_t submitGeometryPackets(nexus::gfx::ICommandBuffer& cmd,
                               std::span<const SceneDrawPacket> packets,
                               nexus::gfx::PipelineHandle fallbackPipeline,
                               nexus::gfx::PipelineHandle fallbackMeshPipeline,
                               const std::unordered_map<MaterialID, nexus::gfx::PipelineHandle>& materialPipelines,
                               const std::unordered_map<MaterialID, nexus::gfx::PipelineHandle>& materialMeshPipelines,
                               bool useMeshShaderPath,
                               uint32_t& outTriangles,
                               uint32_t& outMeshlets,
                               uint32_t& outMeshShaderDraws)
{
    uint32_t submitted = 0;
    outTriangles = 0;
    outMeshlets = 0;
    outMeshShaderDraws = 0;
    nexus::gfx::PipelineHandle boundPipeline{};

    for (const auto& packet : packets) {
        if (packet.indexCount == 0) {
            continue;
        }

        nexus::gfx::PipelineHandle rasterPipeline = fallbackPipeline;
        if (packet.material != kInvalidMaterial) {
            const auto it = materialPipelines.find(packet.material);
            if (it != materialPipelines.end()) {
                rasterPipeline = it->second;
            }
        }

        nexus::gfx::PipelineHandle meshPipeline = fallbackMeshPipeline;
        if (packet.material != kInvalidMaterial) {
            const auto it = materialMeshPipelines.find(packet.material);
            if (it != materialMeshPipelines.end()) {
                meshPipeline = it->second;
            }
        }

        const bool canUseMeshTasks = useMeshShaderPath
                                  && packet.meshletBuffer.valid()
                                  && packet.meshletCount > 0
                                  && meshPipeline.valid();
        const nexus::gfx::PipelineHandle packetPipeline = canUseMeshTasks ? meshPipeline : rasterPipeline;
        if (!packetPipeline.valid()) {
            continue;
        }

        if (!(boundPipeline == packetPipeline)) {
            cmd.bindPipeline(packetPipeline);
            boundPipeline = packetPipeline;
        }

        cmd.bindVertexBuffer(packet.vertexBuffer, 0, 0);
        if (canUseMeshTasks) {
            cmd.drawMeshTasks(packet.meshletCount, 1, 1);
            outMeshlets += packet.meshletCount;
            ++outMeshShaderDraws;
        } else {
            cmd.bindIndexBuffer(packet.indexBuffer, 0, false);
            cmd.drawIndexed(packet.indexCount, 1, packet.firstIndex, 0, 0);
        }
        ++submitted;
        outTriangles += packet.indexCount / 3;
    }

    return submitted;
}

template <typename ImplT>
void invalidateCachedDrawState(ImplT& impl)
{
    impl.albedoLayout = nexus::gfx::TextureLayout::Undefined;
    impl.normalLayout = nexus::gfx::TextureLayout::Undefined;
    impl.velocityLayout = nexus::gfx::TextureLayout::Undefined;
    impl.depthLayout = nexus::gfx::TextureLayout::Undefined;
    impl.shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
    impl.swapchainLayout = nexus::gfx::TextureLayout::Present;
}

template <typename ImplT>
void deferDescriptorSetFree(ImplT& impl,
                            uint32_t frameSlot,
                            nexus::gfx::DescriptorSetHandle set)
{
    if (!set.valid()) {
        return;
    }
    if (frameSlot >= impl.deferredDescriptorSetsByFrame.size()) {
        impl.deferredDescriptorSetsByFrame.resize(static_cast<size_t>(frameSlot) + 1);
    }
    impl.deferredDescriptorSetsByFrame[frameSlot].push_back(set);
}

template <typename ImplT>
void releaseDeferredDescriptorSetsForFrame(ImplT& impl,
                                           nexus::gfx::IDevice& dev,
                                           uint32_t frameSlot)
{
    if (frameSlot >= impl.deferredDescriptorSetsByFrame.size()) {
        return;
    }

    auto& deferredSets = impl.deferredDescriptorSetsByFrame[frameSlot];
    for (const auto set : deferredSets) {
        dev.freeDescriptorSet(set);
    }
    deferredSets.clear();
}

template <typename ImplT>
void releaseAllDeferredDescriptorSets(ImplT& impl, nexus::gfx::IDevice& dev)
{
    for (uint32_t frameSlot = 0;
         frameSlot < static_cast<uint32_t>(impl.deferredDescriptorSetsByFrame.size());
         ++frameSlot) {
        releaseDeferredDescriptorSetsForFrame(impl, dev, frameSlot);
    }
}

template <typename ImplT>
void resetRendererSceneCaches(ImplT& impl,
                              nexus::gfx::IDevice& dev,
                              FrameStats& stats)
{
    releaseAllDeferredDescriptorSets(impl, dev);
    impl.materialPipelines.clear();
    impl.materialMeshPipelines.clear();
    impl.fallbackGeometryPipeline = {};
    impl.fallbackMeshPipeline = {};
    impl.shadowMeshPipeline = {};
    impl.compositeBindings = {};
    if (impl.shadowLightingBuffer.valid()) {
        dev.destroyBuffer(impl.shadowLightingBuffer);
        impl.shadowLightingBuffer = {};
    }
    impl.shadowLightingContract = {};
    impl.shadowLightingDirty = false;
    if (impl.shadow.depthAtlas.valid()) {
        dev.destroyTexture(impl.shadow.depthAtlas);
        impl.shadow = {};
    }
    impl.currentFrame.reset();
    stats = {};
    invalidateCachedDrawState(impl);
}

} // namespace

Renderer::Renderer(nexus::gfx::RenderContext& ctx, nexus::gfx::ISwapchain& swapchain)
    : m_ctx(ctx), m_swapchain(swapchain), m_impl(std::make_unique<Impl>())
{
    auto& dev = ctx.device();
    m_impl->cmdBufs.push_back(dev.allocateCommandBuffer(nexus::gfx::QueueType::Graphics));
    m_impl->cmdBufs.push_back(dev.allocateCommandBuffer(nexus::gfx::QueueType::Compute));
    m_impl->frameFence      = dev.createFence(false);
    m_impl->imageAvailable  = dev.createSemaphore();
    m_impl->renderFinished  = dev.createSemaphore();
    selectRenderPath();
}

void Renderer::setFrameScheduler(nexus::gfx::IFrameScheduler* scheduler) noexcept
{
    auto& dev = m_ctx.device();
    releaseAllDeferredDescriptorSets(*m_impl, dev);

    m_impl->scheduler = scheduler;
    m_impl->deferredDescriptorSetsByFrame.clear();

    if (!scheduler) {
        return;
    }

    m_impl->deferredDescriptorSetsByFrame.resize(scheduler->maxFramesInFlight());
}

Renderer::~Renderer()
{
    auto& dev = m_ctx.device();
    dev.waitIdle();
    releaseAllDeferredDescriptorSets(*m_impl, dev);
    if (m_impl->shadowLightingBuffer.valid()) {
        dev.destroyBuffer(m_impl->shadowLightingBuffer);
        m_impl->shadowLightingBuffer = {};
    }
    destroyShadowTargets();
    destroyGBuffer();
    m_compositeDescSet.destroy(dev);
    m_shadowDescSet.destroy(dev);
    m_frameTiming.destroy(dev);
    for (auto h : m_impl->cmdBufs) dev.freeCommandBuffer(h);
    dev.destroyFence    (m_impl->frameFence);
    dev.destroySemaphore(m_impl->imageAvailable);
    dev.destroySemaphore(m_impl->renderFinished);
}

void Renderer::uploadShadowLightingContract()
{
    if (!m_impl->shadowLightingDirty) return;

    auto& dev = m_ctx.device();
    if (!m_impl->shadowLightingBuffer.valid()) {
        nexus::gfx::BufferDesc bd{};
        bd.sizeBytes = sizeof(ShadowLightingContract);
        bd.usage = nexus::gfx::BufferUsage::StorageBuffer | nexus::gfx::BufferUsage::TransferDst;
        bd.memory = nexus::gfx::MemoryHint::GpuOnly;
        bd.debugName = "ShadowLighting.Contract";
        m_impl->shadowLightingBuffer = dev.createBuffer(bd);
    }

    if (m_impl->shadowLightingBuffer.valid()) {
        dev.uploadBuffer(m_impl->shadowLightingBuffer,
                         &m_impl->shadowLightingContract,
                         sizeof(ShadowLightingContract),
                         0);
        m_impl->shadowLightingDirty = false;
    }
}

void Renderer::destroyShadowTargets()
{
    auto& dev = m_ctx.device();
    if (m_impl->shadow.depthAtlas.valid()) {
        dev.destroyTexture(m_impl->shadow.depthAtlas);
    }
    m_impl->shadow = {};
    m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::ensureShadowTargets(nexus::gfx::Extent2D extent, uint32_t cascadeCount)
{
    if (extent.width == 0 || extent.height == 0 || cascadeCount == 0) return;

    const ShadowAtlasLayout atlasLayout = buildShadowAtlasLayout(extent, cascadeCount);
    if (atlasLayout.cascadeCount == 0) {
        return;
    }

    if (m_impl->shadow.depthAtlas.valid()
        && m_impl->shadow.atlasExtent.width == atlasLayout.atlasExtent.width
        && m_impl->shadow.atlasExtent.height == atlasLayout.atlasExtent.height
        && m_impl->shadow.cascadeExtent.width == extent.width
        && m_impl->shadow.cascadeExtent.height == extent.height
        && m_impl->shadow.cascadeCount == atlasLayout.cascadeCount) {
        return;
    }

    destroyShadowTargets();

    const nexus::gfx::TextureDesc shadowDepthDesc{
        { atlasLayout.atlasExtent.width, atlasLayout.atlasExtent.height, 1 },
        1,
        1,
        nexus::gfx::Format::D32_Float,
        nexus::gfx::TextureUsage::DepthAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "Shadow.DepthAtlas"
    };

    auto& dev = m_ctx.device();
    m_impl->shadow.depthAtlas = dev.createTexture(shadowDepthDesc);
    m_impl->shadow.atlasExtent = atlasLayout.atlasExtent;
    m_impl->shadow.cascadeExtent = extent;
    m_impl->shadow.cascadeCount = atlasLayout.cascadeCount;
    m_impl->shadow.atlasColumns = atlasLayout.atlasColumns;
    m_impl->shadow.atlasRows = atlasLayout.atlasRows;
    m_impl->shadow.cascadeViewports = atlasLayout.cascadeViewports;
    m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::destroyGBuffer()
{
    auto& dev = m_ctx.device();
    if (m_impl->gbuffer.albedoMaterial.valid()) dev.destroyTexture(m_impl->gbuffer.albedoMaterial);
    if (m_impl->gbuffer.normal.valid())         dev.destroyTexture(m_impl->gbuffer.normal);
    if (m_impl->gbuffer.velocity.valid())       dev.destroyTexture(m_impl->gbuffer.velocity);
    if (m_impl->gbuffer.depth.valid())          dev.destroyTexture(m_impl->gbuffer.depth);

    m_impl->gbuffer = {};
    invalidateCachedDrawState(*m_impl);
}

void Renderer::ensureGBuffer(nexus::gfx::Extent2D extent)
{
    if (extent.width == 0 || extent.height == 0) return;

    if (m_impl->gbuffer.albedoMaterial.valid()
        && m_impl->gbuffer.normal.valid()
        && m_impl->gbuffer.velocity.valid()
        && m_impl->gbuffer.depth.valid()
        && m_impl->gbuffer.extent.width == extent.width
        && m_impl->gbuffer.extent.height == extent.height) {
        return;
    }

    destroyGBuffer();

    auto& dev = m_ctx.device();
    const nexus::gfx::TextureDesc colorDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        nexus::gfx::Format::R16G16B16A16_Float,
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.AlbedoMaterial"
    };
    const nexus::gfx::TextureDesc normalDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        nexus::gfx::Format::R16G16B16A16_Float,
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Normal"
    };
    const nexus::gfx::TextureDesc velocityDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        nexus::gfx::Format::R16G16_Float,
        nexus::gfx::TextureUsage::ColorAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Velocity"
    };
    const nexus::gfx::TextureDesc depthDesc{
        { extent.width, extent.height, 1 },
        1,
        1,
        nexus::gfx::Format::D32_Float,
        nexus::gfx::TextureUsage::DepthAttachment | nexus::gfx::TextureUsage::Sampled,
        nexus::gfx::SampleCount::X1,
        nexus::gfx::MemoryHint::GpuOnly,
        false,
        false,
        "GBuffer.Depth"
    };

    m_impl->gbuffer.albedoMaterial = dev.createTexture(colorDesc);
    m_impl->gbuffer.normal         = dev.createTexture(normalDesc);
    m_impl->gbuffer.velocity       = dev.createTexture(velocityDesc);
    m_impl->gbuffer.depth          = dev.createTexture(depthDesc);
    m_impl->gbuffer.extent         = extent;
    m_impl->albedoLayout           = nexus::gfx::TextureLayout::Undefined;
    m_impl->normalLayout           = nexus::gfx::TextureLayout::Undefined;
    m_impl->velocityLayout         = nexus::gfx::TextureLayout::Undefined;
    m_impl->depthLayout            = nexus::gfx::TextureLayout::Undefined;
}

void Renderer::beginFrame()
{
    m_stats = {};

    if (m_impl->captureExporter) {
        m_impl->captureExporter->beginCapture(m_impl->frameIndex);
    }

    if (m_impl->scheduler) {
        // ── Frame scheduler path (VulkanFrameScheduler) ──────────────────────────────────
        m_impl->currentFrame = m_impl->scheduler->beginFrame();
        if (m_impl->currentFrame) {
            const uint32_t slot = m_impl->currentFrame->frameSlot;
            releaseDeferredDescriptorSetsForFrame(*m_impl, m_ctx.device(), slot);
            // Read back timing results written during the previous use of this slot.
            if (m_frameTiming.isReady()) {
                m_lastTimingResult = m_frameTiming.readback(m_ctx.device(), slot);
                m_stats.gpuTimeMs  = m_lastTimingResult.totalGpuMs;
            }
        }
    } else {
        // ── Direct swapchain path ────────────────────────────────────────────────────
        auto& dev = m_ctx.device();
        dev.waitForFence(m_impl->frameFence, UINT64_MAX);
        dev.resetFence  (m_impl->frameFence);
        // Acquire swapchain image BEFORE recording commands
        auto frame = m_swapchain.acquire();
        m_impl->currentImageIndex = frame.imageIndex;
        m_impl->currentFrame.reset();
    }
}

void Renderer::render(const Camera& camera, SceneGraph& scene)
{
    // TAA: advance accumulator and apply sub-pixel jitter to a local camera copy.
    // Culling uses the original (unjittered) frustum; GPU commands use the copy.
    Camera jitteredCamera = camera;
    if (m_settings.enableTAA) {
        m_taaAccumulator.advanceFrame();
        const auto [jx, jy] = m_taaAccumulator.currentJitterOffset();
        jitteredCamera.setJitter(jx, jy);
    }
    m_stats.taaFrameIndex = m_settings.enableTAA
        ? m_taaAccumulator.state().frameIndex : 0u;

    // Snapshot the active render mode after capability downgrade.
    selectRenderPath();
    m_stats.activeRenderMode = m_settings.mode;

    // 1. CPU frustum cull (uses original, unjittered frustum)
    std::vector<Node*> visible;
    visible.reserve(256);
    {
        const auto cullStart = std::chrono::steady_clock::now();
        scene.collectVisible(camera.frustum(), visible);
        const auto cullEnd = std::chrono::steady_clock::now();
        m_stats.cpuCullTimeMs = std::chrono::duration<double, std::milli>(cullEnd - cullStart).count();
    }

    m_stats.totalNodes       = static_cast<uint32_t>(visible.size());
    m_stats.visibleNodes     = static_cast<uint32_t>(visible.size());
    m_stats.drawCalls           = 0;
    m_stats.shadowDrawCalls     = 0;
    m_stats.meshShaderDrawCalls = 0;
    m_stats.triangles           = 0;
    m_stats.meshlets            = 0;
    m_stats.vrsActive              = false;
    m_stats.volumetricActive       = false;
    m_stats.volumetricFroxelCount  = 0;
    m_stats.aoActive               = false;
    m_stats.aoSampleCount          = 0;
    m_stats.ssrActive              = false;
    m_stats.ssrRayCount            = 0;
    m_stats.bloomActive            = false;
    m_stats.bloomPassCount         = 0;
    m_stats.dofActive              = false;
    m_stats.dofSampleCount         = 0;
    m_stats.motionBlurActive       = false;
    m_stats.motionBlurSampleCount  = 0;
    m_stats.tonemapActive          = false;
    m_stats.tonemapOperator        = ToneMappingOperator::Linear;
    m_stats.iblActive              = false;
    m_stats.iblMipLevels           = 0;
    m_stats.oitActive              = false;
    m_stats.oitFragmentCount       = 0;
    m_stats.sssActive              = false;
    m_stats.sssBlurPasses          = 0;
    m_stats.contactShadowsActive   = false;
    m_stats.contactShadowRayCount  = 0;
    m_stats.tiledLightingActive    = false;
    m_stats.lightTileCount         = 0;
    m_stats.maxLightsPerTile       = 0;
    m_stats.clusteredLightingActive      = false;
    m_stats.lightClusterCount            = 0;
    m_stats.clusteredMaxLightsPerCluster = 0;
    m_stats.ssgiActive             = false;
    m_stats.ssgiRayCount           = 0;
    m_stats.decalsActive           = false;
    m_stats.decalCount             = 0;
    // Clamp the requested MSAA count to what the device actually supports.
    m_stats.msaaSamples = std::min(m_settings.msaaSamples,
                                   m_ctx.caps().maxMsaaSamples);

    const std::vector<SceneDrawPacket> drawPackets = buildSceneDrawPackets(visible);

    if (m_impl->scheduler) {
        // ── Frame scheduler path ───────────────────────────────────────────────────
        if (!m_impl->currentFrame) return;  // out-of-date — skip this frame
        auto& fc  = *m_impl->currentFrame;
        auto& cmd = *fc.cmd;
        const uint32_t shadowCascadeCount = m_impl->shadowLightingContract.cascadeCount;
        const bool runShadowPass = m_settings.enableShadows
            && (m_impl->shadowPipeline.valid() || m_impl->shadowMeshPipeline.valid())
            && shadowCascadeCount > 0;
        if (m_settings.enableShadows) {
            uploadShadowLightingContract();
        }
        if (runShadowPass) {
            ensureShadowTargets(fc.extent, shadowCascadeCount);
        }
        ensureGBuffer(fc.extent);
        if (!m_impl->gbuffer.albedoMaterial.valid() || !m_impl->gbuffer.depth.valid()) return;

        if (m_frameTiming.isReady())
            m_frameTiming.beginFrame(cmd, fc.frameSlot);

        if (runShadowPass && m_impl->shadow.depthAtlas.valid()) {
            std::array<nexus::gfx::TextureBarrier, 1> toShadowWrite = {{
                { m_impl->shadow.depthAtlas, m_impl->shadowDepthLayout, nexus::gfx::TextureLayout::DepthWrite }
            }};
            cmd.textureBarriers(toShadowWrite);
            m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::DepthWrite;

            m_impl->shadow.cascadeDrawCalls.fill(0);
            m_impl->shadow.cascadeTriangles.fill(0);
            for (uint32_t cascadeIndex = 0; cascadeIndex < m_impl->shadow.cascadeCount; ++cascadeIndex) {
                const std::vector<SceneDrawPacket> cascadePackets = buildCascadeShadowPackets(
                    drawPackets,
                    cascadeIndex,
                    m_impl->shadowLightingContract);
                const nexus::gfx::Rect2D passRect = m_impl->shadow.cascadeViewports[cascadeIndex];

                std::array<nexus::gfx::ClearValue, 1> shadowClears{};
                shadowClears[0].depth = {0.f, 0};
                cmd.beginRenderPass({}, {}, m_impl->shadow.depthAtlas, shadowClears, passRect);

                nexus::gfx::Viewport shadowVp{};
                shadowVp.x = static_cast<float>(passRect.offset.x);
                shadowVp.y = static_cast<float>(passRect.offset.y);
                shadowVp.width = static_cast<float>(passRect.extent.width);
                shadowVp.height = static_cast<float>(passRect.extent.height);
                shadowVp.minDepth = 0.f;
                shadowVp.maxDepth = 1.f;
                cmd.setViewport(shadowVp);
                cmd.setScissor(passRect);

                uint32_t shadowMeshlets = 0;
                uint32_t shadowMeshShaderDraws = 0;
                const uint32_t shadowDraws = submitShadowMeshPackets(
                    cmd, cascadePackets,
                    m_impl->shadowPipeline,
                    m_impl->shadowMeshPipeline,
                    m_settings.enableMeshShaders && m_ctx.caps().meshShaders,
                    shadowMeshlets,
                    shadowMeshShaderDraws);
                const uint32_t shadowTriangles = shadowTriangleCount(cascadePackets);
                m_stats.drawCalls           += shadowDraws;
                m_stats.shadowDrawCalls     += shadowDraws;
                m_stats.meshShaderDrawCalls += shadowMeshShaderDraws;
                m_stats.triangles += shadowTriangles;
                m_stats.meshlets  += shadowMeshlets;
                m_impl->shadow.cascadeDrawCalls[cascadeIndex] = shadowDraws;
                m_impl->shadow.cascadeTriangles[cascadeIndex] = shadowTriangles;
                cmd.endRenderPass();
            }

            std::array<nexus::gfx::TextureBarrier, 1> toShadowSample = {{
                { m_impl->shadow.depthAtlas, nexus::gfx::TextureLayout::DepthWrite, nexus::gfx::TextureLayout::DepthRead }
            }};
            cmd.textureBarriers(toShadowSample);
            m_impl->shadowDepthLayout = nexus::gfx::TextureLayout::DepthRead;
        } else {
            m_impl->shadow.cascadeDrawCalls.fill(0);
            m_impl->shadow.cascadeTriangles.fill(0);
        }

        nexus::gfx::Viewport vp;
        vp.x = 0.f; vp.y = 0.f;
        vp.width  = static_cast<float>(fc.extent.width);
        vp.height = static_cast<float>(fc.extent.height);
        vp.minDepth = 0.f; vp.maxDepth = 1.f;
        cmd.setViewport(vp);
        cmd.setScissor({{0,0}, fc.extent});

        // Geometry pass: write into GBuffer attachments.
        std::array<nexus::gfx::TextureBarrier, 4> toGeometryPass = {{
            { m_impl->gbuffer.albedoMaterial, m_impl->albedoLayout,   nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.normal,         m_impl->normalLayout,   nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.velocity,       m_impl->velocityLayout, nexus::gfx::TextureLayout::ColorAttachment },
            { m_impl->gbuffer.depth,          m_impl->depthLayout,    nexus::gfx::TextureLayout::DepthWrite }
        }};
        cmd.textureBarriers(toGeometryPass);
        m_impl->albedoLayout   = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->normalLayout   = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->velocityLayout = nexus::gfx::TextureLayout::ColorAttachment;
        m_impl->depthLayout    = nexus::gfx::TextureLayout::DepthWrite;

        std::array<nexus::gfx::TextureHandle, 3> gbufferColors = {
            m_impl->gbuffer.albedoMaterial,
            m_impl->gbuffer.normal,
            m_impl->gbuffer.velocity
        };
        std::array<nexus::gfx::ClearValue, 4> gbufferClears{};
        gbufferClears[0].color = {0.f, 0.f, 0.f, 1.f};
        gbufferClears[1].color = {0.5f, 0.5f, 1.f, 1.f};
        gbufferClears[2].color = {0.f, 0.f, 0.f, 0.f};
        gbufferClears[3].depth = {0.f, 0};

        // ── Variable-Rate Shading (VRS) ───────────────────────────────────────
        // Set the coarse shading rate before the geometry pass; reset to 1×1
        // after so the composite / RT passes always execute at full rate.
        const bool vrsEnabled = m_settings.enableVRS && m_ctx.caps().variableRateShading;
        if (vrsEnabled) {
            cmd.setFragmentShadingRate(m_settings.defaultShadingRate);
            m_stats.vrsActive = true;
        }

        cmd.beginRenderPass({}, gbufferColors, m_impl->gbuffer.depth, gbufferClears,
                            {{0,0}, fc.extent});
        uint32_t geometryTriangles = 0;
        uint32_t geometryMeshlets = 0;
        uint32_t geometryMeshShaderDraws = 0;
        const uint32_t geometryDraws = submitGeometryPackets(cmd,
                                                             drawPackets,
                                                             m_impl->fallbackGeometryPipeline,
                                                             m_impl->fallbackMeshPipeline,
                                                             m_impl->materialPipelines,
                                                             m_impl->materialMeshPipelines,
                                                             m_settings.enableMeshShaders && m_ctx.caps().meshShaders,
                                                             geometryTriangles,
                                                             geometryMeshlets,
                                                             geometryMeshShaderDraws);
        m_stats.drawCalls += geometryDraws;
        m_stats.meshShaderDrawCalls += geometryMeshShaderDraws;
        m_stats.triangles += geometryTriangles;
        m_stats.meshlets += geometryMeshlets;
        cmd.endRenderPass();

        // Restore full shading rate so subsequent passes (composite, RT, denoiser)
        // always execute at 1×1 regardless of the VRS geometry setting.
        if (vrsEnabled)
            cmd.setFragmentShadingRate(nexus::gfx::ShadingRate::Rate1x1);

        if (m_frameTiming.isReady())
            m_frameTiming.markGeometryEnd(cmd, fc.frameSlot);

        // Transition GBuffer for sampling in lighting/composite stage.
        std::array<nexus::gfx::TextureBarrier, 4> toLightingInputs = {{
            { m_impl->gbuffer.albedoMaterial, nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.normal,         nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.velocity,       nexus::gfx::TextureLayout::ColorAttachment, nexus::gfx::TextureLayout::ShaderRead },
            { m_impl->gbuffer.depth,          nexus::gfx::TextureLayout::DepthWrite,      nexus::gfx::TextureLayout::DepthRead }
        }};
        cmd.textureBarriers(toLightingInputs);
        m_impl->albedoLayout   = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->normalLayout   = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->velocityLayout = nexus::gfx::TextureLayout::ShaderRead;
        m_impl->depthLayout    = nexus::gfx::TextureLayout::DepthRead;

        const bool hasCompositeColorTarget = fc.colorTarget.valid();
        if (hasCompositeColorTarget) {
            // Lighting/composite pass target: swapchain color image.
            cmd.textureBarrier({
                fc.colorTarget,
                m_impl->swapchainLayout,
                nexus::gfx::TextureLayout::ColorAttachment
            });
            m_impl->swapchainLayout = nexus::gfx::TextureLayout::ColorAttachment;

            std::array<nexus::gfx::ClearValue, 1> finalClears{};
            finalClears[0].color = {0.05f, 0.05f, 0.08f, 1.f};
            cmd.beginRenderPass({}, {&fc.colorTarget, 1}, {}, finalClears,
                                {{0,0}, fc.extent});
        }
        if (m_impl->lightingCompositePipeline.valid() && hasCompositeColorTarget) {
            cmd.bindPipeline(m_impl->lightingCompositePipeline);

            // Bind composite inputs through the descriptor set contract.
            // On backends that use bindless / push constants the allocate call
            // returns an invalid handle and the bind is skipped gracefully.
            const CompositePassBindingDesc compositeDesc = buildCompositePassBindingDesc();
            if (compositeDesc.readyForComposite()) {
                auto& dev = m_ctx.device();

                // Core GBuffer + sampler + material table bindings (set 0).
                std::array<nexus::gfx::DescriptorBindingDesc, 9> coreBindings{};
                uint32_t nb = 0;
                coreBindings[nb++] = { 0, nexus::gfx::DescriptorType::SampledTexture, {}, compositeDesc.albedoTexture,   {} };
                coreBindings[nb++] = { 1, nexus::gfx::DescriptorType::SampledTexture, {}, compositeDesc.normalTexture,   {} };
                coreBindings[nb++] = { 2, nexus::gfx::DescriptorType::SampledTexture, {}, compositeDesc.velocityTexture, {} };
                coreBindings[nb++] = { 3, nexus::gfx::DescriptorType::SampledTexture, {}, compositeDesc.depthTexture,    {} };
                coreBindings[nb++] = { 4, nexus::gfx::DescriptorType::Sampler, {}, {}, compositeDesc.albedoSampler   };
                coreBindings[nb++] = { 5, nexus::gfx::DescriptorType::Sampler, {}, {}, compositeDesc.normalSampler   };
                coreBindings[nb++] = { 6, nexus::gfx::DescriptorType::Sampler, {}, {}, compositeDesc.velocitySampler };
                coreBindings[nb++] = { 7, nexus::gfx::DescriptorType::Sampler, {}, {}, compositeDesc.depthSampler    };
                if (compositeDesc.materialTable.valid()) {
                    coreBindings[nb++] = { 8, nexus::gfx::DescriptorType::StorageBuffer, compositeDesc.materialTable, {}, {} };
                }

                nexus::gfx::DescriptorSetDesc coreSetDesc{};
                coreSetDesc.bindings  = std::span{ coreBindings.data(), nb };
                coreSetDesc.debugName = "Composite.CoreSet";

                nexus::gfx::DescriptorSetHandle coreSet = dev.allocateDescriptorSet(coreSetDesc);
                if (coreSet.valid()) {
                    cmd.bindDescriptorSet(coreSet, 0);
                    deferDescriptorSetFree(*m_impl, fc.frameSlot, coreSet);
                }

                // Shadow lighting bindings (set 1) — only when fully populated.
                if (compositeDesc.hasShadowInputs()) {
                    std::array<nexus::gfx::DescriptorBindingDesc, 3> shadowBindings{};
                    shadowBindings[0] = { 0, nexus::gfx::DescriptorType::SampledTexture, {}, compositeDesc.shadowDepthTexture, {} };
                    shadowBindings[1] = { 1, nexus::gfx::DescriptorType::Sampler,        {}, {}, compositeDesc.shadowDepthSampler };
                    shadowBindings[2] = { 2, nexus::gfx::DescriptorType::StorageBuffer,  compositeDesc.shadowLightingBuffer, {}, {} };

                    nexus::gfx::DescriptorSetDesc shadowSetDesc{};
                    shadowSetDesc.bindings  = shadowBindings;
                    shadowSetDesc.debugName = "Composite.ShadowSet";

                    nexus::gfx::DescriptorSetHandle shadowSet = dev.allocateDescriptorSet(shadowSetDesc);
                    if (shadowSet.valid()) {
                        cmd.bindDescriptorSet(shadowSet, 1);
                        deferDescriptorSetFree(*m_impl, fc.frameSlot, shadowSet);
                    }
                }
            }

            // Fullscreen triangle pass; shader samples transitioned GBuffer targets.
            cmd.draw(3, 1, 0, 0);
            ++m_stats.drawCalls;
        }
        if (hasCompositeColorTarget) {
            cmd.endRenderPass();

            cmd.textureBarrier({
                fc.colorTarget,
                nexus::gfx::TextureLayout::ColorAttachment,
                nexus::gfx::TextureLayout::Present
            });
            m_impl->swapchainLayout = nexus::gfx::TextureLayout::Present;
        }

        if (m_frameTiming.isReady())
            m_frameTiming.endFrame(cmd, fc.frameSlot);

        // ── RT reflections (HybridRT mode, capability-gated) ─────────────────
        // traceRays() dispatches secondary reflection rays over the GBuffer.
        // Requires HybridRT or PathTrace mode + a valid RT pipeline + RT caps.
        m_stats.rtReflectionsActive = false;
        m_stats.tlasInstanceCount   = 0;
        {
            const bool rtModeActive = (m_settings.mode == RenderMode::HybridRT ||
                                       m_settings.mode == RenderMode::PathTrace);
            const bool rtCaps = m_ctx.caps().rayTracingTier >= 1;
            if (m_settings.enableRTReflect && rtModeActive && rtCaps &&
                m_impl->rayTracingPipeline.valid()) {
                cmd.bindPipeline(m_impl->rayTracingPipeline);

                // Bind RT descriptor layout: set=0 binding=0 → TLAS.
                // Only issued when a TLAS handle has been registered; on Null backend
                // allocateDescriptorSet is a no-op so this path is always safe.
                if (m_impl->sceneTLAS.valid()) {
                    using nexus::gfx::DescriptorBindingDesc;
                    using nexus::gfx::DescriptorType;
                    const DescriptorBindingDesc tlasBinding{
                        .binding     = 0,
                        .type        = DescriptorType::AccelerationStructure,
                        .accelStruct = m_impl->sceneTLAS,
                    };
                    const nexus::gfx::DescriptorSetDesc rtSetDesc{
                        .bindings  = std::span<const DescriptorBindingDesc>(&tlasBinding, 1),
                        .debugName = "RTReflections_set0",
                    };
                    auto rtDescSet = m_ctx.device().allocateDescriptorSet(rtSetDesc);
                    if (rtDescSet.valid()) {
                        m_ctx.device().updateDescriptorSet(rtDescSet,
                            std::span<const DescriptorBindingDesc>(&tlasBinding, 1));
                        cmd.bindDescriptorSet(rtDescSet, 0);
                        deferDescriptorSetFree(*m_impl, fc.frameSlot, rtDescSet);
                    }
                }

                // Use SBT-backed dispatch when a binding table is registered;
                // fall back to bare traceRays when none is set.
                if (m_impl->sbt.valid())
                    cmd.traceRaysWithSBT(m_impl->sbt, fc.extent.width, fc.extent.height, 1u);
                else
                    cmd.traceRays(fc.extent.width, fc.extent.height, 1u);
                m_stats.rtReflectionsActive = true;

                // Count BLAS instances that form the scene TLAS this frame.
                uint32_t tlasCount = 0;
                scene.traverse([&](Node& node, const Mat4&) {
                    if (node.mesh.blas.valid()) ++tlasCount;
                });
                m_stats.tlasInstanceCount = tlasCount;
            }
        }

        // ── Render graph validation (optional, diagnostic) ────────────────
        if (m_impl->renderGraphValidationEnabled) {
            const bool ranShadow   = runShadowPass && m_impl->shadow.depthAtlas.valid();
            RenderGraphDesc rgDesc;
            if (ranShadow) {
                rgDesc.record(RenderPassType::Shadow,
                              nexus::gfx::TextureLayout::ColorAttachment,  // gbuffer irrelevant here
                              nexus::gfx::TextureLayout::DepthWrite);
            }
            rgDesc.record(RenderPassType::Geometry,
                          nexus::gfx::TextureLayout::ColorAttachment,
                          ranShadow ? nexus::gfx::TextureLayout::DepthWrite
                                    : nexus::gfx::TextureLayout::Undefined);
            rgDesc.record(RenderPassType::Composite,
                          nexus::gfx::TextureLayout::ShaderRead,
                          ranShadow ? nexus::gfx::TextureLayout::DepthRead
                                    : nexus::gfx::TextureLayout::DepthRead);
            m_impl->lastRenderGraphReport = RenderGraphValidator::validate(rgDesc);
        }

        // ── Frame capture ─────────────────────────────────────────────────
        if (m_impl->captureExporter) {
            if (runShadowPass && m_impl->shadow.depthAtlas.valid()) {
                uint32_t shadowDraws = 0;
                uint32_t shadowTris  = 0;
                for (uint32_t ci = 0; ci < m_impl->shadow.cascadeCount; ++ci) {
                    shadowDraws += m_impl->shadow.cascadeDrawCalls[ci];
                    shadowTris  += m_impl->shadow.cascadeTriangles[ci];
                }
                FramePassEntry shadowEntry{};
                shadowEntry.passType          = RenderPassType::Shadow;
                shadowEntry.drawCalls         = shadowDraws;
                shadowEntry.triangles         = shadowTris;
                shadowEntry.shadowAtlasLayout = nexus::gfx::TextureLayout::DepthRead; // post-transition
                m_impl->captureExporter->recordPass(shadowEntry);
            }

            FramePassEntry geoEntry{};
            geoEntry.passType           = RenderPassType::Geometry;
            geoEntry.drawCalls          = geometryDraws;
            geoEntry.triangles          = geometryTriangles;
            geoEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead; // post-transition
            m_impl->captureExporter->recordPass(geoEntry);

            FramePassEntry compEntry{};
            compEntry.passType           = RenderPassType::Composite;
            compEntry.drawCalls          = 1; // fullscreen triangle
            compEntry.gbufferColorLayout = nexus::gfx::TextureLayout::ShaderRead;
            compEntry.shadowAtlasLayout  = runShadowPass ? nexus::gfx::TextureLayout::DepthRead
                                                         : nexus::gfx::TextureLayout::Undefined;
            m_impl->captureExporter->recordPass(compEntry);
        }
    } else {
        // ── Direct swapchain path (non-production compatibility fallback) ─────────
        // Scheduler path is the authoritative production flow; this branch is
        // intentionally minimal and does not aim for feature parity.
        auto& dev = m_ctx.device();
        dev.submit(nexus::gfx::QueueType::Graphics,
                   {&m_impl->cmdBufs[0], 1}, {}, {}, m_impl->frameFence);
    }

    // ── Screen-space post-processing passes (backend-agnostic) ───────────────
    // Volumetric, SSAO, SSR, and Bloom run after the GBuffer+composite passes
    // regardless of whether the frame scheduler or the direct swapchain path is used.
    {
        const nexus::gfx::Extent2D ext = m_swapchain.extent();
        auto* ppCmd = m_ctx.device().getCommandBuffer(m_impl->cmdBufs[0]);

        if (m_impl->volumetric.enabled && ppCmd) {
            const uint32_t froxelW = std::max(1u,
                ext.width  / m_impl->volumetric.froxelResolutionDivisor);
            const uint32_t froxelH = std::max(1u,
                ext.height / m_impl->volumetric.froxelResolutionDivisor);
            const uint32_t froxelZ = m_impl->volumetric.froxelSlices;
            const uint32_t groupsX = (froxelW + 7u) / 8u;
            const uint32_t groupsY = (froxelH + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, froxelZ);
            m_stats.volumetricActive      = true;
            m_stats.volumetricFroxelCount = froxelW * froxelH * froxelZ;
        }

        if (m_settings.enableAO && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.aoActive      = true;
            m_stats.aoSampleCount = ext.width * ext.height * m_impl->ao.sampleCount;
        }

        if (m_settings.enableSSR && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.ssrActive   = true;
            m_stats.ssrRayCount = ext.width * ext.height;
        }

        if (m_settings.enableBloom && m_impl->bloom.passes > 0 && ppCmd) {
            const uint32_t groupsX   = (ext.width  + 7u) / 8u;
            const uint32_t groupsY   = (ext.height + 7u) / 8u;
            const uint32_t totalPass = m_impl->bloom.passes * 2u;
            for (uint32_t p = 0; p < totalPass; ++p)
                ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.bloomActive    = true;
            m_stats.bloomPassCount = totalPass;
        }

        // ── Depth of Field ──────────────────────────────────────────────────
        // CoC-based bokeh blur in 8×8 tiles after the composite pass.
        if (m_settings.enableDoF && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.dofActive      = true;
            m_stats.dofSampleCount = ext.width * ext.height * m_impl->dof.sampleRadius;
        }

        // ── Motion Blur ─────────────────────────────────────────────────────
        // Per-pixel velocity-vector integration from the GBuffer velocity buffer.
        if (m_settings.enableMotionBlur && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.motionBlurActive       = true;
            m_stats.motionBlurSampleCount  = ext.width * ext.height
                                             * m_impl->motionBlur.sampleCount;
        }

        // ── Tone Mapping / HDR ──────────────────────────────────────────────
        // Applies exposure + operator curve to the HDR composite buffer.
        if (m_settings.enableToneMapping && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.tonemapActive   = true;
            m_stats.tonemapOperator = m_impl->toneMapping.operator_;
        }

        // ── Image-Based Lighting (IBL) ──────────────────────────────────────
        // Samples prefiltered environment map for specular/diffuse ambient terms.
        if (m_settings.enableIBL && m_impl->ibl.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.iblActive    = true;
            m_stats.iblMipLevels = m_impl->ibl.mipLevels;
        }

        // ── Order-Independent Transparency (OIT) resolve ────────────────────
        // Accumulates weighted transparent fragments then resolves over opaque.
        if (m_settings.enableOIT && m_impl->oit.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.oitActive        = true;
            m_stats.oitFragmentCount = ext.width * ext.height * m_impl->oit.maxLayers;
        }

        // ── Subsurface Scattering (SSS) separable blur ──────────────────────────
        // Runs after GBuffer; dispatches horizontal + vertical passes per blur pair.
        if (m_settings.enableSSS && m_impl->sss.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            const uint32_t totalPasses = m_impl->sss.blurPasses * 2u; // H + V per pair
            for (uint32_t p = 0; p < totalPasses; ++p) {
                ppCmd->dispatch(groupsX, groupsY, 1u);
            }
            m_stats.sssActive    = true;
            m_stats.sssBlurPasses = totalPasses;
        }

        // ── Screen-Space Contact Shadows depth-buffer march ─────────────────────
        // Runs after the shadow pass; before composite.
        if (m_settings.enableContactShadows && m_impl->contactShadow.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.contactShadowsActive  = true;
            m_stats.contactShadowRayCount = ext.width * ext.height;
        }

        // ── Tiled Deferred Lighting tile classification ──────────────────────────
        // Runs after GBuffer; tile classification feeds the composite light list.
        if (m_settings.enableTiledLighting && m_impl->tiledLighting.enabled && ppCmd) {
            const uint32_t ts      = m_impl->tiledLighting.tileSize;
            const uint32_t tilesX  = (ext.width  + ts - 1u) / ts;
            const uint32_t tilesY  = (ext.height + ts - 1u) / ts;
            ppCmd->dispatch(tilesX, tilesY, 1u);
            m_stats.tiledLightingActive = true;
            m_stats.lightTileCount      = tilesX * tilesY;
            m_stats.maxLightsPerTile    = m_impl->tiledLighting.maxLightsPerTile;
        }

        // ── GPU-Driven Clustered Lighting 3-D cluster classification ────────────
        // Runs after GBuffer; cluster assignment replaces/supplements tiled deferred
        // for dense-light scenes.
        if (m_settings.enableClusteredLighting && m_impl->clusteredLighting.enabled && ppCmd) {
            const uint32_t cx = m_impl->clusteredLighting.clusterDimX;
            const uint32_t cy = m_impl->clusteredLighting.clusterDimY;
            const uint32_t cz = m_impl->clusteredLighting.clusterDimZ;
            ppCmd->dispatch(cx, cy, cz);
            m_stats.clusteredLightingActive      = true;
            m_stats.lightClusterCount            = cx * cy * cz;
            m_stats.clusteredMaxLightsPerCluster = m_impl->clusteredLighting.maxLightsPerCluster;
        }

        // ── Screen-Space Global Illumination (SSGI) gather ──────────────────────
        // Runs after GBuffer; dispatches hemisphere ray gather per pixel.
        if (m_settings.enableSSGI && m_impl->ssgi.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.ssgiActive    = true;
            m_stats.ssgiRayCount  = ext.width * ext.height * m_impl->ssgi.rayCount;
        }

        // ── Decal Projection Pass ────────────────────────────────────────────────
        // Runs after GBuffer; projects oriented bounding box decals into GBuffer
        // albedo/normal/roughness channels before lighting composite.
        if (m_settings.enableDecals && m_impl->decals.enabled && ppCmd) {
            const uint32_t groupsX = (ext.width  + 7u) / 8u;
            const uint32_t groupsY = (ext.height + 7u) / 8u;
            ppCmd->dispatch(groupsX, groupsY, 1u);
            m_stats.decalsActive = true;
            m_stats.decalCount   = m_impl->decals.maxDecals;
        }
    }

    // ── Gaussian splat pass (Month 13 first slice) ─────────────────────────
        // Runs after the composite pass.  Backend-agnostic; produces deterministic
        // counters and never alters render-graph layouts.
        if (m_impl->gaussianSplatPass) {
                const auto& ubo = jitteredCamera.ubo();
                m_impl->gaussianSplatPass->setCameraMatrices(ubo.view, ubo.projection);
                const auto splatStats = m_impl->gaussianSplatPass->computeStats();
                m_stats.splatDrawCalls  += splatStats.splatDrawCalls;
                m_stats.submittedSplats += splatStats.submittedSplats;
                m_stats.projectedSplats += splatStats.projectedSplats;
                m_stats.drawCalls       += splatStats.splatDrawCalls;
        }

    // ── Neural denoiser (async-compute scheduling) ──────────────────────────
    // Invoked after the composite pass on the Null backend cmd handle.
    // On a real backend this would be dispatched on the async compute queue.
    m_stats.denoisingActive = false;
    m_stats.activeDenoiser  = nexus::neural::DenoiserBackend::None;
    if (m_settings.enableDenoising && m_neuralRenderer) {
        nexus::neural::DenoiserInput  dnIn{};
        nexus::neural::DenoiserOutput dnOut{};
        nexus::gfx::CmdBufHandle cmd{};  // Null backend: invalid handle is a no-op
        m_neuralRenderer->denoise(cmd, dnIn, dnOut);
        m_stats.denoisingActive = true;
        m_stats.activeDenoiser  = m_neuralRenderer->activeDenoiser();
    }

    // ── Neural upscaler (async-compute scheduling) ──────────────────────────
    // Invoked after the denoiser pass. On a real backend dispatched on the
    // async compute queue; the output feeds the TAA resolve / tone-map pass.
    m_stats.upscalingActive = false;
    m_stats.activeUpscaler  = nexus::neural::UpscalerBackend::None;
    if (m_settings.enableUpscaling && m_neuralRenderer) {
        nexus::neural::UpscalerInput  upIn{};
        nexus::neural::UpscalerOutput upOut{};
        nexus::gfx::CmdBufHandle cmd{};  // Null backend: invalid handle is a no-op
        m_neuralRenderer->upscale(cmd, upIn, upOut);
        m_stats.upscalingActive = true;
        m_stats.activeUpscaler  = m_neuralRenderer->activeUpscaler();
    }

    ++m_impl->frameIndex;
}

void Renderer::endFrame()
{
    if (m_impl->scheduler) {
        if (m_impl->currentFrame)
            m_impl->scheduler->endFrame();
    } else {
        // Present the frame acquired in beginFrame()
        [[maybe_unused]] auto presentResult = m_swapchain.present(m_impl->currentImageIndex, {});
    }

    if (m_impl->captureExporter) {
        m_impl->captureExporter->endCapture();
    }
}

void Renderer::applySettings(const RendererSettings& s)
{
    const bool shadowsWereEnabled = m_settings.enableShadows;
    m_settings = s;

    if (shadowsWereEnabled && !m_settings.enableShadows) {
        // Shadow resources and pass descriptors should not remain externally
        // visible once shadows are disabled.
        destroyShadowTargets();
        m_impl->shadow.cascadeDrawCalls.fill(0);
        m_impl->shadow.cascadeTriangles.fill(0);
    }

    selectRenderPath();
}

void Renderer::setFallbackGeometryPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->fallbackGeometryPipeline = pipeline;
}

void Renderer::setFallbackMeshPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->fallbackMeshPipeline = pipeline;
}

void Renderer::setRayTracingPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->rayTracingPipeline = pipeline;
}

nexus::gfx::PipelineHandle Renderer::rayTracingPipeline() const noexcept
{
    return m_impl->rayTracingPipeline;
}

void Renderer::setSceneTLAS(nexus::gfx::AccelStructHandle tlas) noexcept
{
    m_impl->sceneTLAS = tlas;
}

nexus::gfx::AccelStructHandle Renderer::sceneTLAS() const noexcept
{
    return m_impl->sceneTLAS;
}

void Renderer::setShaderBindingTable(nexus::gfx::SBTHandle sbt) noexcept
{
    m_impl->sbt = sbt;
}

nexus::gfx::SBTHandle Renderer::shaderBindingTable() const noexcept
{
    return m_impl->sbt;
}

void Renderer::setVolumetricSettings(const VolumetricSettings& settings) noexcept
{
    m_impl->volumetric = settings;
}

const VolumetricSettings& Renderer::volumetricSettings() const noexcept
{
    return m_impl->volumetric;
}

void Renderer::setAOSettings(const AOSettings& settings) noexcept { m_impl->ao = settings; }
const AOSettings& Renderer::aoSettings() const noexcept { return m_impl->ao; }

void Renderer::setSSRSettings(const SSRSettings& settings) noexcept { m_impl->ssr = settings; }
const SSRSettings& Renderer::ssrSettings() const noexcept { return m_impl->ssr; }

void Renderer::setBloomSettings(const BloomSettings& settings) noexcept { m_impl->bloom = settings; }
const BloomSettings& Renderer::bloomSettings() const noexcept { return m_impl->bloom; }

void Renderer::setDoFSettings(const DoFSettings& settings) noexcept { m_impl->dof = settings; }
const DoFSettings& Renderer::dofSettings() const noexcept { return m_impl->dof; }

void Renderer::setMotionBlurSettings(const MotionBlurSettings& settings) noexcept { m_impl->motionBlur = settings; }
const MotionBlurSettings& Renderer::motionBlurSettings() const noexcept { return m_impl->motionBlur; }

void Renderer::setToneMappingSettings(const ToneMappingSettings& settings) noexcept { m_impl->toneMapping = settings; }
const ToneMappingSettings& Renderer::toneMappingSettings() const noexcept { return m_impl->toneMapping; }

void Renderer::setIBLSettings(const IBLSettings& settings) noexcept { m_impl->ibl = settings; }
const IBLSettings& Renderer::iblSettings() const noexcept { return m_impl->ibl; }

void Renderer::setOITSettings(const OITSettings& settings) noexcept { m_impl->oit = settings; }
const OITSettings& Renderer::oitSettings() const noexcept { return m_impl->oit; }

void Renderer::setSSSSettings(const SSSSettings& settings) noexcept { m_impl->sss = settings; }
const SSSSettings& Renderer::sssSettings() const noexcept { return m_impl->sss; }

void Renderer::setContactShadowSettings(const ContactShadowSettings& settings) noexcept { m_impl->contactShadow = settings; }
const ContactShadowSettings& Renderer::contactShadowSettings() const noexcept { return m_impl->contactShadow; }

void Renderer::setTiledLightingSettings(const TiledLightingSettings& settings) noexcept { m_impl->tiledLighting = settings; }
const TiledLightingSettings& Renderer::tiledLightingSettings() const noexcept { return m_impl->tiledLighting; }

void Renderer::setClusteredLightingSettings(const ClusteredLightingSettings& settings) noexcept { m_impl->clusteredLighting = settings; }
const ClusteredLightingSettings& Renderer::clusteredLightingSettings() const noexcept { return m_impl->clusteredLighting; }

void Renderer::setSSGISettings(const SSGISettings& settings) noexcept { m_impl->ssgi = settings; }
const SSGISettings& Renderer::ssgiSettings() const noexcept { return m_impl->ssgi; }

void Renderer::setDecalSettings(const DecalSettings& settings) noexcept { m_impl->decals = settings; }
const DecalSettings& Renderer::decalSettings() const noexcept { return m_impl->decals; }

void Renderer::setShadowPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->shadowPipeline = pipeline;
}

void Renderer::setShadowMeshPipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->shadowMeshPipeline = pipeline;
}

void Renderer::setLightingCompositePipeline(nexus::gfx::PipelineHandle pipeline) noexcept
{
    m_impl->lightingCompositePipeline = pipeline;
}

void Renderer::setMaterialPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept
{
    if (!pipeline.valid()) {
        m_impl->materialPipelines.erase(material);
        return;
    }
    m_impl->materialPipelines[material] = pipeline;
}

void Renderer::setMaterialMeshPipeline(MaterialID material, nexus::gfx::PipelineHandle pipeline) noexcept
{
    if (!pipeline.valid()) {
        m_impl->materialMeshPipelines.erase(material);
        return;
    }
    m_impl->materialMeshPipelines[material] = pipeline;
}

void Renderer::clearMaterialPipelines() noexcept
{
    m_impl->materialPipelines.clear();
}

void Renderer::clearMaterialMeshPipelines() noexcept
{
    m_impl->materialMeshPipelines.clear();
}

void Renderer::setCompositeMaterialBindings(const CompositeMaterialBindings& bindings) noexcept
{
    m_impl->compositeBindings = bindings;
}

void Renderer::clearCompositeMaterialBindings() noexcept
{
    m_impl->compositeBindings = {};
}

void Renderer::setShadowLightingContract(const ShadowLightingContract& contract) noexcept
{
    m_impl->shadowLightingContract = contract;
    if (m_impl->shadowLightingContract.cascadeCount > ShadowLightingContract::kMaxCascades)
        m_impl->shadowLightingContract.cascadeCount = ShadowLightingContract::kMaxCascades;
    m_impl->shadowLightingDirty = true;
}

void Renderer::clearShadowLightingContract() noexcept
{
    m_impl->shadowLightingContract = {};
    m_impl->shadowLightingDirty = true;
}

ShadowPassBindingDesc Renderer::buildShadowPassBindingDesc() const noexcept
{
    ShadowPassBindingDesc desc{};
    if (!m_settings.enableShadows) {
        return desc;
    }

    desc.atlasDepthTexture = m_impl->shadow.depthAtlas;
    desc.atlasExtent = m_impl->shadow.atlasExtent;
    desc.cascadeExtent = m_impl->shadow.cascadeExtent;
    desc.atlasDepthLayout = m_impl->shadowDepthLayout;
    desc.cascadeCount = m_impl->shadowLightingContract.cascadeCount;
    desc.atlasColumns = m_impl->shadow.atlasColumns;
    desc.atlasRows = m_impl->shadow.atlasRows;
    const uint32_t n = desc.cascadeCount < ShadowPassBindingDesc::kMaxCascades
                     ? desc.cascadeCount
                     : ShadowPassBindingDesc::kMaxCascades;
    for (uint32_t i = 0; i < n; ++i) {
        desc.cascadePasses[i].cascadeIndex = i;
        desc.cascadePasses[i].atlasRect = m_impl->shadow.cascadeViewports[i];
        desc.cascadePasses[i].drawCalls = m_impl->shadow.cascadeDrawCalls[i];
        desc.cascadePasses[i].triangles = m_impl->shadow.cascadeTriangles[i];
    }
    return desc;
}

CompositePassBindingDesc Renderer::buildCompositePassBindingDesc() const noexcept
{
    CompositePassBindingDesc desc{};
    desc.albedoTexture = m_impl->gbuffer.albedoMaterial;
    desc.normalTexture = m_impl->gbuffer.normal;
    desc.velocityTexture = m_impl->gbuffer.velocity;
    desc.depthTexture = m_impl->gbuffer.depth;
    desc.albedoSampler = m_impl->compositeBindings.albedoMaterialSampler;
    desc.normalSampler = m_impl->compositeBindings.normalSampler;
    desc.velocitySampler = m_impl->compositeBindings.velocitySampler;
    desc.depthSampler = m_impl->compositeBindings.depthSampler;
    desc.materialTable = m_impl->compositeBindings.materialTable;
    desc.materialTableOffsetBytes = m_impl->compositeBindings.materialTableOffsetBytes;

    const bool hasCompleteShadowContract = m_settings.enableShadows
        && m_impl->shadow.depthAtlas.valid()
        && m_impl->compositeBindings.shadowDepthSampler.valid()
        && m_impl->shadowLightingBuffer.valid()
        && m_impl->shadowLightingContract.cascadeCount > 0;
    if (hasCompleteShadowContract) {
        desc.shadowDepthTexture = m_impl->shadow.depthAtlas;
        desc.shadowDepthSampler = m_impl->compositeBindings.shadowDepthSampler;
        desc.shadowLightingBuffer = m_impl->shadowLightingBuffer;
        desc.shadowCascadeCount = m_impl->shadowLightingContract.cascadeCount;
    }

    return desc;
}

ShadowLightingBindingDesc Renderer::buildShadowLightingBindingDesc() const noexcept
{
    ShadowLightingBindingDesc desc{};

    const bool hasCompleteShadowContract = m_settings.enableShadows
        && m_impl->shadow.depthAtlas.valid()
        && m_impl->compositeBindings.shadowDepthSampler.valid()
        && m_impl->shadowLightingBuffer.valid()
        && m_impl->shadowLightingContract.cascadeCount > 0;

    if (hasCompleteShadowContract) {
        desc.shadowDepthTexture   = m_impl->shadow.depthAtlas;
        desc.shadowDepthSampler   = m_impl->compositeBindings.shadowDepthSampler;
        desc.shadowLightingBuffer = m_impl->shadowLightingBuffer;
        desc.shadowCascadeCount   = m_impl->shadowLightingContract.cascadeCount;
        desc.shadowBias           = m_impl->shadowLightingContract.shadowBias;
        desc.normalBias           = m_impl->shadowLightingContract.normalBias;
        const uint32_t n = desc.shadowCascadeCount < ShadowLightingBindingDesc::kMaxCascades
                         ? desc.shadowCascadeCount : ShadowLightingBindingDesc::kMaxCascades;
        for (uint32_t i = 0; i < n; ++i) {
            desc.lightViewProj[i] = m_impl->shadowLightingContract.lightViewProj[i];
            desc.cascadeSplits[i] = m_impl->shadowLightingContract.cascadeSplits[i];
        }
    }

    return desc;
}

void Renderer::resetScene(SceneGraph& scene)
{
    auto& dev = m_ctx.device();
    dev.waitIdle();

    scene.clear();

    resetRendererSceneCaches(*m_impl, dev, m_stats);
}

void Renderer::resetSceneAndDestroyTLAS(SceneGraph& scene)
{
    auto& dev = m_ctx.device();
    dev.waitIdle();

    scene.clearAndDestroyTLAS(dev);

    resetRendererSceneCaches(*m_impl, dev, m_stats);
}

void Renderer::onResize(nexus::gfx::Extent2D newExtent)
{
    m_ctx.device().waitIdle();
    releaseAllDeferredDescriptorSets(*m_impl, m_ctx.device());
    if (m_impl->scheduler) {
        m_impl->scheduler->onResize(newExtent);
    } else {
        m_swapchain.resize(newExtent);
    }
    destroyShadowTargets();
    destroyGBuffer();
    if (m_shadowMapTarget) {
        (void)m_shadowMapTarget->resize(m_ctx.device(), newExtent);
    }
}

void Renderer::selectRenderPath()
{
    const auto& caps = m_ctx.caps();
    const auto  tier = m_ctx.hardwareTier();

    // Downgrade render mode if hardware doesn't support it
    if (m_settings.mode == RenderMode::PathTrace && caps.rayTracingTier < 2)
        m_settings.mode = (caps.rayTracingTier == 1) ? RenderMode::HybridRT : RenderMode::Rasterize;

    if (m_settings.mode == RenderMode::HybridRT && caps.rayTracingTier < 1)
        m_settings.mode = RenderMode::Rasterize;

    // Upscaler degradation on low-end
    if (tier == nexus::gfx::HardwareTier::Low || tier == nexus::gfx::HardwareTier::Mid)
        if (m_settings.upscale != UpscaleMode::Off && m_settings.upscale != UpscaleMode::Bilinear)
            m_settings.upscale = UpscaleMode::Bilinear;
}

// ── Render graph validation API ───────────────────────────────────────────────

void Renderer::setRenderGraphValidationEnabled(bool enabled) noexcept
{
    m_impl->renderGraphValidationEnabled = enabled;
    if (!enabled) {
        m_impl->lastRenderGraphReport = {};
    }
}

bool Renderer::isRenderGraphValidationEnabled() const noexcept
{
    return m_impl->renderGraphValidationEnabled;
}

const RenderGraphValidationReport& Renderer::lastRenderGraphReport() const noexcept
{
    return m_impl->lastRenderGraphReport;
}

// ── Frame capture API ─────────────────────────────────────────────────────────

void Renderer::setFrameCaptureExporter(IFrameCaptureExporter* exporter) noexcept
{
    m_impl->captureExporter = exporter;
}

IFrameCaptureExporter* Renderer::frameCaptureExporter() const noexcept
{
    return m_impl->captureExporter;
}

// ── Gaussian splat pass API ─────────────────────────────────────────────
void Renderer::setGaussianSplatPass(GaussianSplatPass* pass) noexcept
{
    m_impl->gaussianSplatPass = pass;
}

GaussianSplatPass* Renderer::gaussianSplatPass() const noexcept
{
    return m_impl->gaussianSplatPass;
}

// ── Descriptor binding integration (Month 14 Track 1) ────────────────────────

bool Renderer::bindCompositeDescriptors(nexus::gfx::IDevice& dev)
{
    const CompositePassBindingDesc bd = buildCompositePassBindingDesc();
    if (!bd.hasCoreInputs()) return false;

    CompositeDescriptorInputs inputs;
    inputs.albedo   = bd.albedoTexture;
    inputs.normal   = bd.normalTexture;
    inputs.velocity = bd.velocityTexture;
    inputs.depth    = bd.depthTexture;
    // Use the first valid sampler as the shared sampler; preference: albedo.
    inputs.sampler  = bd.albedoSampler.valid() ? bd.albedoSampler : bd.depthSampler;

    return m_compositeDescSet.update(dev, inputs);
}

const CompositeDescriptorSet* Renderer::compositeDescriptorSet() const noexcept
{
    return &m_compositeDescSet;
}

void Renderer::destroyCompositeDescriptors(nexus::gfx::IDevice& dev) noexcept
{
    m_compositeDescSet.destroy(dev);
}

// ── Shadow descriptor set integration (Month 15 Track 1) ─────────────────────

bool Renderer::bindShadowDescriptors(nexus::gfx::IDevice& dev)
{
    const ShadowLightingBindingDesc bd = buildShadowLightingBindingDesc();
    if (!bd.isComplete()) return false;
    return m_shadowDescSet.update(dev,
                                  bd.shadowDepthTexture,
                                  bd.shadowDepthSampler,
                                  bd.shadowLightingBuffer,
                                  bd.shadowCascadeCount);
}

const ShadowDescriptorSet* Renderer::shadowDescriptorSet() const noexcept
{
    return &m_shadowDescSet;
}

void Renderer::destroyShadowDescriptors(nexus::gfx::IDevice& dev) noexcept
{
    m_shadowDescSet.destroy(dev);
}

// ── Shadow map target integration (Month 14 Track 2) ─────────────────────────

void Renderer::setShadowMapTarget(ShadowMapTarget* target) noexcept
{
    m_shadowMapTarget = target;
}

ShadowMapTarget* Renderer::shadowMapTarget() const noexcept
{
    return m_shadowMapTarget;
}

// ── GPU timing layer (Month 16 Track 2) ──────────────────────────────────────

bool Renderer::enableFrameTiming(uint32_t framesInFlight)
{
    disableFrameTiming();
    return m_frameTiming.create(m_ctx.device(), framesInFlight);
}

void Renderer::disableFrameTiming() noexcept
{
    m_frameTiming.destroy(m_ctx.device());
    m_lastTimingResult = {};
}

bool Renderer::isFrameTimingEnabled() const noexcept
{
    return m_frameTiming.isReady();
}

const FrameTimingResult& Renderer::lastFrameTimingResult() const noexcept
{
    return m_lastTimingResult;
}

// ── TAA accessors ─────────────────────────────────────────────────────────────

void Renderer::setTemporalAccumulationConfig(const TemporalAccumulationConfig& cfg) noexcept
{
    m_taaAccumulator.setConfig(cfg);
}

const TemporalAccumulationConfig& Renderer::temporalAccumulationConfig() const noexcept
{
    return m_taaAccumulator.config();
}

const TemporalAccumulationState& Renderer::temporalAccumulationState() const noexcept
{
    return m_taaAccumulator.state();
}

// ── Neural renderer accessors ─────────────────────────────────────────────────

void Renderer::setNeuralRenderer(nexus::neural::INeuralRenderer* nr) noexcept
{
    m_neuralRenderer = nr;
}

nexus::neural::INeuralRenderer* Renderer::neuralRenderer() const noexcept
{
    return m_neuralRenderer;
}

} // namespace nexus::render
