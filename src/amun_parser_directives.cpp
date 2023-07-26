#include "../include/amun_parser.hpp"

auto amun::Parser::parse_declaraions_directive() -> Shared<Statement>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `@` before directive name");
    auto posiiton = hash_token.position;

    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER)) {
        auto directive = peek_current();
        auto directive_name = directive.literal;

        if (directive_name == "intrinsic") {
            return parse_intrinsic_prototype();
        }

        if (directive_name == "extern") {
            // Parse opaque struct
            if (is_next_kind(TokenKind::TOKEN_STRUCT)) {
                advanced_token();
                return parse_structure_declaration(false, true);
            }

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
            return parse_structure_declaration(true, false);
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
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `@` before directive name");
    auto directive = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect symbol as directive name");
    auto directive_name = directive.literal;
    auto posiiton = directive.position;

    if (directive_name == "complete") {
        auto statement = parse_statement();
        if (statement->get_ast_node_type() != AstNodeType::AST_SWITCH_STATEMENT) {
            context->diagnostics.report_error(posiiton, "@complete expect switch statement");
            throw "Stop";
        }

        auto switch_node = std::dynamic_pointer_cast<SwitchStatement>(statement);
        switch_node->should_perform_complete_check = true;
        return switch_node;
    }

    context->diagnostics.report_error(posiiton,
                                      "No statement directive with name " + directive_name);
    throw "Stop";
}

auto amun::Parser::parse_expressions_directive() -> Shared<Expression>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `@` before directive name");
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

    if (directive_name == "vec") {
        auto expression = parse_expression();
        if (expression->get_ast_node_type() != AstNodeType::AST_ARRAY) {
            context->diagnostics.report_error(posiiton, "Expect Array expression after @vec");
            throw "Stop";
        }

        auto array = std::dynamic_pointer_cast<ArrayExpression>(expression);
        return std::make_shared<VectorExpression>(array);
    }

    if (directive_name == "max_value") {
        assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` after @max_value");
        auto type = parse_type();
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after @max_value type");

        if (type->type_kind != TypeKind::NUMBER) {
            context->diagnostics.report_error(posiiton, "@max_value expect only number types");
            throw "Stop";
        }

        auto number_type = std::static_pointer_cast<amun::NumberType>(type);

        Token max_value;
        max_value.kind = number_kind_token_kind[number_type->number_kind];
        max_value.position = posiiton;

        if (is_integer_type(number_type)) {
            max_value.literal = std::to_string(integers_kind_max_value[number_type->number_kind]);
        }
        else {
            if (number_type->number_kind == NumberKind::FLOAT_32) {
                max_value.literal = std::to_string(std::numeric_limits<float32>::max());
            }
            else {
                max_value.literal = std::to_string(std::numeric_limits<float64>::max());
            }
        }

        return std::make_shared<NumberExpression>(max_value, number_type);
    }

    if (directive_name == "min_value") {
        assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` after @min_value");
        auto type = parse_type();
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after @min_value type");

        if (type->type_kind != TypeKind::NUMBER) {
            context->diagnostics.report_error(posiiton, "@min_value expect only number types");
            throw "Stop";
        }

        auto number_type = std::static_pointer_cast<amun::NumberType>(type);

        Token min_value;
        min_value.kind = number_kind_token_kind[number_type->number_kind];
        min_value.position = posiiton;

        if (is_integer_type(number_type)) {
            min_value.literal = std::to_string(integers_kind_min_value[number_type->number_kind]);
        }
        else {
            if (number_type->number_kind == NumberKind::FLOAT_32) {
                min_value.literal = std::to_string(std::numeric_limits<float32>::lowest());
            }
            else {
                min_value.literal = std::to_string(std::numeric_limits<float64>::lowest());
            }
        }

        return std::make_shared<NumberExpression>(min_value, number_type);
    }

    context->diagnostics.report_error(posiiton,
                                      "No expression directive with name " + directive_name);
    throw "Stop";
}

auto amun::Parser::pares_types_directive() -> Shared<amun::Type>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_AT, "Expect `@` before directive name");
    auto posiiton = hash_token.position;

    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER)) {
        auto directive = peek_current();
        auto directive_name = directive.literal;

        if (directive_name == "vec") {
            advanced_token();
            auto type = parse_type();
            if (type->type_kind != TypeKind::STATIC_ARRAY) {
                context->diagnostics.report_error(posiiton, "Expect array type after @vec");
                throw "Stop";
            }

            auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
            return std::make_shared<amun::StaticVectorType>(array_type);
        }
    }
    context->diagnostics.report_error(posiiton, "Expect identifier as directive name");
    throw "Stop";
}