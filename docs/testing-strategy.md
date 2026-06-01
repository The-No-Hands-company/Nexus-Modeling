# Testing Strategy

This document defines how Nexus Modeling validates kernel quality as the codebase scales.

## Goals

- Catch regressions in correctness-critical systems early.
- Keep test execution fast enough for local development loops.
- Ensure GPU paths remain verifiable in CI/headless environments.
- Grow coverage with feature complexity, not only line count.

## Current baseline (v0.12)

- Framework: GoogleTest
- Discovered tests: 2404 (all pass on Null backend; Vulkan-capability tests skip cleanly in headless CI)
- Scope currently covered:
  - Type system and flag semantics
  - Geometry mesh, boolean ops, bevel/chamfer, remesh, inset, hard-surface workflow, modeling shell
  - Meshlet encoding and LOD contracts
  - Parametric solver, constraint graph, serialization, samples
  - Procedural geometry and animation
  - Evaluation graph, expression nodes, subgraph registry and serialization
  - Node scene and scene asset lifecycle, importer, text adapter, dependency graph
  - Animation core, serialization, and skeleton retargeter
  - Simulation: cloth solver, fluid solver, simulation coupling harness
  - Sculpt session, brush, and stroke history serialization
  - Script expression evaluation
  - Camera math and inverse path
  - Scene graph create/find/remove/traversal, frustum culling
  - Null backend lifecycle and command API no-crash coverage
  - RenderContext creation and queue submission wrappers
  - Renderer: behavior, integration, pass ordering, render path parity
  - GBuffer mesh shaders, Gaussian splat pass, descriptor binder
  - Shadow map target and shadow mesh shaders
  - Frame timing layer (GPU timestamp ring)
  - Temporal accumulation (TAA) — 27 tests
  - Async-compute neural denoiser scheduling — 11 tests
  - Async-compute neural upscaler scheduling + DLSS/XeSS perf gate — 12 tests
  - RT/mesh-shader production path (mode-gate, pipeline round-trip, 4-way RT gate) — 13 tests
  - RT pipeline creation contract (Null backend: createRayTracingPipeline, BLAS/TLAS) — 7 tests
  - Scene BVH integration (buildAccelStructs, rebuildTLAS, tlasInstanceCount) — 8 tests
  - Mesh shader production path (enableMeshShaders flag, meshShaderDrawCalls, pipeline slots) — 8 tests
  - RT descriptor layout + TLAS-backed traceRays dispatch (TraceRaysWithSceneTLASOnTier1) — 1 Vulkan test
  - DLSS Ray Reconstruction (DenoiserBackend::DLSS_RR, NeuralBackend::DLSS_RR, factory) — 6 tests
  - FSR 3 upscaler integration (NeuralBackend::FSR3, FSR3Plugin, factory) — 7 tests
  - RT shader binding tables (ShaderBindingTableDesc, createShaderBindingTable, traceRaysWithSBT) — 8 tests
  - Variable-Rate Shading (ShadingRate, enableVRS, vrsActive) — 8 tests
  - Volumetric Lighting pass (VolumetricSettings, froxel dispatch, stats) — 8 tests
  - MSAA resolve (maxMsaaSamples cap, msaaSamples setting/stat, resolveTexture) — 7 tests
  - Screen-Space AO (AOSettings, enableAO, aoActive, aoSampleCount) — 8 tests
  - Screen-Space Reflections (SSRSettings, enableSSR, ssrActive, ssrRayCount) — 8 tests
  - Bloom post-process (BloomSettings, enableBloom, bloomActive, bloomPassCount) — 8 tests
  - Depth of Field (DoFSettings, enableDoF, dofActive, dofSampleCount) — 8 tests
  - Motion Blur (MotionBlurSettings, enableMotionBlur, motionBlurActive, motionBlurSampleCount) — 8 tests
  - Tone Mapping / HDR (ToneMappingOperator, ToneMappingSettings, tonemapActive, tonemapOperator) — 8 tests
  - Image-Based Lighting (IBLSettings, enableIBL, iblActive, iblMipLevels) — 8 tests
  - Order-Independent Transparency (OITSettings, enableOIT, oitActive, oitFragmentCount) — 8 tests
  - AMD FSR 4 upscaler (NeuralBackend::FSR4, FSR4Plugin, factory, enum freeze) — 7 tests
  - Software rasterizer and softrast scenario/extension coverage
  - Automation scripting extension surface — all 28 extension headers
  - CI scenario artifact validation (6 softrast scenarios)
  - API surface freeze audit (93-header manifest)
  - Tooling app CI smoke (headless surfaces)
  - Vulkan shader/pipeline/frame-scheduler integration tests (headless-aware)

## Test layers

1. Fast unit tests
   - Pure logic, math, and data structure semantics.
   - No external runtime dependency.

2. Backend contract tests
   - Validate that IDevice and ICommandBuffer contracts are honored across backends.
   - Null backend acts as always-on CI baseline.

3. Vulkan integration tests
   - Exercise initialization, shader/pipeline creation, and frame scheduling.
   - Headless-capable where possible.

4. Scenario/regression tests
   - Reproduce previously fixed bugs.
   - Include synchronization and transition ordering where practical.
   - Six softrast scenarios produce deterministic artifact bundles (summary.json,
     diagnostics.txt, deterministic_signature.txt) validated in CI.

5. Performance smoke tests
   - `KernelPerfSmoke.Null` — 120 frames, baseline frame-time regression guard.
   - `KernelPerfSmoke.Determinism` — 64 frames × 3 runs, deterministic frame output.
   - Neural dispatch overhead ceiling: average per-frame cost < 50 ms on Null backend.

## Required local quality gates

Run before merging behavior changes:

1. Build:
   - cmake --build build -j$(nproc)
2. Tests:
   - ctest --test-dir build --output-on-failure

## Authoring guidance

- Name tests by behavior, not implementation detail.
- Keep assertions specific and deterministic.
- Avoid over-mocking; prefer real interface interactions where possible.
- Add tests in the same change that introduces behavior changes.
- New render-path features require Null-backend test coverage for all mode-gate branches.
- Perf-sensitive dispatch paths require an overhead ceiling assertion (see NeuralUpscaler tests).
- API surface changes require a manifest update and `ApiFreezeAudit` to pass.

## Expansion roadmap

1. Vulkan RT integration tests
   - Full `traceRays` dispatch with real RT pipeline on physical hardware (v0.4 done; v0.5 expands).
   - DLSS4/XeSS live integration on GPU-capable CI runners.

2. Synchronization regression tests
   - Targeted barrier and transition ordering cases.

3. Resource lifecycle stress tests
   - Create/destroy cycles and resize churn under high frame count.

4. Golden output tests
   - Deterministic image/hash checks for selected render scenarios.
