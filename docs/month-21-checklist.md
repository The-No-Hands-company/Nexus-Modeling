# Month 21 — Async-Compute Upscaler Scheduling + DLSS/XeSS Perf Gate

Status: **complete**. Extends the Month 20 neural renderer integration with
upscaler scheduling and adds the M4 DLSS/XeSS performance gate.

Reference: [ROADMAP.md](../ROADMAP.md), [month-20-checklist.md](month-20-checklist.md).

---

## Track 1 — Upscaler Scheduling

### Motivation

The denoiser path (Month 20) established the dispatch pattern. The upscaler
mirrors it: `INeuralRenderer::upscale()` runs after the denoiser on the
async-compute queue slot, feeding the TAA resolve / tone-map pass.

### Deliverables

1. `RendererSettings::enableUpscaling` — new `bool` flag (default `false`).
2. `FrameStats::upscalingActive` — `bool`; true when upscaler ran this frame.
3. `FrameStats::activeUpscaler` — `nexus::neural::UpscalerBackend`; backend used.
4. `Renderer::render()` calls `m_neuralRenderer->upscale()` after the denoiser
   block when `enableUpscaling == true` and `m_neuralRenderer != nullptr`.

---

## Track 2 — DLSS/XeSS Perf Gate

### Motivation

M4 requires a performance gate for the neural dispatch path. The gate verifies
the combined denoiser+upscaler overhead on the Null backend stays below a
regression ceiling (50 ms average over 64 frames).

### Deliverables

1. `NeuralDispatchOverheadBelowCeilingOnNullBackend` — timing gate test.
2. `NeuralDispatchDeterministicAcrossRuns` — verifies dispatch is side-effect-free.

---

## Track 3 — Tests

New `tests/kernel/test_NeuralUpscaler.cpp` — 12 tests:
- `DefaultUpscalingIsInactive`
- `UpscalingInactiveByDefaultAfterRender`
- `UpscalingInactiveWhenEnabledButNoRendererAttached`
- `UpscalingInactiveWhenRendererAttachedButDisabled`
- `UpscalingActiveWhenEnabledAndRendererAttached`
- `ActiveUpscalerMatchesStubBackend`
- `UpscaleCalledOncePerFrame`
- `DisablingUpscalingStopsUpscaleCall`
- `BothDenoiseAndUpscaleRunInSameFrame`
- `UpscalingDoesNotAffectDrawCallCount`
- `NeuralDispatchOverheadBelowCeilingOnNullBackend`
- `NeuralDispatchDeterministicAcrossRuns`

### Exit criteria

All 12 tests pass. Full suite remains at 100%.
