#include "../include/jot_parser.hpp"
#include "../include/jot_files.hpp"

#include <memory>
#include <vector>

std::shared_ptr<CompilationUnit> JotParser::parse_compilation_unit() {
    std::vector<std::shared_ptr<Statement>> tree_nodes;
    try {
        while (is_source_available()) {
            auto current_token = peek_current().get_kind();
            switch (current_token) {
            case TokenKind::ImportKeyword: {
                auto module_tree_node = parse_import_declaration();
                tree_nodes.insert(std::end(tree_nodes), std::begin(module_tree_node),
                                  std::end(module_tree_node));
                break;
            }
            default: {
                tree_nodes.push_back(parse_declaration_statement());
            }
            }
        }
    } catch (const char *message) {
    }
    return std::make_shared<CompilationUnit>(tree_nodes);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_import_declaration() {
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // import { <string> <string> }
        advanced_token();
        std::vector<std::shared_ptr<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name = consume_kind(
                TokenKind::String, "Expect string as library name after import statement");
            std::string library_path = "../lib/" + library_name.get_literal() + ".jot";

            if (context->is_path_visited(library_path))
                continue;

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic(library_name.get_span(), "Path not exists");
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            context->set_path_visited(library_path);
            tree_nodes.insert(std::end(tree_nodes), std::begin(nodes), std::end(nodes));
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as library name after import statement");
    std::string library_path = "../lib/" + library_name.get_literal() + ".jot";

    if (context->is_path_visited(library_path)) {
        return std::vector<std::shared_ptr<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic(library_name.get_span(), "Path not exists");
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_single_source_file(std::string &path) {
    const char *file_name = path.c_str();
    std::string source_content = read_file_content(file_name);
    auto tokenizer = std::make_unique<JotTokenizer>(file_name, source_content);
    JotParser parser(context, std::move(tokenizer));
    auto compilation_unit = parser.parse_compilation_unit();
    if (context->diagnostics.diagnostics_size() > 0) {
        throw "Stop";
    }
    return compilation_unit->get_tree_nodes();
}

std::shared_ptr<Statement> JotParser::parse_declaration_statement() {
    switch (peek_current().get_kind()) {
    case TokenKind::PrefixKeyword:
    case TokenKind::InfixKeyword:
    case TokenKind::PostfixKeyword: {
        auto call_kind = FunctionCallKind::Prefix;
        if (peek_current().get_kind() == TokenKind::InfixKeyword)
            call_kind = FunctionCallKind::Infix;
        else if (peek_current().get_kind() == TokenKind::PostfixKeyword)
            call_kind = FunctionCallKind::Postfix;
        advanced_token();

        if (is_current_kind(TokenKind::ExternKeyword))
            return parse_function_prototype(call_kind, true);
        else if (is_current_kind(TokenKind::FunKeyword))
            return parse_function_declaration(call_kind);

        context->diagnostics.add_diagnostic(
            peek_current().get_span(), "Prefix, Infix, postfix keyword used only with functions");
        throw "Stop";
    }
    case TokenKind::ExternKeyword: {
        return parse_function_prototype(FunctionCallKind::Normal, true);
    }
    case TokenKind::FunKeyword: {
        return parse_function_declaration(FunctionCallKind::Normal);
    }
    case TokenKind::VarKeyword: {
        return parse_field_declaration();
    }
    case TokenKind::EnumKeyword: {
        return parse_enum_declaration();
    }
    default: {
        context->diagnostics.add_diagnostic(peek_current().get_span(),
                                            "Invalid top level declaration statement");
        throw "Stop";
    }
    }
}

std::shared_ptr<Statement> JotParser::parse_statement() {
    switch (peek_current().get_kind()) {
    case TokenKind::VarKeyword: {
        return parse_field_declaration();
    }
    case TokenKind::IfKeyword: {
        return parse_if_statement();
    }
    case TokenKind::WhileKeyword: {
        return parse_while_statement();
    }
    case TokenKind::ReturnKeyword: {
        return parse_return_statement();
    }
    case TokenKind::OpenBrace: {
        return parse_block_statement();
    }
    default: {
        return parse_expression_statement();
    }
    }
}

std::shared_ptr<FieldDeclaration> JotParser::parse_field_declaration() {
    assert_kind(TokenKind::VarKeyword, "Expect var keyword.");
    auto name = consume_kind(TokenKind::Symbol, "Expect identifier as variable name.");
    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        auto type = parse_type();
        assert_kind(TokenKind::Equal, "Expect = after variable name.");
        auto value = parse_expression();
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
        return std::make_shared<FieldDeclaration>(name, type, value);
    }
    assert_kind(TokenKind::Equal, "Expect = after variable name.");
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<FieldDeclaration>(name, value->get_type_node(), value);
}

std::shared_ptr<FunctionPrototype> JotParser::parse_function_prototype(FunctionCallKind kind,
                                                                       bool is_external) {
    if (is_external)
        assert_kind(TokenKind::ExternKeyword, "Expect external keyword");
    assert_kind(TokenKind::FunKeyword, "Expect function keyword.");
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as function name.");

    std::vector<std::shared_ptr<Parameter>> parameters;
    if (is_current_kind(TokenKind::OpenParen)) {
        advanced_token();
        while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::Comma))
                advanced_token();
        }
        assert_kind(TokenKind::CloseParen, "Expect ) after function parameters.");
    }

    auto parameters_size = parameters.size();

    if (kind == FunctionCallKind::Prefix && parameters_size != 1) {
        context->diagnostics.add_diagnostic(name.get_span(),
                                            "Prefix function must have exactly one parameter");
        throw "Stop";
    }

    if (kind == FunctionCallKind::Infix && parameters_size != 2) {
        context->diagnostics.add_diagnostic(name.get_span(),
                                            "Infix function must have exactly Two parameter");
        throw "Stop";
    }

    if (kind == FunctionCallKind::Postfix && parameters_size != 1) {
        context->diagnostics.add_diagnostic(name.get_span(),
                                            "Postfix function must have exactly one parameter");
        throw "Stop";
    }

    auto name_literal = name.get_literal();
    register_function_call(kind, name_literal);

    auto return_type = parse_type();
    if (is_external)
        assert_kind(TokenKind::Semicolon, "Expect ; after external function declaration");
    return std::make_shared<FunctionPrototype>(name, parameters, return_type,
                                               FunctionCallKind::Normal, is_external);
}

std::shared_ptr<FunctionDeclaration> JotParser::parse_function_declaration(FunctionCallKind kind) {
    auto prototype = parse_function_prototype(kind, false);

    if (is_current_kind(TokenKind::Equal)) {
        auto equal_token = peek_and_advance_token();
        auto value = parse_expression();
        auto return_statement = std::make_shared<ReturnStatement>(equal_token, value);
        assert_kind(TokenKind::Semicolon, "Expect ; after function value");
        return std::make_shared<FunctionDeclaration>(prototype, return_statement);
    }

    if (is_current_kind(TokenKind::OpenBrace)) {
        auto block = parse_block_statement();
        return std::make_shared<FunctionDeclaration>(prototype, block);
    }

    context->diagnostics.add_diagnostic(peek_current().get_span(),
                                        "Invalid function declaration body");
    throw "Stop";
}

std::shared_ptr<EnumDeclaration> JotParser::parse_enum_declaration() {
    auto enum_token = consume_kind(TokenKind::EnumKeyword, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::Symbol, "Expect Symbol as enum name");
    assert_kind(TokenKind::OpenBrace, "Expect { after enum name");
    std::vector<Token> enum_values;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto enum_value = consume_kind(TokenKind::Symbol, "Expect Symbol as enum value");
        enum_values.push_back(enum_value);

        if (is_current_kind(TokenKind::Comma))
            advanced_token();
        else
            break;
    }
    assert_kind(TokenKind::CloseBrace, "Expect } in the end of enum declaration");
    return std::make_shared<EnumDeclaration>(enum_name, enum_values);
}

std::shared_ptr<Parameter> JotParser::parse_parameter() {
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as parameter name.");
    auto type = parse_type();
    return std::make_shared<Parameter>(name, type);
}

std::shared_ptr<BlockStatement> JotParser::parse_block_statement() {
    consume_kind(TokenKind::OpenBrace, "Expect { on the start of block.");
    std::vector<std::shared_ptr<Statement>> statements;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        statements.push_back(parse_statement());
    }
    consume_kind(TokenKind::CloseBrace, "Expect } on the end of block.");
    return std::make_shared<BlockStatement>(statements);
}

std::shared_ptr<ReturnStatement> JotParser::parse_return_statement() {
    auto keyword = consume_kind(TokenKind::ReturnKeyword, "Expect return keyword.");
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return statement");
    return std::make_shared<ReturnStatement>(keyword, value);
}

std::shared_ptr<IfStatement> JotParser::parse_if_statement() {
    auto if_token = consume_kind(TokenKind::IfKeyword, "Expect If keyword.");
    auto condition = parse_expression();
    auto then_block = parse_statement();
    auto conditional_block = std::make_shared<ConditionalBlock>(if_token, condition, then_block);
    std::vector<std::shared_ptr<ConditionalBlock>> conditional_blocks;
    conditional_blocks.push_back(conditional_block);

    while (is_source_available() && is_current_kind(TokenKind::ElseKeyword)) {
        auto else_token = consume_kind(TokenKind::ElseKeyword, "Expect else keyword.");
        if (is_current_kind(TokenKind::IfKeyword)) {
            advanced_token();
            auto elif_condition = parse_expression();
            auto elif_block = parse_statement();
            auto elif_condition_block =
                std::make_shared<ConditionalBlock>(else_token, elif_condition, elif_block);
            conditional_blocks.push_back(elif_condition_block);
        } else {
            auto true_value_token = else_token;
            true_value_token.set_kind(TokenKind::TrueKeyword);
            auto true_expression = std::make_shared<BooleanExpression>(true_value_token);
            auto else_block = parse_statement();
            auto elif_condition_block =
                std::make_shared<ConditionalBlock>(else_token, true_expression, else_block);
            conditional_blocks.push_back(elif_condition_block);
        }
    }
    return std::make_shared<IfStatement>(conditional_blocks);
}

std::shared_ptr<WhileStatement> JotParser::parse_while_statement() {
    auto keyword = consume_kind(TokenKind::WhileKeyword, "Expect while keyword.");
    auto condition = parse_expression();
    auto body = parse_statement();
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

std::shared_ptr<ExpressionStatement> JotParser::parse_expression_statement() {
    auto expression = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<ExpressionStatement>(expression);
}

std::shared_ptr<Expression> JotParser::parse_expression() { return parse_assignment_expression(); }

std::shared_ptr<Expression> JotParser::parse_assignment_expression() {
    auto expression = parse_logical_or_expression();
    if (peek_current().is_assignments_operator()) {
        auto assignments_token = peek_and_advance_token();
        std::shared_ptr<Expression> right_value;
        auto assignments_token_kind = assignments_token.get_kind();
        if (assignments_token_kind == TokenKind::Equal) {
            right_value = parse_assignment_expression();
        } else {
            assignments_token.set_kind(assignments_binary_operators[assignments_token_kind]);
            auto right_expression = parse_assignment_expression();
            right_value =
                std::make_shared<BinaryExpression>(expression, assignments_token, right_expression);
        }
        return std::make_shared<AssignExpression>(expression, assignments_token, right_value);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_logical_or_expression() {
    auto expression = parse_logical_and_expression();
    while (is_current_kind(TokenKind::LogicalOr)) {
        auto or_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, or_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_logical_and_expression() {
    auto expression = parse_equality_expression();
    while (is_current_kind(TokenKind::LogicalAnd)) {
        auto and_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, and_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_equality_expression() {
    auto expression = parse_comparison_expression();
    while (is_current_kind(TokenKind::EqualEqual) || is_current_kind(TokenKind::BangEqual)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_comparison_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_comparison_expression() {
    auto expression = parse_term_expression();
    while (is_current_kind(TokenKind::Greater) || is_current_kind(TokenKind::GreaterEqual) ||
           is_current_kind(TokenKind::Smaller) || is_current_kind(TokenKind::SmallerEqual)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_term_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_term_expression() {
    auto expression = parse_factor_expression();
    while (is_current_kind(TokenKind::Plus) || is_current_kind(TokenKind::Minus)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_factor_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_factor_expression() {
    auto expression = parse_infix_call_expression();
    while (is_current_kind(TokenKind::Star) || is_current_kind(TokenKind::Slash) ||
           is_current_kind(TokenKind::Percent)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_infix_call_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_infix_call_expression() {
    auto expression = parse_prefix_expression();
    auto current_token_literal = peek_current().get_literal();
    if (is_current_kind(TokenKind::Symbol) and context->is_infix_function(current_token_literal)) {
        Token symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(expression);
        arguments.push_back(parse_infix_call_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_prefix_expression() {
    if (peek_current().is_unary_operator()) {
        Token token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        return std::make_shared<UnaryExpression>(token, right);
    }
    return parse_prefix_call_expression();
}

std::shared_ptr<Expression> JotParser::parse_prefix_call_expression() {
    auto current_token_literal = peek_current().get_literal();
    if (is_current_kind(TokenKind::Symbol) and context->is_prefix_function(current_token_literal)) {
        Token symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(parse_prefix_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_postfix_expression();
}

std::shared_ptr<Expression> JotParser::parse_postfix_expression() {
    auto expression = parse_postfix_call_expression();
    while (true) {
        if (is_current_kind(TokenKind::OpenParen)) {
            Token position = peek_and_advance_token();
            std::vector<std::shared_ptr<Expression>> arguments;
            while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
                arguments.push_back(parse_expression());
                if (is_current_kind(TokenKind::Comma))
                    advanced_token();
            }
            assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
            expression = std::make_shared<CallExpression>(position, expression, arguments);
        } else {
            break;
        }
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_postfix_call_expression() {
    auto expression = parse_primary_expression();
    auto current_token_literal = peek_current().get_literal();
    if (is_current_kind(TokenKind::Symbol) and
        context->is_postfix_function(current_token_literal)) {
        Token symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(expression);
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_primary_expression() {
    auto current_token_kind = peek_current().get_kind();
    switch (current_token_kind) {
    case TokenKind::Integer:
    case TokenKind::Integer1Type:
    case TokenKind::Integer8Type:
    case TokenKind::Integer16Type:
    case TokenKind::Integer32Type:
    case TokenKind::Integer64Type:
    case TokenKind::Float:
    case TokenKind::Float32Type:
    case TokenKind::Float64Type: {
        return parse_number_expression();
    }
    case TokenKind::Character: {
        advanced_token();
        return std::make_shared<CharacterExpression>(peek_previous());
    }
    case TokenKind::String: {
        advanced_token();
        return std::make_shared<StringExpression>(peek_previous());
    }
    case TokenKind::TrueKeyword:
    case TokenKind::FalseKeyword: {
        advanced_token();
        return std::make_shared<BooleanExpression>(peek_previous());
    }
    case TokenKind::NullKeyword: {
        advanced_token();
        return std::make_shared<NullExpression>(peek_previous());
    }
    case TokenKind::Symbol: {
        return parse_literal_expression();
    }
    case TokenKind::OpenParen: {
        return parse_group_expression();
    }
    case TokenKind::IfKeyword: {
        return parse_if_expression();
    }
    default: {
        context->diagnostics.add_diagnostic(peek_current().get_span(),
                                            "Unexpected or unsupported expression");
        throw "Stop";
    }
    }
}

std::shared_ptr<NumberExpression> JotParser::parse_number_expression() {
    auto number_token = peek_and_advance_token();
    auto number_kind = get_number_kind(number_token.get_kind());
    auto number_type = std::make_shared<JotNumber>(number_token, number_kind);
    return std::make_shared<NumberExpression>(number_token, number_type);
}

std::shared_ptr<LiteralExpression> JotParser::parse_literal_expression() {
    Token symbol_token = peek_and_advance_token();
    auto type = std::make_shared<JotNamedType>(symbol_token);
    return std::make_shared<LiteralExpression>(symbol_token, type);
}

std::shared_ptr<IfExpression> JotParser::parse_if_expression() {
    Token if_token = peek_and_advance_token();
    auto condition = parse_expression();
    auto then_value = parse_expression();
    Token else_token =
        consume_kind(TokenKind::ElseKeyword, "Expect `else` keyword after then value.");
    auto else_value = parse_expression();
    return std::make_shared<IfExpression>(if_token, else_token, condition, then_value, else_value);
}

std::shared_ptr<GroupExpression> JotParser::parse_group_expression() {
    Token position = peek_and_advance_token();
    auto expression = parse_expression();
    assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
    return std::make_shared<GroupExpression>(position, expression);
}

std::shared_ptr<JotType> JotParser::parse_type() { return parse_type_with_prefix(); }

std::shared_ptr<JotType> JotParser::parse_type_with_prefix() {
    // Parse pointer type
    if (is_current_kind(TokenKind::Star)) {
        advanced_token();
        auto operand = parse_type_with_prefix();
        return std::make_shared<JotPointerType>(operand->get_type_token(), operand);
    }

    // Parse refernse type
    if (is_current_kind(TokenKind::And)) {
        advanced_token();
        auto operand = parse_type_with_prefix();
        return std::make_shared<JotNumber>(operand->get_type_token(), NumberKind::Integer64);
    }

    // Parse function pointer type
    if (is_current_kind(TokenKind::OpenParen)) {
        auto paren_token = peek_current();
        advanced_token();
        std::vector<std::shared_ptr<JotType>> parameters_types;
        while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
            parameters_types.push_back(parse_type());
            if (is_current_kind(TokenKind::Comma))
                advanced_token();
        }
        assert_kind(TokenKind::CloseParen, "Expect ) after function type parameters");
        auto return_type = parse_type();
        return std::make_shared<JotFunctionType>(paren_token, parameters_types, return_type);
    }

    return parse_type_with_postfix();
}

std::shared_ptr<JotType> JotParser::parse_type_with_postfix() {
    auto primary_type = parse_primary_type();
    // TODO: will used later to support [] and maybe ?
    return primary_type;
}

std::shared_ptr<JotType> JotParser::parse_primary_type() {
    if (is_current_kind(TokenKind::Symbol)) {
        return parse_identifier_type();
    }
    context->diagnostics.add_diagnostic(peek_current().get_span(), "Expected symbol as type");
    throw "Stop";
}

std::shared_ptr<JotType> JotParser::parse_identifier_type() {
    Token symbol_token = consume_kind(TokenKind::Symbol, "Expect identifier as type");
    std::string type_literal = symbol_token.get_literal();

    if (type_literal == "int16") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer16);
    }

    if (type_literal == "int32") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer32);
    }

    if (type_literal == "int64") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer64);
    }

    if (type_literal == "float32") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Float32);
    }

    if (type_literal == "float64") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Float64);
    }

    if (type_literal == "char" || type_literal == "int8") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer8);
    }

    if (type_literal == "bool") {
        return std::make_shared<JotNumber>(symbol_token, NumberKind::Integer1);
    }

    if (type_literal == "void") {
        return std::make_shared<JotVoid>(symbol_token);
    }

    context->diagnostics.add_diagnostic(peek_current().get_span(), "Unexpected identifier type");
    throw "Stop";

    /**
     *  TODO: should return a named type node but for now just allow primitive types
     *  return std::make_shared<NamedTypeNode>(symbol_token);
     */
}

NumberKind JotParser::get_number_kind(TokenKind token) {
    switch (token) {
    case TokenKind::Integer: return NumberKind::Integer64;
    case TokenKind::Integer1Type: return NumberKind::Integer1;
    case TokenKind::Integer8Type: return NumberKind::Integer8;
    case TokenKind::Integer16Type: return NumberKind::Integer16;
    case TokenKind::Integer32Type: return NumberKind::Integer32;
    case TokenKind::Integer64Type: return NumberKind::Integer64;
    case TokenKind::Float: return NumberKind::Float64;
    case TokenKind::Float32Type: return NumberKind::Float32;
    case TokenKind::Float64Type: return NumberKind::Float64;
    default: {
        context->diagnostics.add_diagnostic(peek_current().get_span(),
                                            "Token kind is not a number");
        throw "Stop";
    }
    }
}

void JotParser::register_function_call(FunctionCallKind kind, std::string &name) {
    switch (kind) {
    case FunctionCallKind::Prefix: {
        context->set_prefix_function(name);
        break;
    }
    case FunctionCallKind::Infix: {
        context->set_infix_function(name);
        break;
    }
    case FunctionCallKind::Postfix: {
        context->set_postfix_function(name);
        break;
    }
    default: return;
    }
}

void JotParser::advanced_token() {
    previous_token = current_token;
    current_token = next_token;
    next_token = tokenizer->scan_next_token();
}

Token JotParser::peek_and_advance_token() {
    auto current = peek_current();
    advanced_token();
    return current;
}

Token JotParser::peek_previous() { return previous_token.value(); }

Token JotParser::peek_current() { return current_token.value(); }

Token JotParser::peek_next() { return next_token.value(); }

bool JotParser::is_previous_kind(TokenKind kind) { return peek_previous().get_kind() == kind; }

bool JotParser::is_current_kind(TokenKind kind) { return peek_current().get_kind() == kind; }

bool JotParser::is_next_kind(TokenKind kind) { return peek_next().get_kind() == kind; }

Token JotParser::consume_kind(TokenKind kind, const char *message) {
    if (is_current_kind(kind)) {
        advanced_token();
        return previous_token.value();
    }
    context->diagnostics.add_diagnostic(peek_current().get_span(), message);
    throw "Stop";
}

void JotParser::assert_kind(TokenKind kind, const char *message) {
    if (is_current_kind(kind)) {
        advanced_token();
        return;
    }

    auto location = peek_current().get_span();
    if (kind == TokenKind::Semicolon) {
        location = peek_previous().get_span();
    }
    context->diagnostics.add_diagnostic(location, message);
    throw "Stop";
}

bool JotParser::is_source_available() { return not peek_current().is_end_of_file(); }
