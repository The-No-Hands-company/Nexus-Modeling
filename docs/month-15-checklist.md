# Month 15 — Shadow Pipeline Integration and Render Path Parity

Status: **open**. Advances three parallel tracks targeting Milestones M2
(Shadow Pipeline) and M3 (Render Path Parity).

Reference: [ROADMAP.md](../ROADMAP.md), [month-14-checklist.md](month-14-checklist.md).

---

## Track 1 — Shadow Descriptor Set Renderer Integration (M2)

### Motivation

`ShadowDescriptorSet` from `DescriptorBinder.h` (Month 14 Track 1) is not yet
wired into `Renderer` as a managed owned binding. The `buildShadowLightingBindingDesc()`
method exists and is used internally, but callers have no way to ask the renderer
to allocate, update, and own a `ShadowDescriptorSet` — mirroring what
`bindCompositeDescriptors` does for the composite pass.

### Deliverables

1. Add to `Renderer` public API:
   - `bindShadowDescriptors(IDevice&)` — builds desc via
     `buildShadowLightingBindingDesc()`, validates `isComplete()`, calls
     `ShadowDescriptorSet::update(dev, ...)`. Returns `false` when the
     shadow inputs are absent.
   - `shadowDescriptorSet() const noexcept → const ShadowDescriptorSet*`
   - `destroyShadowDescriptors(IDevice&) noexcept`
2. Add private member `ShadowDescriptorSet m_shadowDescSet` to `Renderer`.
3. `destroyShadowDescriptors` called from `Renderer` destructor alongside
   `destroyCompositeDescriptors`.
4. Tests in `test_RendererIntegration.cpp` (new suite `RendererShadowDesc`):
   - `ShadowDescriptorSetInitiallyNotReady`
   - `BindShadowDescriptorsFailsWithoutContract`
   - `BindShadowDescriptorsFailsWithPartialContract`
   - `DestroyShadowDescriptorsBeforeBindIsSafe`
   - `DestroyShadowDescriptorsAfterBindClearsReady`
   - `DoublDestroyShadowDescriptorsIsSafe`

### Exit criteria

1. All 6 new tests pass on Null backend.
2. No existing tests regress.

---

## Track 2 — Pass Ordering and Barrier Sequence Tests (M2/M3)

### Motivation

The `RenderGraphValidator` exists and is tested, but there are no regression
tests that exercise the full shadow → geometry → composite barrier sequence
with an injected `RecordingCommandBuffer`, verifying that transition events
arrive in the correct order. This is needed to harden M2 and prevent future
pass-ordering regressions.

### Deliverables

1. New test file `tests/kernel/test_PassOrdering.cpp`:
   - `PassOrdering.ShadowPassBarrierPrecedesGeometryPass`
   - `PassOrdering.GeometryPassBarrierPrecedesCompositePass`
   - `PassOrdering.DepthTransitionIsDepthWriteBeforeComposite`
   - `PassOrdering.CompositeReadsGBufferAsShaderRead`
   - `PassOrdering.OnResizeInvalidatesAndReallocatesGBuffer` (via RecordingScheduler)
   - `PassOrdering.RTBarrierIsInsertedAfterGeometryPassWhenEnabled`
2. Wired into `tests/CMakeLists.txt`.

### Exit criteria

1. All 6 new tests pass on Null backend.

---

## Track 3 — Render Path Parity Documentation and Contract Tests (M3)

### Motivation

M3 is "Match scheduler and non-scheduler behavior or formally de-scope." The
non-scheduler (direct swapchain) path is documented as a "non-production
compatibility fallback" in `Renderer.cpp`. Month 15 formalizes what the gap
is, tests that the scheduler path produces richer stats, and documents the
formal de-scope decision.

### Deliverables

1. New test file `tests/kernel/test_RenderPathParity.cpp`:
   - `RenderPathParity.NullBackendUsesDirectSwapchainPath`
   - `RenderPathParity.DirectPathProducesZeroDrawCalls`
   - `RenderPathParity.SchedulerPathProducesFrameContextWithValidExtent`
   - `RenderPathParity.ShadowsDisabledByDefaultOnBothPaths`
   - `RenderPathParity.EnableShadowsWithNoContractProducesNoShadowPasses`
   - `RenderPathParity.FrameStatsResetEachFrame`
2. Add a **Render Path Parity** section to `docs/graphics-kernel.md` explaining
   the scheduler vs. non-scheduler distinction and the formal de-scope decision
   for M3.
3. Wired into `tests/CMakeLists.txt`.

### Exit criteria

1. All 6 new tests pass.
2. `docs/graphics-kernel.md` updated with parity section.

---

## Effort Allocation

- 50% — Track 1 (shadow descriptor wiring): direct M2 unblock.
- 30% — Track 2 (pass ordering tests): M2 regression hardening.
- 20% — Track 3 (parity docs + tests): M3 formal closure.
