#include "../include/jot_parser.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_logger.hpp"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

std::shared_ptr<CompilationUnit> JotParser::parse_compilation_unit() {
    std::vector<std::shared_ptr<Statement>> tree_nodes;
    try {
        while (is_source_available()) {
            if (is_current_kind(TokenKind::ImportKeyword)) {
                auto module_tree_node = parse_import_declaration();
                merge_tree_nodes(tree_nodes, module_tree_node);
                continue;
            }

            if (is_current_kind(TokenKind::LoadKeyword)) {
                auto module_tree_node = parse_load_declaration();
                merge_tree_nodes(tree_nodes, module_tree_node);
                continue;
            }

            tree_nodes.push_back(parse_declaration_statement());
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
                context->diagnostics.add_diagnostic_error(library_name.get_span(),
                                                          "No standard library with name " +
                                                              library_name.get_literal());
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            context->set_path_visited(library_path);
            merge_tree_nodes(tree_nodes, nodes);
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
        context->diagnostics.add_diagnostic_error(
            library_name.get_span(), "No standard library with name " + library_name.get_literal());
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_load_declaration() {
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // load { <string> <string> }
        advanced_token();
        std::vector<std::shared_ptr<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name =
                consume_kind(TokenKind::String, "Expect string as file name after load statement");

            std::string library_path = file_parent_path + library_name.get_literal() + ".jot";

            if (context->is_path_visited(library_path))
                continue;

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic_error(library_name.get_span(),
                                                          "Path not exists " + library_path);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            context->set_path_visited(library_path);
            merge_tree_nodes(tree_nodes, nodes);
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as file name after load statement");

    std::string library_path = file_parent_path + library_name.get_literal() + ".jot";

    if (context->is_path_visited(library_path)) {
        return std::vector<std::shared_ptr<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic_error(library_name.get_span(),
                                                  "Path not exists " + library_path);
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
    if (context->diagnostics.get_errors_number() > 0) {
        throw "Stop";
    }
    return compilation_unit->get_tree_nodes();
}

void JotParser::merge_tree_nodes(std::vector<std::shared_ptr<Statement>> &distany,
                                 std::vector<std::shared_ptr<Statement>> &source) {
    distany.insert(std::end(distany), std::begin(source), std::end(source));
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

        context->diagnostics.add_diagnostic_error(
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
        return parse_field_declaration(true);
    }
    case TokenKind::EnumKeyword: {
        return parse_enum_declaration();
    }
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().get_span(),
                                                  "Invalid top level declaration statement");
        throw "Stop";
    }
    }
}

std::shared_ptr<Statement> JotParser::parse_statement() {
    switch (peek_current().get_kind()) {
    case TokenKind::VarKeyword: {
        return parse_field_declaration(false);
    }
    case TokenKind::IfKeyword: {
        return parse_if_statement();
    }
    case TokenKind::WhileKeyword: {
        return parse_while_statement();
    }
    case TokenKind::SwitchKeyword: {
        return parse_switch_statement();
    }
    case TokenKind::ReturnKeyword: {
        return parse_return_statement();
    }
    case TokenKind::DeferKeyword: {
        return parse_defer_statement();
    }
    case TokenKind::BreakKeyword: {
        return parse_break_statement();
    }
    case TokenKind::ContinueKeyword: {
        return parse_continue_statement();
    }
    case TokenKind::OpenBrace: {
        return parse_block_statement();
    }
    default: {
        return parse_expression_statement();
    }
    }
}

std::shared_ptr<FieldDeclaration> JotParser::parse_field_declaration(bool is_global) {
    assert_kind(TokenKind::VarKeyword, "Expect var keyword.");
    auto name = consume_kind(TokenKind::Symbol, "Expect identifier as variable name.");
    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        auto type = parse_type();
        std::shared_ptr<Expression> initalizer;
        if (is_current_kind(TokenKind::Equal)) {
            assert_kind(TokenKind::Equal, "Expect = after variable name.");
            initalizer = parse_expression();
        }
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
        return std::make_shared<FieldDeclaration>(name, type, initalizer, is_global);
    }
    assert_kind(TokenKind::Equal, "Expect = after variable name.");
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<FieldDeclaration>(name, value->get_type_node(), value, is_global);
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
        context->diagnostics.add_diagnostic_error(
            name.get_span(), "Prefix function must have exactly one parameter");
        throw "Stop";
    }

    if (kind == FunctionCallKind::Infix && parameters_size != 2) {
        context->diagnostics.add_diagnostic_error(name.get_span(),
                                                  "Infix function must have exactly Two parameter");
        throw "Stop";
    }

    if (kind == FunctionCallKind::Postfix && parameters_size != 1) {
        context->diagnostics.add_diagnostic_error(
            name.get_span(), "Postfix function must have exactly one parameter");
        throw "Stop";
    }

    auto name_literal = name.get_literal();
    register_function_call(kind, name_literal);

    auto return_type = parse_type();

    // Function can't return fixed size array, you can use pointer format to return allocated array
    if (return_type->get_type_kind() == TypeKind::Array) {
        context->diagnostics.add_diagnostic_error(return_type->get_type_position(),
                                                  "Function cannot return array type " +
                                                      return_type->type_literal());
        throw "Stop";
    }

    if (is_external)
        assert_kind(TokenKind::Semicolon, "Expect ; after external function declaration");
    return std::make_shared<FunctionPrototype>(name, parameters, return_type,
                                               FunctionCallKind::Normal, is_external);
}

std::shared_ptr<FunctionDeclaration> JotParser::parse_function_declaration(FunctionCallKind kind) {
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::FunctionScope;

    auto prototype = parse_function_prototype(kind, false);

    if (is_current_kind(TokenKind::Equal)) {
        auto equal_token = peek_and_advance_token();
        auto value = parse_expression();
        auto return_statement = std::make_shared<ReturnStatement>(equal_token, value, true);
        assert_kind(TokenKind::Semicolon, "Expect ; after function value");
        current_ast_scope = parent_node_scope;
        return std::make_shared<FunctionDeclaration>(prototype, return_statement);
    }

    if (is_current_kind(TokenKind::OpenBrace)) {
        auto block = parse_block_statement();
        current_ast_scope = parent_node_scope;
        return std::make_shared<FunctionDeclaration>(prototype, block);
    }

    context->diagnostics.add_diagnostic_error(peek_current().get_span(),
                                              "Invalid function declaration body");
    throw "Stop";
}

std::shared_ptr<EnumDeclaration> JotParser::parse_enum_declaration() {
    auto enum_token = consume_kind(TokenKind::EnumKeyword, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::Symbol, "Expect Symbol as enum name");

    std::shared_ptr<JotType> element_type = nullptr;

    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        element_type = parse_type();
    } else {
        // Default enumeration element type is integer 32
        element_type = std::make_shared<JotNumberType>(enum_token, NumberKind::Integer32);
    }

    assert_kind(TokenKind::OpenBrace, "Expect { after enum name");
    std::vector<Token> enum_values;
    std::unordered_map<std::string, int> enum_values_indexes;
    int index = 0;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto enum_value = consume_kind(TokenKind::Symbol, "Expect Symbol as enum value");
        enum_values.push_back(enum_value);

        if (enum_values_indexes.count(enum_value.get_literal())) {
            context->diagnostics.add_diagnostic_error(
                enum_value.get_span(), "Can't declare 2 elements with the same name");
            throw "Stop";
        } else {
            enum_values_indexes[enum_value.get_literal()] = index++;
        }

        if (is_current_kind(TokenKind::Comma))
            advanced_token();
    }
    assert_kind(TokenKind::CloseBrace, "Expect } in the end of enum declaration");
    auto enumeration_type =
        std::make_shared<JotEnumType>(enum_name, enum_values_indexes, element_type);
    context->enumerations[enum_name.get_literal()] = enumeration_type;
    return std::make_shared<EnumDeclaration>(enum_name, enumeration_type);
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
    if (is_current_kind(TokenKind::Semicolon)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return keyword");
        return std::make_shared<ReturnStatement>(keyword, nullptr, false);
    }
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return statement");
    return std::make_shared<ReturnStatement>(keyword, value, true);
}

std::shared_ptr<DeferStatement> JotParser::parse_defer_statement() {
    auto defer_token = consume_kind(TokenKind::DeferKeyword, "Expect Defer keyword.");
    if (current_ast_scope == AstNodeScope::FunctionScope) {
        auto expression = parse_expression();
        if (auto call_expression = std::dynamic_pointer_cast<CallExpression>(expression)) {
            assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after defer call statement");
            return std::make_shared<DeferStatement>(defer_token, call_expression);
        }

        context->diagnostics.add_diagnostic_error(defer_token.get_span(),
                                                  "defer keyword expect call expression");
        throw "Stop";
    }
    context->diagnostics.add_diagnostic_error(
        defer_token.get_span(), "defer keyword can only used inside function main block, "
                                "nested blocks such as if or while are not supported yet");
    throw "Stop";
}

std::shared_ptr<BreakStatement> JotParser::parse_break_statement() {
    auto break_token = consume_kind(TokenKind::BreakKeyword, "Expect break keyword.");

    if (current_ast_scope != AstNodeScope::ConditionalScope or loop_stack_size == 0) {
        context->diagnostics.add_diagnostic_error(
            break_token.get_span(), "break keyword can only be used inside at last one while loop");
        throw "Stop";
    }

    if (is_current_kind(TokenKind::Semicolon)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after break call statement");
        return std::make_shared<BreakStatement>(break_token, false, 1);
    }

    auto break_times = parse_expression();
    if (auto number_expr = std::dynamic_pointer_cast<NumberExpression>(break_times)) {
        auto number_value = number_expr->get_value();
        auto number_kind = number_value.get_kind();
        if (number_kind == TokenKind::Float or number_kind == TokenKind::Float32Type or
            number_kind == TokenKind::Float64Type) {

            context->diagnostics.add_diagnostic_error(
                break_token.get_span(),
                "expect break keyword times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.get_literal());
        if (times_int < 1) {
            context->diagnostics.add_diagnostic_error(
                break_token.get_span(), "expect break times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_stack_size) {
            context->diagnostics.add_diagnostic_error(
                break_token.get_span(), "break times can't be bigger than the number of loops you "
                                        "have, expect less than or equals " +
                                            std::to_string(loop_stack_size));
            throw "Stop";
        }

        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after brea statement");
        return std::make_shared<BreakStatement>(break_token, true, times_int);
    }

    context->diagnostics.add_diagnostic_error(break_token.get_span(),
                                              "break keyword times must be a number");
    throw "Stop";
}

std::shared_ptr<ContinueStatement> JotParser::parse_continue_statement() {
    auto continue_token = consume_kind(TokenKind::ContinueKeyword, "Expect continue keyword.");

    if (current_ast_scope != AstNodeScope::ConditionalScope or loop_stack_size == 0) {
        context->diagnostics.add_diagnostic_error(
            continue_token.get_span(),
            "continue keyword can only be used inside at last one while loop");
        throw "Stop";
    }

    if (is_current_kind(TokenKind::Semicolon)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after defer call statement");
        return std::make_shared<ContinueStatement>(continue_token, false, 1);
    }

    auto continue_times = parse_expression();
    if (auto number_expr = std::dynamic_pointer_cast<NumberExpression>(continue_times)) {
        auto number_value = number_expr->get_value();
        auto number_kind = number_value.get_kind();
        if (number_kind == TokenKind::Float or number_kind == TokenKind::Float32Type or
            number_kind == TokenKind::Float64Type) {

            context->diagnostics.add_diagnostic_error(
                continue_token.get_span(),
                "expect continue times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.get_literal());
        if (times_int < 1) {
            context->diagnostics.add_diagnostic_error(
                continue_token.get_span(),
                "expect continue times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_stack_size) {
            context->diagnostics.add_diagnostic_error(
                continue_token.get_span(),
                "continue times can't be bigger than the number of loops you "
                "have, expect less than or equals " +
                    std::to_string(loop_stack_size));
            throw "Stop";
        }

        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after brea statement");
        return std::make_shared<ContinueStatement>(continue_token, true, times_int);
    }

    context->diagnostics.add_diagnostic_error(continue_token.get_span(),
                                              "continue keyword times must be a number");
    throw "Stop";
}

std::shared_ptr<IfStatement> JotParser::parse_if_statement() {
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::ConditionalScope;

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
    current_ast_scope = parent_node_scope;
    return std::make_shared<IfStatement>(conditional_blocks);
}

std::shared_ptr<WhileStatement> JotParser::parse_while_statement() {
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::ConditionalScope;

    auto keyword = consume_kind(TokenKind::WhileKeyword, "Expect while keyword.");
    auto condition = parse_expression();

    loop_stack_size += 1;
    auto body = parse_statement();
    loop_stack_size -= 1;

    current_ast_scope = parent_node_scope;
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

std::shared_ptr<SwitchStatement> JotParser::parse_switch_statement() {
    auto keyword = consume_kind(TokenKind::SwitchKeyword, "Expect switch keyword.");
    auto argument = parse_expression();
    assert_kind(TokenKind::OpenBrace, "Expect { after switch value");

    std::vector<std::shared_ptr<SwitchCase>> switch_cases;
    std::shared_ptr<SwitchCase> default_branch = nullptr;
    bool has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::CloseBrace)) {
        std::vector<std::shared_ptr<Expression>> values;
        if (is_current_kind(TokenKind::ElseKeyword)) {
            if (has_default_branch) {
                context->diagnostics.add_diagnostic_error(
                    keyword.get_span(), "Switch statementscan't has more than one default branch");
                throw "Stop";
            }
            auto else_keyword = consume_kind(TokenKind::ElseKeyword,
                                             "Expect else keyword in switch default branch");
            consume_kind(TokenKind::RightArrow,
                         "Expect -> after else keyword in switch default branch");
            auto default_body = parse_statement();
            default_branch = std::make_shared<SwitchCase>(else_keyword, values, default_body);
            has_default_branch = true;
            continue;
        }

        // Parse all values for this case V1, V2, ..., Vn ->
        while (is_source_available() and !is_current_kind(TokenKind::RightArrow)) {
            auto value = parse_expression();
            values.push_back(value);
            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            }
        }
        auto rightArrow = consume_kind(TokenKind::RightArrow, "Expect -> after branch value");
        auto branch = parse_statement();
        auto switch_case = std::make_shared<SwitchCase>(rightArrow, values, branch);
        switch_cases.push_back(switch_case);
    }
    assert_kind(TokenKind::CloseBrace, "Expect } after switch Statement last branch");
    return std::make_shared<SwitchStatement>(keyword, argument, switch_cases, default_branch);
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
    auto expression = parse_shift_expression();
    while (is_current_kind(TokenKind::Greater) || is_current_kind(TokenKind::GreaterEqual) ||
           is_current_kind(TokenKind::Smaller) || is_current_kind(TokenKind::SmallerEqual)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_shift_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_shift_expression() {
    auto expression = parse_term_expression();
    while (is_current_kind(TokenKind::RightShift) || is_current_kind(TokenKind::LeftShift)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_term_expression();
        expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
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
    auto expression = parse_enum_access_expression();
    while (is_current_kind(TokenKind::Star) || is_current_kind(TokenKind::Slash) ||
           is_current_kind(TokenKind::Percent)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_enum_access_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_enum_access_expression() {
    auto expression = parse_infix_call_expression();
    if (is_current_kind(TokenKind::ColonColon)) {
        auto colons_token = peek_and_advance_token();
        if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression)) {
            auto enum_name = literal->get_name();
            if (context->enumerations.count(enum_name.get_literal())) {
                auto enum_type = context->enumerations[enum_name.get_literal()];
                auto element =
                    consume_kind(TokenKind::Symbol, "Expect identifier as enum field name");

                auto enum_values = enum_type->get_enum_values();
                if (not enum_values.count(element.get_literal())) {
                    context->diagnostics.add_diagnostic_error(
                        element.get_span(), "Can't find element with name " +
                                                element.get_literal() + " in enum " +
                                                enum_name.get_literal());
                    throw "Stop";
                }

                int index = enum_values[element.get_literal()];
                auto enum_element_type =
                    std::make_shared<JotEnumElementType>(enum_name, enum_type->get_element_type());
                return std::make_shared<EnumAccessExpression>(enum_name, element, index,
                                                              enum_element_type);
            } else {
                context->diagnostics.add_diagnostic_error(enum_name.get_span(),
                                                          "Can't find enum declaration with name " +
                                                              enum_name.get_literal());
                throw "Stop";
            }
        } else {
            context->diagnostics.add_diagnostic_error(colons_token.get_span(),
                                                      "Expect identifier as Enum name");
            throw "Stop";
        }
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
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        return std::make_shared<PrefixUnaryExpression>(token, right);
    }

    if (is_current_kind(TokenKind::PlusPlus) or is_current_kind(TokenKind::MinusMinus)) {
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        auto ast_node_type = right->get_ast_node_type();
        if ((ast_node_type == AstNodeType::LiteralExpr) or
            (ast_node_type == AstNodeType::IndexExpr)) {
            return std::make_shared<PrefixUnaryExpression>(token, right);
        }

        context->diagnostics.add_diagnostic_error(
            token.get_span(),
            "Unary ++ or -- expect right expression to be variable or index expression");
        throw "Stop";
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
        } else if (is_current_kind(TokenKind::OpenBracket)) {
            Token position = peek_and_advance_token();
            auto index = parse_expression();
            assert_kind(TokenKind::CloseBracket, "Expect ] after index value");
            expression = std::make_shared<IndexExpression>(position, expression, index);
        } else if (is_current_kind(TokenKind::PlusPlus) or is_current_kind(TokenKind::MinusMinus)) {
            auto token = peek_and_advance_token();
            auto ast_node_type = expression->get_ast_node_type();
            if ((ast_node_type == AstNodeType::LiteralExpr) or
                (ast_node_type == AstNodeType::IndexExpr)) {
                return std::make_shared<PostfixUnaryExpression>(token, expression);
            }
            context->diagnostics.add_diagnostic_error(
                token.get_span(),
                "Unary ++ or -- expect left expression to be variable or index expression");
            throw "Stop";
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
    case TokenKind::OpenBracket: {
        return parse_array_expression();
    }
    case TokenKind::IfKeyword: {
        return parse_if_expression();
    }
    case TokenKind::CastKeyword: {
        return parse_cast_expression();
    }
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().get_span(),
                                                  "Unexpected or unsupported expression");
        throw "Stop";
    }
    }
}

std::shared_ptr<NumberExpression> JotParser::parse_number_expression() {
    auto number_token = peek_and_advance_token();
    auto number_kind = get_number_kind(number_token.get_kind());
    auto number_type = std::make_shared<JotNumberType>(number_token, number_kind);
    return std::make_shared<NumberExpression>(number_token, number_type);
}

std::shared_ptr<LiteralExpression> JotParser::parse_literal_expression() {
    Token symbol_token = peek_and_advance_token();
    auto type = std::make_shared<JotNoneType>(symbol_token);
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

std::shared_ptr<ArrayExpression> JotParser::parse_array_expression() {
    Token position = peek_and_advance_token();
    std::vector<std::shared_ptr<Expression>> values;
    while (is_source_available() && not is_current_kind(CloseBracket)) {
        values.push_back(parse_expression());
        if (is_current_kind(TokenKind::Comma))
            advanced_token();
    }
    assert_kind(TokenKind::CloseBracket, "Expect ] at the end of array values");
    return std::make_shared<ArrayExpression>(position, values);
}

std::shared_ptr<CastExpression> JotParser::parse_cast_expression() {
    auto cast_token = consume_kind(TokenKind::CastKeyword, "Expect cast keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after cast keyword");
    auto cast_to_type = parse_type();
    assert_kind(TokenKind::CloseParen, "Expect `)` after cast type");
    auto expression = parse_expression();
    return std::make_shared<CastExpression>(cast_token, cast_to_type, expression);
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
        return std::make_shared<JotNumberType>(operand->get_type_token(), NumberKind::Integer64);
    }

    // Parse function pointer type
    if (is_current_kind(TokenKind::OpenParen)) {
        auto paren_token = peek_and_advance_token();
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

    // Parse Fixed size array type
    if (is_current_kind(TokenKind::OpenBracket)) {
        auto bracket_token = peek_and_advance_token();
        auto size = parse_number_expression();
        auto number_type = std::dynamic_pointer_cast<JotNumberType>(size->get_type_node());
        if (not number_type->is_integer()) {
            context->diagnostics.add_diagnostic_error(number_type->get_type_position(),
                                                      "Array size must be an integer constants");
            throw "Stop";
        }
        auto number_value = std::atoi(size->get_value().get_literal().c_str());
        assert_kind(TokenKind::CloseBracket, "Expect ] after array size.");
        auto element_type = parse_type();
        return std::make_shared<JotArrayType>(element_type, number_value);
    }

    return parse_type_with_postfix();
}

std::shared_ptr<JotType> JotParser::parse_type_with_postfix() {
    auto primary_type = parse_primary_type();
    // TODO: Can used later to provide sume postfix features such as ? for null safty
    return primary_type;
}

std::shared_ptr<JotType> JotParser::parse_primary_type() {
    if (is_current_kind(TokenKind::Symbol)) {
        return parse_identifier_type();
    }
    context->diagnostics.add_diagnostic_error(peek_current().get_span(), "Expected symbol as type");
    throw "Stop";
}

std::shared_ptr<JotType> JotParser::parse_identifier_type() {
    Token symbol_token = consume_kind(TokenKind::Symbol, "Expect identifier as type");
    std::string type_literal = symbol_token.get_literal();

    if (type_literal == "int16") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Integer16);
    }

    if (type_literal == "int32") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Integer32);
    }

    if (type_literal == "int64") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Integer64);
    }

    if (type_literal == "float32") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Float32);
    }

    if (type_literal == "float64") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Float64);
    }

    if (type_literal == "char" || type_literal == "int8") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Integer8);
    }

    if (type_literal == "bool" || type_literal == "int1") {
        return std::make_shared<JotNumberType>(symbol_token, NumberKind::Integer1);
    }

    if (type_literal == "void") {
        return std::make_shared<JotVoidType>(symbol_token);
    }

    // Check if this type is enumeration type
    if (context->enumerations.count(type_literal)) {
        auto enum_type = context->enumerations[type_literal];
        auto enum_name = enum_type->get_type_token();
        auto enum_element_type =
            std::make_shared<JotEnumElementType>(enum_name, enum_type->get_element_type());
        return enum_element_type;
    }

    // TODO: Check if this type is Structure type

    context->diagnostics.add_diagnostic_error(peek_current().get_span(),
                                              "Unexpected identifier type");
    throw "Stop";
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
        context->diagnostics.add_diagnostic_error(peek_current().get_span(),
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
    context->diagnostics.add_diagnostic_error(peek_current().get_span(), message);
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
    context->diagnostics.add_diagnostic_error(location, message);
    throw "Stop";
}

bool JotParser::is_source_available() { return not peek_current().is_end_of_file(); }
