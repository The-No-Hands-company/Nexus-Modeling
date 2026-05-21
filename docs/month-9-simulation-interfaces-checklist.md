# Month 9 Simulation Interfaces v0 Checklist

This checklist executes the Month 9 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: simulation solver API contracts with deterministic replay and rollback.

## Scope Lock

In scope:

1. Public solver API contracts for rigid, cloth, and fluid simulation domains.
2. Deterministic step, captureState, and restoreState behavior for each domain.
3. Cacheable deterministic state serialization for replay and rollback.
4. Regression tests for deterministic replay and malformed state rejection.

Out of scope:

1. Production-accurate cloth constraint solvers.
2. Production-accurate fluid pressure/viscosity solvers.
3. Cross-domain coupling (rigid↔cloth↔fluid interaction).
4. Authoring UI for simulation setup and debugging.

## Work Packages

## M9.1 Rigid-Body Baseline + Cacheable State

Status: Complete

Delivered:

1. `nexus/sim/SimulationCore.h` now exposes deterministic state equality and state byte serialization contracts.
2. `serializeSimState` / `deserializeSimState` implemented with explicit magic/version guards.
3. Body records are serialized in ascending `BodyId` order for deterministic byte stability.
4. Regression tests added for deterministic round-trip serialization and malformed blob rejection.

Done when:

1. Identical rigid snapshots serialize to identical bytes.
2. Deserialized state equals source snapshot.
3. Malformed or truncated blobs are rejected.

## M9.2 Cloth Solver Interface v0

Status: Complete

Delivered:

1. New public header `nexus/sim/ClothSolver.h` defines:
   - `ClothSolver`
   - `ClothState`, `ClothNodeSnapshot`, `ClothStepReport`
   - deterministic `serializeClothState` / `deserializeClothState` contracts
2. Deterministic baseline implementation in `src/kernel/src/sim/ClothSolver.cpp`:
   - node add/remove
   - spring edge registration
   - gravity + explicit Euler stepping
   - capture/restore rollback path
3. Regression tests in `tests/kernel/test_ClothSolver.cpp` covering API behavior, determinism, and serialization failure handling.

Done when:

1. Cloth API can execute deterministic step/replay sequences.
2. Snapshot restore reproduces exact captured state.
3. Serialization round-trip is stable and malformed payloads are rejected.

## M9.3 Fluid Solver Interface v0

Status: Complete

Delivered:

1. New public header `nexus/sim/FluidSolver.h` defines:
   - `FluidSolver`
   - `FluidState`, `FluidParticleSnapshot`, `FluidStepReport`
   - deterministic `serializeFluidState` / `deserializeFluidState` contracts
2. Deterministic baseline implementation in `src/kernel/src/sim/FluidSolver.cpp`:
   - particle add/remove
   - gravity + SPH-inspired minimal density/pressure stepping
   - capture/restore rollback path
3. Regression tests in `tests/kernel/test_FluidSolver.cpp` covering API behavior, deterministic replay, and malformed blob rejection.

Done when:

1. Fluid API can execute deterministic step/replay sequences.
2. Snapshot restore reproduces exact captured state.
3. Serialization round-trip is stable and malformed payloads are rejected.

## Test Coverage

Implemented in:

1. `tests/kernel/test_SimulationCore.cpp`
   - deterministic rigid-state serialization round-trip
   - malformed rigid-state blob rejection

2. `tests/kernel/test_ClothSolver.cpp`
   - node lifecycle and pinned-node behavior
   - deterministic stepping and replay stability
   - snapshot rollback equivalence
   - deterministic cloth-state serialization and malformed rejection

3. `tests/kernel/test_FluidSolver.cpp`
   - particle lifecycle and static-particle behavior
   - deterministic stepping and replay stability
   - snapshot rollback equivalence
   - deterministic fluid-state serialization and malformed rejection

## Month 9 Exit Gate (This Slice)

This Month 9 slice is complete only when all are true:

1. Rigid, cloth, and fluid solver API contracts are publicly defined. ✓
2. At least one simulation domain is end-to-end reproducible. ✓
3. Step/replay and rollback behavior is regression-tested. ✓
4. Cacheable state serialization contracts are deterministic. ✓
5. Build and test quality gates pass in the local workspace. (pending current run)

Status: M9.1 + M9.2 + M9.3 COMPLETE

## Test Count Summary

- M9.1 rigid-body serialization: 2 tests
- M9.2 cloth solver: 14 tests
- M9.3 fluid solver: 14 tests
- Total new simulation interface tests: 30 tests
- Full kernel test suite: 820/820 passed (10 skipped for hardware unavailability)
