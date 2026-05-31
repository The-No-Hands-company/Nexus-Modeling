# Contributing to Nexus Modeling

Thanks for helping build Nexus Modeling. This project is a C++23 Vulkan-first geometry/rendering kernel with strict correctness and determinism requirements.

## Before You Start

1. Read README.md and docs/README.md.
2. Check ROADMAP.md for current milestone priorities.
3. Pick or create a GitHub Issue before opening a pull request.

## Development Rules

- Keep changes focused and reversible.
- Do not mix unrelated refactors into feature or bug-fix PRs.
- Preserve public API stability under src/kernel/include/nexus.
- Keep Null backend parity for interface and behavior changes.
- Treat synchronization, transitions, and lifetimes as first-class concerns.

## Build and Test (Required)

Run from repository root:

1. cmake -S . -B build
2. cmake --build build -j$(nproc)
3. ctest --test-dir build --output-on-failure

If you cannot run a required gate, state exactly what was skipped and why in the PR.

## Domain Ownership

Each public header namespace is owned by a primary team. Route review requests
and design questions to the primary owner first.

| Domain | Primary Owner | Headers |
|---|---|---|
| Geometry and Parametric | Geometry Core | `nexus/geometry/`, `nexus/parametric/` |
| Rendering and GFX | Render Core | `nexus/gfx/`, `nexus/render/` |
| Neural Integrations | Render Core | `nexus/neural/` |
| Animation | Evaluation and Animation Core | `nexus/animation/` |
| Procedural Evaluation Graph | Evaluation and Animation Core | `nexus/eval/`, `nexus/eval_ext/` |
| Scene and Node Graph | Evaluation and Animation Core | `nexus/scene/` |
| Simulation | Simulation Core | `nexus/sim/` |
| Sculpt | Geometry Core | `nexus/sculpt/` |
| Asset and Data | Asset and Data Core | `nexus/asset/` |
| Scripting and Automation | Scripting and Integration Core | `nexus/automation/`, `nexus/script/` |
| Vulkan Backend | Platform Backend | `nexus/gfx/vulkan/` |
| Kernel Entry | Asset and Data Core | `nexus/Kernel.h` |

Cross-domain changes require reviewers from each impacted domain.

## API Surface Rules

The `1.0-alpha` public API surface is enforced by:
- `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt` — the ground truth list of 93 public headers.
- `tests/kernel/test_ApiFreezeAudit.cpp` — CI-required audit; fails if workspace drifts from manifest.

**Rules for changes to the public surface:**
1. Adding a header: add it to the manifest in the same PR. Include domain assignment and ownership.
2. Removing or renaming: provide a compatibility rationale and migration notes; get domain-owner sign-off.
3. Changing a public type's layout or removing a method: treat as a breaking change — requires explicit milestone targeting.

## Test Authoring Guidance

- Name tests `SubjectVerb` or `SubjectConditionExpectation` (e.g. `RTReflectionsInactiveOnNullBackend`).
- Every new render-path feature needs Null-backend test coverage for all mode-gate branches.
- Perf-sensitive dispatch paths need an overhead ceiling assertion (see `test_NeuralUpscaler.cpp`).
- Use `StubNeuralRenderer` and similar fixtures from existing test files — don't invent new fakes.
- Add the new `.cpp` file to `tests/CMakeLists.txt` in the `nexus_kernel_tests` sources list.

## Definition of Done

A contribution is done when all of the following are true:

1. Behavior is implemented and scoped to a single concern.
2. Tests are added or updated for behavior/regression coverage.
3. Build and test gates pass locally, or skips are documented.
4. Relevant documentation is updated (CHANGELOG.md, affected docs/ files).
5. API manifest updated if any public header was added or removed.
6. No secrets/tokens/credentials are introduced.

## Pull Request Checklist

- Describe user-visible behavior changes.
- Call out synchronization/resource-lifetime implications.
- Link the issue and tests covering the change.
- Include residual risks and untested paths.
- Complete the required `Kernel Capability Declaration` section in `.github/PULL_REQUEST_TEMPLATE.md`.

## Review Style

Reviews prioritize findings in this order:

1. Correctness regressions.
2. Synchronization and resource-lifetime risks.
3. API contract breaks.
4. Missing or weak tests.

Use concrete file/line references and keep summaries secondary to findings.
