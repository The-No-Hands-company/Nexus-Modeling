# Design: Cross-Domain Simulation Coupling (Month 13 Track C)

Status: **design + prototype** — `SimCouplingHarness` prototype lands this month
on the Null backend. No Vulkan plumbing or GPU readback until the design is
further reviewed.

Reference: [month-13-post-alpha-checklist.md](month-13-post-alpha-checklist.md),
Track C.

---

## Problem

The physics solver (`RigidBodySolver`) and the procedural scene (`NodeScene`)
live in separate domains. Nothing currently connects a solver body's simulated
position to the corresponding scene node's transform, meaning animated rigid
bodies cannot drive visible geometry updates.

---

## 1. Identifier Mapping

Each solver body has a stable `BodyId` (non-reusing `uint32_t`, 0 reserved as
invalid). Each scene node has a stable `SceneNodeId` (same underlying type via
`NodeId`, 0 reserved as invalid).

The `SimCouplingHarness` owns a **one-to-one** `BodyId → SceneNodeId` registry:

```
BodyId  ─────── SimCouplingHarness ──────► SceneNodeId
(solver domain)                            (scene domain)
```

Rules:
- One body maps to at most one scene node; one scene node maps to at most one
  body. The harness enforces this at registration time.
- Registration is additive only: to remap, the old entry must be unregistered
  first.
- Neither the solver nor the scene know about each other; the harness is the
  only cross-domain object.

---

## 2. Time-Step Ownership

**The solver owns time.** `RigidBodySolver::step(dt)` is the sole clock
advance; the harness never calls `step`.

The caller's responsibility:

```
1. solver.step(dt)          // advance physics
2. harness.syncFromSolver(solver)  // snapshot current body state
3. scene.evaluate()         // scene reacts to updated payloads
```

The `syncFromSolver` call is a pure read from the solver; it does not mutate
solver state. It writes position and velocity snapshots into the harness's
internal cache.

No sub-step interpolation is performed at this layer. Frame-rate-decoupled
interpolation is a future concern (Track C follow-up).

---

## 3. Scene Node Update Strategy

In this prototype the harness does **not** call `NodeScene::setAsset` (which
would overwrite the node's compute payload). Instead it maintains its own
`BodyId → {SimVec3 position, SimVec3 velocity}` snapshot table. This keeps
the scene's EvalGraph evaluation independent and avoids introducing
coupling-specific `NodePayload` types into the public API.

The planned production path (future slice):
1. Add a `SimTransform` payload type to `NodePayload`.
2. `syncToScene(const RigidBodySolver&, NodeScene&)` writes `SimTransform`
   payloads directly and marks affected nodes dirty.
3. `NodeScene::evaluate()` propagates transforms to child nodes.

---

## 4. Determinism Contract for Replay

The determinism contract mirrors `RigidBodySolver`'s own contract:

> Identical initial state + identical step sequence → identical
> `syncFromSolver` snapshots.

This holds because:
- `syncFromSolver` is a pure read of `getBodyState`, which is deterministic.
- The harness's internal snapshot table is reset each `syncFromSolver` call
  (no accumulation).
- Registration order is irrelevant: snapshots are keyed by `BodyId`, not by
  insertion order.

For replay: capture `SimState` before each step via `captureState()`, replay
the same `step()` sequence, re-run `syncFromSolver` — snapshots are
bit-identical.

---

## 5. Feature Toggle

`RenderContextDesc::enableSimCoupling` (bool, default `false`) signals to the
render context that sim coupling is active. The toggle does not change any
render behavior today; it is reserved for future GPU-side readback
infrastructure (e.g., uploading simulated positions to a GPU buffer before
the draw phase).

The harness itself is CPU-only and always available regardless of the toggle.
Conventionally, callers should only create a `SimCouplingHarness` when
`enableSimCoupling` is true.

---

## 6. Prototype Scope (this month)

In scope:
- `nexus/sim/SimCouplingHarness.h` — public header, `BodyId → SceneNodeId`
  registry, `syncFromSolver`, snapshot queries.
- `src/kernel/src/sim/SimCouplingHarness.cpp` — implementation.
- `RenderContextDesc::enableSimCoupling` toggle (additive field, default false).
- Behavior tests: registration, sync, determinism, two-harness isolation.

Out of scope this month:
- `NodeScene::setAsset` / `SimTransform` payload type.
- GPU readback / Vulkan buffer upload.
- Sub-step interpolation.
- Multi-body constraint coupling across harnesses.
