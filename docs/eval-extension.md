# Eval extension — Subgraphs (Slice 2)

Slice 2 adds `nexus::eval_ext`, the first extension layer on top of
`nexus::EvalGraph`. The directory is named `eval_ext/` (matches the C++
namespace; hyphenated `eval-extension/` would conflict with include-path
conventions used elsewhere in the kernel).

## Public surface

- `nexus/eval_ext/Subgraph.h`
  - `SubgraphTemplate` — host-agnostic graph fragment using a local NodeId
    namespace. Mirrors a subset of `EvalGraph`:
    - `addNode(NodeKind, std::string)` → local id.
    - `connect(local, local)`, `setNodeOutputPayload(local, NodePayload)`.
    - `declareInputPort(name, local)` — marks a source-only local as the
      external entry point for a named input.
    - `declareOutputPort(name, local)` — exposes a local node as a named output.
    - `validate()` returns `SubgraphValidation { ok, hasCycle, issues }`
      with sorted-lexicographically diagnostics.
  - `instantiateSubgraph(host, tmpl, prefix) → std::optional<SubgraphInstance>`
    clones the template into a host `EvalGraph` with deterministic ordering.
  - `SubgraphInstance` records `inputPorts`, `outputPorts`, `hostNodes`,
    and `localToHost`.
  - `subgraphInputProxy(inst, portName)` / `subgraphOutput(inst, portName)`
    look up host NodeIds by port name.

## Determinism contract

- Template nodes are cloned in ascending local-id order.
- Input ports are processed in ascending port-name order (`std::map`).
- Output ports are processed in ascending port-name order.
- Two independent hosts instantiating identical templates produce identical
  `(NodeKind, name)` sequences in evaluation order (covered by
  `InstantiateSubgraph.DeterministicTopoOrderAcrossInstantiations`).

## Validation rules

`SubgraphTemplate::validate()` flags:

- Directed cycles in the template (Kahn's algorithm).
- Input ports targeting locals that have any incoming edge.
- Ports referencing unknown local ids.

`instantiateSubgraph()` calls `validate()` first and returns `std::nullopt`
on failure; any host nodes created before the failure are rolled back via
`host.removeNode(...)`.

## Mapping model

- Each declared input port becomes a host `Constant` proxy node named
  `<prefix>/in/<portName>`. External code feeds values into the subgraph by
  calling `host.setNodeOutputPayload(proxy, ...)` and `host.markDirty(...)`.
- Every other template node is cloned as a host node named
  `<prefix>/<originalName>` with `NodeKind` preserved and `NodePayload`
  copied when present.
- Each template edge is replayed in the host with `localToHost` remapping.
- Output ports map directly to the host clone of the referenced local node.

## Tests

`tests/kernel/test_Subgraph.cpp` covers:

- Empty template; simple chain validates.
- Cycle detection; duplicate / empty / unknown-id port rejection.
- Input-port-must-be-source rule.
- Host node creation, edge replay, and end-to-end `evaluate()` reachability.
- Distinct namespaces across two instantiations.
- Scalar payload routing from proxy through downstream compute.
- Payload preservation on clone.
- Topological-order determinism across two hosts.
- Best-effort rollback on invalid template.
- Clean removal of all instance nodes via `SubgraphInstance::hostNodes`.

## Slice 3 — SubgraphSerialization (landed)

Text-format round-trip serializer for `SubgraphTemplate`.

- `nexus/eval_ext/SubgraphSerialization.h` — `SubgraphSerializer::serialize` / `deserialize`.
- Format: `NEXUS_SUBGRAPH_V1` magic header + tagged line records (`N`, `E`, `P`, `I`, `O`).
- Payload types serialized: `ScalarF32`, `IntegerI64`, `Boolean`, `TextUtf8`.
  Binary/SplatCloud/ReconstructionDiagnostic omitted (runtime-only payloads).
- Token constraint: node names and port names must be non-empty alphanumeric + `_-.`.
- Forward-compatible: unknown tags silently skipped on deserialize.
- 21 tests in `tests/kernel/test_SubgraphSerialization.cpp`.

## Out of scope (deferred past Slice 3)

- Type-checked port contracts (currently any payload kind is allowed).
- Hierarchical subgraphs containing other instances as opaque nodes.
- Live re-instantiation diffing (mutating a template after instantiation).
- Visual-scripting front-end.
