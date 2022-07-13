#include "../include/jot_tokenizer.hpp"
#include "../include/jot_token.hpp"

#include <iostream>

std::vector<Token> JotTokenizer::scan_all_tokens() {
    std::vector<Token> tokens;
    while (is_source_available()) {
       tokens.push_back(scan_next_token());
    }
    tokens.push_back(build_token(TokenKind::EndOfFile));
    return tokens;
}

Token JotTokenizer::scan_next_token() {
    skip_whitespaces();

    start_position = current_position;
    column_start = column_current;

    char c = advance();

    start_position++;
    current_position = start_position;

    column_start++;
    column_current = column_start;

    switch (c) {
    case '(': return build_token(TokenKind::OpenParen); 
    case ')': return build_token(TokenKind::CloseParen); 
    case '[': return build_token(TokenKind::OpenBracket); 
    case ']': return build_token(TokenKind::CloseBracket); 
    case '{': return build_token(TokenKind::OpenBrace); 
    case '}': return build_token(TokenKind::CloseBrace);
    case '.': return build_token(TokenKind::Dot); 
    case ',': return build_token(TokenKind::Comma); 
    case ':': return build_token(TokenKind::Colon); 
    case ';': return build_token(TokenKind::Semicolon); 
    case '&': return build_token(TokenKind::Address);

    case '=': return build_token(match('=') ? TokenKind::EqualEqual : TokenKind::Equal); 
    case '!': return build_token(match('=') ? TokenKind::BangEqual : TokenKind::Bang); 
    case '>': return build_token(match('=') ? TokenKind::GreaterEqual : TokenKind::Greater); 
    case '<': return build_token(match('=') ? TokenKind::SmallerEqual : TokenKind::Smaller); 
    case '+': return build_token(match('=') ? TokenKind::PlusEqual : TokenKind::Plus); 
    case '-': return build_token(match('=') ? TokenKind::MinusEqual : TokenKind::Minus); 
    case '*': return build_token(match('=') ? TokenKind::StarEqual : TokenKind::Star); 
    case '/': return build_token(match('=') ? TokenKind::SlashEqual : TokenKind::Slash); 
    case '%': return build_token(match('=') ? TokenKind::PercentEqual : TokenKind::Percent); 
    
    case 'A':
    case 'B':
    case 'C':
    case 'D':
    case 'E':
    case 'F':
    case 'G':
    case 'H':
    case 'I':
    case 'J':
    case 'K':
    case 'L':
    case 'M':
    case 'N':
    case 'O':
    case 'P':
    case 'Q':
    case 'R':
    case 'S':
    case 'T':
    case 'U':
    case 'V':
    case 'W':
    case 'X':
    case 'Y':
    case 'Z':
    case 'a':
    case 'b':
    case 'c':
    case 'd':
    case 'e':
    case 'f':
    case 'g':
    case 'h':
    case 'i':
    case 'j':
    case 'k':
    case 'l':
    case 'm':
    case 'n':
    case 'o':
    case 'p':
    case 'q':
    case 'r':
    case 's':
    case 't':
    case 'u':
    case 'v':
    case 'w':
    case 'x':
    case 'y':
    case 'z':
    case '_': return consume_symbol();

    case '"': return consume_string();

    case '\'': return consume_character();

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': return consume_number(); 

    case '\0': return build_token(TokenKind::EndOfFile);
    default: return build_token(TokenKind::Invalid, "Unexpected character");
    }
}

Token JotTokenizer::consume_symbol() {
    while (is_alpha_num(peek())) {
        advance();
    }
    size_t len = current_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    if (language_keywords.find(literal) == language_keywords.end()) {
        return build_token(TokenKind::Symbol, literal);
    }
    auto kind = language_keywords[literal];
    return build_token(kind, literal);

}

Token JotTokenizer::consume_number() {
    auto kind = TokenKind::Integer;
    while (is_digit(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        kind = TokenKind::Float;
        
        advance();

        while (is_digit(peek())) {
            advance();
        }
    }

    size_t len = current_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    return build_token(kind, literal);
}

Token JotTokenizer::consume_string() {
    while (is_source_available() && peek() != '"') {
        if (peek() == '\n') line_number++;
        advance();
    }

    if (!is_source_available()) {
        return build_token(TokenKind::Invalid, "Unterminated string");
    }

    advance();

    size_t len = current_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    return build_token(TokenKind::String, literal);
}

Token JotTokenizer::consume_character() {
    advance();
    char c = peek();
    advance();

    if (peek() != '\'') {
        return build_token(TokenKind::Invalid, "Unterminated character");
    }
    
    advance();

    return build_token(TokenKind::Character, std::string(1, c));
}

Token JotTokenizer::build_token(TokenKind kind) {
    return build_token(kind, "");
}

Token JotTokenizer::build_token(TokenKind kind, std::string literal) {
    TokenSpan span = build_token_span();
    return {kind, span, literal};
}

TokenSpan JotTokenizer::build_token_span() {
    return {file_name, line_number, column_start, column_current};
}

void JotTokenizer::skip_whitespaces() {
    while (true) {
        char c = peek();
        switch(c) {
        case ' ':
        case '\r':
        case '\t':
            advance();
            break;
        case '\n':
            line_number++;
            advance();
            column_current = 0;
            break;
        default:
            return;
        }
    }
}

bool JotTokenizer::match(char expected) {
    if (!is_source_available() || expected != peek()) {
        return false;
    }
    current_position++;
    column_current++;
    return true;
}

char JotTokenizer::advance() {
    if (is_source_available()) {
        current_position++;
        column_current++;
        return source_code[current_position - 1];
    }
    return '\0';
}

char JotTokenizer::peek() {
    return source_code[current_position];
}

char JotTokenizer::peek_next() {
    if (current_position + 1 < source_code_length) {
       return source_code[current_position + 1];
    }
    return '\0';
}

bool JotTokenizer::is_digit(char c) {
    return '9' >= c && c >= '0';
}

bool JotTokenizer::is_alpha(char c) {
    if ('z' >= c && c >= 'a') return true;
    return 'Z' >= c && c >= 'A';
}

bool JotTokenizer::is_alpha_num(char c) {
    return is_alpha(c) || is_digit(c);
}

bool JotTokenizer::is_source_available() {
    return current_position < source_code_length;
}
