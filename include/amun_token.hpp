#pragma once

#include "amun_basic.hpp"

#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

enum TokenKind {
    TOKEN_LOAD,
    TOKEN_IMPORT,

    TOKEN_VAR,
    TOKEN_CONST,
    TOKEN_ENUM,
    TOKEN_TYPE,
    TOKEN_STRUCT,
    TOKEN_FUN,
    TOKEN_OPERATOR,
    TOKEN_RETURN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_FOR,
    TOKEN_WHILE,
    TOKEN_SWITCH,
    TOKEN_CAST,
    TOKEN_DEFER,

    TOKEN_BREAK,
    TOKEN_CONTINUE,

    TOKEN_TYPE_SIZE,
    TOKEN_TYPE_ALLIGN,
    TOKEN_VALUE_SIZE,

    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_NULL,
    TOKEN_UNDEFINED,

    TOKEN_VARARGS,

    TOKEN_DOT,
    TOKEN_DOT_DOT,
    TOKEN_COMMA,
    TOKEN_COLON,
    TOKEN_COLON_COLON,
    TOKEN_SEMICOLON,
    TOKEN_AT,

    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,

    TOKEN_OR,
    TOKEN_OR_OR,
    TOKEN_AND,
    TOKEN_AND_AND,

    TOKEN_NOT,

    TOKEN_EQUAL,
    TOKEN_EQUAL_EQUAL,
    TOKEN_BANG,
    TOKEN_BANG_EQUAL,
    TOKEN_GREATER,
    TOKEN_GREATER_EQUAL,
    TOKEN_SMALLER,
    TOKEN_SMALLER_EQUAL,

    TOKEN_RIGHT_SHIFT,
    TOKEN_LEFT_SHIFT,

    TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL,
    TOKEN_STAR_EQUAL,
    TOKEN_SLASH_EQUAL,
    TOKEN_PARCENT_EQUAL,

    TOKEN_PLUS_PLUS,
    TOKEN_MINUS_MINUS,

    TOKEN_RIGHT_ARROW,

    TOKEN_OPEN_PAREN,
    TOKEN_CLOSE_PAREN,
    TOKEN_OPEN_BRACKET,
    TOKEN_CLOSE_BRACKET,
    TOKEN_OPEN_BRACE,
    TOKEN_CLOSE_BRACE,

    TOKEN_IDENTIFIER,
    TOKEN_STRING,
    TOKEN_CHARACTER,

    TOKEN_INT,
    TOKEN_INT1,
    TOKEN_INT8,
    TOKEN_INT16,
    TOKEN_INT32,
    TOKEN_INT64,

    TOKEN_UINT8,
    TOKEN_UINT16,
    TOKEN_UINT32,
    TOKEN_UINT64,

    TOKEN_FLOAT,
    TOKEN_FLOAT32,
    TOKEN_FLOAT64,

    TOKEN_INVALID,
    TOKEN_END_OF_FILE,
};

struct TokenSpan {
    int file_id;
    int line_number;
    int column_start;
    int column_end;
};

struct Token {
    TokenKind kind;
    TokenSpan position;
    std::string literal;
};

struct TwoTokensOperator {
    TokenKind first;
    TokenKind second;
    TokenKind both;
};

// Used for error messages and debugging
static std::unordered_map<TokenKind, const char*> token_kind_literal = {
    {TokenKind::TOKEN_LOAD, "load"},
    {TokenKind::TOKEN_IMPORT, "import"},
    {TokenKind::TOKEN_VAR, "var"},
    {TokenKind::TOKEN_CONST, "const"},
    {TokenKind::TOKEN_FUN, "fun"},
    {TokenKind::TOKEN_OPERATOR, "operator"},
    {TokenKind::TOKEN_ENUM, "enum"},
    {TokenKind::TOKEN_TYPE, "type"},
    {TokenKind::TOKEN_STRUCT, "struct"},
    {TokenKind::TOKEN_RETURN, "return"},
    {TokenKind::TOKEN_IF, "if"},
    {TokenKind::TOKEN_ELSE, "else"},
    {TokenKind::TOKEN_FOR, "for"},
    {TokenKind::TOKEN_WHILE, "while"},
    {TokenKind::TOKEN_SWITCH, "switch"},
    {TokenKind::TOKEN_CAST, "cast"},
    {TokenKind::TOKEN_DEFER, "defer"},

    {TokenKind::TOKEN_TRUE, "true"},
    {TokenKind::TOKEN_FALSE, "false"},
    {TokenKind::TOKEN_NULL, "null"},
    {TokenKind::TOKEN_UNDEFINED, "undefined"},

    {TokenKind::TOKEN_BREAK, "break"},
    {TokenKind::TOKEN_CONTINUE, "continue"},

    {TokenKind::TOKEN_TYPE_SIZE, "type_size"},
    {TokenKind::TOKEN_TYPE_ALLIGN, "type_allign"},
    {TokenKind::TOKEN_VALUE_SIZE, "value_size"},

    {TokenKind::TOKEN_VARARGS, "varargs"},

    {TokenKind::TOKEN_DOT, "."},
    {TokenKind::TOKEN_DOT_DOT, ".."},
    {TokenKind::TOKEN_COMMA, ","},
    {TokenKind::TOKEN_COLON, ":"},
    {TokenKind::TOKEN_COLON_COLON, "::"},
    {TokenKind::TOKEN_SEMICOLON, ";"},
    {TokenKind::TOKEN_AT, "@"},

    {TokenKind::TOKEN_PLUS, "+"},
    {TokenKind::TOKEN_MINUS, "-"},
    {TokenKind::TOKEN_STAR, "*"},
    {TokenKind::TOKEN_SLASH, "/"},
    {TokenKind::TOKEN_PERCENT, "%"},

    {TokenKind::TOKEN_AND, "&"},
    {TokenKind::TOKEN_AND_AND, "&&"},
    {TokenKind::TOKEN_OR, "|"},
    {TokenKind::TOKEN_OR_OR, "||"},

    {TokenKind::TOKEN_NOT, "~"},

    {TokenKind::TOKEN_EQUAL, "="},
    {TokenKind::TOKEN_EQUAL_EQUAL, "=="},
    {TokenKind::TOKEN_BANG, "!"},
    {TokenKind::TOKEN_BANG_EQUAL, "!="},

    {TokenKind::TOKEN_GREATER, ">"},
    {TokenKind::TOKEN_GREATER_EQUAL, ">="},
    {TokenKind::TOKEN_SMALLER, "<"},
    {TokenKind::TOKEN_SMALLER_EQUAL, "<="},

    {TokenKind::TOKEN_RIGHT_SHIFT, ">>"},
    {TokenKind::TOKEN_LEFT_SHIFT, "<<"},

    {TokenKind::TOKEN_PLUS_EQUAL, "+="},
    {TokenKind::TOKEN_MINUS_EQUAL, "-="},
    {TokenKind::TOKEN_STAR_EQUAL, "*="},
    {TokenKind::TOKEN_SLASH_EQUAL, "/="},
    {TokenKind::TOKEN_PARCENT_EQUAL, "%="},

    {TokenKind::TOKEN_PLUS_PLUS, "++"},
    {TokenKind::TOKEN_MINUS_MINUS, "--"},

    {TokenKind::TOKEN_RIGHT_ARROW, "->"},

    {TokenKind::TOKEN_OPEN_PAREN, "("},
    {TokenKind::TOKEN_CLOSE_PAREN, ")"},
    {TokenKind::TOKEN_OPEN_BRACKET, "["},
    {TokenKind::TOKEN_CLOSE_BRACKET, "]"},
    {TokenKind::TOKEN_OPEN_BRACE, "{"},
    {TokenKind::TOKEN_CLOSE_BRACE, "}"},

    {TokenKind::TOKEN_IDENTIFIER, "identifier"},
    {TokenKind::TOKEN_STRING, "string"},
    {TokenKind::TOKEN_CHARACTER, "char"},
    {TokenKind::TOKEN_INT, "int"},
    {TokenKind::TOKEN_INT1, "int1"},
    {TokenKind::TOKEN_INT8, "int8"},
    {TokenKind::TOKEN_INT16, "int16"},
    {TokenKind::TOKEN_INT32, "int32"},
    {TokenKind::TOKEN_INT64, "int64"},
    {TokenKind::TOKEN_UINT8, "uint8"},
    {TokenKind::TOKEN_UINT16, "uint16"},
    {TokenKind::TOKEN_UINT32, "uint32"},
    {TokenKind::TOKEN_UINT64, "uint64"},
    {TokenKind::TOKEN_FLOAT, "float"},
    {TokenKind::TOKEN_FLOAT32, "float32"},
    {TokenKind::TOKEN_FLOAT64, "float64"},

    {TokenKind::TOKEN_INVALID, "Invalid"},
    {TokenKind::TOKEN_END_OF_FILE, "End of the file"},
};

static std::unordered_set<TokenKind> unary_operators = {
    TokenKind::TOKEN_MINUS, TokenKind::TOKEN_BANG, TokenKind::TOKEN_STAR,
    TokenKind::TOKEN_AND,   TokenKind::TOKEN_NOT,
};

static std::unordered_set<TokenKind> assignments_operators = {
    TokenKind::TOKEN_EQUAL,      TokenKind::TOKEN_PLUS_EQUAL,  TokenKind::TOKEN_MINUS_EQUAL,
    TokenKind::TOKEN_STAR_EQUAL, TokenKind::TOKEN_SLASH_EQUAL, TokenKind::TOKEN_PARCENT_EQUAL,
};

static std::unordered_map<TokenKind, TokenKind> assignments_binary_operators = {
    {TokenKind::TOKEN_PLUS_EQUAL, TokenKind::TOKEN_PLUS},
    {TokenKind::TOKEN_MINUS_EQUAL, TokenKind::TOKEN_MINUS},
    {TokenKind::TOKEN_STAR_EQUAL, TokenKind::TOKEN_STAR},
    {TokenKind::TOKEN_SLASH_EQUAL, TokenKind::TOKEN_SLASH},
    {TokenKind::TOKEN_PARCENT_EQUAL, TokenKind::TOKEN_PERCENT},
};

static std::unordered_map<TokenKind, std::string> overloading_operator_literal = {
    // +, -, *, /, %
    {TokenKind::TOKEN_PLUS, "plus"},
    {TokenKind::TOKEN_MINUS, "minus"},
    {TokenKind::TOKEN_STAR, "star"},
    {TokenKind::TOKEN_SLASH, "slash"},
    {TokenKind::TOKEN_PERCENT, "percent"},

    // !, ~
    {TokenKind::TOKEN_BANG, "bang"},
    {TokenKind::TOKEN_NOT, "not"},

    // ==, !=, >, >=, <, <=
    {TokenKind::TOKEN_EQUAL_EQUAL, "eq_eq"},
    {TokenKind::TOKEN_BANG_EQUAL, "not_eq"},
    {TokenKind::TOKEN_GREATER, "gt"},
    {TokenKind::TOKEN_GREATER_EQUAL, "gt_eq"},
    {TokenKind::TOKEN_SMALLER, "lt"},
    {TokenKind::TOKEN_SMALLER_EQUAL, "lt_eq"},

    // &, |
    {TokenKind::TOKEN_AND, "and"},
    {TokenKind::TOKEN_OR, "or"},

    // &&, ||
    {TokenKind::TOKEN_AND_AND, "and_and"},
    {TokenKind::TOKEN_OR_OR, "or_or"},

    // <<, >>
    {TokenKind::TOKEN_RIGHT_SHIFT, "rsh"},
    {TokenKind::TOKEN_LEFT_SHIFT, "lsh"},

    {TokenKind::TOKEN_PLUS_PLUS, "plus_plus"},
    {TokenKind::TOKEN_MINUS_MINUS, "minus_minus"},
};

static std::unordered_set<TokenKind> overloading_prefix_operators = {
    TokenKind::TOKEN_NOT,       TokenKind::TOKEN_BANG,        TokenKind::TOKEN_MINUS,
    TokenKind::TOKEN_PLUS_PLUS, TokenKind::TOKEN_MINUS_MINUS,
};

static std::unordered_set<TokenKind> overloading_infix_operators = {
    // +, -, *, /, %
    TokenKind::TOKEN_PLUS,
    TokenKind::TOKEN_MINUS,
    TokenKind::TOKEN_STAR,
    TokenKind::TOKEN_SLASH,
    TokenKind::TOKEN_PERCENT,

    // ==, !=, >, >=, <, <=
    TokenKind::TOKEN_EQUAL_EQUAL,
    TokenKind::TOKEN_BANG_EQUAL,
    TokenKind::TOKEN_GREATER,
    TokenKind::TOKEN_GREATER_EQUAL,
    TokenKind::TOKEN_SMALLER,
    TokenKind::TOKEN_SMALLER_EQUAL,

    // &, |
    TokenKind::TOKEN_AND,
    TokenKind::TOKEN_OR,

    // &&, ||
    TokenKind::TOKEN_AND_AND,
    TokenKind::TOKEN_OR_OR,

    // <<, >>
    TokenKind::TOKEN_RIGHT_SHIFT,
    TokenKind::TOKEN_LEFT_SHIFT,
};

static std::unordered_set<TokenKind> overloading_postfix_operators = {
    TokenKind::TOKEN_PLUS_PLUS,
    TokenKind::TOKEN_MINUS_MINUS,
};

// A list of operators that created from two tokens for example ++, ==, !=, << ...etc
// This list will used in the parser to check if use write them with speace in the middle
// Note: a - -b is not an error it's minus with and unary expression
static std::vector<TwoTokensOperator> two_tokens_operators = {
    // Binary expressions
    {TokenKind::TOKEN_PLUS, TokenKind::TOKEN_PLUS, TokenKind::TOKEN_PLUS_PLUS},

    // Shift expression
    {TokenKind::TOKEN_SMALLER, TokenKind::TOKEN_SMALLER, TokenKind::TOKEN_LEFT_SHIFT},

    // Comparisons expression
    {TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_EQUAL_EQUAL},
    {TokenKind::TOKEN_BANG, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_BANG_EQUAL},
    {TokenKind::TOKEN_GREATER, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_GREATER_EQUAL},
    {TokenKind::TOKEN_SMALLER, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_SMALLER_EQUAL},

    // Assignment expression
    {TokenKind::TOKEN_PLUS, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_PLUS_EQUAL},
    {TokenKind::TOKEN_MINUS, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_MINUS_EQUAL},
    {TokenKind::TOKEN_STAR, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_STAR_EQUAL},
    {TokenKind::TOKEN_SLASH, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_SLASH_EQUAL},
    {TokenKind::TOKEN_PERCENT, TokenKind::TOKEN_EQUAL, TokenKind::TOKEN_PARCENT_EQUAL},
};

inline auto is_supported_overloading_operator(TokenKind kind) -> bool
{
    return overloading_operator_literal.contains(kind);
}

inline auto is_overloading_prefix_operator(TokenKind kind) -> bool
{
    return overloading_prefix_operators.contains(kind);
}

inline auto is_overloading_infix_operator(TokenKind kind) -> bool
{
    return overloading_infix_operators.contains(kind);
}

inline auto is_overloading_postfix_operator(TokenKind kind) -> bool
{
    return overloading_postfix_operators.contains(kind);
}

inline auto is_assignments_operator_token(Token token) -> bool
{
    return assignments_operators.find(token.kind) != assignments_operators.end();
}

inline auto is_unary_operator_token(Token token) -> bool
{
    return unary_operators.find(token.kind) != unary_operators.end();
}

inline auto is_float_number_token(Token token) -> bool
{
    const auto token_kind = token.kind;
    return token_kind == TokenKind::TOKEN_FLOAT or token_kind == TokenKind::TOKEN_FLOAT32 or
           token_kind == TokenKind::TOKEN_FLOAT64;
}