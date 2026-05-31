# Month 27 — Scene BVH Integration (v0.5 Track 1)

Status: **complete**. Delivers v0.5 Track 1: `SceneGraph::buildAccelStructs`,
`MeshRef::vertexCount`, and `FrameStats::tlasInstanceCount`.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.5-planning.md](v0.5-planning.md),
[month-26-checklist.md](month-26-checklist.md).

---

## Track 1 — MeshRef::vertexCount

`MeshRef` had `indexCount` but no `vertexCount`. The Vulkan `buildBLAS` path needs
both to size the acceleration structure correctly. Added `uint32_t vertexCount = 0`
alongside `indexCount` in `SceneGraph.h`.

---

## Track 2 — SceneGraph::buildAccelStructs

### API

```cpp
// In SceneGraph.h:
[[nodiscard]] uint32_t buildAccelStructs(nexus::gfx::IDevice& device);
```

### Behaviour

1. Traverses all nodes in the scene graph.
2. For each node where `mesh.vertexBuffer.valid() && mesh.indexBuffer.valid() && !mesh.blas.valid()`:
   - Calls `device.buildBLAS(vertexBuffer, indexBuffer, vertexCount, indexCount)`.
   - Stores the result in `node.mesh.blas`.
   - Increments the "built" counter.
3. If any new BLASes were built, calls `rebuildTLAS(device)` to refresh the TLAS.
4. Returns the count of new BLASes built (0 if nothing changed — idempotent).

### Null backend

`NullDevice::buildBLAS` returns a valid stub `AccelStructHandle` regardless of counts.
`NullDevice::buildTLAS` returns a valid stub handle.

---

## Track 3 — FrameStats::tlasInstanceCount

Added to `FrameStats` in `Renderer.h`:

```cpp
uint32_t tlasInstanceCount = 0;  // BLAS instances in scene TLAS this frame
```

Populated in `Renderer::render()` inside the RT dispatch block: only when all four
RT gate conditions hold and `traceRays` fires, the renderer traverses the scene to
count nodes with `mesh.blas.valid()` and writes the result to `tlasInstanceCount`.

On Null backend, `rayTracingTier == 0` gates out the dispatch, so `tlasInstanceCount`
remains 0 even if BLASes are present in the scene. On Tier-1 Vulkan hardware, the
count reflects the live TLAS instance population.

---

## Track 4 — test_SceneBVH.cpp (8 tests)

All run on Null backend; no GPU hardware required.

| Test | Behaviour verified |
|---|---|
| `EmptySceneHasInvalidTLAS` | Fresh `SceneGraph` returns `tlas().valid() == false` |
| `BuildAccelStructsOnEmptySceneBuildsZeroBLASes` | Returns 0 and leaves TLAS invalid |
| `BuildAccelStructsOnMeshNodeBuildsValidBLAS` | Single node with buffers: BLAS built, count = 1 |
| `BuildAccelStructsBuildsValidTLAS` | After build, `tlas().valid() == true` |
| `BuildAccelStructsSecondCallDoesNotRebuildExistingBLASes` | Idempotent: second call returns 0, same BLAS handle |
| `MultiNodeSceneBuildsOneBLASPerNode` | 3 nodes → 3 BLASes, count = 3 |
| `TLASInstanceCountInFrameStatsWhenRTActive` | On Null backend, `tlasInstanceCount == 0` (RT gate blocked at caps check) |
| `ClearAndDestroyTLASInvalidatesHandle` | After clear, `tlas().valid() == false` |

Registered in `tests/CMakeLists.txt`.

### Exit criteria

- All 8 tests pass in Null-only CI.
- Baseline test count advances from 2217 to 2225.
- `SceneGraph::buildAccelStructs` is callable on any scene with mesh nodes.
- `FrameStats::tlasInstanceCount` is populated when RT dispatch fires on real hardware.
