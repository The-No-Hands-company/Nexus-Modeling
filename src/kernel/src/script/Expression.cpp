// ─────────────────────────────────────────────────────────────────────────────
//  Nexus Script — Expression DSL implementation
// ─────────────────────────────────────────────────────────────────────────────
#include <nexus/script/Expression.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <memory>
#include <numbers>
#include <set>
#include <variant>

namespace nexus::script {

namespace {

// ── Token ───────────────────────────────────────────────────────────────────

enum class TokKind {
    End,
    Number,
    Ident,
    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Caret,
    LParen,
    RParen,
    Comma,
};

struct Token {
    TokKind kind = TokKind::End;
    std::string text;
    double      number = 0.0;
    std::size_t column = 0;  // 1-based for diagnostics
};

// ── Lexer ───────────────────────────────────────────────────────────────────

class Lexer {
public:
    Lexer(std::string_view src, std::vector<std::string>& errors)
        : m_src(src), m_errors(errors) {}

    std::vector<Token> tokenize()
    {
        std::vector<Token> out;
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isspace(static_cast<unsigned char>(c))) {
                ++m_pos;
                continue;
            }
            const std::size_t col = m_pos + 1;
            if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
                out.push_back(readNumber(col));
                continue;
            }
            if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
                out.push_back(readIdent(col));
                continue;
            }
            Token t;
            t.column = col;
            t.text   = std::string(1, c);
            switch (c) {
                case '+': t.kind = TokKind::Plus;    break;
                case '-': t.kind = TokKind::Minus;   break;
                case '*': t.kind = TokKind::Star;    break;
                case '/': t.kind = TokKind::Slash;   break;
                case '%': t.kind = TokKind::Percent; break;
                case '^': t.kind = TokKind::Caret;   break;
                case '(': t.kind = TokKind::LParen;  break;
                case ')': t.kind = TokKind::RParen;  break;
                case ',': t.kind = TokKind::Comma;   break;
                default:
                    m_errors.push_back("col " + std::to_string(col) +
                                       ": unexpected character '" +
                                       std::string(1, c) + "'");
                    ++m_pos;
                    continue;
            }
            ++m_pos;
            out.push_back(t);
        }
        Token end;
        end.column = m_src.size() + 1;
        out.push_back(end);
        return out;
    }

private:
    Token readNumber(std::size_t col)
    {
        const std::size_t start = m_pos;
        bool sawDot = false;
        bool sawExp = false;
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isdigit(static_cast<unsigned char>(c))) {
                ++m_pos;
            } else if (c == '.' && !sawDot && !sawExp) {
                sawDot = true;
                ++m_pos;
            } else if ((c == 'e' || c == 'E') && !sawExp) {
                sawExp = true;
                ++m_pos;
                if (m_pos < m_src.size() &&
                    (m_src[m_pos] == '+' || m_src[m_pos] == '-')) {
                    ++m_pos;
                }
            } else {
                break;
            }
        }
        Token t;
        t.kind   = TokKind::Number;
        t.column = col;
        t.text   = std::string(m_src.substr(start, m_pos - start));
        try {
            t.number = std::stod(t.text);
        } catch (...) {
            m_errors.push_back("col " + std::to_string(col) +
                               ": invalid number literal '" + t.text + "'");
            t.number = 0.0;
        }
        return t;
    }

    Token readIdent(std::size_t col)
    {
        const std::size_t start = m_pos;
        while (m_pos < m_src.size()) {
            char c = m_src[m_pos];
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
                ++m_pos;
            } else {
                break;
            }
        }
        Token t;
        t.kind   = TokKind::Ident;
        t.column = col;
        t.text   = std::string(m_src.substr(start, m_pos - start));
        return t;
    }

    std::string_view          m_src;
    std::size_t               m_pos = 0;
    std::vector<std::string>& m_errors;
};

// ── AST ─────────────────────────────────────────────────────────────────────

struct AstNumber;
struct AstVar;
struct AstUnary;
struct AstBinary;
struct AstCall;

using AstNode = std::variant<AstNumber, AstVar, AstUnary, AstBinary, AstCall>;
using AstPtr  = std::shared_ptr<AstNode>;

struct AstNumber {
    double value;
};

struct AstVar {
    std::string name;
};

struct AstUnary {
    char    op;  // '-' or '+'
    AstPtr  child;
};

struct AstBinary {
    char    op;  // '+', '-', '*', '/', '%', '^'
    AstPtr  lhs;
    AstPtr  rhs;
};

struct AstCall {
    std::string         name;
    std::vector<AstPtr> args;
    std::size_t         column = 0;
};

// ── Built-ins ───────────────────────────────────────────────────────────────

bool lookupConstant(std::string_view name, double& out)
{
    if (name == "pi")  { out = std::numbers::pi;       return true; }
    if (name == "tau") { out = 2.0 * std::numbers::pi; return true; }
    if (name == "e")   { out = std::numbers::e;        return true; }
    return false;
}

struct FnSpec {
    int minArity;
    int maxArity;
};

const std::unordered_map<std::string, FnSpec>& functionSpecs()
{
    static const std::unordered_map<std::string, FnSpec> kSpecs = {
        {"abs",   {1, 1}},
        {"ceil",  {1, 1}},
        {"clamp", {3, 3}},
        {"cos",   {1, 1}},
        {"exp",   {1, 1}},
        {"floor", {1, 1}},
        {"log",   {1, 1}},
        {"max",   {2, 2}},
        {"min",   {2, 2}},
        {"mix",   {3, 3}},
        {"pow",   {2, 2}},
        {"round", {1, 1}},
        {"sin",   {1, 1}},
        {"sqrt",  {1, 1}},
        {"step",  {2, 2}},
        {"tan",   {1, 1}},
    };
    return kSpecs;
}

std::optional<double> callBuiltin(const std::string& name,
                                  const std::vector<double>& args)
{
    if (name == "abs")   return std::fabs(args[0]);
    if (name == "ceil")  return std::ceil(args[0]);
    if (name == "clamp") return std::min(std::max(args[0], args[1]), args[2]);
    if (name == "cos")   return std::cos(args[0]);
    if (name == "exp")   return std::exp(args[0]);
    if (name == "floor") return std::floor(args[0]);
    if (name == "log") {
        if (!(args[0] > 0.0)) return std::nullopt;
        return std::log(args[0]);
    }
    if (name == "max")   return std::max(args[0], args[1]);
    if (name == "min")   return std::min(args[0], args[1]);
    if (name == "mix")   return args[0] + (args[1] - args[0]) * args[2];
    if (name == "pow") {
        const double r = std::pow(args[0], args[1]);
        if (!std::isfinite(r)) return std::nullopt;
        return r;
    }
    if (name == "round") return std::round(args[0]);
    if (name == "sin")   return std::sin(args[0]);
    if (name == "sqrt") {
        if (args[0] < 0.0) return std::nullopt;
        return std::sqrt(args[0]);
    }
    if (name == "step")  return args[1] < args[0] ? 0.0 : 1.0;
    if (name == "tan") {
        const double r = std::tan(args[0]);
        if (!std::isfinite(r)) return std::nullopt;
        return r;
    }
    return std::nullopt;
}

// ── Parser (Pratt / precedence-climbing) ────────────────────────────────────

class Parser {
public:
    Parser(std::vector<Token> tokens, std::vector<std::string>& errors)
        : m_tokens(std::move(tokens)), m_errors(errors) {}

    AstPtr parse()
    {
        AstPtr e = parseExpr();
        if (!e) return nullptr;
        if (peek().kind != TokKind::End) {
            m_errors.push_back("col " + std::to_string(peek().column) +
                               ": unexpected trailing token '" + peek().text + "'");
            return nullptr;
        }
        return e;
    }

private:
    const Token& peek() const { return m_tokens[m_pos]; }
    Token consume() { return m_tokens[m_pos++]; }

    AstPtr parseExpr()  // additive
    {
        AstPtr lhs = parseTerm();
        if (!lhs) return nullptr;
        while (peek().kind == TokKind::Plus || peek().kind == TokKind::Minus) {
            const char op = peek().kind == TokKind::Plus ? '+' : '-';
            consume();
            AstPtr rhs = parseTerm();
            if (!rhs) return nullptr;
            lhs = std::make_shared<AstNode>(AstBinary{op, lhs, rhs});
        }
        return lhs;
    }

    AstPtr parseTerm()  // multiplicative
    {
        AstPtr lhs = parseUnary();
        if (!lhs) return nullptr;
        while (peek().kind == TokKind::Star  ||
               peek().kind == TokKind::Slash ||
               peek().kind == TokKind::Percent) {
            char op = '*';
            if (peek().kind == TokKind::Slash)   op = '/';
            if (peek().kind == TokKind::Percent) op = '%';
            consume();
            AstPtr rhs = parseUnary();
            if (!rhs) return nullptr;
            lhs = std::make_shared<AstNode>(AstBinary{op, lhs, rhs});
        }
        return lhs;
    }

    AstPtr parsePower()  // ^ right-associative; right side accepts unary
    {
        AstPtr lhs = parsePrimary();
        if (!lhs) return nullptr;
        if (peek().kind == TokKind::Caret) {
            consume();
            AstPtr rhs = parseUnary();
            if (!rhs) return nullptr;
            return std::make_shared<AstNode>(AstBinary{'^', lhs, rhs});
        }
        return lhs;
    }

    AstPtr parseUnary()
    {
        if (peek().kind == TokKind::Minus) {
            consume();
            AstPtr child = parseUnary();
            if (!child) return nullptr;
            return std::make_shared<AstNode>(AstUnary{'-', child});
        }
        if (peek().kind == TokKind::Plus) {
            consume();
            return parseUnary();
        }
        return parsePower();
    }

    AstPtr parsePrimary()
    {
        const Token& t = peek();
        if (t.kind == TokKind::Number) {
            consume();
            return std::make_shared<AstNode>(AstNumber{t.number});
        }
        if (t.kind == TokKind::Ident) {
            const std::string name = t.text;
            const std::size_t col  = t.column;
            consume();
            if (peek().kind == TokKind::LParen) {
                consume();
                AstCall call;
                call.name   = name;
                call.column = col;
                if (peek().kind != TokKind::RParen) {
                    while (true) {
                        AstPtr arg = parseExpr();
                        if (!arg) return nullptr;
                        call.args.push_back(arg);
                        if (peek().kind == TokKind::Comma) {
                            consume();
                            continue;
                        }
                        break;
                    }
                }
                if (peek().kind != TokKind::RParen) {
                    m_errors.push_back("col " + std::to_string(peek().column) +
                                       ": expected ')' in call to '" + name + "'");
                    return nullptr;
                }
                consume();
                return std::make_shared<AstNode>(std::move(call));
            }
            return std::make_shared<AstNode>(AstVar{name});
        }
        if (t.kind == TokKind::LParen) {
            consume();
            AstPtr inner = parseExpr();
            if (!inner) return nullptr;
            if (peek().kind != TokKind::RParen) {
                m_errors.push_back("col " + std::to_string(peek().column) +
                                   ": expected ')'");
                return nullptr;
            }
            consume();
            return inner;
        }
        m_errors.push_back("col " + std::to_string(t.column) +
                           ": expected expression, got '" + t.text + "'");
        return nullptr;
    }

    std::vector<Token>        m_tokens;
    std::size_t               m_pos = 0;
    std::vector<std::string>& m_errors;
};

// ── Evaluation ──────────────────────────────────────────────────────────────

std::optional<double> evalNode(const AstNode& n,
                               const std::unordered_map<std::string, double>& vars);

std::optional<double> evalNumber(const AstNumber& n, auto&) { return n.value; }

std::optional<double> evalVar(const AstVar& n,
                              const std::unordered_map<std::string, double>& vars)
{
    double c = 0.0;
    if (lookupConstant(n.name, c)) return c;
    const auto it = vars.find(n.name);
    if (it == vars.end()) return std::nullopt;
    return it->second;
}

std::optional<double> evalUnary(const AstUnary& n,
                                const std::unordered_map<std::string, double>& vars)
{
    auto v = evalNode(*n.child, vars);
    if (!v) return std::nullopt;
    return n.op == '-' ? -*v : *v;
}

std::optional<double> evalBinary(const AstBinary& n,
                                 const std::unordered_map<std::string, double>& vars)
{
    auto a = evalNode(*n.lhs, vars);
    if (!a) return std::nullopt;
    auto b = evalNode(*n.rhs, vars);
    if (!b) return std::nullopt;
    switch (n.op) {
        case '+': return *a + *b;
        case '-': return *a - *b;
        case '*': return *a * *b;
        case '/':
            if (*b == 0.0) return std::nullopt;
            return *a / *b;
        case '%':
            if (*b == 0.0) return std::nullopt;
            return std::fmod(*a, *b);
        case '^': {
            const double r = std::pow(*a, *b);
            if (!std::isfinite(r)) return std::nullopt;
            return r;
        }
    }
    return std::nullopt;
}

std::optional<double> evalCall(const AstCall& n,
                               const std::unordered_map<std::string, double>& vars)
{
    std::vector<double> args;
    args.reserve(n.args.size());
    for (const auto& a : n.args) {
        auto v = evalNode(*a, vars);
        if (!v) return std::nullopt;
        args.push_back(*v);
    }
    return callBuiltin(n.name, args);
}

std::optional<double> evalNode(const AstNode& n,
                               const std::unordered_map<std::string, double>& vars)
{
    return std::visit([&](const auto& node) -> std::optional<double> {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, AstNumber>) return evalNumber(node, vars);
        else if constexpr (std::is_same_v<T, AstVar>)    return evalVar(node, vars);
        else if constexpr (std::is_same_v<T, AstUnary>)  return evalUnary(node, vars);
        else if constexpr (std::is_same_v<T, AstBinary>) return evalBinary(node, vars);
        else if constexpr (std::is_same_v<T, AstCall>)   return evalCall(node, vars);
    }, n);
}

// ── Free-variable collection ────────────────────────────────────────────────

void collectVars(const AstNode& n, std::set<std::string>& out)
{
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, AstNumber>) {
            (void)node;
        } else if constexpr (std::is_same_v<T, AstVar>) {
            double c = 0.0;
            if (!lookupConstant(node.name, c)) out.insert(node.name);
        } else if constexpr (std::is_same_v<T, AstUnary>) {
            collectVars(*node.child, out);
        } else if constexpr (std::is_same_v<T, AstBinary>) {
            collectVars(*node.lhs, out);
            collectVars(*node.rhs, out);
        } else if constexpr (std::is_same_v<T, AstCall>) {
            for (const auto& a : node.args) collectVars(*a, out);
        }
    }, n);
}

// ── Static validation pass ──────────────────────────────────────────────────

bool validateCalls(const AstNode& n, std::vector<std::string>& errors)
{
    bool ok = true;
    std::visit([&](const auto& node) {
        using T = std::decay_t<decltype(node)>;
        if constexpr (std::is_same_v<T, AstCall>) {
            const auto& specs = functionSpecs();
            const auto it = specs.find(node.name);
            if (it == specs.end()) {
                errors.push_back("col " + std::to_string(node.column) +
                                 ": unknown function '" + node.name + "'");
                ok = false;
            } else {
                const int n_args = static_cast<int>(node.args.size());
                if (n_args < it->second.minArity || n_args > it->second.maxArity) {
                    errors.push_back("col " + std::to_string(node.column) +
                                     ": '" + node.name + "' expects " +
                                     std::to_string(it->second.minArity) +
                                     " argument(s), got " + std::to_string(n_args));
                    ok = false;
                }
            }
            for (const auto& a : node.args) {
                if (!validateCalls(*a, errors)) ok = false;
            }
        } else if constexpr (std::is_same_v<T, AstUnary>) {
            if (!validateCalls(*node.child, errors)) ok = false;
        } else if constexpr (std::is_same_v<T, AstBinary>) {
            if (!validateCalls(*node.lhs, errors)) ok = false;
            if (!validateCalls(*node.rhs, errors)) ok = false;
        }
    }, n);
    return ok;
}

} // namespace

// ── Expression::Impl ────────────────────────────────────────────────────────

struct Expression::Impl {
    std::string source;
    AstPtr      root;
};

Expression::Expression() : m_impl(std::make_unique<Impl>()) {}
Expression::Expression(Expression&&) noexcept = default;
Expression& Expression::operator=(Expression&&) noexcept = default;
Expression::~Expression() = default;

std::optional<Expression> Expression::compile(std::string_view source)
{
    CompileResult discard;
    return compileWithDiagnostics(source, discard);
}

std::optional<Expression> Expression::compileWithDiagnostics(
    std::string_view source, CompileResult& result)
{
    result.ok     = true;
    result.errors.clear();

    Lexer lex(source, result.errors);
    auto tokens = lex.tokenize();
    if (!result.errors.empty()) {
        result.ok = false;
        std::sort(result.errors.begin(), result.errors.end());
        return std::nullopt;
    }

    Parser parser(std::move(tokens), result.errors);
    AstPtr root = parser.parse();
    if (!root || !result.errors.empty()) {
        result.ok = false;
        std::sort(result.errors.begin(), result.errors.end());
        return std::nullopt;
    }

    if (!validateCalls(*root, result.errors)) {
        result.ok = false;
        std::sort(result.errors.begin(), result.errors.end());
        return std::nullopt;
    }

    Expression e;
    e.m_impl->source = std::string(source);
    e.m_impl->root   = std::move(root);
    return e;
}

std::optional<double> Expression::evaluate(
    const std::unordered_map<std::string, double>& variables) const
{
    if (!m_impl || !m_impl->root) return std::nullopt;
    return evalNode(*m_impl->root, variables);
}

std::vector<std::string> Expression::variableNames() const
{
    std::vector<std::string> out;
    if (!m_impl || !m_impl->root) return out;
    std::set<std::string> uniq;
    collectVars(*m_impl->root, uniq);
    out.assign(uniq.begin(), uniq.end());
    return out;
}

const std::string& Expression::source() const
{
    static const std::string kEmpty;
    return m_impl ? m_impl->source : kEmpty;
}

bool isBuiltinIdentifier(std::string_view name)
{
    double c = 0.0;
    if (lookupConstant(name, c)) return true;
    return functionSpecs().find(std::string(name)) != functionSpecs().end();
}

} // namespace nexus::script
