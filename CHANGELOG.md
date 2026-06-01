# Changelog

## [v0.10] — upcoming

### Graphics Kernel

#### Image-Based Lighting (IBL) — Track 1 (Month 43)
- `IBLSettings` struct — `enabled`, `intensity` (1.0), `diffuseScale` (1.0),
  `specularScale` (1.0), `mipLevels` (6).
- `Renderer::setIBLSettings` / `iblSettings()` — stored on renderer.
- `FrameStats::iblActive` — `true` when IBL compute dispatch ran.
- `FrameStats::iblMipLevels` — prefiltered mip levels sampled this frame.
- `Renderer::render()`: after GBuffer, when `enableIBL && settings.enabled`, dispatches
  compute in 8×8 tiles for specular/diffuse ambient contribution.
- New test file `tests/kernel/test_IBLighting.cpp` — 8 Null-backend tests.

#### Order-Independent Transparency (OIT) — Track 2 (Month 44)
- `OITSettings` struct — `enabled`, `maxLayers` (8), `weightScale` (1.0).
- `Renderer::setOITSettings` / `oitSettings()` — stored on renderer.
- `FrameStats::oitActive` — `true` when OIT resolve pass ran.
- `FrameStats::oitFragmentCount` — `width × height × maxLayers` fragments accumulated.
- `Renderer::render()`: after opaque composite, when `enableOIT && settings.enabled`,
  dispatches weighted-blend OIT resolve in 8×8 tiles.
- New test file `tests/kernel/test_OITransparency.cpp` — 8 Null-backend tests.

#### AMD FSR 4 Upscaler — Track 3 (Month 45)
- `NeuralBackend::FSR4 = 7` — stable enum value; extends existing NeuralBackend chain.
- `UpscalerBackend::FSR4 = 5`, `DenoiserBackend::FSR4` — matching backend identifiers.
- `FSR4Plugin` — header + implementation; same interface as FSR3Plugin; probes
  `ffxFsr4Upscaler*` entry points from the FidelityFX 4 SDK shared library.
- `NeuralRendererFactory::create(FSR4)` — FSR4 probe; falls back to FSR3 then
  deterministic baseline on SDK-absent Null backend.
- Auto-select chain extended: FSR4 probed before FSR3 in `createNeuralRenderer`.
- `perf_smoke` `--neural fsr4` flag wired.
- New test file `tests/kernel/test_FSR4Upscaler.cpp` — 7 Null-backend tests.

---

## [v0.9] — 2026-06-01

### Graphics Kernel

#### Depth of Field — Track 1 (Month 40)
- `DoFSettings` struct — `focalDistance` (10), `focalRange` (5), `maxCoC` (0.05),
  `sampleRadius` (8).
- `Renderer::setDoFSettings` / `dofSettings()` — stored on renderer.
- `FrameStats::dofActive` — `true` when DoF compute pass ran.
- `FrameStats::dofSampleCount` — `width × height × sampleRadius` this frame.
- `Renderer::render()`: after composite, when `enableDoF`, dispatches compute in
  8×8 tiles over the full render target.
- New test file `tests/kernel/test_DepthOfField.cpp` — 8 Null-backend tests.

#### Motion Blur — Track 2 (Month 41)
- `MotionBlurSettings` struct — `shutterAngle` (180°), `sampleCount` (8),
  `maxBlurRadius` (32 px).
- `Renderer::setMotionBlurSettings` / `motionBlurSettings()` — stored on renderer.
- `FrameStats::motionBlurActive` — `true` when motion blur dispatch ran.
- `FrameStats::motionBlurSampleCount` — `width × height × sampleCount` this frame.
- `Renderer::render()`: after DoF, when `enableMotionBlur`, dispatches compute
  reading the GBuffer velocity buffer.
- New test file `tests/kernel/test_MotionBlur.cpp` — 8 Null-backend tests.

#### Tone Mapping / HDR — Track 3 (Month 42)
- `ToneMappingOperator` enum — `Linear=0`, `Reinhard=1`, `ACES=2` (stable values).
- `ToneMappingSettings` struct — `exposure` (1.0), `whitePoint` (1.0),
  `operator_` (ACES).
- `Renderer::setToneMappingSettings` / `toneMappingSettings()` — stored on renderer.
- `FrameStats::tonemapActive` — `true` when tonemap pass ran.
- `FrameStats::tonemapOperator` — enum value used this frame.
- `Renderer::render()`: final pass before present, when `enableToneMapping`, dispatches
  exposure + curve compute in 8×8 tiles.
- New test file `tests/kernel/test_ToneMapping.cpp` — 8 Null-backend tests.

---

## [v0.8] — 2026-06-01

### Graphics Kernel

#### Screen-Space Ambient Occlusion — Track 1 (Month 37)
- `AOSettings` struct — `radius` (0.5), `bias` (0.025), `sampleCount` (16), `blurPasses` (2).
- `Renderer::setAOSettings` / `aoSettings()` — stored on renderer.
- `FrameStats::aoActive` — `true` when SSAO dispatch ran.
- `FrameStats::aoSampleCount` — `width × height × sampleCount` this frame.
- `Renderer::render()`: after GBuffer pass, when `settings.enableAO`, dispatches compute
  in 8×8 tiles over the full render target.
- New test file `tests/kernel/test_ScreenSpaceAO.cpp` — 8 Null-backend tests.

#### Screen-Space Reflections — Track 2 (Month 38)
- `SSRSettings` struct — `maxRaySteps` (64), `stepSize` (0.1), `thickness` (0.05),
  `fadeDistance` (10.0).
- `Renderer::setSSRSettings` / `ssrSettings()` — stored on renderer.
- `FrameStats::ssrActive` — `true` when SSR dispatch ran.
- `FrameStats::ssrRayCount` — `width × height` rays dispatched this frame.
- `Renderer::render()`: after GBuffer pass, when `settings.enableSSR`, dispatches compute
  in 8×8 tiles (before composite).
- New test file `tests/kernel/test_ScreenSpaceReflections.cpp` — 8 Null-backend tests.

#### Bloom Post-Process — Track 3 (Month 39)
- `BloomSettings` struct — `threshold` (1.0), `intensity` (0.04), `radius` (0.85),
  `passes` (5).
- `Renderer::setBloomSettings` / `bloomSettings()` — stored on renderer.
- `FrameStats::bloomActive` — `true` when bloom dispatch chain ran.
- `FrameStats::bloomPassCount` — `passes × 2` (downsample + upsample) this frame.
- `Renderer::render()`: after composite, when `settings.enableBloom`, dispatches
  `passes × 2` compute passes per mip level (Kawase downsample + bicubic upsample).
- New test file `tests/kernel/test_BloomPostProcess.cpp` — 8 Null-backend tests.

---

## [v0.7] — 2026-06-01

### Graphics Kernel

#### MSAA Resolve — Track 3 (Month 36)
- `DeviceCapabilities::maxMsaaSamples` — new cap field; Null backend reports `1` (no MSAA).
- `RendererSettings::msaaSamples` — requested MSAA sample count; clamped to
  `caps().maxMsaaSamples` each frame; `1` = disabled (default).
- `FrameStats::msaaSamples` — actual sample count used this frame (after cap clamp).
- `ICommandBuffer::resolveTexture(src, dst)` — default virtual no-op; Vulkan override
  issues `vkCmdResolveImage` when src sample count > 1.
- `Renderer::render()` stats reset block: `msaaSamples` populated as
  `min(settings.msaaSamples, caps().maxMsaaSamples)`.
- New test file `tests/kernel/test_MSAAResolve.cpp` — 7 Null-backend tests covering
  caps reporting, settings round-trip, stat clamping, and `resolveTexture` no-op.

#### Volumetric Lighting Pass — Track 2 (Month 35)
- `VolumetricSettings` struct — `enabled`, `fogDensity`, `scatteringCoeff`,
  `extinctionCoeff`, `froxelSlices`, `froxelResolutionDivisor`.
- `Renderer::setVolumetricSettings` / `volumetricSettings()` — stored on renderer.
- `FrameStats::volumetricActive` — `true` when the froxel compute pass ran.
- `FrameStats::volumetricFroxelCount` — `froxelW × froxelH × froxelSlices` total cells.
- `Renderer::render()`: after shadow pass, when `settings.enabled`, dispatches a compute
  pass (`cmd.dispatch(groupsX, groupsY, froxelSlices)`) for the froxel integration.
- New test file `tests/kernel/test_VolumetricLighting.cpp` — 8 Null-backend tests.

#### Variable-Rate Shading — Track 1 (Month 34)
- `ShadingRate` enum in `CommandBuffer.h` — `Rate1x1` (0) through `Rate4x4` (5); stable
  values for API-freeze purposes.
- `ICommandBuffer::setFragmentShadingRate(ShadingRate)` — default virtual no-op;
  Vulkan override issues `vkCmdSetFragmentShadingRateKHR`.
- `RendererSettings::enableVRS` — opt-in flag (default `false`); ignored when
  `caps().variableRateShading == false`.
- `RendererSettings::defaultShadingRate` — coarse rate applied to geometry pass
  (default `Rate2x2`).
- `FrameStats::vrsActive` — `true` when `setFragmentShadingRate` fired this frame.
- `Renderer::render()`: before geometry `beginRenderPass`, when
  `enableVRS && caps().variableRateShading`, calls `setFragmentShadingRate(defaultShadingRate)`
  and sets `vrsActive = true`; resets to `Rate1x1` after `endRenderPass`.
- New test file `tests/kernel/test_VariableRateShading.cpp` — 8 Null-backend tests.

---

## [v0.6] — 2026-06-01

### Graphics Kernel

#### RT Shader Binding Tables — Track 3 (Month 33)
- `HitGroup` struct — `closestHit` + optional `anyHit` + optional `intersection` shader handles.
- `ShaderBindingTableDesc` struct — `rayGenShader`, `missShaders[]`, `hitGroups[]`, owning
  `PipelineHandle`; passed to `createShaderBindingTable`.
- `IDevice::createShaderBindingTable(ShaderBindingTableDesc)` — default virtual packs groups
  into a stub `SBTHandle` via the existing `allocateSBT`; Vulkan backend populates
  `VkStridedDeviceAddressRegionKHR` records. Null backend returns a valid stub always.
- `ICommandBuffer::traceRaysWithSBT(SBTHandle, w, h, d)` — default virtual falls back to
  `traceRays(w, h, d)` when the SBT handle is invalid; Vulkan override issues
  `vkCmdTraceRaysKHR` with the four region pointers from the SBT buffer.
- `Renderer::setShaderBindingTable(SBTHandle)` / `shaderBindingTable()` — non-owning SBT
  slot; when valid, `render()` RT block issues `traceRaysWithSBT` instead of bare `traceRays`.
- New test file `tests/kernel/test_RTShaderBindingTable.cpp` — 8 Null-backend tests:
  `createShaderBindingTable` handle validity, pipeline-attached desc no-crash, distinct
  handles, default slot invalid, round-trip set/get, slot clear, render no-crash with SBT,
  and raster stats unaffected by SBT slot.

#### NGX Evaluation Parameter Wiring — Track 2 (Month 32)
- `DLSSPlugin`: `m_featureHandle` and `m_parametersHandle` slots; `AllocParameters`,
  `DestroyParameters`, `SetParameterULL`, `SetParameterF`, `SetParameterVoidP` entry
  points loaded alongside existing pfns.
- `DLSSPlugin::initNGX()` extended: on successful NGX Init, calls `AllocParameters` to
  obtain the per-evaluation parameter block.
- `DLSSPlugin::createFeatureHandle(VkCommandBuffer)` — new private method; calls
  `NVSDK_NGX_VULKAN_CreateFeature` with `featureId=1` (DLSS4) or `featureId=1000` (DLSS-RR)
  depending on `m_rrMode`; result stored in `m_featureHandle`.
- `DLSSPlugin::evaluateFeature(VkCommandBuffer, params)` — thin wrapper around
  `NVSDK_NGX_VULKAN_EvaluateFeature`; no-op when feature or parameter handle is null.
- `DLSSPlugin::upscale()` wired: populates NGX input parameters (color, depth,
  motion vectors, output, render/output resolution, jitter, reset) then calls
  `evaluateFeature`. Falls back to passthrough when feature handle acquisition fails.
- `DLSSPlugin::denoise()` wired: populates color, albedo, normal, motion vectors,
  exposure scale; dispatches via `evaluateFeature`. RR mode uses the same path — the
  correct feature ID was encoded at `createFeatureHandle` time.
- Destructor updated: releases `m_featureHandle`, `m_parametersHandle` before
  `m_ngxHandle` and SDK shutdown.

#### FSR 3 Upscaler Integration — Track 1 (Month 31)
- `NeuralBackend::FSR3` — new enum value (5, API-stable); selects AMD FidelityFX SR 3
  upscaler; falls back to `DeterministicFallbackNeuralRenderer` when SDK is absent.
- `FSR3Plugin` — new `INeuralRenderer` implementation; wraps the FidelityFX SDK via
  `dlopen`/`LoadLibrary`; `available()` true when `ffxFsr3UpscalerContextCreate` resolves.
  `activeUpscaler()` returns `UpscalerBackend::FSR3`; `activeDenoiser()` returns
  `DenoiserBackend::FSR3`. Full `ffxFsr3UpscalerContextDispatch` parameter wiring
  deferred to the Vulkan FFX backend integration milestone.
- `createNeuralRenderer()` `preferFSR` parameter now active — inserts the FSR3 probe
  after XeSS and before the OIDN fallback in the auto-select priority chain.
- `NeuralRendererFactory::create(FSR3, device)` — new factory case; always non-null.
- `perf_smoke` gains `--neural-backend fsr3` / `fsr` token in `parseNeuralBackend`.
- New test file `tests/kernel/test_FSR3Upscaler.cpp` — 7 Null-backend tests covering
  enum distinctness, factory non-null guarantee, SDK-absent fallback, renderer attach,
  upscaling-active gate on fallback, and stable enum value regression guard.

---

## [v0.5] — 2026-06-01

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
