#include "../include/amun_parser.hpp"

auto amun::Parser::parse_declaraions_directive() -> Shared<Statement>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `#` before directive name");
    auto posiiton = hash_token.position;

    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER)) {
        auto directive = peek_current();
        auto directive_name = directive.literal;

        if (directive_name == "intrinsic") {
            return parse_intrinsic_prototype();
        }

        if (directive_name == "extern") {
            return parse_function_prototype(amun::FunctionKind::NORMAL_FUNCTION, true);
        }

        if (directive_name == "prefix") {
            auto token = peek_and_advance_token();
            auto call_kind = amun::FunctionKind::PREFIX_FUNCTION;
            if (is_current_kind(TokenKind::TOKEN_FUN)) {
                return parse_function_declaration(call_kind);
            }

            if (is_current_kind(TokenKind::TOKEN_OPERATOR)) {
                return parse_operator_function_declaraion(call_kind);
            }

            context->diagnostics.report_error(
                token.position, "prefix keyword used only for function and operators");
            throw "Stop";
        }

        if (directive_name == "infix") {
            auto token = peek_and_advance_token();
            auto call_kind = amun::FunctionKind::INFIX_FUNCTION;
            if (is_current_kind(TokenKind::TOKEN_FUN)) {
                return parse_function_declaration(call_kind);
            }

            // Operator functions are infix by default but also it not a syntax error
            if (is_current_kind(TokenKind::TOKEN_OPERATOR)) {
                return parse_operator_function_declaraion(call_kind);
            }

            context->diagnostics.report_error(token.position,
                                              "infix keyword used only for function and operators");
            throw "Stop";
        }

        if (directive_name == "postfix") {
            auto token = peek_and_advance_token();
            auto call_kind = amun::FunctionKind::POSTFIX_FUNCTION;
            if (is_current_kind(TokenKind::TOKEN_FUN)) {
                return parse_function_declaration(call_kind);
            }

            if (is_current_kind(TokenKind::TOKEN_OPERATOR)) {
                return parse_operator_function_declaraion(call_kind);
            }

            context->diagnostics.report_error(
                token.position, "postfix keyword used only for function and operators");
            throw "Stop";
        }

        if (directive_name == "packed") {
            advanced_token();
            return parse_structure_declaration(true);
        }

        context->diagnostics.report_error(posiiton,
                                          "No declaraions directive with name " + directive_name);
        throw "Stop";
    }

    context->diagnostics.report_error(posiiton, "Expect identifier as directive name");
    throw "Stop";
}

auto amun::Parser::parse_statements_directive() -> Shared<Statement>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `#` before directive name");
    auto directive = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect symbol as directive name");
    auto directive_name = directive.literal;
    auto posiiton = directive.position;

    context->diagnostics.report_error(posiiton,
                                      "No statement directive with name " + directive_name);
    throw "Stop";
}

auto amun::Parser::parse_expressions_directive() -> Shared<Expression>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `#` before directive name");
    auto directive = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect symbol as directive name");
    auto directive_name = directive.literal;
    auto posiiton = directive.position;

    if (directive_name == "line") {
        auto current_line = posiiton.line_number;
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_INT64;
        directive_token.position = posiiton;
        directive_token.literal = std::to_string(current_line);
        return std::make_shared<NumberExpression>(directive_token, amun::i64_type);
    }

    if (directive_name == "column") {
        auto current_column = posiiton.column_start;
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_INT64;
        directive_token.position = posiiton;
        directive_token.literal = std::to_string(current_column);
        return std::make_shared<NumberExpression>(directive_token, amun::i64_type);
    }

    if (directive_name == "filepath") {
        auto current_filepath = context->source_manager.resolve_source_path(posiiton.file_id);
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_STRING;
        directive_token.position = posiiton;
        directive_token.literal = current_filepath;
        return std::make_shared<StringExpression>(directive_token);
    }

    context->diagnostics.report_error(posiiton,
                                      "No expression directive with name " + directive_name);
    throw "Stop";
}