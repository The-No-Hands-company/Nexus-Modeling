// ─────────────────────────────────────────────────────────────────────────────
//  FrameTimingLayer — GPU timestamp query pool management
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/render/FrameTimingLayer.h>
#include <nexus/gfx/Device.h>
#include <nexus/gfx/CommandBuffer.h>

namespace nexus::render {

namespace {
// Nanoseconds per millisecond.
constexpr double kNsPerMs = 1'000'000.0;

// Slot index constants for the 4 queries per slot.
constexpr uint32_t kIdxBegin      = 0;
constexpr uint32_t kIdxGeoEnd     = 1;
constexpr uint32_t kIdxShadowEnd  = 2;
constexpr uint32_t kIdxFrameEnd   = 3;
} // namespace

bool FrameTimingLayer::create(nexus::gfx::IDevice& dev, uint32_t framesInFlight)
{
    if (framesInFlight == 0) return false;
    destroy(dev);

    m_pools.reserve(framesInFlight);
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        nexus::gfx::QueryPoolDesc desc{};
        desc.count = kQueriesPerSlot;
        const nexus::gfx::QueryPoolHandle pool = dev.createQueryPool(desc);
        if (!pool.valid()) {
            destroy(dev);
            return false;
        }
        m_pools.push_back(pool);
    }
    m_framesInFlight = framesInFlight;
    m_readyMask      = 0;
    return true;
}

void FrameTimingLayer::destroy(nexus::gfx::IDevice& dev) noexcept
{
    for (auto& p : m_pools) {
        if (p.valid()) dev.destroyQueryPool(p);
    }
    m_pools.clear();
    m_framesInFlight = 0;
    m_readyMask      = 0;
}

void FrameTimingLayer::beginFrame(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot)
{
    if (frameSlot >= m_pools.size() || !m_pools[frameSlot].valid()) return;
    const auto& pool = m_pools[frameSlot];
    cmd.resetQueryPool(pool, 0, kQueriesPerSlot);
    cmd.writeTimestamp(pool, kIdxBegin);
    // Clear this slot's ready bit — it hasn't completed an end yet.
    m_readyMask &= ~(1u << frameSlot);
}

void FrameTimingLayer::markGeometryEnd(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot)
{
    if (frameSlot >= m_pools.size() || !m_pools[frameSlot].valid()) return;
    cmd.writeTimestamp(m_pools[frameSlot], kIdxGeoEnd);
}

void FrameTimingLayer::markShadowEnd(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot)
{
    if (frameSlot >= m_pools.size() || !m_pools[frameSlot].valid()) return;
    cmd.writeTimestamp(m_pools[frameSlot], kIdxShadowEnd);
}

void FrameTimingLayer::endFrame(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot)
{
    if (frameSlot >= m_pools.size() || !m_pools[frameSlot].valid()) return;
    cmd.writeTimestamp(m_pools[frameSlot], kIdxFrameEnd);
    m_readyMask |= (1u << frameSlot);
}

FrameTimingResult FrameTimingLayer::readback(nexus::gfx::IDevice& dev, uint32_t frameSlot)
{
    FrameTimingResult result{};
    if (frameSlot >= m_pools.size()) return result;
    if (!(m_readyMask & (1u << frameSlot))) return result;
    const auto& pool = m_pools[frameSlot];
    if (!pool.valid()) return result;

    uint64_t ts[kQueriesPerSlot] = {};
    dev.readbackTimestamps(pool, 0, kQueriesPerSlot, ts);

    // On Null backend all values are 0 — return zero result.
    if (ts[kIdxBegin] == 0 && ts[kIdxFrameEnd] == 0) return result;

    // Convert raw GPU ticks to milliseconds.  The ns values are already in
    // nanoseconds on our Null/Vulkan paths (readbackTimestamps applies the
    // timestamp period on Vulkan; returns 0 on Null).
    const auto nsToMs = [](uint64_t a, uint64_t b) -> double {
        if (b <= a) return 0.0;
        return static_cast<double>(b - a) / kNsPerMs;
    };

    result.geometryPassMs  = nsToMs(ts[kIdxBegin],     ts[kIdxGeoEnd]);
    result.shadowPassMs    = nsToMs(ts[kIdxGeoEnd],    ts[kIdxShadowEnd]);
    result.compositePassMs = nsToMs(ts[kIdxShadowEnd], ts[kIdxFrameEnd]);
    result.totalGpuMs      = nsToMs(ts[kIdxBegin],     ts[kIdxFrameEnd]);
    return result;
}

} // namespace nexus::render
