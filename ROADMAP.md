# Nexus Modeling Roadmap

This roadmap tracks delivery timing for production-grade kernel milestones. Detailed work items live in GitHub Issues/Projects.

## Active Principle

Document the vision in PRD, the logic in SDD, and the timing in this roadmap.

## Milestones

| Milestone | Status | Priority | Target | Notes |
|---|---|---|---|---|
| M1: Renderer Core Stabilization | **Complete** | Critical | v0.2 | GBuffer sampling contract, descriptor lifecycle, pass sequencing, path parity — all done Month 14–15 |
| M2: Shadow Pipeline | **Complete** | Critical | v0.3 | Depth atlas + lighting contract + `shadowDrawCalls` gate test — all done Month 16–17 |
| M3: Render Path Parity | **Complete** | High | v0.3 | Formally de-scoped Month 15; scheduler path is authoritative; direct path documented |
| M4: Advanced Pipelines | **Complete** | High | v0.4 | GPU timestamps (M16); CPU cull timing (M17); TAA (M18); CI hardening (M19); async-compute denoiser+upscaler + perf gate (M20–21); RT/mesh-shader production path (M22) |
| M5: Contributor Scale-Up | In Progress | High | v0.2-v0.4 | Expand tests/docs and improve onboarding workflow; `TemporalAccumulation` test coverage added (Month 18) |

## Next 4-6 Weeks

1. v0.10 release — Image-Based Lighting (IBL), Order-Independent Transparency (OIT), AMD FSR 4.
2. M5 contributor scale-up — continue onboarding doc expansion and API surface coverage.
3. v0.10 detail — see docs/v0.10-planning.md.

## How Work Is Tracked

- Roadmap timing and sequencing: this file.
- Task execution and ownership: GitHub Issues/Projects.
- Implemented behavior reference: docs and merged pull requests.
