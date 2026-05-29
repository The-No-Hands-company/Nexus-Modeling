# Sculpt kernel module (Slice 1)

Slice 1 introduces `nexus::sculpt`, a CPU-only, headless-safe sculpting layer
that mutates `nexus::geometry::Mesh` in deterministic, undoable strokes.

## Public surface

- `nexus/sculpt/Brush.h`
  - `BrushKind` — `Draw`, `Smooth`, `Inflate`, `Flatten`, `Pinch`.
  - `FalloffShape` — `Constant`, `Linear`, `Smooth`, `Sharp`; `evaluateFalloff(shape, t)`.
  - `BrushParams` — radius, strength, falloff, direction (or vertex-normal mode),
    optional per-vertex displacement cap (Layer-style).
  - `BrushSample` — single stamp with world-space position, pressure, and a
    monotonic `sequence` for ordering.
- `nexus/sculpt/SculptSession.h`
  - Stroke lifecycle: `beginStroke → applySample* → endStroke → StrokeDelta`.
  - `undoStroke(delta)` / `redoStroke(delta)` for non-destructive history.
  - `SculptStats` counters.

## Determinism contract

- Vertex iteration during a stamp is in ascending vertex-index order.
- `applySample` rejects samples whose `sequence` is not strictly greater than
  the previous sample in the same stroke.
- `StrokeDelta::vertexDeltas` is sorted ascending by vertex index, so two
  identical inputs on two sessions yield byte-identical deltas (validated by
  `SculptSession.DeterministicReplayAcrossSessions`).
- Adjacency lists used by `Smooth` are sorted and deduplicated.

## Ownership

`SculptSession` is a non-owning view of a `Mesh`; the mesh must outlive the
session. Call `resync()` after external code mutates the mesh between strokes.

## Brush kernels

| Brush     | Displacement                                                                  |
| --------- | ----------------------------------------------------------------------------- |
| `Draw`    | Along vertex normal (or fixed `direction`); `strength · weight · radius`.     |
| `Smooth`  | Toward 1-ring average; lerp factor `clamp(strength · weight, 0, 1)`.          |
| `Inflate` | Along vertex normal; `strength · weight · radius`.                            |
| `Flatten` | Projection onto plane `(sample.position, direction|normal)`; lerp by weight.  |
| `Pinch`   | Toward `sample.position`; lerp factor `clamp(strength · weight, 0, 1)`.       |

Weights combine `evaluateFalloff(falloff, dist / radius)` with `sample.pressure`.

## Tests

`tests/kernel/test_SculptSession.cpp` covers:

- Falloff monotonicity and boundary values across all shapes.
- `beginStroke` validation (zero radius, zero direction without vertex normal,
  re-entrancy guard).
- Per-brush behavioral assertions (Draw, Inflate, Smooth, Flatten, Pinch).
- Sample-sequence ordering enforcement.
- Undo restores original positions within float tolerance; redo reproduces the post-stroke state.
- Determinism across two independent sessions on identical inputs.
- Empty stroke is a no-op; stats accumulate correctly.

## Slice 2 additions (landed)

- `BrushKind::Crease` — pinches toward 1-ring edge midpoints; sharpens surface edges.
- `BrushKind::Layer` — normal-direction displacement capped at `maxPerVertexDisplacement`.
- `BrushKind::Grab` — rigid translation by stroke delta attenuated by falloff;
  affected vertex set fixed at the grab origin (first sample position).
- 17 behavior tests in `tests/kernel/test_SculptSlice2.cpp`.

## Out of scope (deferred past Slice 2)

- Multires pyramid and dynamic-topology stroke pass.
- GPU compute path (Vulkan) — current implementation is CPU only.
- Symmetry, masking, and stroke serialization to disk.
