# Kernel Contract Gaps — Editor Bring-Up

Captured during Week 3 of the Month-1 editor bring-up (PR-11 through PR-15).

Unlike [editor-bringup-paper-cuts.md](editor-bringup-paper-cuts.md), the items
here are **kernel-facing**: they identify behavior the tooling app legitimately
needs from the public kernel API surface (`src/kernel/include/nexus/...`) and
where today's public contract is either silent, ambiguous, or actively
contradicts what a guided UX workflow expects.

Each entry follows the same shape:

- **Surface** — the public header/method involved.
- **Observed behavior** — what actually happens when the app calls the public
  API exactly as documented.
- **Required behavior** — what a guided editor slice needs to consume the API
  without bespoke workarounds.
- **App-side mitigation** — how the tooling app currently absorbs the gap so
  Month-1 work can continue.
- **Suggested resolution** — minimal kernel change that would let the
  mitigation be removed.

Scope guard: items in this document must graduate to a tracked kernel task
before behavior changes land in `src/kernel/src/...`. Do not silently fix a
gap inside an unrelated PR — landing a kernel fix should also delete the
matching entry here.

---

## CG-1 — `ModelingShell::quickCleanup` fails by default on every starter primitive

> **Status (PR-22, kernel):** **Closed.** All six starter primitives
> (`Box`, `Plane`, `Sphere`, `Cylinder`, `Cone`, `Capsule`) now round-trip
> through `startStarterModel(...) → quickCleanup(...)` with
> `SessionReport::success == true` and the tooling app exits `0`.
>
> Resolution landed in three pieces:
>
> 1. **PR-21 (kernel):** Added `HardSurfaceWorkflow::triangulate()` plus
>    `HardSurfaceStepKind::Triangulate`, and made
>    `ModelingShell::quickCleanup` pre-triangulate before
>    `BevelChamfer`/`Remesh`. `ModelingShell::sessionReport()` was tightened
>    to surface the **first failing step's** message rather than always
>    echoing the cosmetically-last `"Normals rebuilt."`.
> 2. **PR-22 (kernel, `src/kernel/src/geometry/BevelChamfer.cpp`):** Reordered
>    the post-bevel normal recompute to run **before** the validity check so
>    attribute buffer sizes match `positions.size()` when validity is
>    evaluated (eliminates the spurious `GeneratedDegenerateFace`
>    diagnostic). The `NoSharpEdgesDetected` early return (i.e. output ==
>    input for smooth meshes like `Sphere`/`Capsule`) now also OR-sets
>    `SuccessWithWarnings` so it is honestly classified as a success.
> 3. **PR-22 (kernel, `src/kernel/src/geometry/ModelingShell.cpp`):** The
>    `quickCleanup` chain now re-triangulates between `bevelEdges()` and
>    `remesh()` because bevel can introduce hexagonal support faces, which
>    previously made `Remesh` reject the input as `OutputTopologyInvalid`.
>    Effective chain: `Triangulate → BevelChamfer → Triangulate → Remesh →
>    RebuildNormals`.
> 4. **PR-22 (kernel, `BevelChamfer.h`, `RemeshOperation.h`):** Fixed the
>    `isSuccess()` check on both diagnostic reports to use
>    `hasDiagnostic(..., SuccessWithWarnings)` instead of `==
>    SuccessWithWarnings`, so warning-bearing successes (e.g. `NoSharpEdges
>    Detected | SuccessWithWarnings`, `InputTriangulated |
>    SuccessWithWarnings`, `NoChangesApplied | SuccessWithWarnings`) are
>    reported truthfully. The previous `==` comparison silently rejected
>    every warning-bearing success.
> 5. **App-side (PR-22):** `tools/app_ci_smoke.sh` now asserts the starter
>    cleanup step exits `0` (the `(CG-1 tolerated)` carve-out is gone).
>    `ToolingAppSmokeTests.StarterModelCleanupExitCodeReflectsSessionOutcome`
>    asserts `exitCode == 0`. The step-count assertions in
>    `test_ModelingShell.cpp` and `test_ToolingApp.cpp` were bumped to the
>    new 5/7-step shape.

---

## Closing a gap

When a kernel change lands that resolves one of these entries:

1. Delete the entry from this file in the same PR that lands the kernel fix.
2. Tighten any test that was loosened to tolerate the gap (search this file
   for the test name).
3. Note the removal in the PR description so reviewers can confirm the
   contract is now self-consistent.
