#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>

enum TokenKind {
    LoadKeyword,
    ImportKeyword,

    VarKeyword,
    TypeKeyword,
    EnumKeyword,
    StructKeyword,
    FunKeyword,
    ReturnKeyword,
    ExternKeyword,
    IfKeyword,
    ElseKeyword,
    ForKeyword,
    WhileKeyword,
    SwitchKeyword,
    CastKeyword,
    DeferKeyword,

    BreakKeyword,
    ContinueKeyword,

    PrefixKeyword,
    InfixKeyword,
    PostfixKeyword,

    TypeSizeKeyword,
    ValueSizeKeyword,

    TrueKeyword,
    FalseKeyword,
    NullKeyword,

    VarargsKeyword,

    Dot,
    DotDot,
    Comma,
    Colon,
    ColonColon,
    Semicolon,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,

    Or,
    LogicalOr,
    And,
    LogicalAnd,

    Not,

    Equal,
    EqualEqual,
    Bang,
    BangEqual,
    Greater,
    GreaterEqual,
    Smaller,
    SmallerEqual,

    RightShift,
    LeftShift,

    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,

    PlusPlus,
    MinusMinus,

    RightArrow,

    OpenParen,
    CloseParen,
    OpenBracket,
    CloseBracket,
    OpenBrace,
    CloseBrace,

    Symbol,
    String,
    Character,

    Integer,
    Integer1Type,
    Integer8Type,
    Integer16Type,
    Integer32Type,
    Integer64Type,
    Float,
    Float32Type,
    Float64Type,

    Invalid,
    EndOfFile,
};

static std::unordered_map<TokenKind, const char*> token_kind_literal = {
    {TokenKind::LoadKeyword, "Load"},
    {TokenKind::ImportKeyword, "Import"},
    {TokenKind::VarKeyword, "Var"},
    {TokenKind::TypeKeyword, "Type"},
    {TokenKind::FunKeyword, "Fun"},
    {TokenKind::EnumKeyword, "Enum"},
    {TokenKind::StructKeyword, "Struct"},
    {TokenKind::ReturnKeyword, "Return"},
    {TokenKind::ExternKeyword, "Extern"},
    {TokenKind::IfKeyword, "If"},
    {TokenKind::ElseKeyword, "Else"},
    {TokenKind::ForKeyword, "for"},
    {TokenKind::WhileKeyword, "While"},
    {TokenKind::SwitchKeyword, "Switch"},
    {TokenKind::CastKeyword, "Cast"},
    {TokenKind::DeferKeyword, "Defer"},
    {TokenKind::TrueKeyword, "True"},
    {TokenKind::FalseKeyword, "False"},
    {TokenKind::NullKeyword, "Null"},

    {TokenKind::BreakKeyword, "Break"},
    {TokenKind::ContinueKeyword, "Continue"},

    {TokenKind::PrefixKeyword, "Prefix"},
    {TokenKind::InfixKeyword, "Infix"},
    {TokenKind::PostfixKeyword, "Postfix"},

    {TokenKind::TypeSizeKeyword, "TypeSize"},
    {TokenKind::ValueSizeKeyword, "ValueSize"},

    {TokenKind::VarargsKeyword, "Varargs"},

    {TokenKind::Dot, "Dot ."},
    {TokenKind::DotDot, "DotDot .."},
    {TokenKind::Comma, "Comma ,"},
    {TokenKind::Colon, "Colon :"},
    {TokenKind::ColonColon, "ColonColon ::"},
    {TokenKind::Semicolon, "Semicolon ;"},

    {TokenKind::Plus, "+"},
    {TokenKind::Minus, "-"},
    {TokenKind::Star, "*"},
    {TokenKind::Slash, "/"},
    {TokenKind::Percent, "%"},

    {TokenKind::And, "&"},
    {TokenKind::LogicalAnd, "&&"},
    {TokenKind::Or, "|"},
    {TokenKind::LogicalOr, "||"},

    {TokenKind::Not, "~"},

    {TokenKind::Equal, "="},
    {TokenKind::EqualEqual, "=="},
    {TokenKind::Bang, "!"},
    {TokenKind::BangEqual, "!="},

    {TokenKind::Greater, ">"},
    {TokenKind::GreaterEqual, ">="},
    {TokenKind::Smaller, "<"},
    {TokenKind::SmallerEqual, "<="},

    {TokenKind::RightShift, ">>"},
    {TokenKind::LeftShift, "<<"},

    {TokenKind::PlusEqual, "+="},
    {TokenKind::MinusEqual, "-="},
    {TokenKind::StarEqual, "*="},
    {TokenKind::SlashEqual, "/="},
    {TokenKind::PercentEqual, "%="},

    {TokenKind::PlusPlus, "++"},
    {TokenKind::MinusMinus, "--"},

    {TokenKind::RightArrow, "->"},

    {TokenKind::OpenParen, "("},
    {TokenKind::CloseParen, ")"},
    {TokenKind::OpenBracket, "["},
    {TokenKind::CloseBracket, "]"},
    {TokenKind::OpenBrace, "{"},
    {TokenKind::CloseBrace, "}"},

    {TokenKind::Symbol, "Symbol"},
    {TokenKind::String, "String"},
    {TokenKind::Character, "Character"},
    {TokenKind::Integer, "Integer"},
    {TokenKind::Integer1Type, "Integer1Type"},
    {TokenKind::Integer8Type, "Integer8Type"},
    {TokenKind::Integer16Type, "Integer16Type"},
    {TokenKind::Integer32Type, "Integer32Type"},
    {TokenKind::Integer64Type, "Integer64Type"},
    {TokenKind::Float, "Float"},
    {TokenKind::Float32Type, "Float32Type"},
    {TokenKind::Float64Type, "Float64Type"},

    {TokenKind::Invalid, "Invalid"},
    {TokenKind::EndOfFile, "End of the file"},
};

static std::unordered_map<std::string, TokenKind> language_keywords = {
    {"load", TokenKind::LoadKeyword},
    {"import", TokenKind::ImportKeyword},
    {"var", TokenKind::VarKeyword},
    {"type", TokenKind::TypeKeyword},
    {"fun", TokenKind::FunKeyword},
    {"enum", TokenKind::EnumKeyword},
    {"struct", TokenKind::StructKeyword},
    {"return", TokenKind::ReturnKeyword},
    {"extern", TokenKind::ExternKeyword},
    {"if", TokenKind::IfKeyword},
    {"else", TokenKind::ElseKeyword},
    {"for", TokenKind::ForKeyword},
    {"while", TokenKind::WhileKeyword},
    {"switch", TokenKind::SwitchKeyword},
    {"cast", TokenKind::CastKeyword},
    {"defer", TokenKind::DeferKeyword},
    {"true", TokenKind::TrueKeyword},
    {"false", TokenKind::FalseKeyword},
    {"null", TokenKind::NullKeyword},
    {"break", TokenKind::BreakKeyword},
    {"continue", TokenKind::ContinueKeyword},
    {"prefix", TokenKind::PrefixKeyword},
    {"infix", TokenKind::InfixKeyword},
    {"postfix", TokenKind::PostfixKeyword},
    {"varargs", TokenKind::VarargsKeyword},
    {"type_size", TokenKind::TypeSizeKeyword},
    {"value_size", TokenKind::ValueSizeKeyword}};

static std::unordered_set<TokenKind> unary_operators{
    TokenKind::Minus, TokenKind::Bang, TokenKind::Star, TokenKind::And, TokenKind::Not,
};

static std::unordered_set<TokenKind> assignments_operators{
    TokenKind::Equal,     TokenKind::PlusEqual,  TokenKind::MinusEqual,
    TokenKind::StarEqual, TokenKind::SlashEqual, TokenKind::PercentEqual,
};

static std::unordered_map<TokenKind, TokenKind> assignments_binary_operators{
    {TokenKind::PlusEqual, TokenKind::Plus},       {TokenKind::MinusEqual, TokenKind::Minus},
    {TokenKind::StarEqual, TokenKind::Star},       {TokenKind::SlashEqual, TokenKind::Slash},
    {TokenKind::PercentEqual, TokenKind::Percent},
};

struct TokenSpan {
    int file_id;
    int line_number;
    int column_start;
    int column_end;
};

struct Token {
    TokenKind   kind;
    TokenSpan   position;
    std::string literal;
};

inline const char* get_token_kind_literal(TokenKind kind) { return token_kind_literal[kind]; }

inline bool is_assignments_operator_token(Token token)
{
    return assignments_operators.find(token.kind) != assignments_operators.end();
}

inline bool is_unary_operator_token(Token token)
{
    return unary_operators.find(token.kind) != unary_operators.end();
}

inline bool is_float_number_token(Token token)
{
    const auto token_kind = token.kind;
    return token_kind == TokenKind::Float or token_kind == TokenKind::Float32Type or
           token_kind == TokenKind::Float64Type;
}
