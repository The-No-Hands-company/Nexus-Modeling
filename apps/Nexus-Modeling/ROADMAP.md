# Nexus Modeling Roadmap

This roadmap tracks delivery timing for production-grade kernel milestones. Detailed work items live in GitHub Issues/Projects.

## Active Principle

Document the vision in PRD, the logic in SDD, and the timing in this roadmap.

## Milestones

| Milestone | Status | Priority | Target | Notes |
|---|---|---|---|---|
| M1: Renderer Core Stabilization | Completed | Critical | v0.2 | GBuffer sampling contract hardened, descriptor lifecycle verified, 1630 tests passing |
| M2: Shadow Pipeline | Completed | Critical | v0.3 | Cascaded shadow atlas with depth-only pass, per-cascade UBO, push constants, Vulkan HW-verified |
| M3: Render Path Parity | Completed | High | v0.3 | Formally de-scoped: scheduler path is authoritative; non-scheduler path is intentional minimal fallback per SDD |
| M4: Advanced Pipelines | Completed | High | v0.4 | Mesh shader, compute, RT pipeline + texture upload + render loop + frame sync + GPU timestamps — all production implementations, all tested |
| M5: Contributor Scale-Up | In Progress | High | v0.2-v0.4 | Expand tests/docs and improve onboarding workflow |

## Next 4-6 Weeks

1. Month 4: Parametric foundation — constraint graph core, solver v0, serialization.
2. Month 5: Modeling workflow — boolean/bevel/remesh v0, end-to-end hard-surface slice.
3. Continue test coverage expansion, hardening, and doc maintenance.

## How Work Is Tracked

- Roadmap timing and sequencing: this file.
- Task execution and ownership: GitHub Issues/Projects.
- Implemented behavior reference: docs and merged pull requests.
