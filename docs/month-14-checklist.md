# Month 14 — Rendering Depth and Post-Alpha Track Expansion

Status: **open**. Advances four parallel tracks: two rendering-core tracks
(descriptor/material binding and shadow map lifecycle) that are hardware-
agnostic and unblock Milestone M1/M2, and two post-alpha continuation tracks
(RT production Phase 1 infrastructure and simulation-coupling Vulkan data
path) that were deferred at design-only in Month 13.

Reference: [vision-and-roadmap.md](vision-and-roadmap.md),
[ROADMAP.md](../ROADMAP.md) "Next 4–6 Weeks",
[month-13-post-alpha-checklist.md](month-13-post-alpha-checklist.md).

---

## Track 1 — Descriptor and Material Resource Binding (PRIMARY)

### Motivation

The composite pass currently reads GBuffer inputs as opaque handles set by
the caller. No kernel-managed binding contract exists: there is no allocation,
lifetime, or update path owned by the renderer for composite-input descriptor
sets. This means material tables and shadow inputs are wired manually by each
test/tool that uses them rather than through a stable, validated contract.

### Deliverables

1. **`nexus/render/DescriptorBinder.h`** — new public header:
   - `CompositeDescriptorSet`: allocates and updates the descriptor set that
     binds GBuffer albedo/normal/velocity/depth textures + sampler to the
     composite pass. Owns its `DescriptorSetHandle`; frees on destruction.
   - `MaterialTableDescriptorSet`: wraps the material-table buffer binding
     with offset tracking. Validates `materialTableOffsetBytes` on update.
   - `ShadowDescriptorSet`: wraps shadow depth texture + sampler + lighting
     buffer into a single managed binding. Validates `shadowCascadeCount > 0`.
   - All three types expose `isReady() const noexcept → bool` and
     `descriptorSet() const noexcept → DescriptorSetHandle`.

2. **`Renderer` integration**:
   - Add `Renderer::bindCompositeDescriptors(const CompositeInputDesc&,
     IDevice&)` that allocates/updates a `CompositeDescriptorSet` internally
     and makes it available via `Renderer::compositeDescriptorSet()`.
   - `render()` asserts `compositeDescriptorSet().isReady()` before recording
     the composite draw call (in Null backend: assertion is a diagnostic, not
     a crash).

3. **Tests**:
   - `DescriptorBinder.CompositeDescriptorSetAllocatesAndUpdates`
   - `DescriptorBinder.MaterialTableDescriptorSetRejectsInvalidOffset`
   - `DescriptorBinder.ShadowDescriptorSetRequiresCascadeCountNonZero`
   - `RendererBehavior.BindCompositeDescriptorsPopulatesDescriptorSet`
   - API surface manifest entry for `DescriptorBinder.h`.

4. **Docs**: update [graphics-kernel.md](graphics-kernel.md) with the
   descriptor lifecycle section.

### Exit criteria

1. All 5 new tests pass on Null backend.
2. `DescriptorBinder.h` is in the API surface manifest with no removals from
   the existing manifest entries.
3. No existing `RendererBehavior.*` tests regress.

---

## Track 2 — Shadow Map Target Lifecycle (PRIMARY)

### Motivation

The shadow atlas descriptor tests (`ShadowPassBindingDesc*`) pass, but the
shadow map texture itself has no managed lifecycle: there is no documented
create/resize/destroy path that callers can follow, and the depth target's
layout transitions are not tracked between frames. This is Milestone M2 scope.

### Deliverables

1. **`nexus/render/ShadowMapTarget.h`** — new public header:
   - `ShadowMapTarget`: manages the shadow depth texture and sampler via
     `IDevice`. Exposes `create(IDevice&, Extent2D, uint32_t cascadeCount)`,
     `resize(IDevice&, Extent2D)`, and `destroy(IDevice&) noexcept`.
   - `ShadowMapTargetDesc`: config struct (extent, cascade count, depth
     format default `D32_SFLOAT`).
   - Exposes `TextureHandle depthTexture()`, `SamplerHandle depthSampler()`,
     and `bool isReady() const noexcept`.
   - Resize releases old handles before allocating new ones (no handle leak).

2. **`Renderer` integration**:
   - `Renderer::setShadowMapTarget(ShadowMapTarget*)` — symmetric to
     `setGaussianSplatPass`. Null means shadow pass is disabled.
   - `Renderer::onResize(Extent2D)` calls `shadowMapTarget->resize(...)` when
     one is attached.

3. **Tests**:
   - `ShadowMapTarget.CreateIsReady`
   - `ShadowMapTarget.ResizeReleasesAndReallocates`
   - `ShadowMapTarget.DestroyInvalidatesHandles`
   - `ShadowMapTarget.ZeroExtentIsNotReady`
   - `RendererBehavior.SetShadowMapTargetPopulatesShadowInput`
   - API surface manifest entry for `ShadowMapTarget.h`.

4. **Docs**: extend [graphics-kernel.md](graphics-kernel.md) with shadow map
   target lifecycle section.

### Exit criteria

1. All 5 new tests pass on Null backend.
2. `ShadowMapTarget.h` is in the API surface manifest.
3. Existing `VulkanRegression.ShadowPassBindingDesc*` tests continue to pass.

---

## Track 3 — RT Pipeline Phase 1 Infrastructure (SECONDARY)

Hardware gate: requires `VK_KHR_ray_tracing_pipeline` +
`VK_KHR_acceleration_structure`. All new tests are capability-gated and skip
cleanly on the alpha-baseline Intel UHD runner.

Reference: [design-rt-pipeline-month-13.md](design-rt-pipeline-month-13.md),
Phase 1.

### Deliverables

1. **BLAS build**: promote `VulkanRayTracing` from stub — implement
   `buildBLAS(IDevice&, const Mesh&) → AccelStructHandle` with async-compute
   scratch pool.
2. **TLAS build**: implement `SceneGraph::rebuildTLAS(IDevice&)` — replaces
   null `AccelStructHandle` with a real top-level AS when at least one node
   has a valid BLAS.
3. **SBT allocation**: `RenderContext::allocateSBT(const SBTDesc&) →
   SBTHandle` — allocates + uploads the shader binding table aligned to
   `shaderGroupHandleAlignment`.
4. **Barrier insertion**: `FrameScheduler` inserts the GBuffer→RT barrier
   (see design note §3) before the RT primary pass slot.

5. **Tests** (capability-gated, skip on non-RT hardware):
   - `VulkanRT.BuildBLASFromTriangleMesh`
   - `VulkanRT.RebuildTLASUpdatesHandle` (promotes the existing skipped test)
   - `VulkanRT.AllocateSBTAlignmentIsCorrect`
   - `VulkanRT.FrameSchedulerInsertsGBufferRTBarrier`

6. **No new public headers this month** — all changes are internal to the
   Vulkan backend. Existing `AccelStructHandle` and `SBTHandle` opaque types
   in `Types.h` are sufficient.

### Exit criteria

1. All 4 new tests skip cleanly on non-RT hardware and are documented as
   capability-gated in `docs/month-12-alpha-compatibility-report.md` skip
   inventory (add an addendum).
2. When run on RT-capable hardware (`NEXUS_VK_PREFER_CAPS=rt`), all 4 pass.
3. No public API surface removals.

---

## Track 4 — Simulation Coupling Vulkan Data Path (SECONDARY)

Reference: [design-sim-coupling-month-13.md](design-sim-coupling-month-13.md).

The Month 13 prototype proved the solver-to-scene data path on the Null
backend. Month 14 promotes `syncToScene` to write the simulated body
transform into the matching `NodeScene` node's transform payload, and adds
an optional GPU readback path so body positions can be transferred from a
GPU physics solver buffer to the CPU-side harness without a full round-trip
stall.

### Deliverables

1. **`syncToScene(NodeScene&)`** production path:
   - Iterates all registered `BodyId → SceneNodeId` entries.
   - Reads body position + orientation from the harness snapshot (populated
     by `syncFromSolver`).
   - Calls `NodeScene::setNodeTransform(SceneNodeId, Mat4)` for each valid
     pair.
   - Bodies with no matching scene node are silently skipped.

2. **Async GPU readback hook** (Null-only this month, Vulkan deferred):
   - `SimCouplingHarness::setGpuReadbackBuffer(BufferHandle, IDevice&)` —
     stores a reference; on Null, `syncFromSolver` reads from CPU cache as
     before.
   - `SimCouplingHarness::flushGpuReadback()` — on Null: no-op; on Vulkan
     (future): maps the buffer, copies body state, unmaps.
   - The Null backend path must be deterministic: repeated calls with
     identical solver state produce identical scene transforms.

3. **Tests**:
   - `SimCoupling.SyncToSceneUpdatesSingleNodeTransform`
   - `SimCoupling.SyncToSceneSkipsMissingSceneNode`
   - `SimCoupling.SyncToSceneIsIdempotentForUnchangedSolverState`
   - `SimCoupling.GpuReadbackBufferNullBackendIsNoOp`
   - Determinism gate: run `SyncToSceneUpdatesSingleNodeTransform` 5× and
     assert identical `Mat4` output each time.

4. **Docs**: update [design-sim-coupling-month-13.md](design-sim-coupling-month-13.md)
   status from *production path landed* to *Month 14: syncToScene + Null
   readback complete*.

### Exit criteria

1. All 4 new tests pass on Null backend.
2. Determinism gate (5 runs, identical output) passes.
3. No public API surface removals.

---

## Cross-Track Risks

| Risk | Mitigation |
|---|---|
| Track 3 RT code bleeds into Null backend path | Capability gate at `IDevice::caps().supportsRayTracing`; Null always returns false. |
| Track 1 descriptor lifecycle conflicts with existing shadow atlas tests | Both existing shadow atlas VulkanRegression tests run in full CI gate; any regression is a blocker. |
| Track 4 `syncToScene` conflicts with `NodeScene` evaluate ordering | `syncToScene` writes transforms; `evaluate()` is called by the caller after. Contract documented in design note §2. |
| Track 2 shadow texture resize leaks handles on Null backend | `NullDevice::createTexture` returns a new handle each call; `destroy` is a no-op. Resize test must assert handle values differ. |

## Effort Allocation

Following the monthly 65/25/10 ratio:

- 65% — Track 1 (descriptor binding) + Track 2 (shadow target), as both
  unblock Milestone M1 and M2 and are hardware-agnostic.
- 25% — Track 4 (sim coupling syncToScene), as it has no hardware barrier and
  closes a documented gap.
- 10% — Track 3 (RT infra), targeted at scaffolding only since hardware is
  gated.

## How Work Is Tracked

Task execution: GitHub Issues/Projects (one issue per deliverable above).
Design changes: update the relevant design notes in-place.
API changes: update `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt`
in the same PR as the new header.
