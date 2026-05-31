#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Render — Frame Timing Layer (Month 16 Track 2)
//
//  Manages GPU timestamp query pools for per-pass frame timing.  One
//  QueryPoolHandle per in-flight frame slot holds two timestamps (begin/end)
//  for each tracked pass.  Results are read back at the start of the next
//  frame for the same slot so the GPU has finished writing them.
//
//  Usage:
//
//      FrameTimingLayer timing;
//      timing.create(dev, framesInFlight);
//
//      // Inside render loop (per frame):
//      timing.beginFrame(*cmd, frameSlot);
//      // ... geometry pass ...
//      timing.markGeometryEnd(*cmd, frameSlot);
//      // ... shadow pass (optional) ...
//      timing.markShadowEnd(*cmd, frameSlot);
//      // ... composite pass ...
//      timing.endFrame(*cmd, frameSlot);
//
//      // At start of next frame for this slot:
//      FrameTimingResult result = timing.readback(dev, frameSlot);
//      stats.gpuTimeMs = result.totalGpuMs;
//
//      timing.destroy(dev);
//
//  On the Null backend all readback values are 0.0.  On Vulkan the pool uses
//  VK_QUERY_TYPE_TIMESTAMP and the device timestamp period for ns→ms conversion.
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/gfx/Device.h>
#include <nexus/gfx/CommandBuffer.h>
#include <cstdint>
#include <vector>

namespace nexus::render {

struct FrameTimingResult {
    double geometryPassMs  = 0.0;
    double shadowPassMs    = 0.0;
    double compositePassMs = 0.0;
    double totalGpuMs      = 0.0;
};

class FrameTimingLayer {
public:
    FrameTimingLayer() = default;
    ~FrameTimingLayer() = default;

    FrameTimingLayer(const FrameTimingLayer&)            = delete;
    FrameTimingLayer& operator=(const FrameTimingLayer&) = delete;
    FrameTimingLayer(FrameTimingLayer&&)                 = default;
    FrameTimingLayer& operator=(FrameTimingLayer&&)      = default;

    // Allocates one QueryPool per frame slot (kQueriesPerSlot timestamps each).
    // Returns false if allocation fails for any slot.
    [[nodiscard]] bool create(nexus::gfx::IDevice& dev, uint32_t framesInFlight);

    // Frees all query pools.  Safe to call multiple times or before create().
    void destroy(nexus::gfx::IDevice& dev) noexcept;

    [[nodiscard]] bool isReady() const noexcept { return !m_pools.empty(); }

    // Reset the slot's pool and write the frame-start timestamp.
    void beginFrame(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot);

    // Write the geometry-pass-end timestamp.
    void markGeometryEnd(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot);

    // Write the shadow-pass-end timestamp (call only when shadow pass ran).
    void markShadowEnd(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot);

    // Write the frame-end (composite-end) timestamp.
    void endFrame(nexus::gfx::ICommandBuffer& cmd, uint32_t frameSlot);

    // Read back the results for the given slot.  Returns all-zero on Null backend
    // or if the slot has not completed a full begin→end cycle.
    [[nodiscard]] FrameTimingResult readback(nexus::gfx::IDevice& dev,
                                             uint32_t frameSlot);

    // Number of timestamps allocated per frame slot.
    static constexpr uint32_t kQueriesPerSlot = 4; // begin, geoEnd, shadowEnd, frameEnd

private:
    std::vector<nexus::gfx::QueryPoolHandle> m_pools;  // one per frame slot
    uint32_t m_framesInFlight = 0;

    // Bitmask of which slots have received a full begin→end in the current run.
    uint32_t m_readyMask = 0;
};

} // namespace nexus::render
