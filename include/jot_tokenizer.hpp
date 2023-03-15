#pragma once

#include "jot_token.hpp"

#include <vector>

class JotTokenizer {
  public:
    JotTokenizer(int source_file_id, std::string source)
        : source_file_id(source_file_id), source_code(std::move(source)),
          source_code_length(source_code.size()), start_position(0), current_position(0),
          line_number(1), column_start(0), column_current(0)
    {
    }

    auto scan_all_tokens() -> std::vector<Token>;

    auto scan_next_token() -> Token;

    int source_file_id;

  private:
    auto consume_symbol() -> Token;

    auto consume_number() -> Token;

    auto consume_hex_number() -> Token;

    auto consume_binary_number() -> Token;

    auto consume_string() -> Token;

    auto consume_character() -> Token;

    auto consume_one_character() -> char;

    auto build_token(TokenKind) -> Token;

    auto build_token(TokenKind, std::string) -> Token;

    auto build_token_span() -> TokenSpan;

    void skip_whitespaces();

    void skip_single_line_comment();

    void skip_multi_lines_comment();

    auto match(char) -> bool;

    auto advance() -> char;

    auto peek() -> char;

    auto peek_next() -> char;

    static auto is_digit(char) -> bool;

    static auto is_hex_digit(char) -> bool;

    static auto is_binary_digit(char) -> bool;

    static auto is_alpha(char) -> bool;

    static auto is_alpha_num(char) -> bool;

    static auto is_underscore(char) -> bool;

    static auto hex_to_int(char c) -> int8_t;

    static auto hex_to_decimal(const std::string&) -> int64_t;

    static auto binary_to_decimal(const std::string&) -> int64_t;

    auto is_source_available() -> bool;

    std::string source_code;
    size_t      source_code_length;
    size_t      start_position;
    size_t      current_position;
    int         line_number;
    int         column_start;
    int         column_current;
};
