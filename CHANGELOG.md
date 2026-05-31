# Changelog

## [v0.3] — upcoming

### Graphics Kernel

#### TAA Renderer Integration (Month 18)
- `Renderer` now owns a `TemporalAccumulator` instance (`m_taaAccumulator`).
- `Renderer::render()` advances the TAA frame index and applies per-frame subpixel
  camera jitter via `Camera::setJitter` when `RendererSettings::enableTAA` is true.
  Jitter is cleared after each frame via a local jittered camera copy so frustum
  culling is never affected.
- `FrameStats::taaFrameIndex` — new field; reports the TAA accumulator frame counter
  (zero when TAA is disabled).
- New public API on `Renderer`:
  - `setTemporalAccumulationConfig(const TemporalAccumulationConfig&)`
  - `temporalAccumulationConfig() const`
  - `temporalAccumulationState() const`
- New test file `tests/kernel/test_TemporalAccumulation.cpp` — 27 tests covering
  `TemporalAccumulator` unit behaviour and Renderer TAA integration on the Null backend.

#### CPU Frustum-Cull Timing (Month 17)
- `FrameStats::cpuCullTimeMs` — populated from `std::chrono::steady_clock` around
  the `SceneGraph::collectVisible` call in `Renderer::render()`.
- `perf_smoke` now emits `cpu_cull_time_ms=<value>` alongside other per-frame fields.

#### Shadow Pipeline (Month 16–17)
- `FrameStats::shadowDrawCalls` — isolated shadow-pass draw call counter, zero when
  the shadow pipeline is not active or no eligible nodes exist.
- GPU timestamp ring (`FrameTimingLayer`) — 12 tests; `FrameStats::gpuTimeMs` populated.

### API Surface

- `TemporalAccumulationConfig`, `TemporalAccumulationState`, `TemporalAccumulator`
  headers are now part of the tracked API surface manifest.

---

## [v0.2] — released

- Gaussian splat pass (`GaussianSplatPass`) — CPU-side counter path, Null-backend safe.
- Descriptor binder RAII wrappers — `CompositeDescriptorSet`, `MaterialTableDescriptorSet`,
  `ShadowDescriptorSet`.
- Shadow map target (`ShadowMapTarget`) — depth texture + cascade lifecycle.
- GBuffer texture management.
- Render path parity formally de-scoped; scheduler path is authoritative.
- `FrameStats` — initial fields: `drawCalls`, `triangles`, `meshlets`, `splatDrawCalls`,
  `submittedSplats`, `projectedSplats`.
