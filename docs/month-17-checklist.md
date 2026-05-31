# Month 17 — M2 Completion, CPU Cull Timing, Shadow Draw Call Gate Test

Status: **open**. Three tracks: M2 formal closure (shadow draw call gate test +
isolated `shadowDrawCalls` stat), CPU frustum-cull timing (populates the
long-standing `cpuCullTimeMs` stub), and implementation-status table cleanup.

Reference: [ROADMAP.md](../ROADMAP.md), [month-16-checklist.md](month-16-checklist.md).

---

## Track 1 — M2 Final Closure

### Motivation

Month 16 Track 3 verified the "zero" shadow draw call cases.  The remaining M2
gate is `ShadowPassStatsInFrameStatsAfterRender`: when a shadow pipeline, a
cascade contract (cascadeCount > 0), and a scene node with geometry are all
present, the renderer must emit shadow draw calls that are countable in
`FrameStats`.  A dedicated `shadowDrawCalls` counter (distinct from the
aggregate `drawCalls`) makes this assertable.

### Deliverables

1. Add `shadowDrawCalls` field to `FrameStats`.
2. Wire `m_stats.shadowDrawCalls += shadowDraws` alongside the existing
   `m_stats.drawCalls += shadowDraws` line in `Renderer.cpp`.
3. New `PassOrderingFixture` tests:
   - `ShadowPassShadowDrawCallsZeroWithNoContract`
   - `ShadowPassShadowDrawCallsZeroWithNoPipeline`
   - `ShadowPassShadowDrawCallsZeroWithEmptyScene`
   - `ShadowPassShadowDrawCallsPositiveWhenContractPipelineAndNodeSet`
4. Mark M2 as **Complete** in ROADMAP.md.

### Exit criteria

All 4 new tests pass on Null backend.

---

## Track 2 — CPU Cull Timing

### Motivation

`FrameStats::cpuCullTimeMs` has been declared since Month 14 but is always 0.
The frustum-cull loop (`scene.collectVisible`) is the natural measurement point.

### Deliverables

1. Wrap `scene.collectVisible` in `Renderer::render` with
   `std::chrono::steady_clock` and populate `m_stats.cpuCullTimeMs`.
2. Add test `FrameStats_CpuCullTimeMsIsNonNegative` (in
   `test_RendererBehavior.cpp` or a new file).
3. `perf_smoke` emits `cpu_cull_time_ms=N` field alongside `gpu_time_ms`.

### Exit criteria

Test passes; `perf_smoke` output contains `cpu_cull_time_ms=`.

---

## Track 3 — Implementation Status Table Cleanup

### Deliverables

1. `docs/graphics-kernel.md`: GPU timestamps row → `✅ Complete (Month 16)`.
2. ROADMAP.md: M2 → `Complete` after Track 1 lands.
3. Add Month 17 entry to the implementation status table.

---

## Effort Allocation

- 45% — Track 1 (M2 closure): new FrameStats field + 4 tests.
- 35% — Track 2 (CPU cull timing): single-site impl change + test + perf_smoke.
- 20% — Track 3 (documentation): status tables only.
