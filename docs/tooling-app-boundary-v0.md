# Tooling App Boundary v0

Status: Active for Editor Bring-Up Month 1

Owner domain: Product and Tooling Layer

Related docs:

1. [editor-bringup-month-1-plan.md](editor-bringup-month-1-plan.md)
2. [phase-0-dev-viewer-dashboard-spec.md](phase-0-dev-viewer-dashboard-spec.md)
3. [kernel-capability-map.md](kernel-capability-map.md)
4. [ux-baseline-charter.md](ux-baseline-charter.md)

## 1. Intent

Define the first hard boundary for the tooling app so Week 1 through Week 4 implementation does not drift into kernel internals or premature full-editor scope.

## 2. Layer Contract

The tooling app is in the Product and Tooling Layer.

It is a consumer of kernel contracts and generated artifacts. It is not a second kernel implementation.

## 3. Allowed Inputs (Month 1)

The tooling app may consume only:

1. Scenario manifest:
   - `scripts/scenarios/manifest.tsv`
2. Scenario artifacts:
   - `build/scenario_artifacts/<scenario_id>/summary.json`
   - `build/scenario_artifacts/<scenario_id>/diagnostics.txt`
   - `build/scenario_artifacts/<scenario_id>/frame.png`
   - `build/scenario_artifacts/<scenario_id>/frame_hash.txt`
   - `build/scenario_artifacts/<scenario_id>/deterministic_signature.txt`
3. Capability outputs:
   - `build/capability_dashboard.json`
   - `build/known_gaps.json`
4. Public kernel API surfaces under `src/kernel/include/nexus`, including starter workflow APIs such as ModelingShell.

## 4. Forbidden Dependencies

The tooling app must not:

1. include or depend on files under `src/kernel/src`
2. include internal implementation headers outside public `src/kernel/include/nexus`
3. parse ad-hoc log output as contract data when typed artifacts exist
4. duplicate scenario runner, dashboard, or kernel business logic inside app code

## 5. Month 1 Feature Surface

The app v0.1 scope is strictly:

1. list canonical scenarios
2. run scenarios through existing command flow
3. display frame artifact or frame hash
4. display diagnostics
5. display capability dashboard snapshot and known gaps
6. expose one narrow starter modeling flow through public ModelingShell APIs

## 6. Month 1 Non-Goals

The following are explicitly deferred:

1. live scene-edit toolset
2. rich selection model
3. undo/redo implementation
4. transform gizmos/manipulators
5. timeline or node-canvas editing
6. content browser and project-management UX

## 7. Change Protocol

When app work needs missing data:

1. add a typed kernel public contract or deterministic artifact first
2. update docs to capture the new contract
3. consume that contract from app code without internal-header shortcuts

## 8. Verification Rules

Every app-related PR in Month 1 should verify:

1. app target builds with default workspace build
2. app can launch in no-op/help mode without crash
3. app features only use allowed inputs and public kernel APIs
4. no direct dependency on `src/kernel/src` appears in app includes or CMake wiring

## 9. Exit Condition for This Boundary

This boundary remains in force until a follow-up boundary document for Month 2 is approved.

Any attempt to add rich editor features before Month 1 exit must be treated as out-of-scope unless explicitly approved in the bring-up plan update.
