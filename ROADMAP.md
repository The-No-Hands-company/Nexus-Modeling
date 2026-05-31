# Nexus Modeling Roadmap

This roadmap tracks delivery timing for production-grade kernel milestones. Detailed work items live in GitHub Issues/Projects.

## Active Principle

Document the vision in PRD, the logic in SDD, and the timing in this roadmap.

## Milestones

| Milestone | Status | Priority | Target | Notes |
|---|---|---|---|---|
| M1: Renderer Core Stabilization | **Complete** | Critical | v0.2 | GBuffer sampling contract, descriptor lifecycle, pass sequencing, path parity — all done Month 14–15 |
| M2: Shadow Pipeline | In Progress | Critical | v0.3 | Depth atlas + lighting contract done; draw-call verification in progress (Month 16) |
| M3: Render Path Parity | **Complete** | High | v0.3 | Formally de-scoped Month 15; scheduler path is authoritative; direct path documented |
| M4: Advanced Pipelines | In Progress | High | v0.4 | GPU timestamp profiling layer in progress (Month 16); RT/mesh-shader production deferred to rt-capable runner |
| M5: Contributor Scale-Up | In Progress | High | v0.2-v0.4 | Expand tests/docs and improve onboarding workflow |

## Next 4-6 Weeks

1. GPU timestamp profiling layer — `FrameTimingLayer` public API + `FrameStats::gpuTimeMs` population.
2. Shadow pass end-to-end draw-call verification (M2 gate test).
3. `perf_smoke` GPU timing output field.
4. Documentation and status table updates for M1 closure.

## How Work Is Tracked

- Roadmap timing and sequencing: this file.
- Task execution and ownership: GitHub Issues/Projects.
- Implemented behavior reference: docs and merged pull requests.
