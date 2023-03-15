#include "../include/jot_tokenizer.hpp"
#include "../include/jot_logger.hpp"

#include <iostream>
#include <sstream>

auto JotTokenizer::scan_all_tokens() -> std::vector<Token>
{
    std::vector<Token> tokens;
    while (is_source_available()) {
        tokens.push_back(scan_next_token());
    }
    tokens.push_back(build_token(TokenKind::EndOfFile));
    return tokens;
}

auto JotTokenizer::scan_next_token() -> Token
{
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
    case ',': return build_token(TokenKind::Comma);
    case ';': return build_token(TokenKind::Semicolon);
    case '~': return build_token(TokenKind::Not);

    // One or Two character token
    case '.': return build_token(match('.') ? TokenKind::DotDot : TokenKind::Dot);
    case ':': return build_token(match(':') ? TokenKind::ColonColon : TokenKind::Colon);
    case '|': return build_token(match('|') ? TokenKind::LogicalOr : TokenKind::Or);
    case '&': return build_token(match('&') ? TokenKind::LogicalAnd : TokenKind::And);
    case '=': return build_token(match('=') ? TokenKind::EqualEqual : TokenKind::Equal);
    case '!': return build_token(match('=') ? TokenKind::BangEqual : TokenKind::Bang);
    case '*': return build_token(match('=') ? TokenKind::StarEqual : TokenKind::Star);
    case '/': return build_token(match('=') ? TokenKind::SlashEqual : TokenKind::Slash);
    case '%': return build_token(match('=') ? TokenKind::PercentEqual : TokenKind::Percent);

    case '+': {
        if (match('=')) {
            return build_token(TokenKind::PlusEqual);
        }

        if (match('+')) {
            return build_token(TokenKind::PlusPlus);
        }

        return build_token(TokenKind::Plus);
    }

    case '-': {
        if (match('=')) {
            return build_token(TokenKind::MinusEqual);
        }

        if (match('-')) {
            return build_token(TokenKind::MinusMinus);
        }

        if (match('>')) {
            return build_token(TokenKind::RightArrow);
        }

        return build_token(TokenKind::Minus);
    }

    case '>': {
        if (match('=')) {
            return build_token(TokenKind::GreaterEqual);
        }

        return build_token(TokenKind::Greater);
    }

    case '<': {
        if (match('=')) {
            return build_token(TokenKind::SmallerEqual);
        }

        if (match('<')) {
            return build_token(TokenKind::LeftShift);
        }

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

    case '0': {
        if (match('x')) {
            return consume_hex_number();
        }

        if (match('o')) {
            return consume_binary_number();
        }

        return consume_number();
    }

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

auto JotTokenizer::consume_symbol() -> Token
{
    while (is_alpha_num(peek()) or peek() == '_') {
        advance();
    }
    size_t len = current_position - start_position + 1;
    auto   literal = source_code.substr(start_position - 1, len);
    auto   kind = resolve_keyword_kind(literal.c_str());
    return build_token(kind, literal);
}

auto JotTokenizer::consume_number() -> Token
{
    auto kind = TokenKind::Integer;
    while (is_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        kind = TokenKind::Float;
        advance();
        while (is_digit(peek()) or is_underscore(peek())) {
            advance();
        }
    }

    auto number_end_position = current_position;

    // Signed Integers types
    if (match('i')) {
        if (match('1')) {
            kind = match('6') ? Integer16Type : Integer1Type;
        }
        else if (match('8')) {
            kind = Integer8Type;
        }
        else if (match('3') && match('2')) {
            kind = Integer32Type;
        }
        else if (match('6') && match('4')) {
            kind = Integer64Type;
        }
        else {
            return build_token(TokenKind::Invalid, "Invalid integer type");
        }
    }
    // Un Signed Integers types
    if (match('u')) {
        if (match('1') && match('6')) {
            kind = UInteger16Type;
        }
        else if (match('8')) {
            kind = UInteger8Type;
        }
        else if (match('3') && match('2')) {
            kind = UInteger32Type;
        }
        else if (match('6') && match('4')) {
            kind = UInteger64Type;
        }
        else {
            return build_token(TokenKind::Invalid, "Invalid integer type");
        }
    }
    // Floating Pointers types
    else if (match('f')) {
        if (match('3') && match('2')) {
            kind = Float32Type;
        }
        else if (match('6') && match('4')) {
            kind = Float64Type;
        }
        else {
            return build_token(TokenKind::Invalid, "Invalid Float type");
        }
    }

    size_t len = number_end_position - start_position + 1;
    auto   literal = source_code.substr(start_position - 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    return build_token(kind, literal);
}

auto JotTokenizer::consume_hex_number() -> Token
{
    while (is_hex_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    size_t len = current_position - start_position - 1;
    auto   literal = source_code.substr(start_position + 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    auto decimal_value = hex_to_decimal(literal);

    if (decimal_value == -1) {
        return build_token(TokenKind::Invalid, "Out of range hex value");
    }

    return build_token(TokenKind::Integer, std::to_string(decimal_value));
}

auto JotTokenizer::consume_binary_number() -> Token
{
    while (is_binary_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    size_t len = current_position - start_position - 1;
    auto   literal = source_code.substr(start_position + 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    auto decimal_value = binary_to_decimal(literal);

    if (decimal_value == -1) {
        return build_token(TokenKind::Invalid, "Out of range binary value");
    }

    return build_token(TokenKind::Integer, std::to_string(decimal_value));
}

auto JotTokenizer::consume_string() -> Token
{
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

auto JotTokenizer::consume_character() -> Token
{
    char c = consume_one_character();

    if (peek() != '\'') {
        return build_token(TokenKind::Invalid, "Unterminated character");
    }

    advance();

    return build_token(TokenKind::Character, std::string(1, c));
}

auto JotTokenizer::consume_one_character() -> char
{
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
            if (is_digit(first_digit) && is_digit(second_digit)) {
                c = (hex_to_int(first_digit) << 4) + hex_to_int(second_digit);
            }
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

auto JotTokenizer::build_token(TokenKind kind) -> Token { return build_token(kind, ""); }

auto JotTokenizer::build_token(TokenKind kind, std::string literal) -> Token
{
    return {kind, build_token_span(), literal};
}

auto JotTokenizer::build_token_span() -> TokenSpan
{
    return {source_file_id, line_number, column_start, column_current};
}

auto JotTokenizer::skip_whitespaces() -> void
{
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
            if (peek_next() == '/' || peek_next() == '*') {
                advance();
            }
            else {
                return;
            }
            if (match('/')) {
                skip_single_line_comment();
            }
            else if (match('*')) {
                skip_multi_lines_comment();
            }
            break;
        }
        default: return;
        }
    }
}

auto JotTokenizer::skip_single_line_comment() -> void
{
    while (is_source_available() && peek() != '\n') {
        advance();
    }
}

auto JotTokenizer::skip_multi_lines_comment() -> void
{
    while (is_source_available() && (peek() != '*' || peek_next() != '/')) {
        advance();
        if (peek() == '\n') {
            line_number++;
            column_current = 0;
        }
    }
    advance();
    advance();
}

auto JotTokenizer::match(char expected) -> bool
{
    if (!is_source_available() || expected != peek()) {
        return false;
    }
    current_position++;
    column_current++;
    return true;
}

auto JotTokenizer::advance() -> char
{
    if (is_source_available()) {
        current_position++;
        column_current++;
        return source_code[current_position - 1];
    }
    return '\0';
}

inline auto JotTokenizer::peek() -> char { return source_code[current_position]; }

auto JotTokenizer::peek_next() -> char
{
    if (current_position + 1 < source_code_length) {
        return source_code[current_position + 1];
    }
    return '\0';
}

inline auto JotTokenizer::is_digit(char c) -> bool { return '9' >= c && c >= '0'; }

inline auto JotTokenizer::is_hex_digit(char c) -> bool
{
    return is_digit(c) || ('F' >= c && c >= 'A') || ('f' >= c && c >= 'a');
}

inline auto JotTokenizer::is_binary_digit(char c) -> bool { return '1' == c || '0' == c; }

inline auto JotTokenizer::is_alpha(char c) -> bool
{
    if ('z' >= c && c >= 'a') {
        return true;
    }
    return 'Z' >= c && c >= 'A';
}

inline auto JotTokenizer::is_alpha_num(char c) -> bool { return is_alpha(c) || is_digit(c); }

inline auto JotTokenizer::is_underscore(char c) -> bool { return c == '_'; }

auto JotTokenizer::hex_to_int(char c) -> int8_t
{
    return c <= '9' ? c - '0' : c <= 'F' ? c - 'A' : c - 'a';
}

auto JotTokenizer::hex_to_decimal(const std::string& hex) -> int64_t
{
    try {
        return std::stol(hex, nullptr, 16);
    }
    catch (...) {
        return -1;
    }
}

auto JotTokenizer::binary_to_decimal(const std::string& binary) -> int64_t
{
    try {
        return std::stol(binary, nullptr, 2);
    }
    catch (...) {
        return -1;
    }
}

auto JotTokenizer::is_source_available() -> bool { return current_position < source_code_length; }
