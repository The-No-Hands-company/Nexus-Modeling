# Design: Vulkan RT Pipeline Beyond Stub (Month 13 Track B)

Status: **design-only** — no production RT code lands until this note is reviewed
and a runner with RT-capable hardware is available.

Reference: [month-13-post-alpha-checklist.md](month-13-post-alpha-checklist.md),
Track B.

---

## Context

The current RT path in the kernel exists as a complete stub:

- `VulkanRayTracing.cpp/.h` — extension query, BLAS/TLAS placeholder, SBT
  allocation stub.
- `SceneGraph::rebuildTLAS()` — wired to a null `AccelStructHandle`.
- `RenderContextDesc::enableRayTracing` — accepted at device creation but has
  no effect beyond the device extension query.

The Intel UHD 620 used on the alpha-baseline runner does **not** expose
`VK_KHR_ray_tracing_pipeline`. All RT tests are currently skipped in CI.
No production code lands until an RT-capable runner is provisioned.

---

## 1. Acceleration-Structure Ownership Model

### Principle

Ownership follows the producer-consumer rule: whichever layer allocates a
resource is responsible for freeing it.

| Resource | Allocator | Lifetime |
|---|---|---|
| Bottom-level AS (BLAS) | `SceneGraph` (via geometry bridge) | Tied to `Node`; destroyed when node is removed. |
| Top-level AS (TLAS) | `SceneGraph` | Rebuilt each frame dirty-flag cycle; single slot per graph. |
| Scratch buffers | `VulkanRayTracing` helper | Transient; reused across frames via a resize-on-demand pool. |
| SBT buffer | `RenderContext` or owning pass | Per-pipeline; freed when the pipeline is destroyed. |

**No shared ownership** of AS handles across module boundaries. A handle is
opaque (`AccelStructHandle = uint64_t`) to callers outside the Vulkan backend.

### BLAS lifecycle

```
createNode()                     → allocates vertex+index GPU buffers
geometry_bridge::uploadMesh()    → triggers BLAS build (async compute)
removeNode()                     → frees BLAS + GPU buffers (autoDestroyNodeGpuPayload)
```

BLAS compaction is deferred to a post-build pass (not Month 13 scope).
Refitting (dynamic geometry) is out of scope; full rebuilds are used.

---

## 2. SBT Layout and Alignment

The Vulkan spec requires each SBT record to be aligned to
`shaderGroupHandleAlignment` (typically 32 bytes) and the base address to be
aligned to `shaderGroupBaseAlignment` (typically 64 bytes).

### Record groups

| Group | Index | Contents |
|---|---|---|
| RayGen | 0 | `primary_raygen.rgen` |
| Miss | 0 | `primary_miss.rmiss` |
| Miss | 1 | `shadow_miss.rmiss` (for shadow rays) |
| Hit | 0..N-1 | One hit group per material ID (`closest_hit.rchit` + optional `any_hit.rahit`) |

### Stride

```
stride = align_up(shaderGroupHandleSize + maxInlineDataBytes,
                  shaderGroupHandleAlignment)
```

`maxInlineDataBytes` is bounded by `maxShaderGroupStride - shaderGroupHandleSize`
(spec max 4096 bytes). The initial implementation uses **no inline data**; all
material parameters are accessed via a descriptor buffer indexed by
`gl_InstanceCustomIndexEXT`.

### Upload strategy

SBT records are packed into a single `VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR`
buffer with sub-ranges for each group. The buffer is device-local; a staging
copy is issued once at pipeline creation and whenever materials change.

---

## 3. Synchronization with the GBuffer Pass

The RT primary pass follows the GBuffer pass in the frame timeline:

```
GBuffer pass        → produces: Albedo, Normal, Depth attachments
                               Layout: COLOR_ATTACHMENT_OPTIMAL / DEPTH_ATTACHMENT_OPTIMAL

Barrier (GBuffer → RT input):
  srcStage:  COLOR_ATTACHMENT_OUTPUT | LATE_FRAGMENT_TESTS
  dstStage:  RAY_TRACING_SHADER_BIT_KHR
  srcAccess: COLOR_ATTACHMENT_WRITE | DEPTH_STENCIL_ATTACHMENT_WRITE
  dstAccess: SHADER_READ
  layouts:   → SHADER_READ_ONLY_OPTIMAL

RT primary pass     → reads GBuffer textures via descriptors
                    → writes to Radiance image (GENERAL layout, storage image)

Barrier (RT output → composite input):
  srcStage:  RAY_TRACING_SHADER_BIT_KHR
  dstStage:  FRAGMENT_SHADER_BIT | COMPUTE_SHADER_BIT
  srcAccess: SHADER_WRITE
  dstAccess: SHADER_READ
  layout:    GENERAL → SHADER_READ_ONLY_OPTIMAL

Composite pass      → reads Radiance + GBuffer → writes to swapchain
```

TLAS must be rebuilt (if dirty) **before** the RT primary pass command is
recorded, i.e., during the pre-frame update phase. The rebuild uses async
compute when `enableAsyncCompute` is set; a semaphore synchronizes with
graphics queue submission.

---

## 4. Milestone Plan

### Phase 1 — Infrastructure (requires RT-capable runner)

1. Promote `VulkanRayTracing` from stub: implement BLAS build + compact.
2. Implement TLAS build in `SceneGraph::rebuildTLAS`.
3. SBT allocation + upload in `RenderContext`.
4. Barrier insertion in `FrameScheduler` between GBuffer and RT passes.

### Phase 2 — Shaders

1. Primary ray generation shader (`primary_raygen.rgen`).
2. Closest hit + miss shaders for diffuse GI (single bounce).
3. Shadow ray shaders (`shadow_miss.rmiss`, `shadow_any_hit.rahit`).

### Phase 3 — Test promotion

Existing skipped tests that gate Phase 1:
- `VulkanRayTracingTests.*` (requires `VK_KHR_ray_tracing_pipeline`).
- `SceneGraph.RebuildTLASUpdatesHandle`.

These stay skipped until the runner spec is satisfied.

### Runner spec required

- `VK_KHR_ray_tracing_pipeline` + `VK_KHR_acceleration_structure`
- `VkPhysicalDeviceRayTracingPipelinePropertiesKHR::maxRayRecursionDepth ≥ 2`
- Minimum 4 GB VRAM (scratch + AS build memory)

---

## 5. Risks

| Risk | Mitigation |
|---|---|
| AS build latency stalls frame | Async compute with semaphore sync (Phase 1). |
| SBT alignment bugs on some vendors | CI validation layer (`VK_EXT_validation_features`) runs buffer alignment checks. |
| TLAS rebuild every frame is expensive | Dirty-flag skip when no nodes moved (already in `SceneGraph`). |
| GBuffer layout transition ordering | `FrameScheduler` barrier tracking audit (existing Month 12 debt). |
