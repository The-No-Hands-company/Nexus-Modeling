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

## Slice 4 — Port type contracts (landed)

Each named input/output port may declare an optional `NodePayloadType` contract
(default `NodePayloadType::None` = unconstrained).

- `declareInputPort(name, id, contract)` / `declareOutputPort(name, id, contract)` — overloads with explicit contract.
- `inputPortContract(name)` / `outputPortContract(name)` — read the contract; return `None` for unknown ports.
- `setInputPortContract(name, contract)` / `setOutputPortContract(name, contract)` — update after declaration; return `false` for unknown ports.
- `validate()` reports a violation when a constrained port's node carries a non-`None` payload whose type does not match the contract.
- `instantiateSubgraph()` enforces contracts: returns `nullopt` on any violation.
- Serialization: non-`None` contracts are written as `IC <portName> <type>` / `OC <portName> <type>` records after the `I`/`O` records; `None` contracts are omitted.
- 21 tests in `tests/kernel/test_SubgraphPortContracts.cpp`.

## Slice 5 — SubgraphRegistry (landed)

A named catalog of `SubgraphTemplate` instances backed by a `std::map<std::string, SubgraphTemplate>`.

- `SubgraphRegistry::add(name, tmpl)` — registers a template; returns false for duplicate or invalid (non-token) names.
- `find(name)` — const and mutable pointer lookups; nullptr when unknown.
- `contains(name)`, `remove(name)`, `clear()`, `size()`, `empty()`.
- `names()` — sorted ascending.
- `SubgraphRegistrySerializer::serialize(registry, report)` — multi-template text archive (`NEXUS_SUBGRAPH_REGISTRY_V1` header, `T <name>` delimiter per template, followed by single-template records).
- `SubgraphRegistrySerializer::deserialize(data, registry)` — parses the archive; reports duplicate names but continues; unknown top-level tags skipped (forward-compatible).
- `SubgraphRegistrySerializationReport::templateCount` — number of templates written/read.
- Contracts, edges, and payloads all round-trip through the archive format.
- 23 tests in `tests/kernel/test_SubgraphRegistry.cpp`.

## Out of scope (deferred past Slice 5)

- Hierarchical subgraphs containing other instances as opaque nodes.
- Live re-instantiation diffing (mutating a template after instantiation).
- Visual-scripting front-end.
