#pragma once

#include "jot_token.hpp"

#include <vector>

class JotTokenizer {
public:
    JotTokenizer(std::string file, std::string source)
        : file_name(std::move(file)),
        source_code(std::move(source)),
        source_code_length(source_code.size()),
        start_position(0),
        current_position(0),
        line_number(1),
        column_start(0),
        column_current(0) {}

    std::vector<Token> scan_all_tokens();
    
    Token scan_next_token();

private:

    Token consume_symbol();

    Token consume_number();
    
    Token consume_string();
    
    Token consume_character();

    Token build_token(TokenKind);
    
    Token build_token(TokenKind, std::string);

    TokenSpan build_token_span();

    void skip_whitespaces();

    bool match(char);

    char advance();

    char peek();

    char peek_next();

    bool is_digit(char);

    bool is_alpha(char);

    bool is_alpha_num(char);

    bool is_source_available();

    std::string file_name;
    std::string source_code;
    size_t source_code_length;
    size_t start_position;
    size_t current_position;
    size_t line_number;
    size_t column_start;
    size_t column_current;
};
