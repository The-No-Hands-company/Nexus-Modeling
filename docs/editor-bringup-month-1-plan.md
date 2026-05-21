# Editor Bring-Up Month 1 Plan

Status: Execution-ready

Owner domain: Product and Tooling Layer consuming kernel public APIs only

Related docs:

1. [phase-0-dev-viewer-dashboard-spec.md](phase-0-dev-viewer-dashboard-spec.md)
2. [kernel-capability-map.md](kernel-capability-map.md)
3. [ux-baseline-charter.md](ux-baseline-charter.md)
4. [vision-and-roadmap.md](vision-and-roadmap.md)
5. [month-5-modeling-checklist.md](month-5-modeling-checklist.md)
6. [month-6-asset-pipeline-checklist.md](month-6-asset-pipeline-checklist.md)

## 1. Objective

Turn the repository from kernel plus scripts into kernel plus a thin tooling app.

The app in Month 1 is not a full editor. It is a developer-facing shell that:

1. launches from CMake as its own target
2. consumes only public kernel APIs and generated artifacts
3. rehosts the current scenario and dashboard workflow in an app surface
4. proves one narrow authoring slice through ModelingShell

## 2. Scope Lock

In scope:

1. one product/tooling app target added to the build
2. one app boundary design note for kernel-facing and artifact-facing contracts
3. read-only scenario, diagnostics, and dashboard workspace
4. one starter modeling workspace backed by ModelingShell
5. app smoke tests and CI wiring
6. one month-end review note with next-phase backlog split

Out of scope:

1. direct mesh editing UX
2. selection-rich scene manipulation
3. undo and redo implementation
4. transform gizmos and manipulators
5. timeline, graph canvas, or content browser UX
6. app-owned copies of kernel business logic

## 3. Current-State Constraints

The plan assumes the repository state that exists now:

1. there is no application target yet in the top-level build; the build currently exposes kernel libraries, tests, and perf smoke only
2. a text-first viewer flow already exists via tools scripts and scenario artifacts
3. canonical scenarios are declared in scripts/scenarios/manifest.tsv
4. capability dashboard generation already exists through tools/capability_dashboard.sh
5. ModelingShell already provides a public, UI-consumable starter workflow surface
6. editor-only features must stay outside the kernel layer per the capability map

## 4. Build and Boundary Rules

Rules for all Month 1 work:

1. The app may include only public headers under src/kernel/include/nexus.
2. The app may read generated artifacts under build/ and scripts/scenarios/manifest.tsv.
3. The app may invoke existing scenario and dashboard commands, but it may not reimplement their business logic.
4. Any missing information required by the app must be added as an explicit kernel public contract or deterministic artifact.
5. App code must stay isolated from src/kernel/src.

## 5. Deliverable Shape at End of Month

By the end of Week 4 the repository should contain:

1. a launchable tooling app target
2. a read-only viewer workspace for scenarios and dashboard outputs
3. a starter modeling workspace backed by ModelingShell
4. app smoke tests and CI smoke integration
5. a review note splitting follow-up work into kernel contract gaps, app infrastructure, and deferred UX

## 6. Week 1 Backlog — App Shell and Contract Boundary

Goal: establish the application target and prevent architecture drift before UI work starts.

### Day 1

Tasks:

1. Create app folder layout under a product/tooling root.
2. Add a new CMake target for the app, separate from tests and perf smoke.
3. Link the app only against public kernel libraries already exported by the build.

Verify:

1. default workspace build includes the new app target
2. app binary launches and exits cleanly with a help or no-op mode

Done when:

1. one app target builds locally with the rest of the workspace

Suggested PR slice:

1. PR-1 App target scaffold

### Day 2

Tasks:

1. Write the application boundary note.
2. Declare the app as consumer of:
	- scenario manifest
	- scenario artifacts
	- capability dashboard JSON
	- public kernel workflow APIs such as ModelingShell
3. Record the explicit non-goals for Month 1.

Verify:

1. note references only public API roots and artifact contracts
2. note explicitly forbids internal-header usage

Done when:

1. a short design note exists and is linked from docs if needed

Suggested PR slice:

1. PR-2 App boundary note

### Day 3

Tasks:

1. Implement manifest loading inside the app.
2. Mirror the current scenario list behavior from tools/dev_viewer.sh.
3. Add simple list output or first UI list surface.

Verify:

1. app can load canonical scenarios from scripts/scenarios/manifest.tsv
2. malformed or missing manifest is surfaced as an app error state

Done when:

1. scenario list is visible through the app surface

Suggested PR slice:

1. PR-3 Manifest loader and scenario list

### Day 4

Tasks:

1. Implement capability dashboard file loading in the app.
2. Support absent dashboard file without crashing.
3. Display raw or minimally formatted subsystem status summary.

Verify:

1. app loads build/capability_dashboard.json when present
2. app shows a clean empty-state message when it is absent

Done when:

1. dashboard snapshot is visible through the app

Suggested PR slice:

1. PR-4 Dashboard loader and empty-state handling

### Day 5

Tasks:

1. Add Week 1 smoke tests covering:
	- manifest loading
	- dashboard loading
	- missing file handling
	- no-crash startup
2. Add any minimal test data required for adapter coverage.

Verify:

1. app smoke tests pass locally
2. build still passes with the new app target enabled

Week 1 acceptance checks:

1. One new app target builds in the default workspace.
2. The app can read canonical scenarios from scripts/scenarios/manifest.tsv.
3. The app can load and display capability dashboard JSON when present.
4. No kernel internal header dependency is introduced.

Suggested PR slice:

1. PR-5 Week 1 smoke coverage

## 7. Week 2 Backlog — Read-Only Viewer Workspace

Goal: make the app a useful daily front end to the existing scenario pipeline.

### Day 1

Tasks:

1. Add the first shell layout.
2. Reserve panes for:
	- scenario list
	- viewport or image panel
	- diagnostics panel
	- subsystem status panel
	- run controls
3. Keep layout static and minimal.

Verify:

1. app renders all planned regions without interaction debt

Suggested PR slice:

1. PR-6 Viewer layout shell

### Day 2

Tasks:

1. Add artifact loading for summary.json, diagnostics.txt, frame.png, and frame_hash.txt.
2. Implement clean empty-state handling for each artifact type.
3. Show failed scenario state from summary.json status.

Verify:

1. missing summary, diagnostics, and frame artifacts produce clear app messages
2. frame image and frame hash are both supported presentation paths

Suggested PR slice:

1. PR-7 Artifact loader and error states

### Day 3

Tasks:

1. Wire scenario execution from the app to the existing scenario command flow.
2. Refresh loaded artifacts after run completion.
3. Support at least three canonical scenarios end to end.

Verify:

1. app-triggered scenario run produces the same artifact shape as tools/run_scenario.sh
2. refreshed artifact view matches the latest run output

Suggested PR slice:

1. PR-8 Run scenario integration

### Day 4

Tasks:

1. Wire dashboard refresh from the app to the existing dashboard command.
2. Load known gaps output alongside capability status.
3. Surface command failure states in the app instead of terminal-only errors.

Verify:

1. dashboard refresh from the app updates visible subsystem status and known gaps
2. command failure does not crash the app

Suggested PR slice:

1. PR-9 Dashboard refresh and known gaps view

### Day 5

Tasks:

1. Expand viewer smoke tests to cover:
	- scenario run path
	- artifact refresh path
	- malformed dashboard JSON
	- missing artifact states
2. Capture any UX paper cuts as deferred backlog notes, not feature creep.

Verify:

1. app can run at least three canonical scenarios end to end in local validation
2. viewer smoke tests cover manifest parse, artifact load, and failure states

Week 2 acceptance checks:

1. The app lists all canonical scenarios from scripts/scenarios/manifest.tsv.
2. The app can run at least three scenarios end to end and present outputs cleanly.
3. The app can show current dashboard and known gaps generated by tools/capability_dashboard.sh.
4. Viewer smoke tests cover parse, load, refresh, and failure states.

Suggested PR slice:

1. PR-10 Week 2 smoke and polish

## 8. Week 3 Backlog — First True Editor Slice via ModelingShell

Goal: prove one narrow authoring flow without introducing a general-purpose tool system.

### Day 1

Tasks:

1. Add a starter modeling workspace in the app.
2. Expose primitive choice only for the current ModelingShell starter set.
3. Keep the surface aligned with the UX charter simple-by-default rule.

Verify:

1. starter workspace presents primitive choice without advanced modeling panels

Suggested PR slice:

1. PR-11 Starter modeling workspace shell

### Day 2

Tasks:

1. Integrate ModelingShell startStarterModel flow directly through the public header.
2. Display starter session report and current diagnostics overlay data.
3. Do not add hidden app-side workflow behavior.

Verify:

1. user can choose a primitive and create a starter model from the app
2. diagnostics overlay readout is visible after model creation

Suggested PR slice:

1. PR-12 ModelingShell starter flow integration

### Day 3

Tasks:

1. Add quick cleanup action through ModelingShell.
2. Refresh displayed diagnostics after cleanup.
3. Show workflow step reporting from the shell session report.

Verify:

1. quick cleanup runs through public API only
2. updated diagnostics and workflow steps are visible in the app

Suggested PR slice:

1. PR-13 ModelingShell cleanup and diagnostics refresh

### Day 4

Tasks:

1. Add narrow import and export entry points for the slice.
2. Use existing asset surfaces for simple scene and package load paths where applicable.
3. Support exporting resulting scene state or diagnostics snapshot, not complex project packaging.

Verify:

1. app can load a simple scene or package path through public asset surfaces
2. app can export resulting scene or diagnostics output for inspection

Suggested PR slice:

1. PR-14 Narrow asset I/O for starter slice

### Day 5

Tasks:

1. Add a tiny session model storing only:
	- current scenario or starter-model session
	- current artifact paths
	- current diagnostics snapshot
	- current model state handle or equivalent app-side reference
2. Add one end-to-end starter-model smoke test.
3. Record any kernel contract gaps discovered during integration.

Verify:

1. guided starter model flow works end to end in the app
2. one smoke test covers create, cleanup, and diagnostics inspection

Week 3 acceptance checks:

1. A user can launch the app, choose a primitive, create a starter model, run quick cleanup, and inspect diagnostics.
2. The app still compiles and runs without using kernel internals.
3. At least one guided starter model smoke test exists.
4. Missing kernel-facing information is documented as explicit contract work.

Suggested PR slice:

1. PR-15 Week 3 integration and contract-gap log

## 9. Week 4 Backlog — Foundation Hardening for Real Editor Growth

Goal: end the month with a stable shell and clean seams for Month 2 work.

### Day 1

Tasks:

1. Add editor-state scaffolding interfaces for:
	- document or session
	- selection
	- command execution
	- future undo and redo
2. Keep them as explicit seams, not full systems.

Verify:

1. interfaces compile and are consumed by the app without forcing rich UX features

Suggested PR slice:

1. PR-16 Editor-state seams scaffold

### Day 2

Tasks:

1. Add one minimal scene or document panel.
2. Show read-only scene or session outline, current object or workflow status, and diagnostics summary.
3. Avoid building an outliner or editor hierarchy system.

Verify:

1. panel helps orient the user without adding new editing behavior

Suggested PR slice:

1. PR-17 Scene or session panel

### Day 3

Tasks:

1. Add persistence basics for the app shell.
2. Store last workspace pane state and last open scenario or modeling session metadata.
3. Preserve app logs and failure reports in a deterministic location.

Verify:

1. app restarts into the last workspace state without crashing on stale data

Suggested PR slice:

1. PR-18 Persistence and recovery basics

### Day 4

Tasks:

1. Add CI smoke coverage for the app target.
2. Include:
	- build app target
	- no-crash launch smoke
	- scenario-open smoke
	- ModelingShell starter-flow smoke
3. Keep the CI surface headless-safe.

Verify:

1. app target participates in CI without special manual steps

Suggested PR slice:

1. PR-19 CI app smoke integration

### Day 5

Tasks:

1. Produce the Month 1 editor bring-up review note.
2. Answer:
	- which kernel contracts were sufficient
	- which app seams were missing
	- what must be built before real editor interaction
	- what remains out of scope for Month 2
3. Split the next-phase backlog into:
	- kernel contract gaps
	- app infrastructure
	- deferred rich UX

Verify:

1. review note is specific enough to drive the next month without repeating discovery work

Week 4 acceptance checks:

1. The app shell is stable enough to use as a daily developer tool.
2. One narrow authoring slice works through public kernel APIs.
3. CI includes app smoke checks.
4. The next-phase editor backlog is split cleanly into kernel contract gaps, app infrastructure, and deferred rich UX.

Suggested PR slice:

1. PR-20 Month-end review and backlog split

## 10. Verification Gates

Run these throughout the month as changes land:

1. cmake --build build -j$(nproc)
2. ctest --test-dir build --output-on-failure
3. app smoke tests
4. scenario-open smoke via app harness
5. starter-model smoke via app harness

If a change modifies scenario or dashboard behavior, also run:

1. tools/run_scenario_pack.sh
2. tools/validate_dashboard_artifacts.sh

## 11. Success Criteria

Month 1 is successful only when all are true:

1. the repository contains a real app target, not only scripts
2. the app provides a better developer experience than the current shell scripts for scenario and dashboard inspection
3. the app proves one real authoring slice via ModelingShell
4. the app layer does not depend on kernel internals
5. smoke coverage exists for startup, scenario loading, dashboard loading, and starter modeling flow
6. a documented next-phase backlog exists without collapsing into premature rich-editor scope

## 12. Suggested Phase Order After Month 1

If Month 1 succeeds, choose one path for the next month and defer the other:

1. Editor infrastructure path:
	- document model
	- selection model
	- command system
	- undo and redo foundation
2. Narrow authoring path:
	- transform tool
	- primitive placement workflow
	- one additional modeling workflow beyond ModelingShell

Recommended default: choose editor infrastructure first, because the current repository has enough kernel capability to make UI visible but not enough editor-state scaffolding for safe interactive growth.
