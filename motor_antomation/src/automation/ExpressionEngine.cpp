#include "ExpressionEngine.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <algorithm>

namespace MotorStudio {

// ============================================================================
// Internal helpers
// ============================================================================

namespace {

/// Characters that terminate a variable / channel name scan.
bool isDelimiterChar(char c) noexcept
{
    return std::isspace(static_cast<unsigned char>(c)) != 0 ||
           c == '+' || c == '-' || c == '*' || c == '/' || c == '%' ||
           c == '=' || c == '!' || c == '<' || c == '>' ||
           c == '&' || c == '|' || c == '(' || c == ')' || c == ',' ||
           c == '\0';
}

/// Peek ahead in the source string without bounds check (caller guarantees i < n).
char safePeek(const std::string& s, size_t i, size_t n) noexcept
{
    return (i < n) ? s[i] : '\0';
}

/// Determine whether token type is a binary operator that needs left/right children.
bool isBinaryOpToken(TokenType t) noexcept
{
    switch (t) {
    case TokenType::Plus:  case TokenType::Minus:
    case TokenType::Mul:   case TokenType::Div:   case TokenType::Mod:
    case TokenType::Eq:    case TokenType::Neq:
    case TokenType::Lt:    case TokenType::Gt:
    case TokenType::Le:    case TokenType::Ge:
    case TokenType::And:   case TokenType::Or:
        return true;
    default:
        return false;
    }
}

/// Return precedence for binary operators (higher = binds tighter).
int binaryPrecedence(TokenType t) noexcept
{
    switch (t) {
    case TokenType::Or:                          return 1;   // ||
    case TokenType::And:                         return 2;   // &&
    case TokenType::Eq: case TokenType::Neq:
    case TokenType::Lt: case TokenType::Gt:
    case TokenType::Le: case TokenType::Ge:      return 3;   // == != < > <= >=
    case TokenType::Plus: case TokenType::Minus: return 4;   // + -
    case TokenType::Mul: case TokenType::Div:
    case TokenType::Mod:                         return 5;   // * / %
    default:                                     return 0;
    }
}

/// Determine whether a token type can start an expression atom.
bool isAtomStart(TokenType t) noexcept
{
    return t == TokenType::Number ||
           t == TokenType::VarRef ||
           t == TokenType::ChannelRef ||
           t == TokenType::Identifier ||
           t == TokenType::LParen ||
           t == TokenType::Not ||
           t == TokenType::Minus;   // unary minus
}

} // anonymous namespace

// ============================================================================
// Token peek / match helpers
// ============================================================================

const Token& ExpressionEngine::peek(const std::vector<Token>& tokens, size_t pos)
{
    static const Token eofToken{TokenType::Eof, ""};
    return (pos < tokens.size()) ? tokens[pos] : eofToken;
}

bool ExpressionEngine::match(const std::vector<Token>& tokens, size_t& pos, TokenType type)
{
    if (pos < tokens.size() && tokens[pos].type == type) {
        ++pos;
        return true;
    }
    return false;
}

bool ExpressionEngine::isBinaryOp(TokenType t)
{
    return isBinaryOpToken(t);
}

// ============================================================================
// Lexer — scan input string → vector of Token
// ============================================================================

std::vector<Token> ExpressionEngine::tokenize(const std::string& expr)
{
    std::vector<Token> tokens;
    const size_t n = expr.size();
    size_t i = 0;

    while (i < n) {
        char c = expr[i];

        // ---- Whitespace -------------------------------------------------
        if (std::isspace(static_cast<unsigned char>(c))) {
            ++i;
            continue;
        }

        // ---- Numbers (integer and float) --------------------------------
        if (std::isdigit(static_cast<unsigned char>(c)) ||
            (c == '.' && i + 1 < n && std::isdigit(static_cast<unsigned char>(expr[i + 1])))) {
            size_t start = i;
            bool hasDot = false;
            while (i < n) {
                char nc = expr[i];
                if (std::isdigit(static_cast<unsigned char>(nc))) {
                    ++i;
                } else if (nc == '.' && !hasDot) {
                    hasDot = true;
                    ++i;
                } else {
                    break;
                }
            }
            Token t;
            t.type = TokenType::Number;
            t.text = expr.substr(start, i - start);
            t.numberValue = std::strtod(t.text.c_str(), nullptr);
            tokens.push_back(std::move(t));
            continue;
        }

        // ---- Variable reference: $name ----------------------------------
        if (c == '$') {
            ++i;  // skip $
            if (i >= n || isDelimiterChar(expr[i])) {
                // Trailing / empty $ → produce empty VarRef; parser will reject.
                Token t;
                t.type = TokenType::VarRef;
                t.text.clear();
                tokens.push_back(std::move(t));
                continue;
            }
            size_t start = i;
            while (i < n && !isDelimiterChar(expr[i])) {
                // Advance past UTF-8 continuation bytes correctly.
                unsigned char uc = static_cast<unsigned char>(expr[i]);
                if (uc < 0x80) {
                    ++i;
                } else {
                    // Multi-byte UTF-8: advance one Unicode codepoint.
                    int extra = 0;
                    if      ((uc & 0xE0) == 0xC0) extra = 1;
                    else if ((uc & 0xF0) == 0xE0) extra = 2;
                    else if ((uc & 0xF8) == 0xF0) extra = 3;
                    else                          extra = 0;  // invalid, treat as single byte
                    ++i;
                    for (int b = 0; b < extra && i < n; ++b) ++i;
                }
            }
            Token t;
            t.type = TokenType::VarRef;
            t.text = expr.substr(start, i - start);
            tokens.push_back(std::move(t));
            continue;
        }

        // ---- Identifiers (keywords / function names) --------------------
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t start = i;
            while (i < n && (std::isalnum(static_cast<unsigned char>(expr[i])) || expr[i] == '_'))
                ++i;
            std::string ident = expr.substr(start, i - start);

            // "channel:name" pattern
            if (ident == "channel" && i < n && expr[i] == ':') {
                ++i;  // skip ':'
                if (i >= n || isDelimiterChar(expr[i])) {
                    // Trailing / empty channel name — produce empty ref; parser rejects.
                    Token t;
                    t.type = TokenType::ChannelRef;
                    t.text.clear();
                    tokens.push_back(std::move(t));
                    continue;
                }
                size_t chStart = i;
                while (i < n && !isDelimiterChar(expr[i])) {
                    unsigned char uc = static_cast<unsigned char>(expr[i]);
                    if (uc < 0x80) {
                        ++i;
                    } else {
                        int extra = 0;
                        if      ((uc & 0xE0) == 0xC0) extra = 1;
                        else if ((uc & 0xF0) == 0xE0) extra = 2;
                        else if ((uc & 0xF8) == 0xF0) extra = 3;
                        ++i;
                        for (int b = 0; b < extra && i < n; ++b) ++i;
                    }
                }
                Token t;
                t.type = TokenType::ChannelRef;
                t.text = expr.substr(chStart, i - chStart);
                tokens.push_back(std::move(t));
                continue;
            }

            // Plain identifier (function name, etc.)
            Token t;
            t.type = TokenType::Identifier;
            t.text = ident;
            tokens.push_back(std::move(t));
            continue;
        }

        // ---- Single-char and double-char operators ----------------------
        switch (c) {
        case '(':
            tokens.push_back({TokenType::LParen, "("});
            ++i; continue;
        case ')':
            tokens.push_back({TokenType::RParen, ")"});
            ++i; continue;
        case ',':
            tokens.push_back({TokenType::Comma, ","});
            ++i; continue;
        case '+':
            tokens.push_back({TokenType::Plus, "+"});
            ++i; continue;
        case '-':
            tokens.push_back({TokenType::Minus, "-"});
            ++i; continue;
        case '*':
            tokens.push_back({TokenType::Mul, "*"});
            ++i; continue;
        case '/':
            tokens.push_back({TokenType::Div, "/"});
            ++i; continue;
        case '%':
            tokens.push_back({TokenType::Mod, "%"});
            ++i; continue;
        case '!':
            if (i + 1 < n && expr[i + 1] == '=') {
                tokens.push_back({TokenType::Neq, "!="});
                i += 2;
            } else {
                tokens.push_back({TokenType::Not, "!"});
                ++i;
            }
            continue;
        case '=':
            if (i + 1 < n && expr[i + 1] == '=') {
                tokens.push_back({TokenType::Eq, "=="});
                i += 2;
            } else {
                // Single '=' is not a valid token — skip it so parser can error cleanly.
                // We push an Eof immediately to force a parse failure.
                ++i;
                tokens.push_back({TokenType::Error, ""});
                return tokens;
            }
            continue;
        case '<':
            if (i + 1 < n && expr[i + 1] == '=') {
                tokens.push_back({TokenType::Le, "<="});
                i += 2;
            } else {
                tokens.push_back({TokenType::Lt, "<"});
                ++i;
            }
            continue;
        case '>':
            if (i + 1 < n && expr[i + 1] == '=') {
                tokens.push_back({TokenType::Ge, ">="});
                i += 2;
            } else {
                tokens.push_back({TokenType::Gt, ">"});
                ++i;
            }
            continue;
        case '&':
            if (i + 1 < n && expr[i + 1] == '&') {
                tokens.push_back({TokenType::And, "&&"});
                i += 2;
            } else {
                // Lone '&' is invalid — skip and force parse error
                ++i;
                tokens.push_back({TokenType::Error, ""});
                return tokens;
            }
            continue;
        case '|':
            if (i + 1 < n && expr[i + 1] == '|') {
                tokens.push_back({TokenType::Or, "||"});
                i += 2;
            } else {
                // Lone '|' is invalid — skip and force parse error
                ++i;
                tokens.push_back({TokenType::Error, ""});
                return tokens;
            }
            continue;
        default:
            // Unrecognized character — skip it; parser will see an unexpected
            // token stream and return nullptr.
            ++i;
            break;
        }
    }

    tokens.push_back({TokenType::Eof, ""});
    return tokens;
}

// ============================================================================
// Parser — recursive descent with standard precedence climbing
// ============================================================================

std::unique_ptr<ExprNode> ExpressionEngine::parse(const std::string& expr)
{
    auto tokens = tokenize(expr);
    size_t pos = 0;
    auto ast = parseExpression(tokens, pos);
    if (!ast) return nullptr;
    // Ensure we consumed the entire input (peek is Eof).
    if (peek(tokens, pos).type != TokenType::Eof)
        return nullptr;
    return ast;
}

// parseExpression  →  parseOr
std::unique_ptr<ExprNode> ExpressionEngine::parseExpression(
    const std::vector<Token>& tokens, size_t& pos)
{
    return parseOr(tokens, pos);
}

// parseOr  →  parseAnd  { '||' parseAnd }
std::unique_ptr<ExprNode> ExpressionEngine::parseOr(
    const std::vector<Token>& tokens, size_t& pos)
{
    auto left = parseAnd(tokens, pos);
    if (!left) return nullptr;

    while (peek(tokens, pos).type == TokenType::Or) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume ||
        auto right = parseAnd(tokens, pos);
        if (!right) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::BinaryOp;
        node->op = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// parseAnd  →  parseComparison  { '&&' parseComparison }
std::unique_ptr<ExprNode> ExpressionEngine::parseAnd(
    const std::vector<Token>& tokens, size_t& pos)
{
    auto left = parseComparison(tokens, pos);
    if (!left) return nullptr;

    while (peek(tokens, pos).type == TokenType::And) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume &&
        auto right = parseComparison(tokens, pos);
        if (!right) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::BinaryOp;
        node->op = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

// parseComparison  →  parseAdditive  { ('==' | '!=' | '<' | '>' | '<=' | '>=') parseAdditive }
std::unique_ptr<ExprNode> ExpressionEngine::parseComparison(
    const std::vector<Token>& tokens, size_t& pos)
{
    auto left = parseAdditive(tokens, pos);
    if (!left) return nullptr;

    TokenType tt = peek(tokens, pos).type;
    while (tt == TokenType::Eq  || tt == TokenType::Neq ||
           tt == TokenType::Lt  || tt == TokenType::Gt  ||
           tt == TokenType::Le  || tt == TokenType::Ge) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume operator
        auto right = parseAdditive(tokens, pos);
        if (!right) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::BinaryOp;
        node->op = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
        tt = peek(tokens, pos).type;
    }
    return left;
}

// parseAdditive  →  parseMultiplicative  { ('+' | '-') parseMultiplicative }
std::unique_ptr<ExprNode> ExpressionEngine::parseAdditive(
    const std::vector<Token>& tokens, size_t& pos)
{
    auto left = parseMultiplicative(tokens, pos);
    if (!left) return nullptr;

    TokenType tt = peek(tokens, pos).type;
    while (tt == TokenType::Plus || tt == TokenType::Minus) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume operator
        auto right = parseMultiplicative(tokens, pos);
        if (!right) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::BinaryOp;
        node->op = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
        tt = peek(tokens, pos).type;
    }
    return left;
}

// parseMultiplicative  →  parseUnary  { ('*' | '/' | '%') parseUnary }
std::unique_ptr<ExprNode> ExpressionEngine::parseMultiplicative(
    const std::vector<Token>& tokens, size_t& pos)
{
    auto left = parseUnary(tokens, pos);
    if (!left) return nullptr;

    TokenType tt = peek(tokens, pos).type;
    while (tt == TokenType::Mul || tt == TokenType::Div || tt == TokenType::Mod) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume operator
        auto right = parseUnary(tokens, pos);
        if (!right) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::BinaryOp;
        node->op = op;
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
        tt = peek(tokens, pos).type;
    }
    return left;
}

// parseUnary  →  ('!' | '-') parseUnary  |  parsePrimary
std::unique_ptr<ExprNode> ExpressionEngine::parseUnary(
    const std::vector<Token>& tokens, size_t& pos)
{
    TokenType tt = peek(tokens, pos).type;

    if (tt == TokenType::Not || tt == TokenType::Minus) {
        TokenType op = tokens[pos].type;
        ++pos;  // consume ! or -
        auto operand = parseUnary(tokens, pos);  // right-recursive for stacking: !!x
        if (!operand) return nullptr;
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::UnaryOp;
        node->op = op;
        node->left = std::move(operand);  // store operand in 'left' for unary
        return node;
    }

    return parsePrimary(tokens, pos);
}

// parsePrimary  →  Number | VarRef | ChannelRef | Identifier '(' ... ')' | '(' expr ')'
std::unique_ptr<ExprNode> ExpressionEngine::parsePrimary(
    const std::vector<Token>& tokens, size_t& pos)
{
    TokenType tt = peek(tokens, pos).type;

    // Number literal
    if (tt == TokenType::Number) {
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::Number;
        node->numberValue = tokens[pos].numberValue;
        ++pos;
        return node;
    }

    // Variable reference: $x
    if (tt == TokenType::VarRef) {
        const std::string& name = tokens[pos].text;
        ++pos;
        if (name.empty()) return nullptr;   // $ at end of input
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::VarRef;
        node->name = name;
        return node;
    }

    // Channel reference: channel:name
    if (tt == TokenType::ChannelRef) {
        const std::string& name = tokens[pos].text;
        ++pos;
        if (name.empty()) return nullptr;   // channel: at end of input
        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::ChannelRef;
        node->name = name;
        return node;
    }

    // Function call: ident '(' args... ')'
    if (tt == TokenType::Identifier) {
        std::string funcName = tokens[pos].text;
        ++pos;  // consume identifier

        if (!match(tokens, pos, TokenType::LParen))
            return nullptr;  // bare identifier is invalid in this grammar

        auto node = std::make_unique<ExprNode>();
        node->type = ExprType::FuncCall;
        node->name = funcName;

        // Optional argument list
        if (peek(tokens, pos).type != TokenType::RParen) {
            do {
                auto arg = parseExpression(tokens, pos);
                if (!arg) return nullptr;
                node->args.push_back(std::move(arg));
            } while (match(tokens, pos, TokenType::Comma));
        }

        if (!match(tokens, pos, TokenType::RParen))
            return nullptr;  // missing closing paren

        return node;
    }

    // Parenthesised sub-expression
    if (tt == TokenType::LParen) {
        ++pos;  // consume '('
        auto node = parseExpression(tokens, pos);
        if (!node) return nullptr;
        if (!match(tokens, pos, TokenType::RParen))
            return nullptr;  // missing ')'
        return node;
    }

    // Unexpected token
    return nullptr;
}

// ============================================================================
// Evaluator — walk AST, resolve via ValueProvider
// ============================================================================

std::optional<double> ExpressionEngine::evaluate(const std::string& expr,
                                                  const ValueProvider& provider)
{
    auto ast = parse(expr);
    if (!ast) return std::nullopt;
    return evaluateAst(ast.get(), provider);
}

std::optional<double> ExpressionEngine::evaluateAst(const ExprNode* node,
                                                     const ValueProvider& provider)
{
    if (!node) return std::nullopt;

    switch (node->type) {

    case ExprType::Number:
        return node->numberValue;

    case ExprType::VarRef: {
        if (!provider.getVariable) return std::nullopt;
        auto val = provider.getVariable(node->name);
        return val;  // std::optional<double> — nullopt if not found
    }

    case ExprType::ChannelRef: {
        if (!provider.getChannel) return std::nullopt;
        auto val = provider.getChannel(node->name);
        return val;
    }

    case ExprType::BinaryOp: {
        auto lhs = evaluateAst(node->left.get(), provider);
        auto rhs = evaluateAst(node->right.get(), provider);
        if (!lhs || !rhs) return std::nullopt;

        double l = *lhs;
        double r = *rhs;

        switch (node->op) {
        case TokenType::Plus:  return l + r;
        case TokenType::Minus: return l - r;
        case TokenType::Mul:   return l * r;
        case TokenType::Div:
            if (r == 0.0) return std::nullopt;
            return l / r;
        case TokenType::Mod:
            if (r == 0.0) return std::nullopt;
            return std::fmod(l, r);
        case TokenType::Eq:    return (l == r) ? 1.0 : 0.0;
        case TokenType::Neq:   return (l != r) ? 1.0 : 0.0;
        case TokenType::Lt:    return (l < r)  ? 1.0 : 0.0;
        case TokenType::Gt:    return (l > r)  ? 1.0 : 0.0;
        case TokenType::Le:    return (l <= r) ? 1.0 : 0.0;
        case TokenType::Ge:    return (l >= r) ? 1.0 : 0.0;
        case TokenType::And:   return ((l != 0.0) && (r != 0.0)) ? 1.0 : 0.0;
        case TokenType::Or:    return ((l != 0.0) || (r != 0.0)) ? 1.0 : 0.0;
        default:
            return std::nullopt;
        }
    }

    case ExprType::UnaryOp: {
        auto operand = evaluateAst(node->left.get(), provider);
        if (!operand) return std::nullopt;

        switch (node->op) {
        case TokenType::Not:
            return (*operand == 0.0) ? 1.0 : 0.0;
        case TokenType::Minus:
            return -(*operand);
        default:
            return std::nullopt;
        }
    }

    case ExprType::FuncCall: {
        // Evaluate all arguments
        std::vector<double> argVals;
        argVals.reserve(node->args.size());
        for (const auto& arg : node->args) {
            auto v = evaluateAst(arg.get(), provider);
            if (!v) return std::nullopt;
            argVals.push_back(*v);
        }

        const std::string& fn = node->name;

        if (fn == "min") {
            if (argVals.empty()) return std::nullopt;
            double m = argVals[0];
            for (size_t i = 1; i < argVals.size(); ++i)
                if (argVals[i] < m) m = argVals[i];
            return m;
        }

        if (fn == "max") {
            if (argVals.empty()) return std::nullopt;
            double m = argVals[0];
            for (size_t i = 1; i < argVals.size(); ++i)
                if (argVals[i] > m) m = argVals[i];
            return m;
        }

        if (fn == "abs") {
            if (argVals.size() != 1) return std::nullopt;
            return std::abs(argVals[0]);
        }

        if (fn == "avg") {
            if (argVals.empty()) return std::nullopt;
            double sum = 0.0;
            for (double v : argVals) sum += v;
            return sum / static_cast<double>(argVals.size());
        }

        // Unknown function
        return std::nullopt;
    }

    } // switch

    return std::nullopt;
}

} // namespace MotorStudio
