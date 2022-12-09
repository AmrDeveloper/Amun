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
    std::vector<Token> scan_all_tokens();

    Token scan_next_token();

    int source_file_id;

  private:
    Token consume_symbol();

    Token consume_number();

    Token consume_hex_number();

    Token consume_binary_number();

    Token consume_string();

    Token consume_character();

    char consume_one_character();

    Token build_token(TokenKind);

    Token build_token(TokenKind, std::string);

    TokenSpan build_token_span();

    void skip_whitespaces();

    void skip_single_line_comment();

    void skip_multi_lines_comment();

    bool match(char);

    char advance();

    char peek();

    char peek_next();

    static bool is_digit(char);

    static bool is_hex_digit(char);

    static bool is_binary_digit(char);

    static bool is_alpha(char);

    static bool is_alpha_num(char);

    static bool is_underscore(char);

    static int8_t hex_to_int(char c);

    static int64_t hex_to_decimal(const std::string&);

    static int64_t binary_to_decimal(const std::string&);

    bool is_source_available();

    std::string source_code;
    size_t      source_code_length;
    size_t      start_position;
    size_t      current_position;
    int         line_number;
    int         column_start;
    int         column_current;
};
