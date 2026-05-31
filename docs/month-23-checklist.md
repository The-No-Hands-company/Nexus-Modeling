# Month 23 — v0.3 Release + M5 Contributor Scale-Up

Status: **complete**. Closes the v0.3 release window and establishes the M5 contributor
onboarding baseline. Sets up v0.4 planning.

Reference: [ROADMAP.md](../ROADMAP.md), [month-22-checklist.md](month-22-checklist.md).

---

## Track 1 — v0.3 Release

### Deliverables

1. `CMakeLists.txt` project version bumped `0.1.0` → `0.3.0`.
2. `CHANGELOG.md` v0.3 section stamped with release date `2026-05-31`;
   v0.4 upcoming section opened.
3. `docs/api-contract-alpha-summary.md` — header count updated 39 → 93; full domain
   ownership table revised with per-domain counts; complete 93-header inventory added;
   v0.3 domain summary explains the +54 additions.
4. `docs/testing-strategy.md` — baseline updated 534 → 2210; scope section expanded to
   cover all M1–M4 test areas; perf smoke and scenario artifact layers documented;
   expansion roadmap updated to reflect remaining Vulkan RT and live-integration gaps.
5. `docs/getting-started.md` — test-suite table updated with v0.3 counts (2210 total).

### Exit criteria

- Version string in `CMakeLists.txt` reads `0.3.0`.
- CHANGELOG v0.3 section dated; no "upcoming" on v0.3.
- API summary and testing-strategy docs consistent with workspace state.

---

## Track 2 — M5 Contributor Scale-Up

### Deliverables

1. `CONTRIBUTING.md` expanded with:
   - Domain ownership table (12 domains, primary owners, header namespaces).
   - API surface rules (manifest update, removal rationale, breaking-change policy).
   - Test authoring guidance (naming convention, Null-backend coverage requirement,
     perf ceiling requirement, CMakeLists registration).
2. New `docs/contributing-kernel.md` — comprehensive kernel contribution guide:
   - Repository layout reference.
   - Null backend parity requirement explained.
   - Step-by-step recipes: adding `FrameStats` field, `RendererSettings` flag, public
     header, test file.
   - Vulkan-only code gating rules.
   - Render-pass sequence documented (8-step order in `Renderer::render()`).
   - RT/mesh-shader path notes.
   - Perf smoke constraints.
   - CI checklist.

### Exit criteria

- `CONTRIBUTING.md` includes domain ownership table and API surface rules section.
- `docs/contributing-kernel.md` exists and covers all recipe areas listed above.

---

## Track 3 — v0.4 Planning

### Deliverables

1. New `docs/v0.4-planning.md`:
   - Motivation: live DLSS4/XeSS on Vulkan RT hardware.
   - Three tracks: VulkanNeuralRenderer, live RT dispatch tests, hardware perf gates.
   - Prerequisites table (Vulkan RT runner, DLSS SDK, XeSS SDK).
   - Out-of-scope section.
   - Exit criteria (5 items).
2. `ROADMAP.md` Next 4-6 Weeks updated to point at v0.4 and `docs/v0.4-planning.md`.

### Exit criteria

- `docs/v0.4-planning.md` exists with all three tracks documented.
- `ROADMAP.md` no longer references v0.3 as upcoming.
