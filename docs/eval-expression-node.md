# Eval ExpressionNode adapter (Slice 4 + Slice 5)

Slice 4 bridges `nexus::script::Expression` into the `EvalGraph` runtime so
that a scalar expression's free variables resolve from upstream `ScalarF32`
output payload slots rather than a hard-coded variable map.

Slice 5 extends the upstream payload resolution with type promotion: `IntegerI64`
and `Boolean` upstream payloads are widened to `double` for expression evaluation.
All other payload kinds (`TextUtf8`, `Binary`, …) continue to cause a `compute`
failure.

## Public surface

- `nexus/eval/ExpressionNode.h`
  - `NodeKind::Expression` — new enum value in `EvalGraph.h`
  - `struct ExpressionBinding { std::string variable; NodeId sourceNode; }`
  - `class ExpressionNodeAdapter` (move-only, pimpl):
    - `static create(EvalGraph&, name, source, bindings) → std::optional<ExpressionNodeAdapter>`
    - `nodeId() → NodeId`
    - `expression() → const nexus::script::Expression&`
    - `bindings() → const std::vector<ExpressionBinding>&`
    - `compute(NodeComputeContext&) → bool`

## Contracts

- `create` returns empty when source fails to compile, any `sourceNode` is
  absent in the graph, or any `variable` name is empty.
- `compute` returns false (and leaves `outputPayload` unchanged) when an
  upstream payload is missing, is not a promotable numeric type (`ScalarF32`,
  `IntegerI64`, `Boolean`), or expression evaluation returns `std::nullopt`
  (domain error, division by zero, missing variable).
- Type promotion order in `compute`: `ScalarF32` → cast to `double`; `IntegerI64`
  → cast to `double`; `Boolean` → `false`=0.0, `true`=1.0; anything else → fail.
- Deterministic: identical inputs yield bit-identical `ScalarF32` output.

## Dispatch pattern

The graph-level compute callback must dispatch to the right adapter per
`NodeId`. A minimal dispatch lambda:

```cpp
EvalGraph g;
auto adapter = ExpressionNodeAdapter::create(g, "result", "x * 2 + 1", {{"x", x}});

g.setComputeCallback([&](NodeComputeContext& ctx) -> bool {
    if (ctx.kind == NodeKind::Expression && ctx.id == adapter->nodeId())
        return adapter->compute(ctx);
    return true; // passthrough for constant/geometry/animation nodes
});
g.evaluate();
```

Multiple expression nodes in one graph: collect adapters in a map keyed by
`nodeId()` and dispatch by lookup.

## Tests

`tests/kernel/test_ExpressionNode.cpp` covers:

- Creation: success, compile failure, unknown source node, empty variable name,
  zero-binding constant expression.
- Accessors: source text, binding list, node name.
- Evaluation: single variable, multi-variable (`sqrt(a²+b²)`), constant
  expression with no bindings, expression node feeding downstream.
- Error paths: missing upstream payload, domain error (`sqrt(-1)`),
  non-numeric upstream payload (`TextUtf8`).
- Type promotion (Slice 5): `IntegerI64` upstream widened to `double`; `Boolean`
  upstream widened to 0.0 / 1.0.
- Determinism: two independent graphs on identical inputs produce identical output.
- Re-evaluation after upstream value change.
- `NodeKind::Expression` recognized by `EvalGraph::nodeKind`.

## Out of scope (deferred past Slice 5)

- Multi-output expression nodes (one result port only).
- Serialization of expression nodes (JSON / binary round-trip).
- Editor UI for binding expressions to graph ports.
