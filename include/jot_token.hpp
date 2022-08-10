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
    ExternKeyword,
    IfKeyword,
    ElseKeyword,
    WhileKeyword,

    PrefixKeyword,
    InfixKeyword,

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
    {TokenKind::ExternKeyword, "Extern"},
    {TokenKind::IfKeyword, "If"},
    {TokenKind::ElseKeyword, "Else"},
    {TokenKind::WhileKeyword, "While"},
    {TokenKind::TrueKeyword, "True"},
    {TokenKind::FalseKeyword, "False"},
    {TokenKind::NullKeyword, "Null"},

    {TokenKind::PrefixKeyword, "Prefix"},
    {TokenKind::InfixKeyword, "Infix"},

    {TokenKind::Dot, "Dot ."},
    {TokenKind::Comma, "Comma ,"},
    {TokenKind::Colon, "Colon :"},
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
    {"return", TokenKind::ReturnKeyword}, {"extern", TokenKind::ExternKeyword},
    {"if", TokenKind::IfKeyword},         {"else", TokenKind::ElseKeyword},
    {"while", TokenKind::WhileKeyword},   {"true", TokenKind::TrueKeyword},
    {"false", TokenKind::FalseKeyword},   {"null", TokenKind::NullKeyword},
    {"prefix", TokenKind::PrefixKeyword}, {"infix", TokenKind::InfixKeyword}};

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

    void set_kind(TokenKind new_kind) { kind = new_kind; }

    TokenSpan get_span() { return span; }

    std::string get_literal() { return literal; }

    void set_literal(std::string new_literal) { literal = std::move(new_literal); }

    std::string get_kind_literal() { return token_kind_literal[kind]; }

    bool is_invalid() { return kind == TokenKind::Invalid; }

    bool is_end_of_file() { return kind == TokenKind::EndOfFile; }

    bool is_unary_operator() { return unary_operators.find(kind) != unary_operators.end(); }

    bool is_assignments_operator() {
        return assignments_operators.find(kind) != assignments_operators.end();
    }

  private:
    TokenKind kind;
    TokenSpan span;
    std::string literal;
};
