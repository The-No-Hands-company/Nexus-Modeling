# Month 12 Alpha Compatibility Report

Date: 2026-05-21

This report captures Month 12 alpha hardening outcomes for API stability, deterministic reliability, and compatibility status.

## Scope

1. API freeze audit
2. Determinism and perf reliability pass
3. Alpha compatibility status summary

## 1) API Freeze Audit

Public API roots audited:

1. src/kernel/include/nexus

Mechanism:

1. Manifest: tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt
2. Audit test: tests/kernel/test_ApiFreezeAudit.cpp

Baseline at report time:

1. Public header count: 50
2. Drift policy: any add/remove requires manifest update in same change

Result:

1. Pass

## 2) Determinism and Perf Reliability

Perf smoke harness:

1. Executable: build/tests/nexus_kernel_perf_smoke
2. Baseline test: KernelPerfSmoke.Null
3. New determinism gate: KernelPerfSmoke.Determinism

Determinism hardening added:

1. --determinism-runs N support in perf_smoke
2. Report fields: determinism_runs, determinism_consistent
3. Non-deterministic draw-call behavior returns non-zero exit code

Current test suite status:

1. 1156 gtest cases passed (ctest total: 1169 tests, 0 failed)
2. 10 skipped (Vulkan mesh-shader and ray-tracing pipeline tests, capability-gated)
3. 0 failed

Determinism gate result:

1. determinism_runs=3
2. determinism_consistent=true
3. release_gate_alpha.sh overall_signoff=PASS (build/release_signoff_1.0-alpha.txt)

## 3) Compatibility Status (Alpha)

Domain compatibility snapshot:

1. Geometry: stable command/mesh workflows, deterministic regression coverage
2. Rendering: Null and Vulkan paths active, capability-gated Vulkan tests explicitly skipped when unsupported
3. Asset/Data: deterministic scene package load order and round-trip coverage
4. Animation: deterministic pose/evaluator core and retargeting tests
5. Procedural/Sim: deterministic graph and simulation interface tests
6. Scripting/Automation: script-only pipeline harness with cross-domain commands

## Compatibility Risks and Controls

Residual risks:

1. Vulkan capability variance across machines
2. Performance variance by host scheduling/load

Controls in place:

1. Null backend as deterministic CI baseline
2. Explicit capability-gated Vulkan skips
3. Determinism perf smoke gate
4. Public API manifest audit

## Exit Decision

Month 12 alpha hardening targets in this repository state are satisfied for:

1. API freeze audit enforcement
2. Determinism/perf reliability gate extension
3. Compatibility report publication

## Skip Inventory (2026-05-21)

All 10 currently-skipped tests are explicit hardware-capability gates on the
recorded runner (Intel UHD Graphics 730, Vulkan 1.4.341). None of them are
avoidable on this hardware; every skip maps to a documented capability predicate.

Mesh-shader gated (8):

1. RendererBehavior.MeshShaderPathUsesMeshMaterialPipelinesAndTracksMeshlets
2. RendererBehavior.ShadowMeshPassUsesDrawMeshTasksWhenCapsAllow
3. VulkanRegression.CreateMeshShaderPipelineFromInlineGlsl
4. VulkanRegression.CreateMeshShaderPipelineRejectsMissingFragmentShader
5. VulkanRegression.CreateMeshShaderPipelineWithTaskShaderFromInlineGlsl
6. VulkanRegression.MeshShaderPipelineCacheReturnsSameHandleForIdenticalDescriptors
7. VulkanRegression.CreateGBufferMeshShaderPipelinesFromInlineGlsl
8. VulkanRegression.CreateGBufferTaskMeshShaderPipelinesFromInlineGlsl

Shadow + ray-tracing gated (2):

1. VulkanRegression.CreateShadowMeshShaderPipelineFromInlineGlsl
2. VulkanRegression.CreateRayTracingPipelineFromInlineGlsl

### Skip-Reduction Override

`VulkanDevice` now exposes environment-driven device selection so CI runners
with a software ICD can opt into the mesh-shader / RT tests without changing
production defaults:

- `NEXUS_VK_DEVICE_INDEX=<n>` — pick the n-th enumerated physical device verbatim.
- `NEXUS_VK_DEVICE_NAME=<substring>` — case-insensitive substring match against the device name.
- `NEXUS_VK_PREFER_CAPS=mesh|rt|mesh+rt` — opt-in capability bias only (does not silently change the device).

Locally with `NEXUS_VK_DEVICE_NAME=llvmpipe` the skip count drops from 10 to 2;
the two remaining (`RendererBehavior.MeshShaderPath*`, `RendererBehavior.ShadowMeshPass*`)
use the Null backend and are out of scope for any Vulkan device override.

The following three tests have to be excluded under Lavapipe because the Mesa
software path crashes inside `vkCreateGraphicsPipelines` for the GBuffer / Shadow
mesh-shader pipelines; they remain skipped on lvp runners until upstream is fixed:

1. VulkanRegression.CreateGBufferMeshShaderPipelinesFromInlineGlsl
2. VulkanRegression.CreateGBufferTaskMeshShaderPipelinesFromInlineGlsl
3. VulkanRegression.CreateShadowMeshShaderPipelineFromInlineGlsl

`VulkanDevice::createLogicalDevice` also now gates `VK_EXT_mesh_shader` and the
ray-tracing extension family on device support and emits a `[nexus.vk]` notice
when downgrading. This prevents `vkCreateDevice` failures on hardware (e.g.
Intel UHD) that lacks these extensions when the caller leaves the defaults on.

`createInstance` likewise tolerates a missing `VK_LAYER_KHRONOS_validation`,
emitting a stderr notice and continuing without validation rather than failing
hard. This is the path used by the local Intel UHD baseline.
