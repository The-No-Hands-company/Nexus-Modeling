# Month 33 — RT Shader Binding Tables (v0.6 Track 3)

Status: **complete**. Delivers `ShaderBindingTableDesc`, `IDevice::createShaderBindingTable`,
`ICommandBuffer::traceRaysWithSBT`, `Renderer::setShaderBindingTable`, and 8 Null-backend
tests.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.6-planning.md](v0.6-planning.md),
[month-32-checklist.md](month-32-checklist.md).

---

## Track 3 — ShaderBindingTableDesc + HitGroup

New structs in `Device.h`:

```cpp
struct HitGroup {
    ShaderHandle closestHit;    // required
    ShaderHandle anyHit;        // optional (invalid = unused)
    ShaderHandle intersection;  // optional (procedural geometry)
};

struct ShaderBindingTableDesc {
    ShaderHandle                  rayGenShader;
    std::span<const ShaderHandle> missShaders;
    std::span<const HitGroup>     hitGroups;
    PipelineHandle                pipeline;
    const char*                   debugName = nullptr;
};
```

---

## Track 3 — IDevice::createShaderBindingTable

Default virtual implementation packs groups into a stub via `allocateSBT(1, 32, 64)`.
The Null backend inherits the default — `createShaderBindingTable` always returns a
valid `SBTHandle` regardless of the desc content.

Vulkan override (future): maps `missShaders` and `hitGroups` to
`VkStridedDeviceAddressRegionKHR` records, writes shader group handles into the SBT
buffer, and returns the filled handle.

---

## Track 3 — ICommandBuffer::traceRaysWithSBT

Default virtual method:

```cpp
virtual void traceRaysWithSBT(SBTHandle sbt, uint32_t w, uint32_t h, uint32_t d = 1) {
    (void)sbt;
    traceRays(w, h, d);  // fallback when SBT is not overridden
}
```

Vulkan override (future): issues `vkCmdTraceRaysKHR` with the four
`VkStridedDeviceAddressRegionKHR` pointers embedded in the SBT buffer.

---

## Track 3 — Renderer SBT integration

`Renderer::setShaderBindingTable(SBTHandle)` / `shaderBindingTable()` — non-owning
slot. In `render()` RT dispatch block:

```cpp
if (m_impl->sbt.valid())
    cmd.traceRaysWithSBT(m_impl->sbt, fc.extent.width, fc.extent.height, 1u);
else
    cmd.traceRays(fc.extent.width, fc.extent.height, 1u);
```

Backward compatible: callers that never set an SBT continue to dispatch via bare
`traceRays` as before.

---

## Track 3 — test_RTShaderBindingTable.cpp (8 tests)

All run on Null backend.

| Test | Behaviour verified |
|---|---|
| `CreateSBTReturnsValidHandleOnNullBackend` | `createShaderBindingTable` → valid handle |
| `CreateSBTWithPipelineHandleDoesNotCrash` | Desc with RT pipeline handle accepted |
| `TwoSBTHandlesAreDistinct` | Handles have distinct IDs |
| `RendererDefaultSBTIsInvalid` | Slot starts as invalid handle |
| `SetShaderBindingTableRoundTrip` | Set → get returns same ID |
| `ClearSBTSlotWithInvalidHandle` | Setting `{}` clears the slot |
| `RendererWithSBTRenderDoesNotCrash` | `render()` with SBT registered no-throws |
| `SBTSlotDoesNotAffectRasterFrameStats` | `rtReflectionsActive == false` on Null |

### Exit criteria

- All 8 tests pass in Null-only CI.
- Baseline test count advances from 2247 to 2255.
- `IDevice::createShaderBindingTable` returns a valid handle on Null backend.
- `Renderer::setShaderBindingTable` gates `traceRaysWithSBT` vs `traceRays` dispatch.
- No API regressions — existing RT tests (`test_RTProductionPath`, `test_SceneBVH`,
  `test_VulkanRTDispatch`) all still pass.
