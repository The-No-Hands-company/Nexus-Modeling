# Month 22 — RT/Mesh-Shader Production Path — M4 Completion

Status: **complete**. Closes M4 by adding the remaining RT production path API
and deterministic mode-gate tests.

Reference: [ROADMAP.md](../ROADMAP.md), [month-21-checklist.md](month-21-checklist.md).

---

## Track 1 — RT Production Path API

### Deliverables

1. `FrameStats::activeRenderMode` — `RenderMode`; populated from `m_settings.mode`
   after `selectRenderPath()` capability downgrade each frame.
2. `FrameStats::rtReflectionsActive` — `bool`; true when the RT reflection
   `traceRays()` dispatch ran this frame.
3. `Renderer::setRayTracingPipeline(PipelineHandle)` / `rayTracingPipeline()` —
   new non-owning pipeline slot for HybridRT / PathTrace modes.
4. `Renderer::render()` (scheduler path) dispatches `cmd.traceRays()` when:
   - `m_settings.enableRTReflect == true`
   - `m_settings.mode == HybridRT || PathTrace` (post-downgrade)
   - `m_ctx.caps().rayTracingTier >= 1`
   - `m_impl->rayTracingPipeline.valid()`
   All four must hold; Null backend always fails the caps check → no dispatch.

---

## Track 2 — Tests

New `tests/kernel/test_RTProductionPath.cpp` — 13 tests:
- `DefaultActiveRenderModeIsRasterize`
- `PathTraceModeDowngradesToRasterizeOnNullBackend`
- `HybridRTModeDowngradesToRasterizeOnNullBackend`
- `RasterizeModeRemainsRasterize`
- `ActiveRenderModeIsStableAcrossMultipleFrames`
- `RTPipelineDefaultIsInvalid`
- `RTPipelineRoundTrip`
- `ClearRTPipelineStoresInvalidHandle`
- `RTReflectionsInactiveByDefault`
- `RTReflectionsInactiveOnNullBackendEvenWhenEnabled`
- `RTReflectionsRequireValidRTPipeline`
- `RTReflectionsInactiveDoesNotAffectDrawCallCount`
- `PathTraceDowngradePreservesDenoisingStats`

### Exit criteria

All 13 tests pass. M4 milestone marked **Complete**. Full suite at 100%.
