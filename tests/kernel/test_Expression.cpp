// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Script — Expression DSL behavior tests (Slice 3)
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/script/Expression.h>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <unordered_map>

namespace nexus::script::testing {

namespace {

std::optional<double> evalSrc(std::string_view src,
                              const std::unordered_map<std::string, double>& vars = {})
{
    auto e = Expression::compile(src);
    if (!e) return std::nullopt;
    return e->evaluate(vars);
}

} // namespace

// ── Literals & whitespace ───────────────────────────────────────────────────

TEST(ScriptExpression, ParsesIntegerAndFloatLiterals)
{
    EXPECT_DOUBLE_EQ(*evalSrc("42"),         42.0);
    EXPECT_DOUBLE_EQ(*evalSrc("3.14"),       3.14);
    EXPECT_DOUBLE_EQ(*evalSrc(" 1.5e2 "),    150.0);
    EXPECT_DOUBLE_EQ(*evalSrc("1e-3"),       0.001);
}

// ── Operator precedence & associativity ────────────────────────────────────

TEST(ScriptExpression, RespectsArithmeticPrecedence)
{
    EXPECT_DOUBLE_EQ(*evalSrc("1 + 2 * 3"),   7.0);
    EXPECT_DOUBLE_EQ(*evalSrc("(1 + 2) * 3"), 9.0);
    EXPECT_DOUBLE_EQ(*evalSrc("10 - 4 - 3"),  3.0);  // left-assoc
    EXPECT_DOUBLE_EQ(*evalSrc("8 / 4 / 2"),   1.0);  // left-assoc
}

TEST(ScriptExpression, PowerIsRightAssociative)
{
    EXPECT_DOUBLE_EQ(*evalSrc("2^3^2"), 512.0);   // 2^(3^2)
    EXPECT_DOUBLE_EQ(*evalSrc("(2^3)^2"), 64.0);
}

TEST(ScriptExpression, UnaryMinusBindsBelowPower)
{
    // -2^2 should parse as -(2^2) = -4 (matches Python / mathematical convention)
    EXPECT_DOUBLE_EQ(*evalSrc("-2^2"), -4.0);
    EXPECT_DOUBLE_EQ(*evalSrc("(-2)^2"), 4.0);
    EXPECT_DOUBLE_EQ(*evalSrc("--3"), 3.0);
}

TEST(ScriptExpression, ModuloOperator)
{
    EXPECT_DOUBLE_EQ(*evalSrc("10 % 3"), 1.0);
    EXPECT_FALSE(evalSrc("5 % 0").has_value());
}

// ── Variables ──────────────────────────────────────────────────────────────

TEST(ScriptExpression, ResolvesVariables)
{
    auto e = Expression::compile("x + y * 2");
    ASSERT_TRUE(e.has_value());
    EXPECT_DOUBLE_EQ(*e->evaluate({{"x", 1.0}, {"y", 3.0}}), 7.0);
}

TEST(ScriptExpression, MissingVariableYieldsNullopt)
{
    auto e = Expression::compile("a + b");
    ASSERT_TRUE(e.has_value());
    EXPECT_FALSE(e->evaluate({{"a", 1.0}}).has_value());
}

TEST(ScriptExpression, VariableNamesAreSortedUniqueAndExcludeBuiltins)
{
    auto e = Expression::compile("sin(theta) + cos(theta) + pi + radius");
    ASSERT_TRUE(e.has_value());
    const auto names = e->variableNames();
    ASSERT_EQ(names.size(), 2u);
    EXPECT_EQ(names[0], "radius");
    EXPECT_EQ(names[1], "theta");
}

// ── Built-ins ──────────────────────────────────────────────────────────────

TEST(ScriptExpression, BuiltinConstants)
{
    EXPECT_DOUBLE_EQ(*evalSrc("pi"),  std::numbers::pi);
    EXPECT_DOUBLE_EQ(*evalSrc("tau"), 2.0 * std::numbers::pi);
    EXPECT_DOUBLE_EQ(*evalSrc("e"),   std::numbers::e);
}

TEST(ScriptExpression, BuiltinFunctions)
{
    EXPECT_DOUBLE_EQ(*evalSrc("abs(-7)"),           7.0);
    EXPECT_DOUBLE_EQ(*evalSrc("sqrt(16)"),          4.0);
    EXPECT_DOUBLE_EQ(*evalSrc("min(3, 5)"),         3.0);
    EXPECT_DOUBLE_EQ(*evalSrc("max(3, 5)"),         5.0);
    EXPECT_DOUBLE_EQ(*evalSrc("clamp(7, 0, 5)"),    5.0);
    EXPECT_DOUBLE_EQ(*evalSrc("clamp(-2, 0, 5)"),   0.0);
    EXPECT_DOUBLE_EQ(*evalSrc("mix(0, 10, 0.5)"),   5.0);
    EXPECT_DOUBLE_EQ(*evalSrc("floor(2.7)"),        2.0);
    EXPECT_DOUBLE_EQ(*evalSrc("ceil(2.1)"),         3.0);
    EXPECT_DOUBLE_EQ(*evalSrc("round(2.5)"),        3.0);
    EXPECT_DOUBLE_EQ(*evalSrc("pow(2, 10)"),        1024.0);
    EXPECT_DOUBLE_EQ(*evalSrc("step(1, 2)"),        1.0);
    EXPECT_DOUBLE_EQ(*evalSrc("step(2, 1)"),        0.0);
}

TEST(ScriptExpression, DomainErrorsReturnNullopt)
{
    EXPECT_FALSE(evalSrc("sqrt(-1)").has_value());
    EXPECT_FALSE(evalSrc("log(0)").has_value());
    EXPECT_FALSE(evalSrc("1 / 0").has_value());
}

// ── Diagnostics ─────────────────────────────────────────────────────────────

TEST(ScriptExpression, ParseErrorsReturnSortedDiagnostics)
{
    CompileResult r;
    auto e = Expression::compileWithDiagnostics("1 +", r);
    EXPECT_FALSE(e.has_value());
    EXPECT_FALSE(r.ok);
    ASSERT_FALSE(r.errors.empty());
    for (std::size_t i = 1; i < r.errors.size(); ++i) {
        EXPECT_LE(r.errors[i - 1], r.errors[i]);
    }
}

TEST(ScriptExpression, UnknownFunctionIsReported)
{
    CompileResult r;
    auto e = Expression::compileWithDiagnostics("nope(1)", r);
    EXPECT_FALSE(e.has_value());
    EXPECT_FALSE(r.ok);
    bool found = false;
    for (const auto& m : r.errors) {
        if (m.find("unknown function 'nope'") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(ScriptExpression, ArityMismatchIsReported)
{
    CompileResult r;
    auto e = Expression::compileWithDiagnostics("clamp(1, 2)", r);
    EXPECT_FALSE(e.has_value());
    bool found = false;
    for (const auto& m : r.errors) {
        if (m.find("'clamp'") != std::string::npos &&
            m.find("got 2") != std::string::npos) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);
}

TEST(ScriptExpression, UnexpectedCharacterIsReported)
{
    CompileResult r;
    auto e = Expression::compileWithDiagnostics("1 $ 2", r);
    EXPECT_FALSE(e.has_value());
    EXPECT_FALSE(r.errors.empty());
}

// ── Determinism & source ────────────────────────────────────────────────────

TEST(ScriptExpression, EvaluationIsDeterministic)
{
    auto e = Expression::compile("sin(a) * b + clamp(c, 0, 1)");
    ASSERT_TRUE(e.has_value());
    const std::unordered_map<std::string, double> env = {
        {"a", 1.234}, {"b", 7.5}, {"c", 0.42},
    };
    const auto first = e->evaluate(env);
    ASSERT_TRUE(first.has_value());
    for (int i = 0; i < 100; ++i) {
        const auto v = e->evaluate(env);
        ASSERT_TRUE(v.has_value());
        EXPECT_EQ(*v, *first);
    }
}

TEST(ScriptExpression, SourceTextIsPreserved)
{
    const std::string src = "  a + b * 2.5 ";
    auto e = Expression::compile(src);
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->source(), src);
}

TEST(ScriptExpression, BuiltinIdentifierProbe)
{
    EXPECT_TRUE(isBuiltinIdentifier("pi"));
    EXPECT_TRUE(isBuiltinIdentifier("sin"));
    EXPECT_TRUE(isBuiltinIdentifier("clamp"));
    EXPECT_FALSE(isBuiltinIdentifier("radius"));
    EXPECT_FALSE(isBuiltinIdentifier(""));
}

} // namespace nexus::script::testing
