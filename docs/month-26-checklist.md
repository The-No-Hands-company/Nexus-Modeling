# Month 26 — v0.4 Release + RT Pipeline Unit Tests

Status: **complete**. Stamps v0.4 release, restores the full RT dispatch test,
adds RT pipeline creation unit tests, and opens v0.5 planning.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.5-planning.md](v0.5-planning.md),
[month-25-checklist.md](month-25-checklist.md).

---

## Track 1 — v0.4 Release

### Deliverables

1. `CMakeLists.txt` project version: `0.3.0` → `0.4.0`.
2. `CHANGELOG.md` v0.4 section stamped `2026-05-31`; v0.5 upcoming section opened.
3. `docs/testing-strategy.md` baseline updated `2210` → `2217` (7 new RT pipeline tests).
4. `docs/api-contract-alpha-summary.md` date line updated to note v0.4.
5. `ROADMAP.md` next-steps updated to point at v0.5.

### Exit criteria

- CMakeLists.txt version reads `0.4.0`.
- CHANGELOG v0.4 has a release date.
- v0.5 upcoming section is open.

---

## Track 2 — test_VulkanRTDispatch.cpp Fix

### Problem

Month 24 introduced `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` using
`dev.createRTPipeline(RTPipelineDesc)` — a method that does not exist.
Month 25 replaced it with `RTGateFourConditionsOnVulkan` (which is correct) but
the full dispatch test was never added back.

### Fix

Added `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` using the correct API:
`IDevice::createRayTracingPipeline(RayTracingPipelineDesc)`.

Steps in the test:
1. Requires Tier-1 RT hardware (GTEST_SKIP otherwise).
2. Compiles a minimal ray-gen shader via `dev.createShader(ShaderStage::RayGen)`.
3. Creates an RT pipeline via `dev.createRayTracingPipeline(desc)`.
4. Sets `enableRTReflect + HybridRT` and calls `renderer.setRayTracingPipeline(rtPipe)`.
5. Renders one frame; asserts `rtReflectionsActive == true` and `activeRenderMode == HybridRT`.
6. Skips gracefully if shader compilation or pipeline creation fails.

Test count: 8 → 9.

### Exit criteria

- File compiles cleanly with `NEXUS_BACKEND_VULKAN=ON`.
- `TraceRaysDispatchSetsRTReflectionsActiveOnTier1` passes on a Tier-1 RT device.
- All 9 tests skip cleanly in headless CI.

---

## Track 3 — test_RTPipelineCreation.cpp

### Deliverables

New `tests/kernel/test_RTPipelineCreation.cpp` — 7 Null-backend unit tests:

| Test | Behaviour verified |
|---|---|
| `DefaultPipelineHandleIsInvalid` | `PipelineHandle{}` returns `valid() == false` |
| `NullBackendCreateRayTracingPipelineReturnsValidHandle` | Null backend returns valid handle from minimal desc |
| `MultipleRayTracingPipelinesHaveDistinctHandles` | Two creates produce distinct `id` values |
| `RayTracingPipelineHandleRoundTripOnRenderer` | `setRayTracingPipeline` + `rayTracingPipeline()` + clear |
| `RayTracingPipelineDescWithMaxRecursionDepthZeroIsAccepted` | Depth=0 accepted on Null backend |
| `NullBackendBuildBLASReturnsValidHandle` | `buildBLAS` on Null backend returns valid `AccelStructHandle` |
| `NullBackendBuildTLASFromSingleBLASReturnsValidHandle` | `buildTLAS({blas})` returns valid handle |

Registered in `tests/CMakeLists.txt` in the `nexus_kernel_tests` sources list.

### Exit criteria

- All 7 tests pass in Null-only CI.
- Baseline test count advances from 2210 to 2217.

---

## Track 4 — v0.5 Planning

### Deliverables

1. New `docs/v0.5-planning.md`:
   - Track 1: `SceneGraph::buildAccelStructs` + `Renderer::setSceneTLAS`.
   - Track 2: RT descriptor layout + `traceRays` with real scene TLAS.
   - Track 3: DLSS Ray Reconstruction (`DenoiserBackend::DLSS_RR`, `NeuralBackend::DLSS_RR`).
   - Track 4: Mesh shader production path (`enableMeshShaders` flag, `FrameStats::meshShaderDrawCalls`).
   - Prerequisites, out-of-scope, and 6-item exit criteria.

### Exit criteria

- `docs/v0.5-planning.md` exists with all four tracks and exit criteria.
- ROADMAP.md next-steps point at v0.5 and `docs/v0.5-planning.md`.
