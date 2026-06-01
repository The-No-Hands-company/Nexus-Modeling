# Month 29 — Ray-Generation Shader Integration (v0.5 Track 2)

Status: **complete**. Delivers v0.5 Track 2: `DescriptorType::AccelerationStructure`,
`Renderer::setSceneTLAS`, RT descriptor set binding in `render()`, and
`TraceRaysWithSceneTLASOnTier1` Vulkan test.

Reference: [ROADMAP.md](../ROADMAP.md), [v0.5-planning.md](v0.5-planning.md),
[month-28-checklist.md](month-28-checklist.md).

---

## Track 2 — DescriptorType::AccelerationStructure

New enum value added to `DescriptorType` in `Device.h`:

```cpp
AccelerationStructure = 6,  // ray-tracing TLAS/BLAS handle (VkAccelerationStructureKHR)
```

`DescriptorBindingDesc` gains a corresponding field:

```cpp
AccelStructHandle accelStruct = {};  // valid for AccelerationStructure type
```

This lets the device backend bind TLAS/BLAS handles into descriptor sets using the same
`allocateDescriptorSet` / `updateDescriptorSet` API already used for buffers and textures.

---

## Track 2 — Renderer::setSceneTLAS

New API in `Renderer`:

```cpp
void setSceneTLAS(nexus::gfx::AccelStructHandle tlas) noexcept;
[[nodiscard]] nexus::gfx::AccelStructHandle sceneTLAS() const noexcept;
```

Non-owning: the renderer stores the handle but does not destroy it. The caller owns
the underlying TLAS resource (typically via `SceneGraph`).

---

## Track 2 — RT Descriptor Binding in Renderer::render()

When all four RT gate conditions hold and `sceneTLAS().valid()`:

1. Allocates an RT descriptor set with layout: `set=0, binding=0 → AccelerationStructure`.
2. Updates the set with the live TLAS handle.
3. Binds the set at index 0 on the command buffer before `cmd.traceRays()`.
4. Defers the set's release to the current frame slot (same lifetime as shadow/composite sets).

On the Null backend, `allocateDescriptorSet` returns an invalid handle (no-op), so this
path is always safe in headless CI.

---

## Track 2 — test_VulkanRTDispatch.cpp additions (1 test → total 10)

`TraceRaysWithSceneTLASOnTier1` — builds a single-triangle mesh node, calls
`scene.buildAccelStructs(dev)` to produce a BLAS + TLAS, passes the TLAS to the renderer
via `setSceneTLAS`, renders one frame, and asserts:

- `rtReflectionsActive == true`
- `tlasInstanceCount > 0`

Skips cleanly when Vulkan is unavailable, `rayTracingTier < 1`, or BLAS/TLAS build fails.

### Exit criteria

- `TraceRaysWithSceneTLASOnTier1` passes on a Tier-1 RT device.
- All 9 prior `VulkanRTDispatch` tests still pass.
- Baseline test count advances from 2233 to 2234.
- Null-backend suite unaffected (no new Null tests; all 2233 continue to pass).
