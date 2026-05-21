# Phase 0 Spec: Kernel Capability Dashboard + Dev Viewer

Status: Draft for immediate execution

Owner domain: Product and Tooling Layer (consumes kernel APIs only)

Related docs:

1. vision-and-roadmap.md
2. kernel-capability-map.md
3. PRD.md
4. SDD.md
5. testing-strategy.md
6. perf-benchmark-matrix.md
7. memory-lifetime-report.md

## 1. Intent

Phase 0 creates a developer-facing visibility layer so kernel progress is visible as runnable artifacts, not only test output.

This phase must stay kernel-first and avoid UI debt by keeping the UI thin, read-mostly, and contract-driven.

## 2. Scope

In scope:

1. Kernel Capability Dashboard.
2. Canonical showcase scenarios (5 to 10).
3. Minimal Dev Viewer (scenario load, run pipeline, frame + diagnostics display).
4. Monthly vertical demo package with reproducible artifacts.

Out of scope:

1. General modeling editor UX.
2. Interactive authoring tools (extrude/inset/bevel gizmo flows).
3. New kernel features added only to serve UI polish.
4. Workflow parity with DCC applications.

## 3. Architecture Boundaries

Phase 0 must follow the capability map boundaries.

Rules:

1. No direct access from UI to internal kernel implementation under src/kernel/src.
2. Dev Viewer and Dashboard may use only public API contracts under src/kernel/include/nexus and stable script entry points.
3. Any missing data needed by viewer/dashboard must be added as typed kernel contract surfaces first.
4. Scenario execution must work on Null backend for deterministic CI and headless runs.
5. Scheduler path remains the authoritative render path for production-like checks.

Layer split:

1. Kernel Core: data production and execution.
2. Product and Tooling Layer: dashboard/viewer presentation.
3. Integration Layer: artifact export and CI publishing.

## 4. Deliverable A: Kernel Capability Dashboard

## 4.1 Dashboard Objective

Expose a single status page that answers:

1. Which subsystems are healthy now?
2. Are deterministic checks passing?
3. Is perf smoke trending stable?
4. What are current known gaps and their severity?

## 4.2 Subsystem Status Model

Subsystems for Phase 0 reporting:

1. Graphics Backend (Null, Vulkan)
2. Render Pipeline
3. Scene and Eval Graph
4. Geometry Core
5. Parametric Foundation
6. Simulation Core
7. Gaussian and Neural Integration
8. Asset and Automation

Each subsystem exposes:

1. status: green | yellow | red
2. confidence: high | medium | low
3. required_gates: pass/fail counts for build + tests
4. deterministic_signature: stable hash + last change timestamp
5. perf_summary: current median, baseline median, delta percent
6. known_gaps: array of gap identifiers

## 4.3 Dashboard Artifact Contract

Output file:

1. build/capability_dashboard.json

Top-level fields:

1. generated_at_utc
2. git_commit
3. environment
4. subsystems
5. deterministic_checks
6. perf_smoke
7. known_gaps
8. monthly_demo_status

The dashboard is data-first. Any HTML/GUI rendering reads this JSON and adds no hidden state.

## 4.4 Known Gaps Ledger Contract

Source of truth file:

1. docs/hardening-debt-ledger.md (human-readable)
2. build/known_gaps.json (machine-readable export)

Each gap includes:

1. id
2. domain
3. severity: S1 | S2 | S3
4. user_impact
5. deterministic_risk: none | low | medium | high
6. owner
7. target_milestone
8. status

## 5. Deliverable B: Canonical Showcase Scenarios

## 5.1 Objective

Provide a stable set of scripted scenarios that demonstrate real capability and produce deterministic output artifacts.

## 5.2 Scenario Count

Phase 0 target: 8 scenarios (acceptable range 5 to 10).

## 5.3 Initial Scenario Pack

1. Render scheduler baseline scene
2. Shadow chain contract scene
3. Composite binding contract scene
4. Eval graph deterministic evaluation scene
5. NodeScene reconstruction diagnostics scene
6. Parametric rectangle replay scene
7. Simulation deterministic trajectory scene
8. Gaussian splat + mesh coexistence scene

## 5.4 Scenario Contract

Each scenario has:

1. scenario_id
2. purpose
3. backend_mode: null | vulkan
4. setup script or command contract
5. expected deterministic outputs
6. pass/fail predicates

Recommended location:

1. scripts/scenarios/

Artifact output location:

1. build/scenario_artifacts/<scenario_id>/

Standard artifacts per scenario:

1. summary.json
2. diagnostics.txt
3. frame.png (if applicable)
4. frame_hash.txt
5. deterministic_signature.txt

## 5.5 Deterministic Signature Policy

For each scenario, deterministic signature is computed from:

1. ordered evaluation/report lines
2. selected typed diagnostic snapshots
3. frame hash (if image-producing)

Signature mismatch is a regression candidate and must be triaged.

## 6. Deliverable C: Minimal Dev Viewer

## 6.1 Objective

A thin viewer for developers to run scenarios and inspect outputs quickly without building editor workflows.

## 6.2 Functional Slice

Required features:

1. List available scenarios from a manifest.
2. Load selected scenario.
3. Run scenario pipeline.
4. Display latest frame artifact (if present).
5. Display deterministic diagnostics and signature.
6. Display subsystem status snapshot from capability dashboard.

Explicit non-features:

1. Modeling tools
2. Manipulators
3. Undo/redo authoring stack
4. Asset browser UX
5. Plugin management UX

## 6.3 Viewer Data Flow

1. Viewer triggers scenario runner command.
2. Runner writes artifacts to build/scenario_artifacts.
3. Dashboard generator updates build/capability_dashboard.json.
4. Viewer reads artifacts + dashboard JSON and renders them.

No mutable runtime state in viewer beyond current selection.

## 6.4 Technology Constraint

Any UI stack is acceptable if it keeps these constraints:

1. Build integration must remain simple in CI.
2. Viewer must run in headless-safe mode for smoke checks.
3. Viewer code remains isolated from kernel internals.

## 7. Deliverable D: Monthly Vertical Demo

## 7.1 Objective

Publish one constrained, reproducible demo slice every month to show tangible progress.

## 7.2 Monthly Demo Package

Package location:

1. build/monthly_demo/<YYYY-MM>/

Required contents:

1. demo_summary.md
2. scenario_ids.txt
3. capability_dashboard.json snapshot
4. deterministic_signatures.txt
5. representative frame artifacts
6. known_gaps_snapshot.json

## 7.3 Demo Selection Rule

Each month chooses one vertical slice that spans at least 3 subsystems and is runnable on Null backend.

Examples:

1. Parametric solve -> NodeScene integration -> render diagnostic export.
2. Geometry op chain -> scene update -> composite pass artifact.
3. Gaussian load -> mixed scene render -> deterministic report.

## 8. CLI and Automation Contracts

Phase 0 standard commands (names may be adjusted, intent fixed):

1. tools/capability_dashboard.sh
   - Generates build/capability_dashboard.json
2. tools/run_scenario.sh <scenario_id>
   - Executes scenario and writes artifacts
3. tools/run_scenario_pack.sh
   - Executes all canonical scenarios
4. tools/validate_dashboard_artifacts.sh
   - Validates capability_dashboard.json and known_gaps.json structure before artifact publish
5. tools/monthly_demo_report.sh --month YYYY-MM
   - Builds monthly demo package

All commands must return non-zero on contract violations.

## 9. Test and Quality Gates

Required gates for Phase 0 additions:

1. cmake --build build -j$(nproc)
2. ctest --test-dir build --output-on-failure
3. Scenario pack deterministic replay check (run twice, compare signatures)
4. Dashboard schema validation check

Required tests:

1. Dashboard contract tests (field presence and deterministic ordering)
2. Scenario artifact contract tests
3. Dev Viewer adapter tests (manifest parse, artifact load, missing-file handling)
4. Monthly demo package completeness tests
5. Artifact schema validation command passes before publish.

## 10. Exit Criteria for Phase 0

Phase 0 is complete only when all are true:

1. Dashboard reports all listed subsystems with status, deterministic checks, perf trend, and known gaps.
2. At least 5 canonical scenarios exist; target 8 completed.
3. Scenario outputs are deterministic across repeated Null backend runs.
4. Dev Viewer can load and run scenarios and display frame + diagnostics.
5. First monthly demo package is generated and archived.
6. No kernel-internal headers are used by dashboard/viewer.

## 11. Milestone Plan (Execution-Ready)

Week 1:

1. Define dashboard JSON schema and known-gaps JSON export.
2. Implement first 3 scenarios and artifact schema.
3. Add deterministic signature computation helper.

Week 2:

1. Complete full canonical scenario pack (5 to 10).
2. Implement capability dashboard generator command.
3. Add CI artifact publish step for dashboard + scenario outputs.

Week 3:

1. Implement minimal Dev Viewer (manifest, run, frame, diagnostics, status).
2. Add viewer smoke tests and error-state handling.

Week 4:

1. Implement monthly demo pack command.
2. Publish first monthly vertical demo.
3. Close first-pass known gaps triage and set next-phase priorities.

## 12. Anti-Debt Guardrails

To avoid UI debt and architecture drift:

1. UI cannot introduce kernel-side behavior branches.
2. Viewer cannot own business logic; it only displays contract outputs.
3. Any new required metric must be added as typed contract or deterministic artifact, never scraped from logs ad hoc.
4. Keep scenario definitions declarative and versioned.
5. Prefer additive contract evolution and preserve backward compatibility for artifact readers.

## 13. Risks and Mitigations

Risk 1: Dashboard becomes manually curated and stale.

1. Mitigation: derive all fields from generated artifacts and gates.

Risk 2: Viewer evolves into premature editor shell.

1. Mitigation: enforce non-feature list and require scope review for interaction additions.

Risk 3: Deterministic checks fail on Vulkan-dependent paths.

1. Mitigation: Null backend is required baseline; Vulkan artifacts are optional and tagged environment-dependent.

Risk 4: Scenario maintenance burden grows.

1. Mitigation: cap canonical scenario pack to 10 and enforce ownership per domain.

## 14. Immediate Next Actions

1. Create dashboard JSON schema and placeholder generator.
2. Define scenario manifest format and add first 3 scenarios.
3. Stand up a text-first viewer (terminal or minimal window) before any richer UI shell.
4. Add first monthly demo target using the parametric replay slice.

## 15. Phase 0 Slice 1 Scaffold (Implemented)

The first implementation slice scaffolds the following files:

1. Dashboard schema:
   - tools/schema/capability_dashboard.schema.json
2. Dashboard generator stub:
   - tools/capability_dashboard.sh
3. Scenario manifest format and first 3 scenarios:
   - scripts/scenarios/manifest.tsv
   - scripts/scenarios/render_scheduler_baseline.sh
   - scripts/scenarios/nodescene_reconstruction_diagnostics.sh
   - scripts/scenarios/parametric_rectangle_replay.sh
4. Scenario runner:
   - tools/run_scenario.sh
5. Text-first Dev Viewer shell:
   - tools/dev_viewer.sh

This slice is intentionally minimal and contract-oriented. It validates the data flow from scenario execution to artifact emission and dashboard/viewer consumption without introducing editor-style UI scope.

## 16. Phase 0 Slice 2 Upgrade (Implemented)

The second implementation slice upgrades scaffold behavior to real gate and replay checks:

1. Dashboard generator now derives gate state from command runs and emits machine-readable known gaps:
   - tools/capability_dashboard.sh
   - build/known_gaps.json
2. Scenario pack deterministic replay checker (two-pass compare, non-zero on drift):
   - tools/run_scenario_pack.sh
3. Monthly vertical demo text report command from existing artifacts:
   - tools/monthly_demo_report.sh
4. Dev viewer command wiring for new flows:
   - tools/dev_viewer.sh (run-pack, monthly-report)

## 17. Phase 0 Slice 3 Scenario Coverage Expansion (Implemented)

The scenario pack now reaches the minimum canonical coverage threshold (5 scenarios):

1. Added EvalGraph deterministic evaluation scenario:
   - scripts/scenarios/eval_graph_deterministic_evaluation.sh
2. Added Simulation deterministic trajectory scenario:
   - scripts/scenarios/simulation_deterministic_trajectory.sh
3. Updated canonical scenario manifest to include 5 entries:
   - scripts/scenarios/manifest.tsv

This keeps the pack within the Phase 0 target range (5 to 10) and preserves deterministic replay enforcement through the existing two-pass comparison flow.

## 18. Phase 0 Slice 4 Canonical Pack Completion + Contracts (Implemented)

Phase 0 scenario coverage now reaches the target of 8 canonical scenarios and adds automated contract checks:

1. Added 3 canonical scenarios to complete the target pack:
   - scripts/scenarios/shadow_chain_contract_scene.sh
   - scripts/scenarios/composite_binding_contract_scene.sh
   - scripts/scenarios/gaussian_mesh_coexistence_scene.sh
2. Scenario artifact contract tests added under kernel tests:
   - tests/kernel/test_ScenarioArtifacts.cpp
   - Validates manifest parsing and canonical count target.
   - Runs each manifest scenario through tools/run_scenario.sh and verifies required artifacts.
3. Dashboard gap logic tightened for canonical coverage:
   - tools/capability_dashboard.sh now flags P0-SCENARIO-COVERAGE when manifest canonical total is below 8.
   - Dashboard scenarios section now includes canonical_target and canonical_total.

## 19. Phase 0 Slice 5 Asset/Automation Attribution Split (Implemented)

Asset and Automation diagnostics now include a second dedicated scenario and split scenario check groups:

1. Added canonical scenario for SceneAssetImporter dependency-mode determinism:
   - scripts/scenarios/asset_importer_dependency_mode_scene.sh
2. Updated manifest with tenth canonical scenario:
   - scripts/scenarios/manifest.tsv
3. Asset and Automation subsystem now reports split scenario checks for clearer diagnostics:
   - asset_graph
   - automation_pipeline
   - tools/capability_dashboard.sh
