#pragma once
// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Script — Expression DSL (Slice 3)
//
//  A small, sandboxed, deterministic scalar expression language intended for
//  parameter expressions (driving values inside EvalGraph / parametric solver
//  inputs without spawning a general-purpose interpreter).
//
//  Surface guarantees:
//    - `compile()` does not throw and produces a sorted error list on failure.
//    - `evaluate()` does not throw; it returns std::nullopt on missing vars,
//      domain errors, or division by zero.
//    - `variableNames()` returns lexicographically sorted, de-duplicated free
//      identifiers (constants like `pi` and built-in functions are excluded).
//
//  Grammar (Pratt / precedence-climbing):
//     expr   := term (( '+' | '-' ) term)*
//     term   := factor (( '*' | '/' | '%' ) factor)*
//     factor := unary ('^' factor)?              // right-associative
//     unary  := ('-' | '+')? primary
//     primary := number
//              | identifier
//              | identifier '(' arglist? ')'
//              | '(' expr ')'
//     arglist := expr (',' expr)*
//
//  Built-in constants: pi, tau, e.
//  Built-in functions: abs, ceil, clamp(x,lo,hi), cos, exp, floor, log, max,
//                      min, mix(a,b,t), pow, round, sin, sqrt, step(edge,x),
//                      tan.
// ─────────────────────────────────────────────────────────────────────────────
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexus::script {

struct CompileResult {
    bool ok = true;
    // Errors are lex-sorted on every return path.
    std::vector<std::string> errors;
};

class Expression {
public:
    // Compile `source` into an evaluable expression. Returns std::nullopt on
    // parse/lex failure. Use `compileWithDiagnostics` to also retrieve errors.
    [[nodiscard]] static std::optional<Expression> compile(std::string_view source);

    // Compile and report. On failure the returned optional is empty and
    // `result.errors` contains sorted diagnostics. On success `errors` is empty.
    [[nodiscard]] static std::optional<Expression> compileWithDiagnostics(
        std::string_view source, CompileResult& result);

    Expression(Expression&&) noexcept;
    Expression& operator=(Expression&&) noexcept;
    Expression(const Expression&) = delete;
    Expression& operator=(const Expression&) = delete;
    ~Expression();

    // Evaluate against an external variable binding. Returns std::nullopt for
    // missing variables, division by zero, or out-of-domain math (e.g.
    // sqrt(-1), log(0)). The result is deterministic for identical inputs.
    [[nodiscard]] std::optional<double> evaluate(
        const std::unordered_map<std::string, double>& variables) const;

    // Sorted, de-duplicated list of free identifiers referenced by the
    // expression. Built-in constants and function names are excluded.
    [[nodiscard]] std::vector<std::string> variableNames() const;

    // Original source text passed to `compile`.
    [[nodiscard]] const std::string& source() const;

private:
    Expression();
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

// Convenience: returns true iff `name` is a recognized built-in identifier
// (constant or function). Useful for editor tooling and tests.
[[nodiscard]] bool isBuiltinIdentifier(std::string_view name);

} // namespace nexus::script
