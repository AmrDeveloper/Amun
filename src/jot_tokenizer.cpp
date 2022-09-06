#include "../include/jot_tokenizer.hpp"
#include "../include/jot_logger.hpp"

#include <iostream>
#include <sstream>

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
    // One character token
    case '(': return build_token(TokenKind::OpenParen);
    case ')': return build_token(TokenKind::CloseParen);
    case '[': return build_token(TokenKind::OpenBracket);
    case ']': return build_token(TokenKind::CloseBracket);
    case '{': return build_token(TokenKind::OpenBrace);
    case '}': return build_token(TokenKind::CloseBrace);
    case '.': return build_token(TokenKind::Dot);
    case ',': return build_token(TokenKind::Comma);
    case ';': return build_token(TokenKind::Semicolon);
    case '~': return build_token(TokenKind::Not);

    // One or Two character token
    case ':': return build_token(match(':') ? TokenKind::ColonColon : TokenKind::Colon);
    case '|': return build_token(match('|') ? TokenKind::LogicalOr : TokenKind::Or);
    case '&': return build_token(match('&') ? TokenKind::LogicalAnd : TokenKind::And);
    case '=': return build_token(match('=') ? TokenKind::EqualEqual : TokenKind::Equal);
    case '!': return build_token(match('=') ? TokenKind::BangEqual : TokenKind::Bang);
    case '+': return build_token(match('=') ? TokenKind::PlusEqual : TokenKind::Plus);
    case '-': return build_token(match('=') ? TokenKind::MinusEqual : TokenKind::Minus);
    case '*': return build_token(match('=') ? TokenKind::StarEqual : TokenKind::Star);
    case '/': return build_token(match('=') ? TokenKind::SlashEqual : TokenKind::Slash);
    case '%': return build_token(match('=') ? TokenKind::PercentEqual : TokenKind::Percent);

    case '>': {
        if (match('='))
            return build_token(TokenKind::GreaterEqual);

        if (match('>'))
            return build_token(TokenKind::RightShift);

        return build_token(TokenKind::Greater);
    }

    case '<': {
        if (match('='))
            return build_token(TokenKind::SmallerEqual);

        if (match('<'))
            return build_token(TokenKind::LeftShift);

        return build_token(TokenKind::Smaller);
    }

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
    while (is_alpha_num(peek()) or peek() == '_') {
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
        while (is_digit(peek()))
            advance();
    }

    auto number_end_position = current_position;

    if (match('i')) {
        if (match('1'))
            kind = match('6') ? Integer16Type : Integer1Type;
        else if (match('8'))
            kind = Integer8Type;
        else if (match('3') && match('2'))
            kind = Integer32Type;
        else if (match('6') && match('4'))
            kind = Integer64Type;
        else
            return build_token(TokenKind::Invalid, "Invalid integer type");
    } else if (match('f')) {
        if (match('3') && match('2'))
            kind = Float32Type;
        else if (match('6') && match('4'))
            kind = Float64Type;
        else
            return build_token(TokenKind::Invalid, "Invalid Float type");
    }

    size_t len = number_end_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    return build_token(kind, literal);
}

Token JotTokenizer::consume_string() {
    std::stringstream stream;
    while (is_source_available() && peek() != '"') {
        stream << consume_one_character();
    }

    if (not is_source_available()) {
        return build_token(TokenKind::Invalid, "Unterminated string");
    }

    advance();
    return build_token(TokenKind::String, stream.str());
}

Token JotTokenizer::consume_character() {
    char c = consume_one_character();

    if (peek() != '\'') {
        return build_token(TokenKind::Invalid, "Unterminated character");
    }

    advance();

    return build_token(TokenKind::Character, std::string(1, c));
}

char JotTokenizer::consume_one_character() {
    char c = advance();
    if (c == '\\') {
        char escape = peek();
        switch (escape) {
        case 'a': {
            advance();
            c = '\a';
            break;
        }
        case 'b': {
            advance();
            c = '\b';
            break;
        }
        case 'f': {
            advance();
            c = '\f';
            break;
        }
        case 'n': {
            advance();
            c = '\n';
            break;
        }
        case 'r': {
            advance();
            c = '\r';
            break;
        }
        case 't': {
            advance();
            c = '\t';
            break;
        }
        case 'v': {
            advance();
            c = '\v';
            break;
        }
        case '0': {
            advance();
            c = '\0';
            break;
        }
        case '\'': {
            advance();
            c = '\'';
            break;
        }
        case '\\': {
            advance();
            c = '\\';
            break;
        }
        case '"': {
            advance();
            c = '"';
            break;
        }
        case 'x': {
            advance();
            char first_digit = advance();
            char second_digit = advance();
            if (is_digit(first_digit) && is_digit(second_digit))
                c = (hex_to_int(first_digit) << 4) + hex_to_int(second_digit);
            else {
                jot::loge << "escaped hex 2 character must be integers\n";
                return -1;
            }
            break;
        }
        default: {
            jot::loge << "Not escaped character\n";
            return -1;
        }
        }
    }
    return c;
}

Token JotTokenizer::build_token(TokenKind kind) { return build_token(kind, ""); }

Token JotTokenizer::build_token(TokenKind kind, std::string literal) {
    TokenSpan span = build_token_span();
    return {kind, span, literal};
}

TokenSpan JotTokenizer::build_token_span() {
    return {file_name, line_number, column_start, column_current};
}

void JotTokenizer::skip_whitespaces() {
    while (is_source_available()) {
        char c = peek();
        switch (c) {
        case ' ':
        case '\r':
        case '\t': advance(); break;
        case '\n':
            line_number++;
            advance();
            column_current = 0;
            break;
        case '/': {
            if (peek_next() == '/' || peek_next() == '*')
                advance();
            else
                return;
            if (match('/'))
                skip_single_line_comment();
            else if (match('*'))
                skip_multi_lines_comment();
            break;
        }
        default: return;
        }
    }
}

void JotTokenizer::skip_single_line_comment() {
    while (is_source_available() && peek() != '\n') {
        advance();
    }
}

void JotTokenizer::skip_multi_lines_comment() {
    while (is_source_available() && !(peek() == '*' && peek_next() == '/')) {
        advance();
        if (peek() == '\n') {
            line_number++;
            column_current = 0;
        }
    }
    advance();
    advance();
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

char JotTokenizer::peek() { return source_code[current_position]; }

char JotTokenizer::peek_next() {
    if (current_position + 1 < source_code_length) {
        return source_code[current_position + 1];
    }
    return '\0';
}

bool JotTokenizer::is_digit(char c) { return '9' >= c && c >= '0'; }

bool JotTokenizer::is_alpha(char c) {
    if ('z' >= c && c >= 'a')
        return true;
    return 'Z' >= c && c >= 'A';
}

bool JotTokenizer::is_alpha_num(char c) { return is_alpha(c) || is_digit(c); }

int8_t JotTokenizer::hex_to_int(char c) {
    return c <= '9' ? c - '0' : c <= 'F' ? c - 'A' : c - 'a';
}

bool JotTokenizer::is_source_available() { return current_position < source_code_length; }
