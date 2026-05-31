# Kernel Contribution Guide

This guide covers adding or changing code in `src/kernel/` — the C++23 graphics kernel that powers Nexus Modeling. Read CONTRIBUTING.md first for the general workflow.

## Repository layout

```
src/kernel/
  include/nexus/          # Public headers — API surface; all changes tracked in manifest
    gfx/                  # Device, command buffer, types, Vulkan backend entry
    render/               # Renderer, camera, scene graph, RT/mesh-shader passes
    neural/               # INeuralRenderer interface (DLSS4 / XeSS / OIDN)
    geometry/             # Mesh, boolean ops, remesh, bevel, inset, etc.
    parametric/           # Constraint solver, parametric samples, serialization
    animation/            # Skeleton, animation core, serialization
    eval/                 # EvalGraph, expression nodes, geometry nodes
    eval_ext/             # Subgraph, registry, serialization
    scene/                # NodeScene, serialization
    sculpt/               # Brush, SculptSession, stroke history
    sim/                  # Cloth, fluid, simulation coupling
    asset/                # SceneAsset, importer, dependency graph
    automation/           # Per-domain scripting extension headers
    script/               # Expression evaluation
  src/                    # Private implementation files (not part of public API)
    render/               # Renderer.cpp and render-pass implementations
    backend/null/         # Null backend — always-on CI baseline
    backend/vulkan/       # Vulkan backend — requires NEXUS_BACKEND_VULKAN=ON

tests/kernel/             # GoogleTest suite — one .cpp per feature area
  fixtures/               # Shared test data, API surface manifest, golden files
  perf_smoke.cpp          # Frame-timing and determinism regression binary

tools/                    # CI scripts and scenario runners
```

## Null backend parity requirement

Every new API must work on the Null backend. The Null backend:
- Accepts all device/resource creation calls and returns valid handles.
- Executes command-buffer calls without side effects.
- Reports `rayTracingTier = 0`, so `selectRenderPath()` always downgrades
  `PathTrace` / `HybridRT` → `Rasterize`.

This lets CI run the full test suite without GPU hardware.

## Adding a new `FrameStats` field

1. Add the field to `FrameStats` in `src/kernel/include/nexus/render/Renderer.h`.
2. Populate it inside `Renderer::render()` in `src/kernel/src/render/Renderer.cpp`.
3. Add tests in `tests/kernel/` — at minimum: default value, populated-after-render value,
   and a Null-backend no-crash path.

## Adding a new `RendererSettings` flag

1. Add the field (with a `false` default) to `RendererSettings`.
2. Gate the dispatch inside `render()` on the new flag, plus any capability checks.
3. Add tests: flag-off default, flag-on with preconditions met, flag-on with preconditions unmet.

## Adding a new public header

1. Place it under `src/kernel/include/nexus/<domain>/`.
2. Add the path to `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt`.
3. Update `docs/api-contract-alpha-summary.md` — increment domain header count and total.
4. Verify `./build/tests/nexus_kernel_tests --gtest_filter="ApiFreezeAudit.*"` passes.

## Adding a new test file

1. Create `tests/kernel/test_<Feature>.cpp`.
2. Add it to `nexus_kernel_tests` sources in `tests/CMakeLists.txt`.
3. Use the `NullDevice` / `NullRenderContext` fixture pattern from existing tests; avoid
   constructing your own backend objects from scratch.
4. Run `ctest --test-dir build -R <TestSuite> --output-on-failure` to confirm the new
   tests are discovered and pass before submitting.

## Vulkan-only code

Guard Vulkan-specific includes and code with `#ifdef NEXUS_BACKEND_VULKAN`. The CI
baseline builds without a Vulkan SDK — unconditional Vulkan includes break the build.

## RendererSettings / render path

`Renderer::render()` follows this sequence every frame:

1. `selectRenderPath()` — capability downgrade; sets `m_settings.mode` to the achievable mode.
2. Snapshot `m_stats.activeRenderMode = m_settings.mode`.
3. Frustum cull + draw calls (geometry pass, shadow pass, composite pass).
4. Gaussian splat pass.
5. Neural denoiser dispatch (gated on `enableDenoising` + `m_neuralRenderer != nullptr`).
6. Neural upscaler dispatch (gated on `enableUpscaling` + `m_neuralRenderer != nullptr`).
7. RT reflections dispatch (gated on all four: `enableRTReflect` + RT mode + RT caps tier ≥ 1 + valid RT pipeline).
8. Render graph validation.

New passes should be inserted at the appropriate point in this sequence and gated on the relevant `RendererSettings` flag.

## RT/mesh-shader paths

- `Renderer::setRayTracingPipeline(PipelineHandle)` stores a non-owning pipeline handle
  in `Renderer::Impl`.
- `cmd.traceRays(width, height, depth)` is a no-op on the Null backend (RT caps absent).
- Tests must verify the 4-way gate: each of the four preconditions independently blocks dispatch.

## Performance smoke

`tests/kernel/perf_smoke.cpp` runs outside GoogleTest. It measures per-frame dispatch cost
over 120 frames (Null backend) and checks determinism over 3 × 64 frame runs.

- Do not add artificial sleeps or blocking waits inside `render()`.
- Neural dispatch paths must stay under a 50 ms per-frame ceiling on the Null backend
  (enforced by `NeuralDispatchOverheadBelowCeilingOnNullBackend` in `test_NeuralUpscaler.cpp`).

## CI checklist

Before marking a PR ready for review:

- [ ] `cmake --build build -j$(nproc)` — clean build, no warnings promoted to errors
- [ ] `ctest --test-dir build --output-on-failure` — all 2210+ tests pass
- [ ] `ApiFreezeAudit.*` passes if any public header was touched
- [ ] `KernelPerfSmoke.Null` and `KernelPerfSmoke.Determinism` are not marked "Not Run"
- [ ] CHANGELOG.md updated under `[v0.3]` (or next release) with a one-liner for the change
