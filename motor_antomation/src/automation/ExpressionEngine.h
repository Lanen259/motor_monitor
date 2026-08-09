#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace MotorStudio {

// ============================================================================
// Token types for lexical analysis
// ============================================================================
enum class TokenType {
    Number,       // 3.14, 42
    String,       // "hello"
    Identifier,   // function names: min, max, abs, avg
    Plus,         // +
    Minus,        // -
    Mul,          // *
    Div,          // /
    Mod,          // %
    Eq,           // ==
    Neq,          // !=
    Lt,           // <
    Gt,           // >
    Le,           // <=
    Ge,           // >=
    And,          // &&
    Or,           // ||
    Not,          // !
    LParen,       // (
    RParen,       // )
    Comma,        // ,
    ChannelRef,   // channel:xxx
    VarRef,       // $xxx
    Error,        // 非法字符（孤立 & / | / = 等）——解析器见到即失败
    Eof           // end of input
};

// ============================================================================
// Token
// ============================================================================
struct Token {
    TokenType type = TokenType::Eof;
    std::string text;
    double numberValue = 0.0;
};

// ============================================================================
// AST node types
// ============================================================================
enum class ExprType {
    Number,       // literal number
    VarRef,       // $variable
    ChannelRef,   // channel:name
    BinaryOp,     // lhs op rhs
    UnaryOp,      // op operand (! and unary -)
    FuncCall      // func(args...)
};

// ============================================================================
// AST node
// ============================================================================
struct ExprNode {
    ExprType type = ExprType::Number;
    double numberValue = 0.0;
    std::string name;                                    // variable / channel / function name
    TokenType op = TokenType::Plus;                      // operator for BinaryOp / UnaryOp
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
    std::vector<std::unique_ptr<ExprNode>> args;         // for function calls
};

// ============================================================================
// Value provider — callbacks for variable and channel resolution at evaluation time
// ============================================================================
struct ValueProvider {
    std::function<std::optional<double>(const std::string& varName)> getVariable;
    std::function<std::optional<double>(const std::string& channelName)> getChannel;
};

// ============================================================================
// ExpressionEngine — lex, parse, evaluate
// ============================================================================
class ExpressionEngine {
public:
    /// Parse + evaluate in one call.
    /// Returns std::nullopt on syntax error, unknown variable/channel, or
    /// runtime error (e.g. division by zero).
    static std::optional<double> evaluate(const std::string& expr,
                                          const ValueProvider& provider);

    /// Parse only — returns AST for caching / repeated evaluation.
    /// Returns nullptr on syntax error.
    static std::unique_ptr<ExprNode> parse(const std::string& expr);

    /// Evaluate a pre-parsed AST.
    /// Returns std::nullopt on runtime error.
    static std::optional<double> evaluateAst(const ExprNode* node,
                                             const ValueProvider& provider);

private:
    // ---- Lexer ---------------------------------------------------------------
    static std::vector<Token> tokenize(const std::string& expr);

    // ---- Recursive-descent parser (precedence climbing) -----------------------
    static std::unique_ptr<ExprNode> parseExpression(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseOr(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseAnd(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseComparison(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseAdditive(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseMultiplicative(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parseUnary(const std::vector<Token>& tokens, size_t& pos);
    static std::unique_ptr<ExprNode> parsePrimary(const std::vector<Token>& tokens, size_t& pos);

    // ---- Helpers -------------------------------------------------------------
    static const Token& peek(const std::vector<Token>& tokens, size_t pos);
    static bool match(const std::vector<Token>& tokens, size_t& pos, TokenType type);
    static bool isBinaryOp(TokenType t);
};

} // namespace MotorStudio
