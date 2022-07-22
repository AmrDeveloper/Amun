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
    FunKeyword,
    ReturnKeyword,
    IfKeyword,
    ElifKeyword,
    ElseKeyword,
    WhileKeyword,

    TrueKeyword,
    FalseKeyword,
    NullKeyword,

    Dot,
    Comma,
    Colon,
    Semicolon,

    Plus,
    Minus,
    Star,
    Slash,
    Percent,
    Address,

    Equal,
    EqualEqual,
    Bang,
    BangEqual,
    Greater,
    GreaterEqual,
    Smaller,
    SmallerEqual,

    PlusEqual,
    MinusEqual,
    StarEqual,
    SlashEqual,
    PercentEqual,

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
    Float,

    Invalid,
    EndOfFile,
};

static std::unordered_map<TokenKind, const char *> token_kind_literal = {
    {TokenKind::LoadKeyword, "Load"},
    {TokenKind::ImportKeyword, "Import"},
    {TokenKind::VarKeyword, "Var"},
    {TokenKind::TypeKeyword, "Type"},
    {TokenKind::FunKeyword, "Fun"},
    {TokenKind::EnumKeyword, "Enum"},
    {TokenKind::ReturnKeyword, "Return"},
    {TokenKind::IfKeyword, "If"},
    {TokenKind::ElifKeyword, "Else if"},
    {TokenKind::ElseKeyword, "Else"},
    {TokenKind::WhileKeyword, "While"},
    {TokenKind::TrueKeyword, "True"},
    {TokenKind::FalseKeyword, "False"},
    {TokenKind::NullKeyword, "Null"},

    {TokenKind::Dot, "Dot ."},
    {TokenKind::Comma, "Comma ,"},
    {TokenKind::Colon, "Colon :"},
    {TokenKind::Semicolon, "Semicolon ;"},

    {TokenKind::Plus, "+"},
    {TokenKind::Minus, "-"},
    {TokenKind::Star, "*"},
    {TokenKind::Slash, "/"},
    {TokenKind::Percent, "%"},
    {TokenKind::Address, "&"},

    {TokenKind::Equal, "="},
    {TokenKind::EqualEqual, "=="},
    {TokenKind::Bang, "!"},
    {TokenKind::BangEqual, "!="},

    {TokenKind::Greater, ">"},
    {TokenKind::GreaterEqual, ">="},
    {TokenKind::Smaller, "<"},
    {TokenKind::SmallerEqual, "<="},

    {TokenKind::PlusEqual, "+="},
    {TokenKind::MinusEqual, "-="},
    {TokenKind::StarEqual, "*="},
    {TokenKind::SlashEqual, "/="},
    {TokenKind::PercentEqual, "%="},

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
    {TokenKind::Float, "Float"},

    {TokenKind::Invalid, "Invalid"},
    {TokenKind::EndOfFile, "End of the file"},
};

static std::unordered_map<std::string, TokenKind> language_keywords = {
    {"load", TokenKind::LoadKeyword},     {"import", TokenKind::ImportKeyword},
    {"var", TokenKind::VarKeyword},       {"type", TokenKind::TypeKeyword},
    {"fun", TokenKind::FunKeyword},       {"enum", TokenKind::EnumKeyword},
    {"return", TokenKind::ReturnKeyword}, {"if", TokenKind::IfKeyword},
    {"elif", TokenKind::ElifKeyword},     {"else", TokenKind::ElseKeyword},
    {"while", TokenKind::WhileKeyword},   {"true", TokenKind::TrueKeyword},
    {"false", TokenKind::FalseKeyword},   {"null", TokenKind::NullKeyword},
};

static std::unordered_set<TokenKind> unary_operators{
    TokenKind::Minus,
    TokenKind::Bang,
    TokenKind::Star,
    TokenKind::Address,
};

class TokenSpan {
  public:
    TokenSpan(std::string file, size_t l, size_t cs, size_t cd)
        : file_name(std::move(file)), line_number(l), column_start(cs), column_end(cd) {}

    std::string get_file_name() { return file_name; }

    size_t get_line_number() { return line_number; }

    size_t get_column_start() { return column_start; }

    size_t get_column_end() { return column_end; }

  private:
    std::string file_name;
    size_t line_number;
    size_t column_start;
    size_t column_end;
};

class Token {
  public:
    Token(TokenKind k, TokenSpan s, std::string literal)
        : kind(k), span(s), literal(std::move(literal)) {}

    TokenKind get_kind() { return kind; }

    TokenSpan get_span() { return span; }

    std::string get_literal() { return literal; }

    std::string get_kind_literal() { return token_kind_literal[kind]; }

    bool is_invalid() { return kind == TokenKind::Invalid; }

    bool is_end_of_file() { return kind == TokenKind::EndOfFile; }

    bool is_unary_operator() { return unary_operators.find(kind) != unary_operators.end(); }

  private:
    TokenKind kind;
    TokenSpan span;
    std::string literal;
};
