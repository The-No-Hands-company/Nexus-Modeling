# Month 13 — Post-Alpha Track Kickoff

Status: open. Lands the first slice of post-alpha work without disturbing the
v1.0-alpha API contract.

Reference: [vision-and-roadmap.md](vision-and-roadmap.md) Month 13 section.

## Tracks

### A. Gaussian splat render integration (PRIMARY FIRST SLICE)

The scripting / harness layer already exposes Gaussian-splat primitives
(`gaussian.load_ply`, `gaussian.describe`). Month 13 promotes these into a
real render pass so a splat asset can be drawn from a scene node without going
through the script harness.

Deliverables:

1. Public header: `nexus/render/GaussianSplatPass.h` describing pass inputs
   (splat buffer handle, view + projection, blend mode) and pass outputs.
2. Null-backend pass implementation that produces stable, deterministic
   counters (splat draw count, projected splat count, discard count).
3. Renderer integration: `Renderer::setGaussianSplatPass(...)` symmetric to
   `setShadowPipeline`. Default off; opt-in per scene.
4. Tests:
   - Null-backend behavior test: splat count counted on `lastFrameStats()`.
   - Round-trip test: PLY → device buffer → frame stats.
   - API surface manifest entry for the new public symbols.
5. Docs: extend [graphics-kernel.md](graphics-kernel.md) with the splat-pass
   section.

Exit criteria:

1. `RendererBehavior.GaussianSplatPassCountsSplatDraws` passes on Null.
2. `nexus_kernel_perf_smoke` reports `splat_draw_calls` when a splat scene is
   used (additive field, does not change `backend=` or `scenario=`).
3. No removals from the alpha API surface manifest.

### B. Vulkan RT pipeline beyond stub (SECONDARY)

Promote the current ray-tracing pipeline path past the stub. Gated on having
an RT-capable runner; Intel UHD on the alpha-baseline box does not qualify.

Deliverables (design-first this month):

1. Design note `docs/design-rt-pipeline-month-13.md` covering:
   - Acceleration-structure ownership model (who builds, who frees).
   - SBT layout + alignment story.
   - Synchronization with the GBuffer pass.
2. Milestone plan: which existing skipped tests get promoted first, and the
   exact runner spec required to host them.

Exit criteria:

1. Design note merged. No production RT code lands until the design is
   reviewed.

### C. Cross-domain simulation coupling (SECONDARY)

Design-first. Prove the solver-to-render data path on the Null backend before
any Vulkan plumbing.

Deliverables (design-first this month):

1. Design note `docs/design-sim-coupling-month-13.md` covering:
   - Identifier mapping between solver entities and scene nodes.
   - Time-step ownership: who advances, who samples.
   - Determinism contract for replay.
2. Prototype harness running on the Null backend, off by default, behind a
   feature toggle in `RenderContextDesc`. No public API surface change yet.

Exit criteria:

1. Design note merged.
2. Prototype harness runs end-to-end deterministically on the Null backend.

## Risks / Watch

1. Track A must not regress GBuffer + composite sequencing in
   [docs/month-3-render-checklist.md](month-3-render-checklist.md).
2. Tracks B and C must stay off the v1.0-alpha hot path: no public-header
   churn this month from either.
3. Any Vulkan-side validation work for Track A should reuse the
   `NEXUS_VK_DEVICE_*` overrides documented in the Month 12 compatibility
   report rather than introducing new device-selection mechanisms.
