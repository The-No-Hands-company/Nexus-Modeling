# Month 18 — TAA Integration, API Surface Hardening, v0.3 Prep

Status: **open**. Three tracks: wire `TemporalAccumulator` into the Renderer
(applies per-frame camera jitter and advances the TAA frame index), expand
dedicated test coverage for `TemporalAccumulation.h` (M5), and lay the
groundwork for the v0.3 release candidate.

Reference: [ROADMAP.md](../ROADMAP.md), [month-17-checklist.md](month-17-checklist.md).

---

## Track 1 — TAA Renderer Integration

### Motivation

`TemporalAccumulator` and `Camera::setJitter` both exist but are never
connected. The `RendererSettings.enableTAA` flag is declared but ignored.
Wiring them closes the feedback loop for temporal anti-aliasing and unlocks
the upstream denoiser scheduling work.

### Deliverables

1. `Renderer` owns a `TemporalAccumulator m_taaAccumulator` instance.
2. `Renderer::render()` — when `m_settings.enableTAA` is true — calls
   `m_taaAccumulator.advanceFrame()` and applies
   `camera.setJitter(jx, jy)` using `currentJitterOffset()`.
3. `Renderer::endFrame()` clears jitter via `camera.clearJitter()` so the
   next frame starts clean. (Camera is passed by const-ref; store the mutable
   pointer when settings.enableTAA is enabled.)
4. `FrameStats` gains `uint32_t taaFrameIndex` — the TAA accumulator frame
   counter (zero when TAA is disabled).
5. `Renderer::setTemporalAccumulationConfig(TemporalAccumulationConfig)` and
   `Renderer::temporalAccumulationConfig()` accessors.
6. Tests (in new `test_TemporalAccumulation.cpp`):
   - `TAADisabledByDefaultJitterIsZero`
   - `TAAEnabledAdvancesFrameIndex`
   - `TAAFrameIndexInFrameStats`
   - `TAAConfigRoundtrip`
   - `TAAJitterOffsetChangesEachFrame`
   - `TAADisableStopsJitterAdvance`

### Exit criteria

All 6 tests pass on Null backend.

---

## Track 2 — Dedicated TemporalAccumulation Test File

### Motivation

`TemporalAccumulator` is tested inline in `test_GaussianSplatting.cpp` (8
tests). A dedicated file with broader coverage improves discoverability and
separation of concerns.

### Deliverables

1. New `tests/kernel/test_TemporalAccumulation.cpp` with 12 tests:
   - Lifecycle: default construction, custom config round-trip
   - Jitter: Halton pattern produces distinct offsets across frames
   - Jitter: uniform pattern produces grid offsets
   - Jitter: sequence wraps at sampleCount
   - Blend: alpha = blendFactor when motionRejected=false
   - Blend: alpha clamped when motionRejected=true
   - State: frameIndex advances monotonically
   - State: prevView/prevProj persisted across advanceFrame
   - Config: setConfig replaces configuration live
   - Config: zero blendFactor returns zero resolveBlendAlpha
   - Config: blendFactor=1.0 returns 1.0 resolveBlendAlpha
   - Renderer integration: taaFrameIndex in FrameStats increments per frame
2. `test_TemporalAccumulation.cpp` added to `tests/CMakeLists.txt`.

### Exit criteria

All 12 tests pass.

---

## Track 3 — v0.3 Release Groundwork

### Deliverables

1. `CHANGELOG.md` — new file documenting v0.2→v0.3 changes.
2. `docs/graphics-kernel.md` — add TAA wiring row to implementation status.
3. ROADMAP.md — M4 status update; add v0.3 release target note.

---

## Effort Allocation

- 50% — Track 1 (TAA integration): Renderer wiring + 6 integration tests.
- 35% — Track 2 (TemporalAccumulation tests): 12 unit tests.
- 15% — Track 3 (v0.3 docs): CHANGELOG + status updates.
