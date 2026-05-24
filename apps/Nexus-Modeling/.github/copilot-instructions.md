# Copilot Instructions for Nexus Modeling

Nexus Modeling is a C++23 Vulkan-first geometry/rendering kernel. Prioritize correctness, explicit synchronization, and API stability over quick hacks.

## Repository intent

- Build a production-grade 3D modeling/rendering kernel that can scale to a very large codebase.
- Maintain compatibility with Nexus-Cloud integration expectations.
- Preserve deterministic, CI-safe behavior on the Null backend.

## Architecture boundaries

- Public API headers: src/kernel/include/nexus/
- Internal implementation: src/kernel/src/
- Tests: tests/kernel/
- Do not expose internal headers as public API.
- Do not introduce cross-layer shortcuts that bypass IDevice/ICommandBuffer contracts.

## Required quality gates

When changing behavior or interfaces, run:

1. cmake --build build -j$(nproc)
2. ctest --test-dir build --output-on-failure

If something cannot be run locally, state what was skipped and why.

## Coding expectations

- Keep changes focused and reversible.
- Avoid unrelated refactors.
- Prefer explicit lifetime ownership and cleanup.
- Treat synchronization and resource transitions as first-class design constraints.
- Keep Null backend behavior in parity with interface changes.
- Public API entry points must reject non-finite float inputs. This repo builds with `-ffast-math`, so use IEEE-754 bit tests for finiteness.

## Renderer/kernel expectations

- Keep the scheduler-driven render path authoritative.
- New rendering features should land on the scheduler path first.
- Layout transitions and pass ordering must be explicit.
- Do not regress GBuffer + composite sequencing guarantees.
- Keep reversed-Z behavior consistent unless intentionally redesigned.

## Testing expectations

- Add or update tests for each behavior change.
- Prefer behavior-level assertions over implementation-detail assertions.
- Add regression tests for fixed bugs.
- Keep tests deterministic and headless-safe and runnable on the Null backend.

## Documentation expectations

- Update README.md for workflow or architecture changes.
- Update docs/ when adding or changing subsystem behavior.
- Keep comments concise and useful for non-obvious logic.

## VS Code guidance

This repository uses GitHub Copilot Chat in VS Code. There is no separate VS Code skill file format equivalent to `.claude/commands` in the same way; instead, use this instruction file and the workflow guidance in `docs/vscode-agent-setup.md`.

### Workflow personas

- `software-architect` — Design APIs, cross-module architecture, and high-level feature shape.
- `build-engineer` — Wire CMake targets, sources, and build-system fixes.
- `code-reviewer` — Review diffs for correctness, determinism, and hardening.
- `test-engineer` — Add deterministic GoogleTest coverage and register tests.
- `geometry-engineer` — Implement geometry and mesh contracts.
- `render-engineer` — Implement scheduler-driven rendering and validator behavior.
- `gpu-backend-engineer` — Implement `gfx` backend and Vulkan/Null integration.
- `parametric-engineer` — Implement parametric/eval/scene graph logic.
- `asset-pipeline-engineer` — Implement asset import/export and automation pipelines.
- `animation-engineer` — Implement animation sampling and playback state.
- `simulation-engineer` — Implement the simulation solver, driver, and coupling.
- `neural-engineer` — Implement neural rendering glue only.

### Workflow names

- `feature` — Full pipeline from design through implementation, test, and review.
- `harden` — Non-finite-input and public API hardening.
- `kreview` — Focused code-review workflow.

## Security and safety

- Never commit secrets, credentials, tokens, or private keys.
- Avoid destructive git commands unless explicitly requested.

## Preferred review style

- Prioritize findings in this order: correctness regressions, synchronization/resource-lifetime risks, API contract breaks, then missing or weak tests.
- Report concrete, actionable findings first with file/line references.
- Explicitly call out residual risk and untested paths before approving.
