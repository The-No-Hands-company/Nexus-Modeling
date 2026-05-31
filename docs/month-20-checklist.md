# Month 20 — Async-Compute Denoiser Scheduling

Status: **complete**. One track: wire `INeuralRenderer` into `Renderer` so the
post-composite denoising pass can be dispatched (on a real GPU, over the
async-compute queue). Tests use a `StubNeuralRenderer` on the Null backend.

Reference: [ROADMAP.md](../ROADMAP.md), [month-18-checklist.md](month-18-checklist.md).

---

## Track 1 — Neural Renderer Integration

### Motivation

`INeuralRenderer` (DLSS4 / XeSS / OIDN) exists but is never called by `Renderer`.
Wiring it closes M4's async-compute denoising milestone and provides the call-site
that future async-compute queue dispatch will slot into.

### Deliverables

1. `RendererSettings::enableDenoising` — new `bool` flag (default `false`).
2. `FrameStats::denoisingActive` — `bool`; true when denoiser ran this frame.
3. `FrameStats::activeDenoiser` — `nexus::neural::DenoiserBackend`; backend used.
4. `Renderer::setNeuralRenderer(INeuralRenderer*)` / `neuralRenderer()` accessors.
5. `Renderer::render()` calls `m_neuralRenderer->denoise()` after the composite
   pass when `enableDenoising == true` and `m_neuralRenderer != nullptr`.
6. New `tests/kernel/test_NeuralDenoiser.cpp` — 11 tests:
   - `DefaultDenoisingIsInactive`
   - `AttachDetachRoundTrip`
   - `DenoisingInactiveByDefaultAfterRender`
   - `DenoisingInactiveWhenEnabledButNoRendererAttached`
   - `DenoisingInactiveWhenRendererAttachedButDisabled`
   - `DenoisingActiveWhenEnabledAndRendererAttached`
   - `ActiveDenoiserMatchesStubBackend`
   - `DenoiseCalledOncePerFrame`
   - `DisablingDenoisingStopsDenoiseCall`
   - `DetachingRendererStopsDenoiseCall`
   - `DenoisingActiveDoesNotAffectDrawCallCount`

### Exit criteria

All 11 tests pass on Null backend. Full suite remains at 100%.

---

## Effort Allocation

- 80% — Track 1: implementation + 11 tests.
- 20% — Track 3: docs (`CHANGELOG.md`, `graphics-kernel.md`, `ROADMAP.md`, checklist).
