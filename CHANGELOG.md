# Changelog

## [v0.5] — upcoming

### Graphics Kernel

#### DLSS Ray Reconstruction — Track 3 (Month 30)
- `DenoiserBackend::DLSS_RR` — new enum value, distinct from `DLSS4`; identifies the
  NVIDIA DLSS Ray Reconstruction denoiser path (`NVSDK_NGX_Feature_RayReconstruction`).
- `NeuralBackend::DLSS_RR` — new factory selection enum value; inserts between `DLSS4`
  and `XeSS` in the ordering (value 2, stable for API-freeze purposes).
- `DLSSPlugin` updated: constructor gains `bool rrMode = false` parameter; `activeDenoiser()`
  returns `DLSS_RR` when `m_rrMode && m_ngxAvailable`; `denoise()` branches to the RR
  code path (passthrough until full NGX parameter wiring in the Vulkan RT milestone).
- `NeuralRendererFactory::create(DLSS_RR, device)` — new factory case; constructs a
  `DLSSPlugin(rrMode=true)` when `NEXUS_BACKEND_VULKAN + NEXUS_ENABLE_DLSS` are defined
  and NGX initialises; falls back to `DeterministicFallbackNeuralRenderer` otherwise.
- `perf_smoke` gains `--neural-backend dlss-rr` / `dlss_rr` token in `parseNeuralBackend`.
- New test file `tests/kernel/test_DLSSRayReconstruction.cpp` — 6 Null-backend tests:
  enum distinctness, factory non-null, SDK-absent fallback, renderer attach no-crash,
  denoising-active gate on fallback, and stable enum value regression guard.

#### Ray-Generation Shader Integration — Track 2 (Month 29)
- `DescriptorType::AccelerationStructure` — new enum value (6) for RT TLAS/BLAS bindings;
  `DescriptorBindingDesc::accelStruct` field added alongside existing buffer/texture/sampler.
- `Renderer::setSceneTLAS(AccelStructHandle)` / `sceneTLAS()` — non-owning TLAS slot;
  when valid, the renderer allocates an RT descriptor set at set=0 binding=0 before
  `traceRays` and defers its release to the end of the current frame slot.
- `Renderer::render()` RT dispatch block updated: when `sceneTLAS().valid()`, binds an
  `AccelerationStructure` descriptor set on the command buffer before `cmd.traceRays()`.
  On the Null backend `allocateDescriptorSet` is a no-op, so this path is always safe.
- `test_VulkanRTDispatch.cpp` extended with `TraceRaysWithSceneTLASOnTier1` (10th test):
  builds a single-triangle scene, calls `buildAccelStructs`, attaches the TLAS via
  `setSceneTLAS`, renders one frame, and asserts `rtReflectionsActive == true` and
  `tlasInstanceCount > 0` on Tier-1 RT hardware. Skips cleanly in headless CI.

#### Mesh Shader Production Path — Track 4 (Month 28)
- `RendererSettings::enableMeshShaders` — new flag (default `true`); when false, the renderer
  skips `drawMeshTasks` dispatch even if the device reports `meshShaders == true`.
- `FrameStats::meshShaderDrawCalls` — count of draw calls dispatched via `drawMeshTasks` this
  frame (geometry + shadow passes combined). Reset to 0 at the start of every frame.
- `Renderer::render()` geometry and shadow paths: mesh dispatch now gated on
  `m_settings.enableMeshShaders && m_ctx.caps().meshShaders`; each firing increments
  `m_stats.meshShaderDrawCalls` via per-call-site locals aggregated after submission.
- New test file `tests/kernel/test_MeshShaderProductionPath.cpp` — 8 Null-backend tests:
  default flag state, round-trip flag set/clear, no dispatch without caps, no dispatch with
  flag disabled, draw-call isolation from raster counter, per-frame reset, and pipeline-slot
  API (`setFallbackMeshPipeline`, `setShadowMeshPipeline`) smoke on Null backend.

#### Scene BVH Integration — Track 1 (Month 27)
- `MeshRef::vertexCount` — new field (was missing alongside `indexCount`); required by
  `buildBLAS` to correctly size the acceleration structure on Vulkan hardware.
- `SceneGraph::buildAccelStructs(IDevice&)` — iterates all mesh nodes, builds a BLAS for
  each node that has vertex+index buffers but no BLAS yet, then calls `rebuildTLAS`.
  Returns the count of new BLASes built. Idempotent: nodes with an existing BLAS are skipped.
- `FrameStats::tlasInstanceCount` — count of scene nodes with a valid BLAS this frame;
  populated in the RT dispatch block when `rtReflectionsActive` fires.
- `Renderer::render()` RT block extended: traverses `scene` to count BLAS-equipped nodes
  and writes the result to `m_stats.tlasInstanceCount` when RT dispatch runs.
- New test file `tests/kernel/test_SceneBVH.cpp` — 8 Null-backend tests covering empty
  scene, single-node BLAS build, TLAS validity after build, idempotent second call, multi-
  node build count, `tlasInstanceCount` gate behavior, and `clearAndDestroyTLAS`.

#### v0.4 Release + RT Pipeline Unit Tests (Month 26)
- Project version bumped to `0.4.0`; v0.4 release stamped 2026-05-31.
- Fixed `test_VulkanRTDispatch.cpp`: restored full end-to-end
  `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` test using the correct
  `IDevice::createRayTracingPipeline(RayTracingPipelineDesc)` API (Month 24/25 had
  used the non-existent `createRTPipeline`). Test count: 8 → 9.
- New test file `tests/kernel/test_RTPipelineCreation.cpp` — 7 Null-backend unit tests
  covering RT pipeline creation contract: default handle validity, Null backend create,
  handle round-trip on Renderer, distinct handles per create, minimal/full desc configs,
  and Null backend BLAS/TLAS create.
- `docs/v0.5-planning.md` — planning document for DLSS Ray Reconstruction, full RT
  shader pipeline (BVH scene integration), and mesh shader production path completion.

---

## [v0.4] — 2026-05-31

### Graphics Kernel

#### Hardware Perf Gates + RT Dispatch Test Fix (Month 25)
- Fixed `test_VulkanRTDispatch.cpp`: removed non-existent `createRTPipeline` / `RTPipelineDesc`
  call; replaced with `RTGateFourConditionsOnVulkan` — verifies that on Tier-1 Vulkan hardware
  with `enableRTReflect + HybridRT` mode, `rtReflectionsActive` stays false when no RT pipeline
  is registered (gate condition 4). Test count drops by 0 net (same file, same test count: 8).
- `perf_smoke` extended with three new CLI flags:
  - `--rt` — enables `enableRTReflect + HybridRT` mode in the renderer; exercises the RT
    gate evaluation path every frame.
  - `--neural-backend <auto|dlss|dlss4|xess|oidn|bilinear>` — attaches a
    `NeuralRendererFactory::create()` renderer with upscaling and denoising enabled.
  - `--gpu-ceiling-ms <value>` — asserts `gpuTimeMs < value`; only enforced when
    `gpuTimeMs > 0` (real GPU timing available); exit code 3 on violation.
  - `--neural-overhead-ceiling-ms <value>` — triggers two-phase measurement (with/without
    neural) and asserts the per-frame delta is below the ceiling; exit code 3 on violation.
- `perf_smoke` gracefully skips Vulkan-mode runs (exit 0, prints "SKIPPED: …") when Vulkan
  is unavailable, so the new ctest entries are safe in headless CI.
- `nexus_neural` added to `nexus_kernel_perf_smoke` link libraries.
- New ctest entries: `KernelPerfSmoke.VulkanRT` (120 frames, `--rt`, 33 ms GPU ceiling) and
  `KernelPerfSmoke.DLSSSteadyState` (64 frames, `--neural-backend dlss`, 4 ms overhead ceiling).
- Report now emits `rt_reflections_active`, `upscaling_active`, `denoising_active`, and
  `neural_overhead_ms` fields alongside existing fields.

#### VulkanNeuralRenderer + Live RT Dispatch Tests (Month 24)
- Fixed `DLSSPlugin::activeDenoiser()` / `activeUpscaler()` — now return `DLSS4` when NGX
  initialises successfully (were always returning `None`).
- Fixed `XeSSPlugin::activeUpscaler()` — returns `XeSS` when the XeSS context is live.
- Fixed `OIDNDenoiser::activeDenoiser()` — returns `OIDN_CPU` when `oidnNewDevice` succeeds.
- `NeuralBackend` enum added to `nexus/neural/NeuralRenderer.h` (`Auto`, `DLSS4`, `XeSS`,
  `OIDN_CPU`, `Bilinear`).
- `NeuralRendererFactory::create(NeuralBackend, IDevice&)` — named factory; `Auto` reuses the
  priority chain; explicit backends short-circuit to the requested plugin or fall back to
  `Bilinear`; never returns `nullptr`.
- New test file `tests/kernel/test_VulkanRTDispatch.cpp` — 8 Vulkan-only tests (registered
  under `if(NEXUS_BACKEND_VULKAN)` in `tests/CMakeLists.txt`). All skip cleanly when Vulkan
  is unavailable, `rayTracingTier < 1`, or the relevant SDK is absent. Covers: Vulkan context
  creation, RT capability reporting, `traceRays` dispatch on Tier-1 hardware, factory
  `Auto`/`Bilinear`/`DLSS4`/`XeSS`/`OIDN_CPU` backends, end-to-end `rtReflectionsActive`
  and `upscalingActive` / `denoisingActive` stats.

#### v0.3 Release + M5 Contributor Scale-Up (Month 23)
- Project version bumped to `0.3.0` in CMakeLists.txt; v0.3 release stamped 2026-05-31.
- `docs/api-contract-alpha-summary.md` updated to reflect 93-header manifest (was 39 at alpha
  baseline); full domain ownership table and per-domain delta summary added.
- `docs/testing-strategy.md` updated to 2210-test baseline; scope section expanded to cover
  all M1–M4 test areas including perf smoke and scenario artifact validation.
- `CONTRIBUTING.md` expanded with domain ownership map, API surface rules, and test authoring
  guidance.
- New `docs/contributing-kernel.md` — detailed kernel contribution guide covering Null backend
  parity, public header lifecycle, test authoring patterns, and the render-pass sequence.
- New `docs/v0.4-planning.md` — planning document for live DLSS4/XeSS integration on Vulkan
  RT hardware (VulkanNeuralRenderer, live RT dispatch tests, hardware perf gates).
- `docs/getting-started.md` test-suite table updated with v0.3 totals (2210 tests).

---

## [v0.3] — 2026-05-31

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
