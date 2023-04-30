#include "../include/amun_tokenizer.hpp"
#include "../include/amun_logger.hpp"

#include <iostream>
#include <sstream>

auto amun::Tokenizer::scan_all_tokens() -> std::vector<Token>
{
    std::vector<Token> tokens;
    while (is_source_available()) {
        tokens.push_back(scan_next_token());
    }
    tokens.push_back(build_token(TokenKind::TOKEN_END_OF_FILE));
    return tokens;
}

auto amun::Tokenizer::scan_next_token() -> Token
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
    case '(': return build_token(TokenKind::TOKEN_OPEN_PAREN);
    case ')': return build_token(TokenKind::TOKEN_CLOSE_PAREN);
    case '[': return build_token(TokenKind::TOKEN_OPEN_BRACKET);
    case ']': return build_token(TokenKind::TOKEN_CLOSE_BRACKET);
    case '{': return build_token(TokenKind::TOKEN_OPEN_BRACE);
    case '}': return build_token(TokenKind::TOKEN_CLOSE_BRACE);
    case ',': return build_token(TokenKind::TOKEN_COMMA);
    case ';': return build_token(TokenKind::TOKEN_SEMICOLON);
    case '~': return build_token(TokenKind::TOKEN_NOT);

    // One or Two character token
    case '.': {
        return build_token(match('.') ? TokenKind::TOKEN_DOT_DOT : TokenKind::TOKEN_DOT);
    }
    case ':': {
        return build_token(match(':') ? TokenKind::TOKEN_COLON_COLON : TokenKind::TOKEN_COLON);
    }
    case '|': {
        return build_token(match('|') ? TokenKind::TOKEN_OR_OR : TokenKind::TOKEN_OR);
    }
    case '&': {
        return build_token(match('&') ? TokenKind::TOKEN_AND_AND : TokenKind::TOKEN_AND);
    }
    case '=': {
        return build_token(match('=') ? TokenKind::TOKEN_EQUAL_EQUAL : TokenKind::TOKEN_EQUAL);
    }
    case '!': {
        return build_token(match('=') ? TokenKind::TOKEN_BANG_EQUAL : TokenKind::TOKEN_BANG);
    }
    case '*': {
        return build_token(match('=') ? TokenKind::TOKEN_STAR_EQUAL : TokenKind::TOKEN_STAR);
    }
    case '/': {
        return build_token(match('=') ? TokenKind::TOKEN_SLASH_EQUAL : TokenKind::TOKEN_SLASH);
    }
    case '%': {
        return build_token(match('=') ? TokenKind::TOKEN_PARCENT_EQUAL : TokenKind::TOKEN_PERCENT);
    }
    case '@': {
        return build_token(TokenKind::TOKEN_AT);
    }

    case '+': {
        if (match('=')) {
            return build_token(TokenKind::TOKEN_PLUS_EQUAL);
        }

        if (match('+')) {
            return build_token(TokenKind::TOKEN_PLUS_PLUS);
        }

        return build_token(TokenKind::TOKEN_PLUS);
    }

    case '-': {
        if (match('=')) {
            return build_token(TokenKind::TOKEN_MINUS_EQUAL);
        }

        if (match('-')) {
            if (match('-')) {
                return build_token(TokenKind::TOKEN_UNDEFINED);
            }
            return build_token(TokenKind::TOKEN_MINUS_MINUS);
        }

        if (match('>')) {
            return build_token(TokenKind::TOKEN_RIGHT_ARROW);
        }

        return build_token(TokenKind::TOKEN_MINUS);
    }

    case '>': {
        if (match('=')) {
            return build_token(TokenKind::TOKEN_GREATER_EQUAL);
        }

        return build_token(TokenKind::TOKEN_GREATER);
    }

    case '<': {
        if (match('=')) {
            return build_token(TokenKind::TOKEN_SMALLER_EQUAL);
        }

        if (match('<')) {
            return build_token(TokenKind::TOKEN_LEFT_SHIFT);
        }

        return build_token(TokenKind::TOKEN_SMALLER);
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

    case '\0': return build_token(TokenKind::TOKEN_END_OF_FILE);
    default: return build_token(TokenKind::TOKEN_INVALID, "unexpected character");
    }
}

auto amun::Tokenizer::consume_symbol() -> Token
{
    while (is_alpha_num(peek()) or peek() == '_') {
        advance();
    }
    size_t len = current_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    auto kind = resolve_keyword_token_kind(literal.c_str());
    return build_token(kind, literal);
}

auto amun::Tokenizer::consume_number() -> Token
{
    auto kind = TokenKind::TOKEN_INT;
    while (is_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    if (peek() == '.' && is_digit(peek_next())) {
        kind = TokenKind::TOKEN_FLOAT;
        advance();
        while (is_digit(peek()) or is_underscore(peek())) {
            advance();
        }
    }

    auto number_end_position = current_position;

    // Signed Integers types
    if (match('i')) {
        if (match('1')) {
            kind = match('6') ? TokenKind::TOKEN_INT16 : TokenKind::TOKEN_INT1;
        }
        else if (match('8')) {
            kind = TokenKind::TOKEN_INT8;
        }
        else if (match('3') && match('2')) {
            kind = TokenKind::TOKEN_INT32;
        }
        else if (match('6') && match('4')) {
            kind = TokenKind::TOKEN_INT64;
        }
        else {
            return build_token(TokenKind::TOKEN_INVALID,
                               "invalid width for singed integer literal, expect 8, 16, 32 or 64");
        }
    }
    // Un Signed Integers types
    if (match('u')) {
        if (match('1') && match('6')) {
            kind = TokenKind::TOKEN_UINT16;
        }
        else if (match('8')) {
            kind = TokenKind::TOKEN_UINT8;
        }
        else if (match('3') && match('2')) {
            kind = TokenKind::TOKEN_UINT32;
        }
        else if (match('6') && match('4')) {
            kind = TokenKind::TOKEN_UINT64;
        }
        else {
            return build_token(
                TokenKind::TOKEN_INVALID,
                "invalid width for unsinged integer literal, expect 8, 16, 32 or 64");
        }
    }
    // Floating Pointers types
    else if (match('f')) {
        if (match('3') && match('2')) {
            kind = TOKEN_FLOAT32;
        }
        else if (match('6') && match('4')) {
            kind = TOKEN_FLOAT64;
        }
        else {
            return build_token(TokenKind::TOKEN_INVALID,
                               "invalid width for floating point literal, expect 32 or 64");
        }
    }
    else if (is_alpha(peek())) {
        return build_token(TokenKind::TOKEN_INVALID,
                           "invalid suffix for number literal, expect i, u or f");
    }

    size_t len = number_end_position - start_position + 1;
    auto literal = source_code.substr(start_position - 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    return build_token(kind, literal);
}

auto amun::Tokenizer::consume_hex_number() -> Token
{
    while (is_hex_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    size_t len = current_position - start_position - 1;
    auto literal = source_code.substr(start_position + 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    auto decimal_value = hex_to_decimal(literal);

    if (decimal_value == -1) {
        return build_token(TokenKind::TOKEN_INVALID, "hex integer literal is too large");
    }

    return build_token(TokenKind::TOKEN_INT, std::to_string(decimal_value));
}

auto amun::Tokenizer::consume_binary_number() -> Token
{
    while (is_binary_digit(peek()) or is_underscore(peek())) {
        advance();
    }

    size_t len = current_position - start_position - 1;
    auto literal = source_code.substr(start_position + 1, len);
    literal.erase(std::remove(literal.begin(), literal.end(), '_'), literal.end());
    auto decimal_value = binary_to_decimal(literal);

    if (decimal_value == -1) {
        return build_token(TokenKind::TOKEN_INVALID, "binary integer literal is too large");
    }

    return build_token(TokenKind::TOKEN_INT, std::to_string(decimal_value));
}

auto amun::Tokenizer::consume_string() -> Token
{
    std::stringstream stream;
    while (is_source_available() && peek() != '"') {
        char c = consume_one_character();
        if (c == -1) {
            return build_token(TokenKind::TOKEN_INVALID, "invalid character");
        }

        stream << c;
    }

    if (not is_source_available()) {
        return build_token(TokenKind::TOKEN_INVALID, "unterminated double quote string");
    }

    advance();
    return build_token(TokenKind::TOKEN_STRING, stream.str());
}

auto amun::Tokenizer::consume_character() -> Token
{
    char c = consume_one_character();
    if (c == -1) {
        return build_token(TokenKind::TOKEN_INVALID, "invalid character");
    }

    if (peek() != '\'') {
        return build_token(TokenKind::TOKEN_INVALID, "unterminated single quote character");
    }

    advance();

    return build_token(TokenKind::TOKEN_CHARACTER, std::string(1, c));
}

auto amun::Tokenizer::consume_one_character() -> char
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
                return -1;
            }
            break;
        }
        default: {
            return -1;
        }
        }
    }
    return c;
}

auto amun::Tokenizer::build_token(TokenKind kind) -> Token { return build_token(kind, ""); }

auto amun::Tokenizer::build_token(TokenKind kind, std::string literal) -> Token
{
    return {kind, build_token_span(), literal};
}

auto amun::Tokenizer::build_token_span() -> TokenSpan
{
    return {source_file_id, line_number, column_start, column_current};
}

auto amun::Tokenizer::skip_whitespaces() -> void
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

auto amun::Tokenizer::skip_single_line_comment() -> void
{
    while (is_source_available() && peek() != '\n') {
        advance();
    }
}

auto amun::Tokenizer::skip_multi_lines_comment() -> void
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

    // If multi line comments end with new line update the line number
    if (peek() == '\n') {
        line_number++;
        column_current = 0;
    }
}

auto amun::Tokenizer::match(char expected) -> bool
{
    if (!is_source_available() || expected != peek()) {
        return false;
    }
    current_position++;
    column_current++;
    return true;
}

auto amun::Tokenizer::advance() -> char
{
    if (is_source_available()) {
        current_position++;
        column_current++;
        return source_code[current_position - 1];
    }
    return '\0';
}

inline auto amun::Tokenizer::peek() -> char { return source_code[current_position]; }

auto amun::Tokenizer::peek_next() -> char
{
    if (current_position + 1 < source_code_length) {
        return source_code[current_position + 1];
    }
    return '\0';
}

inline auto amun::Tokenizer::is_digit(char c) -> bool { return '9' >= c && c >= '0'; }

inline auto amun::Tokenizer::is_hex_digit(char c) -> bool
{
    return is_digit(c) || ('F' >= c && c >= 'A') || ('f' >= c && c >= 'a');
}

inline auto amun::Tokenizer::is_binary_digit(char c) -> bool { return '1' == c || '0' == c; }

inline auto amun::Tokenizer::is_alpha(char c) -> bool
{
    if ('z' >= c && c >= 'a') {
        return true;
    }
    return 'Z' >= c && c >= 'A';
}

inline auto amun::Tokenizer::is_alpha_num(char c) -> bool { return is_alpha(c) || is_digit(c); }

inline auto amun::Tokenizer::is_underscore(char c) -> bool { return c == '_'; }

auto amun::Tokenizer::hex_to_int(char c) -> int8_t
{
    return c <= '9' ? c - '0' : c <= 'F' ? c - 'A' : c - 'a';
}

auto amun::Tokenizer::hex_to_decimal(const std::string& hex) -> int64_t
{
    try {
        return std::stol(hex, nullptr, 16);
    }
    catch (...) {
        return -1;
    }
}

auto amun::Tokenizer::binary_to_decimal(const std::string& binary) -> int64_t
{
    try {
        return std::stol(binary, nullptr, 2);
    }
    catch (...) {
        return -1;
    }
}

auto amun::Tokenizer::resolve_keyword_token_kind(const char* keyword) -> TokenKind
{
    switch (strlen(keyword)) {
    case 2: {
        if (str2Equals("if", keyword)) {
            return TokenKind::TOKEN_IF;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 3: {
        if (str3Equals("fun", keyword)) {
            return TokenKind::TOKEN_FUN;
        }
        if (str3Equals("var", keyword)) {
            return TokenKind::TOKEN_VAR;
        }
        if (str3Equals("for", keyword)) {
            return TokenKind::TOKEN_FOR;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 4: {
        if (str4Equals("load", keyword)) {
            return TokenKind::TOKEN_LOAD;
        }
        if (str4Equals("null", keyword)) {
            return TokenKind::TOKEN_NULL;
        }
        if (str4Equals("true", keyword)) {
            return TokenKind::TOKEN_TRUE;
        }
        if (str4Equals("cast", keyword)) {
            return TokenKind::TOKEN_CAST;
        }
        if (str4Equals("else", keyword)) {
            return TokenKind::TOKEN_ELSE;
        }
        if (str4Equals("enum", keyword)) {
            return TokenKind::TOKEN_ENUM;
        }
        if (str4Equals("type", keyword)) {
            return TokenKind::TOKEN_TYPE;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 5: {
        if (str5Equals("while", keyword)) {
            return TokenKind::TOKEN_WHILE;
        }
        if (str5Equals("defer", keyword)) {
            return TokenKind::TOKEN_DEFER;
        }
        if (str5Equals("false", keyword)) {
            return TokenKind::TOKEN_FALSE;
        }
        if (str5Equals("break", keyword)) {
            return TokenKind::TOKEN_BREAK;
        }
        if (str5Equals("const", keyword)) {
            return TokenKind::TOKEN_CONST;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 6: {
        if (str6Equals("import", keyword)) {
            return TokenKind::TOKEN_IMPORT;
        }
        if (str6Equals("struct", keyword)) {
            return TokenKind::TOKEN_STRUCT;
        }
        if (str6Equals("return", keyword)) {
            return TokenKind::TOKEN_RETURN;
        }
        if (str6Equals("switch", keyword)) {
            return TokenKind::TOKEN_SWITCH;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 7: {
        if (str7Equals("varargs", keyword)) {
            return TokenKind::TOKEN_VARARGS;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 8: {
        if (str8Equals("continue", keyword)) {
            return TokenKind::TOKEN_CONTINUE;
        }
        if (str8Equals("operator", keyword)) {
            return TokenKind::TOKEN_OPERATOR;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 9: {
        if (str9Equals("type_size", keyword)) {
            return TokenKind::TOKEN_TYPE_SIZE;
        }
        if (str9Equals("undefined", keyword)) {
            return TokenKind::TOKEN_UNDEFINED;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    case 10: {
        if (str10Equals("value_size", keyword)) {
            return TokenKind::TOKEN_VALUE_SIZE;
        }
        return TokenKind::TOKEN_IDENTIFIER;
    }
    default: {
        return TokenKind::TOKEN_IDENTIFIER;
    }
    }
}

auto amun::Tokenizer::is_source_available() -> bool
{
    return current_position < source_code_length;
}
