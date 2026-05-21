# Month 5 Modeling Workflow Slice 1 Checklist

This checklist executes the Month 5 roadmap commitment in
[vision-and-roadmap.md](vision-and-roadmap.md).

Primary slice: Modeling Workflow Slice 1.

## Scope Lock

In scope:

1. Primitive set expansion for core hard-surface modeling starts.
2. First chained hard-surface workflow API that composes existing operations.
3. Simple-by-default modeling shell API for guided starter workflows.
4. Deterministic behavior tests for new primitives and workflow reporting.

Out of scope:

1. Full interactive modeling shell UI and viewport tool UX.
2. Advanced boolean/bevel/remesh robustness redesign beyond v0 operation contracts.
3. Production-grade topology editing tools (edge slide, loop cut, symmetry authoring, etc.).

## Work Packages

## R5.1 Primitive Set Expansion

Status: Complete

Delivered:

1. Added new primitive APIs in `nexus/geometry/Mesh.h`:
   - `makeSphere`
   - `makeCylinder`
   - `makeCone`
   - `makeCapsule`
2. Implemented the primitive generators in
   `src/kernel/src/geometry/Mesh.cpp`.
3. Preserved geometry conventions:
   - Y-up, right-hand coordinate frame
   - CCW winding for outward-facing surfaces
4. Added per-vertex UV emission for each new primitive constructor.

Done when:

1. New primitives generate valid meshes and maintain deterministic topology output.

## R5.2 HardSurfaceWorkflow Pipeline API

Status: Complete

Delivered:

1. Added public workflow API in `nexus/geometry/HardSurfaceWorkflow.h`.
2. Implemented chained operation execution in
   `src/kernel/src/geometry/HardSurfaceWorkflow.cpp` for:
   - init
   - apply transform
   - boolean union / difference / intersection
   - bevel/chamfer
   - remesh
   - rebuild normals
3. Added per-step reporting via `HardSurfaceStepReport` and
   `HardSurfaceStepKind`.
4. Kept workflow deterministic and headless-safe for Null backend CI use.

Done when:

1. A mesh can be processed through a chained workflow with stable step reporting
   and valid output mesh ownership semantics.

## R5.3 Tests and Contract Updates

Status: Complete

Delivered:

1. Added coverage in `tests/kernel/test_HardSurfaceWorkflow.cpp` for:
   - primitive validity, bounds, and resolution expectations
   - normal reconstruction behavior
   - workflow step ordering and reporting
   - remesh/bevel/rebuild-normal end-to-end flow behavior
2. Registered the new test unit in `tests/CMakeLists.txt`.
3. Updated API freeze fixture in
   `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt` to include
   `nexus/geometry/HardSurfaceWorkflow.h`.

Done when:

1. New Month 5 surface area compiles, is covered by deterministic tests, and
   passes API freeze manifest validation.

## R5.4 Simple-by-default Modeling Shell

Status: Complete

Delivered:

1. Added public shell API in `nexus/geometry/ModelingShell.h`.
2. Implemented starter workflow orchestration in
   `src/kernel/src/geometry/ModelingShell.cpp`:
   - starter primitive selection
   - guided intro steps for first-use flow
   - quick cleanup pass (bevel + remesh + normals)
   - diagnostic overlay refresh for UI consumption
3. Added deterministic test coverage in `tests/kernel/test_ModelingShell.cpp`.
4. Updated API freeze fixture in
   `tests/kernel/fixtures/api_surface_manifest_alpha_v1.txt` to include
   `nexus/geometry/ModelingShell.h`.

Done when:

1. New-user starter flow can run from primitive setup to validated mesh output
   through one lean shell interface.

## Test Coverage

Implemented in `tests/kernel/test_HardSurfaceWorkflow.cpp`:

1. Sphere primitive validity, bounds, UV range, and face-count expectations.
2. Cylinder primitive validity, bounds, face-count, and normals path checks.
3. Cone primitive validity, bounds, face-count, and normals path checks.
4. Capsule primitive validity, bounds, and normals path checks.
5. HardSurfaceWorkflow init/reporting contract behavior.
6. HardSurfaceWorkflow transform/remesh/bevel/rebuild-normal chaining behavior.
7. End-to-end workflow smoke paths for box and sphere inputs.

Implemented in `tests/kernel/test_ModelingShell.cpp`:

1. Guided intro step contract stability.
2. Starter primitive shell initialization behavior.
3. Quick cleanup step ordering through wrapped workflow operations.
4. Session report state reflection.
5. Diagnostics mode filtering path.

## Month 5 Exit Gate

Month 5 Slice 1 is complete only when all are true:

1. Sphere/cylinder/cone/capsule primitives are public, deterministic, and tested.
2. HardSurfaceWorkflow provides a stable chained API for key hard-surface steps.
3. ModelingShell provides a guided starter toolset over workflow + diagnostics.
4. Step reporting captures operation outcome and ordering across the chain.
5. API freeze manifest is intentionally updated for additive public surface changes.
6. Build and test quality gates pass in the local workspace.

Status: COMPLETE

Test baseline: 742 discovered / 742 passed / 0 failed (10 skipped by capability gating).
