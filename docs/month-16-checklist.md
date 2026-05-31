# Month 16 — M1 Closure, GPU Timing Layer, Shadow End-to-End

Status: **open**. Three tracks: M1 formal closure, GPU timestamp profiling
layer (unblocks M4 perf gate), and M2 shadow end-to-end draw-call verification.

Reference: [ROADMAP.md](../ROADMAP.md), [month-15-checklist.md](month-15-checklist.md).

---

## Track 1 — M1 Closure (Renderer Core Stabilization)

### Motivation

All M1 deliverables are complete:
- GBuffer sampling contract: DescriptorBinder + Renderer integration (Month 14–15)
- Pass sequencing hardened: barrier ordering tests (Month 15)
- Composite + shadow descriptor sets owned by Renderer
- Render path parity formally de-scoped (Month 15)

M1 should be marked complete in ROADMAP.md and a closure note added to
`docs/graphics-kernel.md`.

### Deliverables

1. Update ROADMAP.md: M1 status `In Progress` → `Complete`.
2. Update `docs/graphics-kernel.md` implementation status table: mark all
   previously-`🔲 Stub` / `🔲 Planned` items that are actually implemented.
3. Add a "Month 16 — M1 closure" line to the implementation status table.

---

## Track 2 — GPU Timestamp Profiling Layer (M4 unblock)

### Motivation

`FrameStats::gpuTimeMs` and `FrameStats::cpuCullTimeMs` exist but are never
populated. The Vulkan backend has `writeTimestamp` + `readbackTimestamps` wired.
The missing piece is a `FrameTimingLayer` (or equivalent helper) that:
- Allocates a `QueryPoolHandle` for GPU timestamps in the Renderer.
- Records `resetQueryPool` + `writeTimestamp` at the start and end of the
  frame geometry pass.
- Reads results back via `readbackTimestamps` at the start of the next frame.
- Populates `FrameStats::gpuTimeMs`.

### Deliverables

1. New public header **`nexus/render/FrameTimingLayer.h`**:
   ```
   struct FrameTimingResult {
       double geometryPassMs  = 0.0;
       double shadowPassMs    = 0.0;
       double compositePassMs = 0.0;
       double totalGpuMs      = 0.0;
   };
   class FrameTimingLayer {
       bool create(IDevice& dev, uint32_t framesInFlight);
       void destroy(IDevice& dev) noexcept;
       void beginFrame(ICommandBuffer& cmd, uint32_t frameSlot);
       void endFrame  (ICommandBuffer& cmd, uint32_t frameSlot);
       [[nodiscard]] FrameTimingResult readback(IDevice& dev, uint32_t frameSlot);
       [[nodiscard]] bool isReady() const noexcept;
   };
   ```
2. Implementation in `src/kernel/src/render/FrameTimingLayer.cpp`.
3. Wire into Renderer: call `beginFrame`/`endFrame`/`readback` and populate
   `FrameStats::gpuTimeMs` from `FrameTimingResult::totalGpuMs`.
4. `perf_smoke` emits `gpu_time_ms=N` (populated on Null backend as 0.0;
   non-zero on Vulkan with real timestamps).
5. New test file `tests/kernel/test_FrameTimingLayer.cpp` (12 tests):
   - Lifecycle (create/destroy/double-destroy)
   - isReady before/after create
   - beginFrame/endFrame no-crash on Null
   - readback returns zero on Null
   - Renderer integration: gpuTimeMs is zero on Null, non-negative always
6. `FrameTimingLayer.h` added to API surface manifest.

### Exit criteria

1. All 12 tests pass on Null backend.
2. `perf_smoke` emits `gpu_time_ms` field.

---

## Track 3 — Shadow Pass End-to-End Draw Call Verification (M2)

### Motivation

The shadow atlas, depth texture, sampler, descriptor set, and lighting contract
are all implemented. But there is no test that verifies shadow draw calls appear
in `FrameStats` when the renderer runs a shadow pass (i.e. a shadow pipeline,
cascade contract, and scene nodes are all set). This is the final M2 gate test.

### Deliverables

1. Add to `tests/kernel/test_PassOrdering.cpp` (new suite `ShadowPassDrawCalls`):
   - `ShadowPassDrawCallsZeroWithNoContract` — no contract: 0 draw calls
   - `ShadowPassDrawCallsZeroWithNoPipeline` — contract set, no pipeline: 0 calls
   - `ShadowPassDrawCallsZeroWithNoNodes` — contract + pipeline, empty scene: 0 calls
   - `ShadowPassStatsInFrameStatsAfterRender` — contract + pipeline + node:
     shadow draw calls > 0 in FrameStats
2. Add M2 status update to ROADMAP.md: `Shadow Pipeline` → `In Progress (draw calls verified)`.

### Exit criteria

1. All 4 new tests pass on Null backend via RecordingScheduler path.

---

## Effort Allocation

- 15% — Track 1 (M1 closure): documentation only.
- 55% — Track 2 (GPU timing layer): new public API + impl + tests.
- 30% — Track 3 (shadow end-to-end): test-only (implementation exists).
