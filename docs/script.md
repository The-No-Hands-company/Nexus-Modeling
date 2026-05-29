# Script — Expression DSL (Slice 3)

Slice 3 adds `nexus::script`, a sandboxed scalar expression language for
parameter expressions. It complements existing subsystems:

- `automation::AutomationScript` — line-oriented batch commands (CI/headless).
- `eval::EvalGraph` / `eval_ext::SubgraphTemplate` — node-graph evaluation.
- `script::Expression` *(this slice)* — embeddable scalar expressions for
  driving parameter values (constraint inputs, blend weights, animation
  drivers, etc.).

## Public surface

- `nexus/script/Expression.h`
  - `class Expression` (move-only, pimpl):
    - `static compile(std::string_view) → std::optional<Expression>`
    - `static compileWithDiagnostics(source, CompileResult&) → std::optional<Expression>`
    - `evaluate(const std::unordered_map<std::string,double>&) → std::optional<double>`
    - `variableNames() → std::vector<std::string>` (sorted, unique, excludes built-ins)
    - `source() → const std::string&`
  - `struct CompileResult { bool ok; std::vector<std::string> errors; }` (errors lex-sorted)
  - `bool isBuiltinIdentifier(std::string_view)`

## Grammar

```
expr    := term (('+' | '-') term)*
term    := factor (('*' | '/' | '%') factor)*
factor  := unary ('^' factor)?         // right-associative
unary   := ('-' | '+')? primary
primary := number
         | identifier
         | identifier '(' arglist? ')'
         | '(' expr ')'
arglist := expr (',' expr)*
```

Unary `-` binds below `^`, so `-2^2 = -4` (matches Python and mathematical
convention).

## Built-ins

- Constants: `pi`, `tau`, `e`.
- Functions: `abs`, `ceil`, `clamp(x,lo,hi)`, `cos`, `exp`, `floor`, `log`,
  `max(a,b)`, `min(a,b)`, `mix(a,b,t)`, `pow(b,e)`, `round`, `sin`, `sqrt`,
  `step(edge,x)`, `tan`.

## Safety / determinism contract

- `compile()` and `evaluate()` are `noexcept`-style: no exceptions escape;
  every failure mode produces `std::nullopt`.
- `evaluate()` returns `std::nullopt` on:
  - Missing variables (caller must satisfy `variableNames()`).
  - Division or modulo by zero.
  - `sqrt(x)` with `x < 0`, `log(x)` with `x ≤ 0`.
  - Any operation producing non-finite IEEE-754 (`pow`, `tan`, `^`).
- Repeated evaluation with identical inputs yields bit-identical output
  (covered by `EvaluationIsDeterministic`).
- Diagnostics in `CompileResult::errors` are lexicographically sorted.

## Tests

`tests/kernel/test_Expression.cpp` covers:

- Integer, float, and scientific-notation literals.
- Additive / multiplicative precedence and left-associativity.
- Right-associative `^`; unary-minus binding vs. `^`.
- Modulo and division-by-zero handling.
- Variable resolution, missing-variable nullopt, sorted-unique
  `variableNames()` excluding built-in constants.
- All built-in functions and constants.
- Domain errors (`sqrt(-1)`, `log(0)`, `1/0`).
- Parse-error reporting, sorted diagnostics, unknown-function and
  arity-mismatch messages, unexpected-character.
- Determinism across 100 repeated evaluations.
- Source-text preservation; `isBuiltinIdentifier` probe.

## Out of scope for Slice 3

- Bytecode / JIT compilation (current impl is shared-ptr AST walk).
- Vector/matrix or string values — scalar `double` only.
- User-defined functions or variable assignment / statements.
- Integration adapters into `EvalGraph` nodes — **landed in Slice 4** (`nexus/eval/ExpressionNode.h`).
- Localized number formats; the lexer assumes C locale.
