# Changelog

## [v0.3] — upcoming

### Graphics Kernel

#### RT/Mesh-Shader Production Path — M4 Complete (Month 22)
- `FrameStats::activeRenderMode` — reflects the render mode actually used after
  capability downgrade (e.g. PathTrace requested on a no-RT backend → Rasterize).
- `Renderer::setRayTracingPipeline(PipelineHandle)` / `rayTracingPipeline()` —
  new API slot for the RT pipeline used in HybridRT and PathTrace modes.
- `FrameStats::rtReflectionsActive` — true when the RT reflections pass ran
  (`enableRTReflect` + HybridRT/PathTrace mode + RT caps + valid RT pipeline).
- `traceRays` dispatch wired into the scheduler render path, gated on all four
  conditions; safely no-ops on Null backend (RT caps absent → downgrade to Rasterize).
- **M4 milestone closed.** All M4 exit criteria are now satisfied.
- New test file `tests/kernel/test_RTProductionPath.cpp` — 13 tests.

#### Async-Compute Upscaler Scheduling + DLSS/XeSS Perf Gate (Month 21)
- `RendererSettings::enableUpscaling` — new flag; when true and a neural renderer
  is attached, `Renderer::render()` calls `INeuralRenderer::upscale()` after the
  denoiser pass.
- `FrameStats::upscalingActive` — true when the upscaler ran this frame.
- `FrameStats::activeUpscaler` — reflects the `UpscalerBackend` reported by the
  attached `INeuralRenderer` (`Bilinear`, `DLSS4`, `XeSS`, `FSR3`, or `None`).
- Denoiser and upscaler can run together in a single frame when both flags are set.
- DLSS/XeSS perf gate: `NeuralDispatchOverheadBelowCeilingOnNullBackend` asserts
  the average per-frame dispatch cost across 64 Null-backend frames stays under
  50 ms, preventing silent scheduling overhead regressions.
- New test file `tests/kernel/test_NeuralUpscaler.cpp` — 12 tests.

#### Async-Compute Denoiser Scheduling (Month 20)
- `Renderer` now accepts a non-owning `INeuralRenderer*` via `setNeuralRenderer`.
- `RendererSettings::enableDenoising` — new flag; when true and a neural renderer
  is attached, `Renderer::render()` calls `INeuralRenderer::denoise()` after the
  composite pass on the async-compute command slot.
- `FrameStats::denoisingActive` — true when the denoiser ran this frame.
- `FrameStats::activeDenoiser` — reflects the backend reported by the attached
  `INeuralRenderer` (`OIDN_CPU`, `DLSS4`, `XeSS`, `FSR3`, or `None`).
- New test file `tests/kernel/test_NeuralDenoiser.cpp` — 11 tests covering
  attach/detach contract, per-frame dispatch, backend reflection, and draw-call
  isolation. All tests use a `StubNeuralRenderer` on the Null backend.

#### CI Smoke Suite Hardening (Month 19)
- Fixed 6 broken softrast scenario scripts (`softrast_multi_object`,
  `softrast_textured_sphere`, `softrast_remesh_render`, `softrast_geometry_ops`,
  `softrast_sculpt_render`, `softrast_parametric_render`) — now produce required
  `summary.json` / `diagnostics.txt` / `deterministic_signature.txt` artifacts.
- Fixed `nexus_kernel_perf_smoke` build failure on environments without the
  Vulkan SDK by gating Vulkan includes behind `#ifdef NEXUS_BACKEND_VULKAN`.
  `KernelPerfSmoke.Null` and `KernelPerfSmoke.Determinism` now pass in CI.

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
