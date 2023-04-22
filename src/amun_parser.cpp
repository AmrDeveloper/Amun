#include "../include/amun_parser.hpp"
#include "../include/amun_files.hpp"
#include "../include/amun_logger.hpp"
#include "../include/amun_name_mangle.hpp"
#include "../include/amun_type.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

auto amun::Parser::parse_compilation_unit() -> Shared<CompilationUnit>
{
    std::vector<Shared<Statement>> tree_nodes;
    try {
        // Init current and next token inside try catch
        // to handle case if first two token are invalid
        advanced_token();
        advanced_token();

        while (is_source_available()) {
            // Handle importing std file
            if (is_current_kind(TokenKind::TOKEN_IMPORT)) {
                auto module_tree_node = parse_import_declaration();
                append_vectors(tree_nodes, module_tree_node);
                continue;
            }

            // Handle loading user file
            if (is_current_kind(TokenKind::TOKEN_LOAD)) {
                auto module_tree_node = parse_load_declaration();
                append_vectors(tree_nodes, module_tree_node);
                continue;
            }

            // Handle type alias declarations
            if (is_current_kind(TokenKind::TOKEN_TYPE)) {
                parse_type_alias_declaration();
                continue;
            }

            tree_nodes.push_back(parse_declaration_statement());
        }
    }
    catch (const char* message) {
        // Stop parsing at this position and return to the compiler class to report errors and stop
    }
    return std::make_shared<CompilationUnit>(tree_nodes);
}

auto amun::Parser::parse_import_declaration() -> std::vector<Shared<Statement>>
{
    advanced_token();
    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        // import { <string> <string> }
        advanced_token();
        std::vector<Shared<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
            auto library_name = consume_kind(
                TokenKind::TOKEN_STRING, "Expect string as library name after import statement");
            std::string library_path =
                AMUN_LIBRARIES_PREFIX + library_name.literal + AMUN_LANGUAGE_EXTENSION;

            if (context->source_manager.is_path_registered(library_path)) {
                continue;
            }

            if (!amun::is_file_exists(library_path)) {
                context->diagnostics.report_error(
                    library_name.position, "No standard library with name " + library_name.literal);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            append_vectors(tree_nodes, nodes);
        }
        assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect Close brace `}` after import block");

        check_unnecessary_semicolon_warning();

        return tree_nodes;
    }

    auto library_name = consume_kind(TokenKind::TOKEN_STRING,
                                     "Expect string as library name after import statement");

    check_unnecessary_semicolon_warning();

    std::string library_path =
        AMUN_LIBRARIES_PREFIX + library_name.literal + AMUN_LANGUAGE_EXTENSION;

    if (context->source_manager.is_path_registered(library_path)) {
        return std::vector<Shared<Statement>>();
    }

    if (!amun::is_file_exists(library_path)) {
        context->diagnostics.report_error(library_name.position,
                                          "No standard library with name " + library_name.literal);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

auto amun::Parser::parse_load_declaration() -> std::vector<Shared<Statement>>
{
    advanced_token();
    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        // load { <string> <string> }
        advanced_token();
        std::vector<Shared<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
            auto library_name = consume_kind(TokenKind::TOKEN_STRING,
                                             "Expect string as file name after load statement");

            std::string library_path =
                file_parent_path + library_name.literal + AMUN_LANGUAGE_EXTENSION;

            if (context->source_manager.is_path_registered(library_path)) {
                continue;
            }

            if (!amun::is_file_exists(library_path)) {
                context->diagnostics.report_error(library_name.position,
                                                  "Path not exists " + library_path);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            append_vectors(tree_nodes, nodes);
        }
        assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect Close brace `}` after import block");

        check_unnecessary_semicolon_warning();

        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::TOKEN_STRING, "Expect string as file name after load statement");

    check_unnecessary_semicolon_warning();

    std::string library_path = file_parent_path + library_name.literal + AMUN_LANGUAGE_EXTENSION;

    if (context->source_manager.is_path_registered(library_path)) {
        return std::vector<Shared<Statement>>();
    }

    if (!amun::is_file_exists(library_path)) {
        context->diagnostics.report_error(library_name.position, "Path not exists " + library_path);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

auto amun::Parser::parse_compiletime_constants_declaraion() -> Shared<ConstDeclaration>
{
    auto const_token = peek_and_advance_token();
    auto name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect const declaraion name");
    assert_kind(TokenKind::TOKEN_EQUAL, "Expect = after const variable name");
    auto expression = parse_expression();
    check_compiletime_constants_expression(expression, name.position);
    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after const declaraion");
    context->constants_table_map.define(name.literal, expression);
    return std::make_shared<ConstDeclaration>(name, expression);
}

auto amun::Parser::parse_type_alias_declaration() -> void
{
    auto type_token = consume_kind(TokenKind::TOKEN_TYPE, "Expect type keyword");
    auto alias_token =
        consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier for type alias");

    // Make sure alias name is unique
    if (context->type_alias_table.contains(alias_token.literal)) {
        context->diagnostics.report_error(alias_token.position,
                                          "There already a type with name " + alias_token.literal);
        throw "Stop";
    }

    assert_kind(TokenKind::TOKEN_EQUAL, "Expect = after alias name");
    auto actual_type = parse_type();

    if (amun::is_enum_type(actual_type)) {
        context->diagnostics.report_error(alias_token.position,
                                          "You can't use type alias for enum name");
        throw "Stop";
    }

    if (amun::is_enum_element_type(actual_type)) {
        context->diagnostics.report_error(alias_token.position,
                                          "You can't use type alias for enum element");
        throw "Stop";
    }

    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after actual type");
    context->type_alias_table.define_alias(alias_token.literal, actual_type);
}

auto amun::Parser::parse_single_source_file(std::string& path) -> std::vector<Shared<Statement>>
{
    const char* file_name = path.c_str();
    std::string source_content = amun::read_file_content(file_name);
    int file_id = context->source_manager.register_source_path(path);
    amun::Tokenizer tokenizer(file_id, source_content);
    amun::Parser parser(context, tokenizer);
    auto compilation_unit = parser.parse_compilation_unit();
    if (context->diagnostics.level_count(amun::DiagnosticLevel::ERROR) > 0) {
        throw "Stop";
    }
    return compilation_unit->get_tree_nodes();
}

auto amun::Parser::parse_declaration_statement() -> Shared<Statement>
{
    switch (peek_current().kind) {
    case TokenKind::TOKEN_PREFIX:
    case TokenKind::TOKEN_INFIX:
    case TokenKind::TOKEN_POSTFIX: {
        auto call_kind = amun::FunctionKind::PREFIX_FUNCTION;
        if (peek_current().kind == TokenKind::TOKEN_INFIX) {
            call_kind = amun::FunctionKind::INFIX_FUNCTION;
        }
        else if (peek_current().kind == TokenKind::TOKEN_POSTFIX) {
            call_kind = amun::FunctionKind::POSTFIX_FUNCTION;
        }
        advanced_token();

        if (is_current_kind(TokenKind::TOKEN_EXTERN)) {
            return parse_function_prototype(call_kind, true);
        }
        else if (is_current_kind(TokenKind::TOKEN_FUN)) {
            return parse_function_declaration(call_kind);
        }

        context->diagnostics.report_error(
            peek_current().position, "Prefix, Infix, postfix keyword used only with functions");
        throw "Stop";
    }
    case TokenKind::TOKEN_EXTERN: {
        return parse_function_prototype(amun::FunctionKind::NORMAL_FUNCTION, true);
    }
    case TokenKind::TOKEN_INTRINSIC: {
        return parse_intrinsic_prototype();
    }
    case TokenKind::TOKEN_FUN: {
        return parse_function_declaration(amun::FunctionKind::NORMAL_FUNCTION);
    }
    case TokenKind::TOKEN_OPERATOR: {
        return parse_operator_function_declaraion();
    }
    case TokenKind::TOKEN_VAR: {
        return parse_field_declaration(true);
    }
    case TokenKind::TOKEN_CONST: {
        return parse_compiletime_constants_declaraion();
    }
    case TokenKind::TOKEN_STRUCT: {
        return parse_structure_declaration(false);
    }
    case TokenKind::TOKEN_PACKED: {
        advanced_token();
        return parse_structure_declaration(true);
    }
    case TokenKind::TOKEN_ENUM: {
        return parse_enum_declaration();
    }
    default: {
        context->diagnostics.report_error(peek_current().position,
                                          "Invalid top level declaration statement");
        throw "Stop";
    }
    }
}

auto amun::Parser::parse_statement() -> Shared<Statement>
{
    switch (peek_current().kind) {
    case TokenKind::TOKEN_VAR: {
        return parse_field_declaration(false);
    }
    case TokenKind::TOKEN_CONST: {
        return parse_compiletime_constants_declaraion();
    }
    case TokenKind::TOKEN_IF: {
        return parse_if_statement();
    }
    case TokenKind::TOKEN_FOR: {
        return parse_for_statement();
    }
    case TokenKind::TOKEN_WHILE: {
        return parse_while_statement();
    }
    case TokenKind::TOKEN_SWITCH: {
        return parse_switch_statement();
    }
    case TokenKind::TOKEN_RETURN: {
        return parse_return_statement();
    }
    case TokenKind::TOKEN_DEFER: {
        return parse_defer_statement();
    }
    case TokenKind::TOKEN_BREAK: {
        return parse_break_statement();
    }
    case TokenKind::TOKEN_CONTINUE: {
        return parse_continue_statement();
    }
    case TokenKind::TOKEN_OPEN_BRACE: {
        return parse_block_statement();
    }
    default: {
        return parse_expression_statement();
    }
    }
}

auto amun::Parser::parse_field_declaration(bool is_global) -> Shared<FieldDeclaration>
{
    assert_kind(TokenKind::TOKEN_VAR, "Expect var keyword.");
    auto name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier as variable name.");

    if (is_current_kind(TokenKind::TOKEN_COLON)) {
        advanced_token();
        auto type = parse_type();
        Shared<Expression> initalizer;
        if (is_current_kind(TokenKind::TOKEN_EQUAL)) {
            assert_kind(TokenKind::TOKEN_EQUAL, "Expect `=` after field declaraion name.");
            initalizer = parse_expression();
        }
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after field declaration");
        return std::make_shared<FieldDeclaration>(name, type, initalizer, is_global);
    }

    assert_kind(TokenKind::TOKEN_EQUAL, "Expect `=` or `:` after field declaraion name.");
    auto value = parse_expression();
    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after field declaration");
    return std::make_shared<FieldDeclaration>(name, amun::none_type, value, is_global);
}

auto amun::Parser::parse_intrinsic_prototype() -> Shared<IntrinsicPrototype>
{
    auto intrinsic_keyword = consume_kind(TokenKind::TOKEN_INTRINSIC, "Expect intrinsic keyword");

    std::string intrinsic_name;
    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        advanced_token();
        auto intrinsic_token =
            consume_kind(TokenKind::TOKEN_STRING, "Expect intrinsic native name.");
        intrinsic_name = intrinsic_token.literal;
        if (!is_valid_intrinsic_name(intrinsic_name)) {
            context->diagnostics.report_error(intrinsic_token.position,
                                              "Intrinsic name can't have space or be empty");
            throw "Stop";
        }
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after native intrinsic name.");
    }

    assert_kind(TokenKind::TOKEN_FUN, "Expect function keyword.");
    Token name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier as function name.");

    bool is_generic_function = is_current_kind(TokenKind::TOKEN_SMALLER);
    if (is_generic_function) {
        context->diagnostics.report_error(name.position,
                                          "intrinsic function can't has generic parameter");
        throw "Stop";
    }

    if (intrinsic_name.empty()) {
        intrinsic_name = name.literal;
    }

    bool has_varargs = false;
    Shared<amun::Type> varargs_type = nullptr;
    std::vector<Shared<Parameter>> parameters;
    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        advanced_token();
        while (is_source_available() && not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
            if (has_varargs) {
                context->diagnostics.report_error(
                    previous_token->position, "Varargs must be the last parameter in the function");
                throw "Stop";
            }

            if (is_current_kind(TokenKind::TOKEN_VARARGS)) {
                advanced_token();
                if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) &&
                    current_token->literal == "Any") {
                    advanced_token();
                }
                else {
                    varargs_type = parse_type();
                }
                has_varargs = true;
                continue;
            }

            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after function parameters.");
    }

    // Register current function declaration kind
    context->functions[name.literal] = amun::FunctionKind::NORMAL_FUNCTION;

    // If function prototype has no explicit return type,
    // make return type to be void
    Shared<amun::Type> return_type;
    if (is_current_kind(TokenKind::TOKEN_SEMICOLON) ||
        is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        return_type = amun::void_type;
    }
    else {
        return_type = parse_type();
    }

    // Function can't return fixed size array, you can use pointer format to return allocated array
    if (return_type->type_kind == amun::TypeKind::STATIC_ARRAY) {
        context->diagnostics.report_error(name.position, "Function cannot return array type " +
                                                             amun::get_type_literal(return_type));
        throw "Stop";
    }

    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after external function declaration");

    return std::make_shared<IntrinsicPrototype>(name, intrinsic_name, parameters, return_type,
                                                has_varargs, varargs_type);
}

auto amun::Parser::parse_function_prototype(amun::FunctionKind kind, bool is_external)
    -> Shared<FunctionPrototype>
{
    if (is_external) {
        assert_kind(TokenKind::TOKEN_EXTERN, "Expect external keyword");
    }

    assert_kind(TokenKind::TOKEN_FUN, "Expect function keyword.");
    Token name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier as function name.");

    // Parse generic parameters declarations if they exists
    std::vector<std::string> generics_parameters;
    bool is_generic_function = is_current_kind(TokenKind::TOKEN_SMALLER);
    if (is_external && is_generic_function) {
        context->diagnostics.report_error(name.position,
                                          "external function can't has generic parameter");
        throw "Stop";
    }

    if (is_generic_function) {
        advanced_token();
        while (is_source_available() && !is_current_kind(TokenKind::TOKEN_GREATER)) {
            auto parameter = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect parameter name");
            check_generic_parameter_name(parameter);
            generics_parameters.push_back(parameter.literal);
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }

        assert_kind(TokenKind::TOKEN_GREATER, "Expect > after struct type parameters");
    }

    bool has_varargs = false;
    Shared<amun::Type> varargs_type = nullptr;
    std::vector<Shared<Parameter>> parameters;
    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        advanced_token();
        while (is_source_available() && not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
            if (has_varargs) {
                context->diagnostics.report_error(
                    previous_token->position, "Varargs must be the last parameter in the function");
                throw "Stop";
            }

            if (is_current_kind(TokenKind::TOKEN_VARARGS)) {
                advanced_token();
                if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) &&
                    current_token->literal == "Any") {
                    advanced_token();
                }
                else {
                    varargs_type = parse_type();
                }
                has_varargs = true;
                continue;
            }

            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after function parameters.");
    }

    auto parameters_size = parameters.size();

    if (kind == amun::FunctionKind::PREFIX_FUNCTION && parameters_size != 1) {
        context->diagnostics.report_error(name.position,
                                          "Prefix function must have exactly one parameter");
        throw "Stop";
    }

    if (kind == amun::FunctionKind::INFIX_FUNCTION && parameters_size != 2) {
        context->diagnostics.report_error(name.position,
                                          "Infix function must have exactly Two parameter");
        throw "Stop";
    }

    if (kind == amun::FunctionKind::POSTFIX_FUNCTION && parameters_size != 1) {
        context->diagnostics.report_error(name.position,
                                          "Postfix function must have exactly one parameter");
        throw "Stop";
    }

    // Register current function declaration kind
    context->functions[name.literal] = kind;

    // If function prototype has no explicit return type,
    // make return type to be void
    Shared<amun::Type> return_type;
    if (is_current_kind(TokenKind::TOKEN_SEMICOLON) ||
        is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        return_type = amun::void_type;
    }
    else {
        return_type = parse_type();
    }

    // External function has no body so must end with ;
    if (is_external) {
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after external function declaration");
    }

    // Perform special validation for main function
    if (name.literal == "main") {
        // Check that main hasn't prefix, infix or postfix keyword
        if (kind != amun::FunctionKind::NORMAL_FUNCTION) {
            context->diagnostics.report_error(name.position,
                                              "main can't be prefix, infix or postfix function");
            throw "Stop";
        }

        // Check that main isn't external
        if (is_external) {
            context->diagnostics.report_error(name.position, "main can't be external function");
            throw "Stop";
        }

        // Check that return type is void or any integer type
        if (!(amun::is_void_type(return_type) || amun::is_integer32_type(return_type) ||
              amun::is_integer64_type(return_type))) {
            context->diagnostics.report_error(
                name.position, "main has invalid return type expect void, int32 or int64");
            throw "Stop";
        }
    }

    return std::make_shared<FunctionPrototype>(name, parameters, return_type, is_external,
                                               has_varargs, varargs_type, is_generic_function,
                                               generics_parameters);
}

auto amun::Parser::parse_function_declaration(amun::FunctionKind kind)
    -> Shared<FunctionDeclaration>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = amun::AstNodeScope::FUNCTION_SCOPE;
    context->constants_table_map.push_new_scope();

    auto prototype = parse_function_prototype(kind, false);

    if (is_current_kind(TokenKind::TOKEN_EQUAL)) {
        auto equal_token = peek_and_advance_token();
        auto value = parse_expression();
        auto return_statement = std::make_shared<ReturnStatement>(equal_token, value, true);
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after function value");
        current_ast_scope = parent_node_scope;
        context->constants_table_map.pop_current_scope();
        return std::make_shared<FunctionDeclaration>(prototype, return_statement);
    }

    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        loop_levels_stack.push(0);
        auto block = parse_block_statement();

        check_unnecessary_semicolon_warning();

        loop_levels_stack.pop();
        auto close_brace = previous_token;

        // If function return type is void and has no return at the end, emit one
        if (amun::is_void_type(prototype->get_return_type()) &&
            (block->statements.empty() ||
             block->statements.back()->get_ast_node_type() != AstNodeType::AST_RETURN)) {
            auto void_return =
                std::make_shared<ReturnStatement>(close_brace.value(), nullptr, false);
            block->statements.push_back(void_return);
        }

        current_ast_scope = parent_node_scope;
        context->constants_table_map.pop_current_scope();
        return std::make_shared<FunctionDeclaration>(prototype, block);
    }

    auto posiiton = peek_previous().position;
    context->diagnostics.report_error(
        posiiton, "function declaration without a body: `{ <body> }` or `= <value>;`");
    throw "Stop";
}

auto amun::Parser::parse_operator_function_declaraion() -> Shared<OperatorFunctionDeclaraion>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = amun::AstNodeScope::FUNCTION_SCOPE;
    context->constants_table_map.push_new_scope();

    auto operator_keyword = peek_and_advance_token();
    auto operator_token = peek_and_advance_token();
    if (is_previous_kind(TokenKind::TOKEN_GREATER) && is_current_kind(TokenKind::TOKEN_GREATER)) {
        advanced_token();
        operator_token.kind = TokenKind::TOKEN_RIGHT_SHIFT;
    }

    if (!is_supported_overloading_operator(operator_token.kind)) {
        context->diagnostics.report_error(operator_keyword.position,
                                          "Unsupported Operator for operator overloading function");
        throw "Stop";
    }

    std::vector<Shared<Parameter>> parameters;
    parameters.reserve(2);
    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        advanced_token();
        while (is_source_available() && not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after function parameters.");
    }

    if (parameters.size() != 2) {
        context->diagnostics.report_error(operator_keyword.position,
                                          "Invalid number of parameters, expect 2");
        throw "Stop";
    }

    auto left_type = parameters[0]->type;
    auto right_type = parameters[1]->type;
    auto mangled_name = mangle_operator_function(operator_token.kind, left_type, right_type);
    Token name = {TokenKind::TOKEN_IDENTIFIER, operator_token.position, mangled_name};

    Shared<amun::Type> return_type;
    if (is_current_kind(TokenKind::TOKEN_SEMICOLON) ||
        is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        return_type = amun::void_type;
    }
    else {
        return_type = parse_type();
    }

    auto prototype = std::make_shared<FunctionPrototype>(name, parameters, return_type);

    if (is_current_kind(TokenKind::TOKEN_EQUAL)) {
        auto equal_token = peek_and_advance_token();
        auto value = parse_expression();
        auto return_statement = std::make_shared<ReturnStatement>(equal_token, value, true);
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; after function value");
        current_ast_scope = parent_node_scope;
        context->constants_table_map.pop_current_scope();
        auto declaration = std::make_shared<FunctionDeclaration>(prototype, return_statement);
        return std::make_shared<OperatorFunctionDeclaraion>(operator_token, declaration);
    }

    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        loop_levels_stack.push(0);
        auto block = parse_block_statement();

        check_unnecessary_semicolon_warning();

        loop_levels_stack.pop();
        auto close_brace = previous_token;

        // If function return type is void and has no return at the end, emit one
        if (amun::is_void_type(prototype->get_return_type()) &&
            (block->statements.empty() ||
             block->statements.back()->get_ast_node_type() != AstNodeType::AST_RETURN)) {
            auto void_return =
                std::make_shared<ReturnStatement>(close_brace.value(), nullptr, false);
            block->statements.push_back(void_return);
        }

        current_ast_scope = parent_node_scope;
        context->constants_table_map.pop_current_scope();
        auto declaration = std::make_shared<FunctionDeclaration>(prototype, block);
        return std::make_shared<OperatorFunctionDeclaraion>(operator_token, declaration);
    }

    auto posiiton = operator_keyword.position;
    context->diagnostics.report_error(
        posiiton, "operator function declaration without a body: `{ <body> }` or `= <value>;`");
    throw "Stop";
}

auto amun::Parser::parse_structure_declaration(bool is_packed) -> Shared<StructDeclaration>
{
    auto struct_token = consume_kind(TokenKind::TOKEN_STRUCT, "Expect struct keyword");
    auto struct_name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect Symbol as struct name");
    auto struct_name_str = struct_name.literal;

    // Make sure this name is unique
    if (context->structures.contains(struct_name_str)) {
        context->diagnostics.report_error(struct_name.position,
                                          "There is already struct with name " + struct_name_str);
        throw "Stop";
    }

    // Make sure this name is unique and no alias use it
    if (context->type_alias_table.contains(struct_name_str)) {
        context->diagnostics.report_error(struct_name.position,
                                          "There is already a type with name " + struct_name_str);
        throw "Stop";
    }

    current_struct_name = struct_name.literal;

    std::vector<std::string> generics_parameters;

    // Parse generic parameters declarations if they exists
    bool is_generic_struct = is_current_kind(TokenKind::TOKEN_SMALLER);
    if (is_generic_struct) {
        advanced_token();
        while (is_source_available() && !is_current_kind(TokenKind::TOKEN_GREATER)) {
            auto parameter = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect parameter name");
            check_generic_parameter_name(parameter);
            generics_parameters.push_back(parameter.literal);
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::TOKEN_GREATER, "Expect > after struct type parameters");
    }

    std::vector<std::string> fields_names;
    std::vector<Shared<amun::Type>> fields_types;
    assert_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { after struct name");
    while (is_source_available() && !is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        auto field_name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect Symbol as struct name");

        if (is_contains(fields_names, field_name.literal)) {
            context->diagnostics.report_error(field_name.position,
                                              "There is already struct member with name " +
                                                  field_name.literal);
            throw "Stop";
        }

        fields_names.push_back(field_name.literal);
        auto field_type = parse_type();

        // Handle Incomplete field type case
        if (field_type->type_kind == amun::TypeKind::NONE) {
            context->diagnostics.report_error(
                field_name.position, "Field type isn't fully defined yet, you can't use it "
                                     "until it defined but you can use *" +
                                         struct_name_str);
            throw "Stop";
        }

        fields_types.push_back(field_type);
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect ; at the end of struct field declaration");
    }

    assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } in the end of struct declaration");

    check_unnecessary_semicolon_warning();

    auto structure_type =
        std::make_shared<amun::StructType>(struct_name_str, fields_names, fields_types,
                                           generics_parameters, is_packed, is_generic_struct);

    // Resolve un solved types
    // This code will executed only if there are field with type of pointer to the current struct
    // In parsing type function we return a pointer to amun none type,
    // Now we replace it with pointer to current struct type after it created
    if (current_struct_unknown_fields > 0) {
        // Pointer to current struct type
        auto struct_pointer_ty = std::make_shared<amun::PointerType>(structure_type);

        const auto fields_size = fields_types.size();
        for (size_t i = 0; i < fields_size; i++) {
            const auto field_type = fields_types[i];

            // If Field type is pointer to none that mean it point to struct itself
            if (amun::is_pointer_of_type(field_type, amun::none_type)) {
                // Update field type from *None to *Itself
                structure_type->fields_types[i] = struct_pointer_ty;
                current_struct_unknown_fields--;
                continue;
            }

            // Update field type from [?]*None to [?]*Itself
            if (amun::is_array_of_type(field_type, amun::none_ptr_type)) {
                auto array_type = std::static_pointer_cast<amun::StaticArrayType>(field_type);
                array_type->element_type = struct_pointer_ty;

                structure_type->fields_types[i] = array_type;
                current_struct_unknown_fields--;
                continue;
            }
        }
    }

    assert(current_struct_unknown_fields == 0);

    context->structures[struct_name_str] = structure_type;
    context->type_alias_table.define_alias(struct_name_str, structure_type);
    current_struct_name = "";
    generic_parameters_names.clear();
    return std::make_shared<StructDeclaration>(structure_type);
}

auto amun::Parser::parse_enum_declaration() -> Shared<EnumDeclaration>
{
    auto enum_token = consume_kind(TokenKind::TOKEN_ENUM, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect Symbol as enum name");

    Shared<amun::Type> element_type = nullptr;

    if (is_current_kind(TokenKind::TOKEN_COLON)) {
        advanced_token();
        element_type = parse_type();
    }
    else {
        // Default enumeration element type is integer 32
        element_type = amun::i32_type;
    }

    assert_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { after enum name");
    std::vector<Token> enum_values;
    std::unordered_map<std::string, int> enum_values_indexes;
    std::set<int> explicit_values;
    int index = 0;
    bool has_explicit_values = false;

    while (is_source_available() && !is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        auto enum_value = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect Symbol as enum value");
        enum_values.push_back(enum_value);
        auto enum_field_literal = enum_value.literal;

        if (enum_values_indexes.contains(enum_field_literal)) {
            context->diagnostics.report_error(enum_value.position,
                                              "Can't declare 2 elements with the same name");
            throw "Stop";
        }

        if (is_current_kind(TokenKind::TOKEN_EQUAL)) {
            advanced_token();
            auto field_value = parse_expression();
            if (field_value->get_ast_node_type() != AstNodeType::AST_NUMBER) {
                context->diagnostics.report_error(
                    enum_value.position, "Enum field explicit value must be integer expression");
                throw "Stop";
            }

            auto number_expr = std::dynamic_pointer_cast<NumberExpression>(field_value);
            auto number_value_token = number_expr->get_value();
            if (is_float_number_token(number_value_token)) {
                context->diagnostics.report_error(
                    enum_value.position,
                    "Enum field explicit value must be integer value not float");
                throw "Stop";
            }

            auto explicit_value = std::stoi(number_value_token.literal);
            if (explicit_values.contains(explicit_value)) {
                context->diagnostics.report_error(
                    enum_value.position, "There is also one enum field with explicit value " +
                                             std::to_string(explicit_value));
                throw "Stop";
            }

            explicit_values.insert(explicit_value);
            enum_values_indexes[enum_field_literal] = explicit_value;
            has_explicit_values = true;
        }
        else {
            if (has_explicit_values) {
                context->diagnostics.report_error(
                    enum_value.position,
                    "You must add explicit value to all enum fields or to no one");
                throw "Stop";
            }

            enum_values_indexes[enum_field_literal] = index++;
        }

        if (is_current_kind(TokenKind::TOKEN_COMMA)) {
            advanced_token();
        }
    }
    assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } in the end of enum declaration");

    check_unnecessary_semicolon_warning();

    auto enum_type = std::make_shared<amun::EnumType>(enum_name, enum_values_indexes, element_type);
    context->enumerations[enum_name.literal] = enum_type;
    return std::make_shared<EnumDeclaration>(enum_name, enum_type);
}

auto amun::Parser::parse_parameter() -> Shared<Parameter>
{
    Token name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier as parameter name.");
    auto type = parse_type();
    return std::make_shared<Parameter>(name, type);
}

auto amun::Parser::parse_block_statement() -> Shared<BlockStatement>
{
    consume_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { on the start of block.");
    std::vector<Shared<Statement>> statements;
    while (is_source_available() && !is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        statements.push_back(parse_statement());
    }
    consume_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } on the end of block.");
    return std::make_shared<BlockStatement>(statements);
}

auto amun::Parser::parse_return_statement() -> Shared<ReturnStatement>
{
    auto keyword = consume_kind(TokenKind::TOKEN_RETURN, "Expect return keyword.");
    if (is_current_kind(TokenKind::TOKEN_SEMICOLON)) {
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after return keyword");
        return std::make_shared<ReturnStatement>(keyword, nullptr, false);
    }
    auto value = parse_expression();
    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after return statement");
    return std::make_shared<ReturnStatement>(keyword, value, true);
}

auto amun::Parser::parse_defer_statement() -> Shared<DeferStatement>
{
    auto defer_token = consume_kind(TokenKind::TOKEN_DEFER, "Expect Defer keyword.");
    auto expression = parse_expression();
    if (auto call_expression = std::dynamic_pointer_cast<CallExpression>(expression)) {
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after defer call statement");
        return std::make_shared<DeferStatement>(defer_token, call_expression);
    }

    context->diagnostics.report_error(defer_token.position, "defer keyword expect call expression");
    throw "Stop";
}

auto amun::Parser::parse_break_statement() -> Shared<BreakStatement>
{
    auto break_token = consume_kind(TokenKind::TOKEN_BREAK, "Expect break keyword.");

    if (current_ast_scope != amun::AstNodeScope::CONDITION_SCOPE or loop_levels_stack.top() == 0) {
        context->diagnostics.report_error(
            break_token.position, "break keyword can only be used inside at last one while loop");
        throw "Stop";
    }

    if (is_current_kind(TokenKind::TOKEN_SEMICOLON)) {
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after break call statement");
        return std::make_shared<BreakStatement>(break_token, false, 1);
    }

    auto break_times = parse_expression();
    if (auto number_expr = std::dynamic_pointer_cast<NumberExpression>(break_times)) {
        auto number_value = number_expr->get_value();
        if (is_float_number_token(number_value)) {
            context->diagnostics.report_error(
                break_token.position,
                "expect break keyword times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.literal);
        if (times_int < 1) {
            context->diagnostics.report_error(
                break_token.position, "expect break times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_levels_stack.top()) {
            context->diagnostics.report_error(
                break_token.position, "break times can't be bigger than the number of loops you "
                                      "have, expect less than or equals " +
                                          std::to_string(loop_levels_stack.top()));
            throw "Stop";
        }

        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after brea statement");
        return std::make_shared<BreakStatement>(break_token, true, times_int);
    }

    context->diagnostics.report_error(break_token.position, "break keyword times must be a number");
    throw "Stop";
}

auto amun::Parser::parse_continue_statement() -> Shared<ContinueStatement>
{
    auto continue_token = consume_kind(TokenKind::TOKEN_CONTINUE, "Expect continue keyword.");

    if (current_ast_scope != amun::AstNodeScope::CONDITION_SCOPE or loop_levels_stack.top() == 0) {
        context->diagnostics.report_error(
            continue_token.position,
            "continue keyword can only be used inside at last one while loop");
        throw "Stop";
    }

    if (is_current_kind(TokenKind::TOKEN_SEMICOLON)) {
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after defer call statement");
        return std::make_shared<ContinueStatement>(continue_token, false, 1);
    }

    auto continue_times = parse_expression();
    if (auto number_expr = std::dynamic_pointer_cast<NumberExpression>(continue_times)) {
        auto number_value = number_expr->get_value();
        auto number_kind = number_value.kind;
        if (number_kind == TokenKind::TOKEN_FLOAT or number_kind == TokenKind::TOKEN_FLOAT32 or
            number_kind == TokenKind::TOKEN_FLOAT64) {

            context->diagnostics.report_error(
                continue_token.position,
                "expect continue times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.literal);
        if (times_int < 1) {
            context->diagnostics.report_error(
                continue_token.position,
                "expect continue times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_levels_stack.top()) {
            context->diagnostics.report_error(
                continue_token.position,
                "continue times can't be bigger than the number of loops you "
                "have, expect less than or equals " +
                    std::to_string(loop_levels_stack.top()));
            throw "Stop";
        }

        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after brea statement");
        return std::make_shared<ContinueStatement>(continue_token, true, times_int);
    }

    context->diagnostics.report_error(continue_token.position,
                                      "continue keyword times must be a number");
    throw "Stop";
}

auto amun::Parser::parse_if_statement() -> Shared<IfStatement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = amun::AstNodeScope::CONDITION_SCOPE;

    auto if_token = consume_kind(TokenKind::TOKEN_IF, "Expect If keyword.");
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before if condition");
    auto condition = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after if condition");
    auto then_block = parse_statement();
    auto conditional_block = std::make_shared<ConditionalBlock>(if_token, condition, then_block);
    std::vector<Shared<ConditionalBlock>> conditional_blocks;
    conditional_blocks.push_back(conditional_block);

    bool has_else_branch = false;
    while (is_source_available() && is_current_kind(TokenKind::TOKEN_ELSE)) {
        auto else_token = consume_kind(TokenKind::TOKEN_ELSE, "Expect else keyword.");
        if (is_current_kind(TokenKind::TOKEN_IF)) {
            advanced_token();
            auto elif_condition = parse_expression();
            auto elif_block = parse_statement();
            auto elif_condition_block =
                std::make_shared<ConditionalBlock>(else_token, elif_condition, elif_block);
            conditional_blocks.push_back(elif_condition_block);
            continue;
        }

        if (has_else_branch) {
            context->diagnostics.report_error(else_token.position,
                                              "You already declared else branch");
            throw "Stop";
        }

        auto true_value_token = else_token;
        true_value_token.kind = TokenKind::TOKEN_TRUE;
        auto true_expression = std::make_shared<BooleanExpression>(true_value_token);
        auto else_block = parse_statement();
        auto else_condition_block =
            std::make_shared<ConditionalBlock>(else_token, true_expression, else_block);
        conditional_blocks.push_back(else_condition_block);
        has_else_branch = true;
    }
    current_ast_scope = parent_node_scope;
    return std::make_shared<IfStatement>(conditional_blocks, has_else_branch);
}

auto amun::Parser::parse_for_statement() -> Shared<Statement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = amun::AstNodeScope::CONDITION_SCOPE;

    auto keyword = consume_kind(TokenKind::TOKEN_FOR, "Expect for keyword.");

    // Parse for ever statement
    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
        loop_levels_stack.top() += 1;
        auto body = parse_statement();
        loop_levels_stack.top() -= 1;
        current_ast_scope = parent_node_scope;
        return std::make_shared<ForeverStatement>(keyword, body);
    }

    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before for names and collection");

    // Parse optional element name or it as default
    std::string element_name = "it";
    std::string index_name = "it_index";
    auto expr = parse_expression();
    if (is_current_kind(TokenKind::TOKEN_COLON) || is_current_kind(TokenKind::TOKEN_COMMA)) {
        if (expr->get_ast_node_type() != AstNodeType::AST_LITERAL) {
            context->diagnostics.report_error(keyword.position,
                                              "Optional named variable must be identifier");
            throw "Stop";
        }

        if (is_current_kind(TokenKind::TOKEN_COMMA)) {
            index_name = previous_token->literal;
            // Consume the comma
            advanced_token();
            element_name = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect element name").literal;
        }
        else {
            // Previous token is the LiteralExpression that contains variable name
            element_name = previous_token->literal;
        }

        assert_kind(TokenKind::TOKEN_COLON, "Expect `:` after element name in foreach");
        expr = parse_expression();
    }

    // For range statemnet for x -> y
    if (is_current_kind(TokenKind::TOKEN_DOT_DOT)) {
        advanced_token();
        const auto range_end = parse_expression();

        // If there is : after range the mean we have custom step expression
        // If not use nullptr as default value for step (Handled in the Backend)
        Shared<Expression> step = nullptr;
        if (is_current_kind(TokenKind::TOKEN_COLON)) {
            advanced_token();
            step = parse_expression();
        }

        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after for names and collection");

        loop_levels_stack.top() += 1;
        auto body = parse_statement();
        loop_levels_stack.top() -= 1;

        current_ast_scope = parent_node_scope;

        return std::make_shared<ForRangeStatement>(keyword, element_name, expr, range_end, step,
                                                   body);
    }

    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after for names and collection");

    // Parse For each statement
    loop_levels_stack.top() += 1;
    auto body = parse_statement();
    loop_levels_stack.top() -= 1;

    current_ast_scope = parent_node_scope;

    return std::make_shared<ForEachStatement>(keyword, element_name, index_name, expr, body);
}

auto amun::Parser::parse_while_statement() -> Shared<WhileStatement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = amun::AstNodeScope::CONDITION_SCOPE;

    auto keyword = consume_kind(TokenKind::TOKEN_WHILE, "Expect while keyword.");

    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before while condition");
    auto condition = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after while condition");

    loop_levels_stack.top() += 1;
    auto body = parse_statement();
    loop_levels_stack.top() -= 1;

    current_ast_scope = parent_node_scope;
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

auto amun::Parser::parse_switch_statement() -> Shared<SwitchStatement>
{
    auto keyword = consume_kind(TokenKind::TOKEN_SWITCH, "Expect switch keyword.");

    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before switch argument");
    auto argument = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after switch argument");

    assert_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { after switch value");

    std::vector<Shared<SwitchCase>> switch_cases;
    Shared<SwitchCase> default_branch = nullptr;
    bool has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        std::vector<Shared<Expression>> values;
        if (is_current_kind(TokenKind::TOKEN_ELSE)) {
            if (has_default_branch) {
                context->diagnostics.report_error(
                    keyword.position, "Switch statementscan't has more than one default branch");
                throw "Stop";
            }
            auto else_keyword =
                consume_kind(TokenKind::TOKEN_ELSE, "Expect else keyword in switch default branch");
            consume_kind(TokenKind::TOKEN_RIGHT_ARROW,
                         "Expect -> after else keyword in switch default branch");
            auto default_body = parse_statement();
            default_branch = std::make_shared<SwitchCase>(else_keyword, values, default_body);
            has_default_branch = true;
            continue;
        }

        // Parse all values for this case V1, V2, ..., Vn ->
        while (is_source_available() and !is_current_kind(TokenKind::TOKEN_RIGHT_ARROW)) {
            auto value = parse_expression();
            values.push_back(value);
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }
        auto right_arrow =
            consume_kind(TokenKind::TOKEN_RIGHT_ARROW, "Expect -> after branch value");
        auto branch = parse_statement();
        auto switch_case = std::make_shared<SwitchCase>(right_arrow, values, branch);
        switch_cases.push_back(switch_case);
    }
    assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } after switch Statement last branch");
    return std::make_shared<SwitchStatement>(keyword, argument, switch_cases, default_branch);
}

auto amun::Parser::parse_expression_statement() -> Shared<ExpressionStatement>
{
    auto expression = parse_expression();
    assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after field declaration");
    return std::make_shared<ExpressionStatement>(expression);
}

auto amun::Parser::parse_expression() -> Shared<Expression>
{
    return parse_assignment_expression();
}

auto amun::Parser::parse_assignment_expression() -> Shared<Expression>
{
    auto expression = parse_logical_or_expression();
    if (is_assignments_operator_token(peek_current())) {
        auto assignments_token = peek_and_advance_token();
        Shared<Expression> right_value;
        auto assignments_token_kind = assignments_token.kind;
        if (assignments_token_kind == TokenKind::TOKEN_EQUAL) {
            right_value = parse_assignment_expression();
        }
        else {
            assignments_token.kind = assignments_binary_operators[assignments_token_kind];
            auto right_expression = parse_assignment_expression();
            right_value =
                std::make_shared<BinaryExpression>(expression, assignments_token, right_expression);
        }
        return std::make_shared<AssignExpression>(expression, assignments_token, right_value);
    }
    return expression;
}

auto amun::Parser::parse_logical_or_expression() -> Shared<Expression>
{
    auto expression = parse_logical_and_expression();
    while (is_current_kind(TokenKind::TOKEN_OR_OR)) {
        auto or_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, or_token, right);
    }
    return expression;
}

auto amun::Parser::parse_logical_and_expression() -> Shared<Expression>
{
    auto expression = parse_equality_expression();
    while (is_current_kind(TokenKind::TOKEN_AND_AND)) {
        auto and_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, and_token, right);
    }
    return expression;
}

auto amun::Parser::parse_equality_expression() -> Shared<Expression>
{
    auto expression = parse_comparison_expression();
    while (is_current_kind(TokenKind::TOKEN_EQUAL_EQUAL) ||
           is_current_kind(TokenKind::TOKEN_BANG_EQUAL)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_comparison_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

auto amun::Parser::parse_comparison_expression() -> Shared<Expression>
{
    auto expression = parse_shift_expression();
    while (is_current_kind(TokenKind::TOKEN_GREATER) ||
           is_current_kind(TokenKind::TOKEN_GREATER_EQUAL) ||
           is_current_kind(TokenKind::TOKEN_SMALLER) ||
           is_current_kind(TokenKind::TOKEN_SMALLER_EQUAL)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_shift_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

auto amun::Parser::parse_shift_expression() -> Shared<Expression>
{
    auto expression = parse_term_expression();
    while (true) {
        if (is_current_kind(TokenKind::TOKEN_LEFT_SHIFT)) {
            auto operator_token = peek_and_advance_token();
            auto right = parse_term_expression();
            expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
            continue;
        }

        if (is_current_kind(TokenKind::TOKEN_GREATER) && is_next_kind(TokenKind::TOKEN_GREATER)) {
            advanced_token();
            auto operator_token = peek_and_advance_token();
            operator_token.kind = TokenKind::TOKEN_RIGHT_SHIFT;
            auto right = parse_term_expression();
            expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
            continue;
        }

        break;
    }

    return expression;
}

auto amun::Parser::parse_term_expression() -> Shared<Expression>
{
    auto expression = parse_factor_expression();
    while (is_current_kind(TokenKind::TOKEN_PLUS) || is_current_kind(TokenKind::TOKEN_MINUS)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_factor_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

auto amun::Parser::parse_factor_expression() -> Shared<Expression>
{
    auto expression = parse_enum_access_expression();
    while (is_current_kind(TokenKind::TOKEN_STAR) || is_current_kind(TokenKind::TOKEN_SLASH) ||
           is_current_kind(TokenKind::TOKEN_PERCENT)) {
        Token operator_token = peek_and_advance_token();
        auto right = parse_enum_access_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

auto amun::Parser::parse_enum_access_expression() -> Shared<Expression>
{
    auto expression = parse_infix_call_expression();
    if (is_current_kind(TokenKind::TOKEN_COLON_COLON)) {
        auto colons_token = peek_and_advance_token();
        if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression)) {
            auto enum_name = literal->get_name();
            if (context->enumerations.contains(enum_name.literal)) {
                auto enum_type = context->enumerations[enum_name.literal];
                auto element = consume_kind(TokenKind::TOKEN_IDENTIFIER,
                                            "Expect identifier as enum field name");

                auto enum_values = enum_type->values;
                if (not enum_values.contains(element.literal)) {
                    context->diagnostics.report_error(
                        element.position, "Can't find element with name " + element.literal +
                                              " in enum " + enum_name.literal);
                    throw "Stop";
                }

                int index = enum_values[element.literal];
                auto enum_element_type = std::make_shared<amun::EnumElementType>(
                    enum_name.literal, enum_type->element_type);
                return std::make_shared<EnumAccessExpression>(enum_name, element, index,
                                                              enum_element_type);
            }
            else {
                context->diagnostics.report_error(enum_name.position,
                                                  "Can't find enum declaration with name " +
                                                      enum_name.literal);
                throw "Stop";
            }
        }
        else {
            context->diagnostics.report_error(colons_token.position,
                                              "Expect identifier as Enum name");
            throw "Stop";
        }
    }
    return expression;
}

auto amun::Parser::parse_infix_call_expression() -> Shared<Expression>
{
    auto expression = parse_prefix_expression();
    auto current_token_literal = peek_current().literal;

    // Parse Infix function call as a call expression
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) and
        is_function_declaration_kind(current_token_literal, amun::FunctionKind::INFIX_FUNCTION)) {
        auto symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(expression);
        arguments.push_back(parse_infix_call_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }

    return expression;
}

auto amun::Parser::parse_prefix_expression() -> Shared<Expression>
{
    if (is_unary_operator_token(peek_current())) {
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        return std::make_shared<PrefixUnaryExpression>(token, right);
    }

    if (is_current_kind(TokenKind::TOKEN_PLUS_PLUS) or
        is_current_kind(TokenKind::TOKEN_MINUS_MINUS)) {
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        auto ast_node_type = right->get_ast_node_type();
        if ((ast_node_type == AstNodeType::AST_LITERAL) or
            (ast_node_type == AstNodeType::AST_INDEX) or (ast_node_type == AstNodeType::AST_DOT)) {
            return std::make_shared<PrefixUnaryExpression>(token, right);
        }

        context->diagnostics.report_error(
            token.position,
            "Unary ++ or -- expect right expression to be variable or index expression");
        throw "Stop";
    }

    return parse_prefix_call_expression();
}

auto amun::Parser::parse_prefix_call_expression() -> Shared<Expression>
{
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) and
        is_function_declaration_kind(current_token_literal, amun::FunctionKind::PREFIX_FUNCTION)) {
        Token symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(parse_prefix_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_postfix_increment_or_decrement();
}

auto amun::Parser::parse_postfix_increment_or_decrement() -> Shared<Expression>
{
    auto expression = parse_call_or_access_expression();

    if (is_current_kind(TokenKind::TOKEN_PLUS_PLUS) or
        is_current_kind(TokenKind::TOKEN_MINUS_MINUS)) {
        auto token = peek_and_advance_token();
        auto ast_node_type = expression->get_ast_node_type();
        if ((ast_node_type == AstNodeType::AST_LITERAL) or
            (ast_node_type == AstNodeType::AST_INDEX) or (ast_node_type == AstNodeType::AST_DOT)) {
            return std::make_shared<PostfixUnaryExpression>(token, expression);
        }
        context->diagnostics.report_error(
            token.position,
            "Unary ++ or -- expect left expression to be variable or index expression");
        throw "Stop";
    }

    return expression;
}

auto amun::Parser::parse_call_or_access_expression() -> Shared<Expression>
{
    auto expression = parse_enumeration_attribute_expression();
    while (is_current_kind(TokenKind::TOKEN_DOT) || is_current_kind(TokenKind::TOKEN_OPEN_PAREN) ||
           is_current_kind(TokenKind::TOKEN_OPEN_BRACKET) ||
           (is_current_kind(TokenKind::TOKEN_SMALLER) &&
            expression->get_ast_node_type() == AstNodeType::AST_LITERAL)) {

        // Parse structure field access expression
        if (is_current_kind(TokenKind::TOKEN_DOT)) {
            auto dot_token = peek_and_advance_token();

            // Struct field access using field name
            if (is_current_kind(TokenKind::TOKEN_IDENTIFIER)) {
                auto field_name =
                    consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect literal as field name");
                expression = std::make_shared<DotExpression>(dot_token, expression, field_name);
                continue;
            }

            // Tuple field access using field index
            if (is_current_kind(TokenKind::TOKEN_INT)) {
                auto field_name =
                    consume_kind(TokenKind::TOKEN_INT, "Expect literal as field name");
                auto access = std::make_shared<DotExpression>(dot_token, expression, field_name);
                access->field_index = str_to_int(field_name.literal.c_str());
                expression = access;
                continue;
            }

            context->diagnostics.report_error(
                dot_token.position,
                "DotExpression `.` must followed by symnol or integer for struct or tuple access");
            throw "Stop";
        }

        // Parse function call expression with generic parameters
        if (is_current_kind(TokenKind::TOKEN_SMALLER)) {
            auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression);

            if (!context->functions.contains(literal->name.literal)) {
                return expression;
            }

            std::vector<Shared<amun::Type>> generic_arguments;
            auto position = peek_and_advance_token();
            while (!is_current_kind(TokenKind::TOKEN_GREATER)) {
                generic_arguments.push_back(parse_type());
                if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                    advanced_token();
                }
            }

            advanced_token();

            assert_kind(TokenKind::TOKEN_OPEN_PAREN,
                        "Expect ( after in the end of call expression");

            std::vector<Shared<Expression>> arguments;
            while (not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
                arguments.push_back(parse_expression());
                if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                    advanced_token();
                }
            }

            assert_kind(TokenKind::TOKEN_CLOSE_PAREN,
                        "Expect ) after in the end of call expression");

            // Add support for optional lambda after call expression
            if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
                arguments.push_back(parse_lambda_expression());
            }

            expression = std::make_shared<CallExpression>(position, expression, arguments,
                                                          generic_arguments);
            continue;
        }

        // Parse function call expression
        if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
            auto position = peek_and_advance_token();
            std::vector<Shared<Expression>> arguments;
            while (not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
                arguments.push_back(parse_expression());
                if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                    advanced_token();
                }
            }

            assert_kind(TokenKind::TOKEN_CLOSE_PAREN,
                        "Expect ) after in the end of call expression");

            // Add support for optional lambda after call expression
            if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
                arguments.push_back(parse_lambda_expression());
            }

            expression = std::make_shared<CallExpression>(position, expression, arguments);
            continue;
        }

        // Parse Index Expression
        if (is_current_kind(TokenKind::TOKEN_OPEN_BRACKET)) {
            auto position = peek_and_advance_token();
            auto index = parse_expression();
            assert_kind(TokenKind::TOKEN_CLOSE_BRACKET, "Expect ] after index value");
            expression = std::make_shared<IndexExpression>(position, expression, index);
            continue;
        }
    }

    return expression;
}

auto amun::Parser::parse_enumeration_attribute_expression() -> Shared<Expression>
{
    auto expression = parse_postfix_call_expression();
    if (is_current_kind(TokenKind::TOKEN_DOT) and
        expression->get_ast_node_type() == AstNodeType::AST_LITERAL) {
        auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression);
        auto literal_str = literal->get_name().literal;
        if (context->enumerations.contains(literal_str)) {
            auto dot_token = peek_and_advance_token();
            auto attribute =
                consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect attribute name for enum");
            auto attribute_str = attribute.literal;
            if (attribute_str == "count") {
                auto count = context->enumerations[literal_str]->values.size();
                Token number_token = {TokenKind::TOKEN_INT, attribute.position,
                                      std::to_string(count)};
                auto number_type = amun::i64_type;
                return std::make_shared<NumberExpression>(number_token, number_type);
            }

            context->diagnostics.report_error(attribute.position,
                                              "Un supported attribute for enumeration type");
            throw "Stop";
        }
    }
    return expression;
}

auto amun::Parser::parse_postfix_call_expression() -> Shared<Expression>
{
    auto expression = parse_initializer_expression();
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) and
        is_function_declaration_kind(current_token_literal, amun::FunctionKind::POSTFIX_FUNCTION)) {
        Token symbol_token = peek_current();
        auto literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(expression);
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return expression;
}

auto amun::Parser::parse_initializer_expression() -> Shared<Expression>
{
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) &&
        context->type_alias_table.contains(current_token->literal)) {
        auto resolved_type = context->type_alias_table.resolve_alias(current_token->literal);
        if (amun::is_struct_type(resolved_type) || amun::is_generic_struct_type(resolved_type)) {
            if (is_next_kind(TokenKind::TOKEN_OPEN_PAREN) ||
                is_next_kind(TokenKind::TOKEN_OPEN_BRACE) ||
                is_next_kind(TokenKind::TOKEN_SMALLER)) {
                auto type = parse_type();
                auto token = peek_current();

                std::vector<std::shared_ptr<Expression>> arguments;

                // Check if this constructor has arguments
                if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
                    advanced_token();
                    while (not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
                        arguments.push_back(parse_expression());
                        if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                            advanced_token();
                        }
                        else {
                            break;
                        }
                    }

                    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) at the end of initializer");
                }

                // Check if this constructor has outside lambda argument
                if (is_current_kind(TokenKind::TOKEN_OPEN_BRACE)) {
                    auto lambda_argument = parse_lambda_expression();
                    arguments.push_back(lambda_argument);
                }

                return std::make_shared<InitializeExpression>(token, type, arguments);
            }
        }
    }

    return parse_function_call_with_lambda_argument();
}

auto amun::Parser::parse_function_call_with_lambda_argument() -> Shared<Expression>
{
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER) and
        is_next_kind(TokenKind::TOKEN_OPEN_BRACE) and
        is_function_declaration_kind(current_token->literal, amun::FunctionKind::NORMAL_FUNCTION)) {
        auto symbol_token = peek_current();
        auto literal = parse_literal_expression();

        std::vector<Shared<Expression>> arguments;
        arguments.push_back(parse_lambda_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_primary_expression();
}

auto amun::Parser::parse_primary_expression() -> Shared<Expression>
{
    auto current_token_kind = peek_current().kind;
    switch (current_token_kind) {
    case TokenKind::TOKEN_INT:
    case TokenKind::TOKEN_INT1:
    case TokenKind::TOKEN_INT8:
    case TokenKind::TOKEN_INT16:
    case TokenKind::TOKEN_INT32:
    case TokenKind::TOKEN_INT64:
    case TokenKind::TOKEN_UINT8:
    case TokenKind::TOKEN_UINT16:
    case TokenKind::TOKEN_UINT32:
    case TokenKind::TOKEN_UINT64:
    case TokenKind::TOKEN_FLOAT:
    case TokenKind::TOKEN_FLOAT32:
    case TokenKind::TOKEN_FLOAT64: {
        return parse_number_expression();
    }
    case TokenKind::TOKEN_CHARACTER: {
        advanced_token();
        return std::make_shared<CharacterExpression>(peek_previous());
    }
    case TokenKind::TOKEN_STRING: {
        advanced_token();
        return std::make_shared<StringExpression>(peek_previous());
    }
    case TokenKind::TOKEN_TRUE:
    case TokenKind::TOKEN_FALSE: {
        advanced_token();
        return std::make_shared<BooleanExpression>(peek_previous());
    }
    case TokenKind::TOKEN_NULL: {
        advanced_token();
        return std::make_shared<NullExpression>(peek_previous());
    }
    case TokenKind::TOKEN_IDENTIFIER: {
        // Resolve const or non const variable
        auto name = peek_current();
        if (context->constants_table_map.is_defined(name.literal)) {
            advanced_token();
            return context->constants_table_map.lookup(name.literal);
        }
        return parse_literal_expression();
    }
    case TokenKind::TOKEN_OPEN_PAREN: {
        return parse_group_or_tuple_expression();
    }
    case TokenKind::TOKEN_OPEN_BRACKET: {
        return parse_array_expression();
    }
    case TokenKind::TOKEN_OPEN_BRACE: {
        return parse_lambda_expression();
    }
    case TokenKind::TOKEN_IF: {
        return parse_if_expression();
    }
    case TokenKind::TOKEN_SWITCH: {
        return parse_switch_expression();
    }
    case TokenKind::TOKEN_CAST: {
        return parse_cast_expression();
    }
    case TokenKind::TOKEN_TYPE_SIZE: {
        return parse_type_size_expression();
    }
    case TokenKind::TOKEN_VALUE_SIZE: {
        return parse_value_size_expression();
    }
    case TokenKind::TOKEN_HASH: {
        return parse_directive_expression();
    }
    default: {
        unexpected_token_error();
    }
    }
}

auto amun::Parser::parse_lambda_expression() -> Shared<LambdaExpression>
{
    auto open_paren =
        consume_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { at the start of lambda expression");

    std::vector<Shared<Parameter>> parameters;

    Shared<amun::Type> return_type;

    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        advanced_token();

        // Parse Parameters
        while (not is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
            parameters.push_back(parse_parameter());

            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
            else {
                break;
            }
        }

        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after lambda parameters");
        return_type = parse_type();
        assert_kind(TokenKind::TOKEN_RIGHT_ARROW, "Expect -> after lambda return type");
    }
    else {
        // Default return type for lambda expression
        return_type = amun::void_type;
    }

    loop_levels_stack.push(0);

    // Parse lambda expression body
    std::vector<Shared<Statement>> body;
    while (not is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        body.push_back(parse_statement());
    }

    loop_levels_stack.pop();

    auto lambda_body = std::make_shared<BlockStatement>(body);

    auto close_brace =
        consume_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } at the end of lambda expression");

    // If lambda body has no statements or end without return statement
    // we can implicity insert return statement without value
    if (amun::is_void_type(return_type) &&
        (body.empty() || body.back()->get_ast_node_type() != AstNodeType::AST_RETURN)) {
        auto void_return = std::make_shared<ReturnStatement>(close_brace, nullptr, false);
        lambda_body->statements.push_back(void_return);
    }

    return std::make_shared<LambdaExpression>(open_paren, parameters, return_type, lambda_body);
}

auto amun::Parser::parse_number_expression() -> Shared<NumberExpression>
{
    auto number_token = peek_and_advance_token();
    auto number_kind = get_number_kind(number_token.kind);
    auto number_type = std::make_shared<amun::NumberType>(number_kind);
    return std::make_shared<NumberExpression>(number_token, number_type);
}

auto amun::Parser::parse_literal_expression() -> Shared<LiteralExpression>
{
    Token symbol_token = peek_and_advance_token();
    auto type = std::make_shared<amun::NoneType>();
    return std::make_shared<LiteralExpression>(symbol_token, type);
}

auto amun::Parser::parse_if_expression() -> Shared<IfExpression>
{
    Token if_token = peek_and_advance_token();
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before if condition");
    auto condition = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after if condition");
    auto then_value = parse_expression();
    Token else_token =
        consume_kind(TokenKind::TOKEN_ELSE, "Expect `else` keyword after then value.");
    auto else_value = parse_expression();
    return std::make_shared<IfExpression>(if_token, else_token, condition, then_value, else_value);
}

auto amun::Parser::parse_switch_expression() -> Shared<SwitchExpression>
{
    auto keyword = consume_kind(TokenKind::TOKEN_SWITCH, "Expect switch keyword.");
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect ( before swich argument");
    auto argument = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) after swich argument");
    assert_kind(TokenKind::TOKEN_OPEN_BRACE, "Expect { after switch value");
    std::vector<Shared<Expression>> cases;
    std::vector<Shared<Expression>> values;
    Shared<Expression> default_value;
    bool has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::TOKEN_CLOSE_BRACE)) {
        if (is_current_kind(TokenKind::TOKEN_ELSE)) {
            if (has_default_branch) {
                context->diagnostics.report_error(
                    keyword.position, "Switch expression can't has more than one default branch");
                throw "Stop";
            }
            assert_kind(TokenKind::TOKEN_ELSE, "Expect else keyword in switch default branch");
            assert_kind(TokenKind::TOKEN_RIGHT_ARROW,
                        "Expect -> after else keyword in switch default branch");
            default_value = parse_expression();
            assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after switch case value");
            has_default_branch = true;
            continue;
        }

        do {
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                if (cases.size() > values.size()) {
                    advanced_token();
                }
                else {
                    context->diagnostics.report_error(
                        current_token->position,
                        "In Switch expression can't use `,` with no value before it");
                    throw "Stop";
                }
            }

            auto case_expression = parse_expression();
            auto case_node_type = case_expression->get_ast_node_type();
            if (case_node_type == AstNodeType::AST_NUMBER ||
                case_node_type == AstNodeType::AST_ENUM_ELEMENT) {
                cases.push_back(case_expression);
                continue;
            }

            // Assert that case node type must be valid type
            context->diagnostics.report_error(
                keyword.position, "Switch expression case must be integer or enum element");
            throw "Stop";
        } while (is_current_kind(TokenKind::TOKEN_COMMA));

        size_t case_values_count = cases.size() - values.size();
        assert_kind(TokenKind::TOKEN_RIGHT_ARROW, "Expect -> after branch value");
        auto right_value_expression = parse_expression();
        for (size_t i = 0; i < case_values_count; i++) {
            values.push_back(right_value_expression);
        }
        assert_kind(TokenKind::TOKEN_SEMICOLON, "Expect semicolon `;` after switch case value");
    }

    if (not has_default_branch) {
        context->diagnostics.report_error(keyword.position,
                                          "Switch expression must has a default case");
        throw "Stop";
    }

    if (cases.empty()) {
        context->diagnostics.report_error(
            keyword.position, "Switch expression must has at last one case and default case");
        throw "Stop";
    }

    assert_kind(TokenKind::TOKEN_CLOSE_BRACE, "Expect } after switch Statement last branch");
    return std::make_shared<SwitchExpression>(keyword, argument, cases, values, default_value);
}

auto amun::Parser::parse_group_or_tuple_expression() -> Shared<Expression>
{
    Token position = peek_and_advance_token();
    auto expression = parse_expression();

    // Parse Tuple values expression
    if (is_current_kind(TokenKind::TOKEN_COMMA)) {
        auto token = peek_and_advance_token();
        std::vector<Shared<Expression>> values;
        values.push_back(expression);

        while (!is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
            values.push_back(parse_expression());
            if (is_current_kind(TokenKind::TOKEN_COMMA)) {
                advanced_token();
            }
        }

        assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) at the end of tuple values expression");
        return std::make_shared<TupleExpression>(token, values);
    }

    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect ) at the end of group expression");
    return std::make_shared<GroupExpression>(position, expression);
}

auto amun::Parser::parse_array_expression() -> Shared<ArrayExpression>
{
    Token position = peek_and_advance_token();
    std::vector<Shared<Expression>> values;
    while (is_source_available() && not is_current_kind(TOKEN_CLOSE_BRACKET)) {
        values.push_back(parse_expression());
        if (is_current_kind(TokenKind::TOKEN_COMMA)) {
            advanced_token();
        }
    }
    assert_kind(TokenKind::TOKEN_CLOSE_BRACKET, "Expect ] at the end of array values");
    return std::make_shared<ArrayExpression>(position, values);
}

auto amun::Parser::parse_cast_expression() -> Shared<CastExpression>
{
    auto cast_keyword = consume_kind(TokenKind::TOKEN_CAST, "Expect cast keyword");
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` after cast keyword");
    auto target_type = parse_type();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after cast type");
    auto expression = parse_expression();
    return std::make_shared<CastExpression>(cast_keyword, target_type, expression);
}

auto amun::Parser::parse_type_size_expression() -> Shared<TypeSizeExpression>
{
    auto token = consume_kind(TokenKind::TOKEN_TYPE_SIZE, "Expect type_size keyword");
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` after type_size keyword");
    auto type = parse_type();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after type_size type");
    return std::make_shared<TypeSizeExpression>(token, type);
}

auto amun::Parser::parse_value_size_expression() -> Shared<ValueSizeExpression>
{
    auto token = consume_kind(TokenKind::TOKEN_VALUE_SIZE, "Expect value_size keyword");
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` after value_size keyword");
    auto value = parse_expression();
    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after value_size type");
    return std::make_shared<ValueSizeExpression>(token, value);
}

auto amun::Parser::parse_directive_expression() -> Shared<Expression>
{
    auto hash_token = consume_kind(TokenKind::TOKEN_HASH, "Expect `#` before directive name");
    auto directive_name =
        consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect symbol as directive name");
    auto directive_name_literal = directive_name.literal;

    if (directive_name_literal == "line") {
        auto posiiton = directive_name.position;
        auto current_line = posiiton.line_number;
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_INT64;
        directive_token.position = posiiton;
        directive_token.literal = std::to_string(current_line);
        return std::make_shared<NumberExpression>(directive_token, amun::i64_type);
    }

    if (directive_name_literal == "column") {
        auto posiiton = directive_name.position;
        auto current_column = posiiton.column_start;
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_INT64;
        directive_token.position = posiiton;
        directive_token.literal = std::to_string(current_column);
        return std::make_shared<NumberExpression>(directive_token, amun::i64_type);
    }

    if (directive_name_literal == "filepath") {
        auto posiiton = directive_name.position;
        auto current_filepath = context->source_manager.resolve_source_path(posiiton.file_id);
        Token directive_token;
        directive_token.kind = TokenKind::TOKEN_STRING;
        directive_token.position = posiiton;
        directive_token.literal = current_filepath;
        return std::make_shared<StringExpression>(directive_token);
    }

    context->diagnostics.report_error(directive_name.position,
                                      "No directive with name " + directive_name_literal);
    throw "Stop";
}

auto amun::Parser::check_generic_parameter_name(Token name) -> void
{
    auto literal = name.literal;
    auto position = name.position;

    // Check that parameter name is not a built in primitive type
    if (primitive_types.contains(literal)) {
        context->diagnostics.report_error(
            position, "primitives type can't be used as generic parameter name " + literal);
        throw "Stop";
    }

    // Make sure this name is not a struct name
    if (context->structures.contains(literal)) {
        context->diagnostics.report_error(
            position, "Struct name can't be used as generic parameter name " + literal);
        throw "Stop";
    }

    // Make sure this name is not an enum name
    if (context->enumerations.contains(literal)) {
        context->diagnostics.report_error(
            position, "Enum name can't be used as generic parameter name " + literal);
        throw "Stop";
    }

    // Make sure this name is unique and no alias use it
    if (context->type_alias_table.contains(literal)) {
        context->diagnostics.report_error(
            position, "You can't use alias as generic parameter name " + literal);
        throw "Stop";
    }

    // Make sure this generic parameter type is unique in this node
    if (!generic_parameters_names.insert(literal).second) {
        context->diagnostics.report_error(
            position, "You already declared generic parameter with name " + literal);
        throw "Stop";
    }
}

auto amun::Parser::check_compiletime_constants_expression(Shared<Expression> expression,
                                                          TokenSpan position) -> void
{
    auto ast_node_type = expression->get_ast_node_type();

    // Now we just check that the value is primitive but later must allow more types
    if (ast_node_type == AstNodeType::AST_CHARACTER || ast_node_type == AstNodeType::AST_STRING ||
        ast_node_type == AstNodeType::AST_NUMBER || ast_node_type == AstNodeType::AST_BOOL) {
        return;
    }

    // Allow negative number and later should allow prefix unary with constants right
    if (ast_node_type == AstNodeType::AST_PREFIX_UNARY) {
        auto prefix_unary = std::dynamic_pointer_cast<PrefixUnaryExpression>(expression);
        if (prefix_unary->right->get_ast_node_type() == AstNodeType::AST_NUMBER &&
            prefix_unary->operator_token.kind == TokenKind::TOKEN_MINUS) {
            return;
        }
    }

    context->diagnostics.report_error(position, "Value must be a compile time constants");
    throw "Stop";
}

auto amun::Parser::unexpected_token_error() -> void
{
    auto current_token = peek_current();
    auto position = current_token.position;
    std::string token_literal = token_kind_literal[current_token.kind];
    context->diagnostics.report_error(position,
                                      "expected expression, found `" + token_literal + "`");
    throw "Stop";
}

auto amun::Parser::check_unnecessary_semicolon_warning() -> void
{
    if (is_current_kind(TokenKind::TOKEN_SEMICOLON)) {
        auto semicolon = peek_and_advance_token();
        if (context->options.should_report_warns) {
            context->diagnostics.report_warning(semicolon.position, "remove unnecessary semicolon");
        }
    }
}

auto amun::Parser::get_number_kind(TokenKind token) -> amun::NumberKind
{
    switch (token) {
    case TokenKind::TOKEN_INT: return amun::NumberKind::INTEGER_64;
    case TokenKind::TOKEN_INT1: return amun::NumberKind::INTEGER_1;
    case TokenKind::TOKEN_INT8: return amun::NumberKind::INTEGER_8;
    case TokenKind::TOKEN_INT16: return amun::NumberKind::INTEGER_16;
    case TokenKind::TOKEN_INT32: return amun::NumberKind::INTEGER_32;
    case TokenKind::TOKEN_INT64: return amun::NumberKind::INTEGER_64;
    case TokenKind::TOKEN_UINT8: return amun::NumberKind::U_INTEGER_8;
    case TokenKind::TOKEN_UINT16: return amun::NumberKind::U_INTEGER_16;
    case TokenKind::TOKEN_UINT32: return amun::NumberKind::U_INTEGER_32;
    case TokenKind::TOKEN_UINT64: return amun::NumberKind::U_INTEGER_64;
    case TokenKind::TOKEN_FLOAT: return amun::NumberKind::FLOAT_64;
    case TokenKind::TOKEN_FLOAT32: return amun::NumberKind::FLOAT_32;
    case TokenKind::TOKEN_FLOAT64: return amun::NumberKind::FLOAT_64;
    default: {
        context->diagnostics.report_error(peek_current().position, "Token kind is not a number");
        throw "Stop";
    }
    }
}

auto amun::Parser::is_function_declaration_kind(std::string& fun_name, amun::FunctionKind kind)
    -> bool
{
    if (context->functions.contains(fun_name)) {
        return context->functions[fun_name] == kind;
    }
    return false;
}

auto amun::Parser::is_valid_intrinsic_name(std::string& name) -> bool
{
    if (name.empty()) {
        return false;
    }

    if (name.find(' ') != std::string::npos) {
        return false;
    }

    return true;
}

auto amun::Parser::advanced_token() -> void
{
    auto scanned_token = tokenizer.scan_next_token();

    // Check if this token is an error from tokenizer
    if (scanned_token.kind == TokenKind::TOKEN_INVALID) {
        context->diagnostics.report_error(scanned_token.position, scanned_token.literal);
        throw "Stop";
    }

    previous_token = current_token;
    current_token = next_token;
    next_token = scanned_token;
}

auto amun::Parser::peek_and_advance_token() -> Token
{
    auto current = peek_current();
    advanced_token();
    return current;
}

auto amun::Parser::peek_previous() -> Token { return previous_token.value(); }

auto amun::Parser::peek_current() -> Token { return current_token.value(); }

auto amun::Parser::peek_next() -> Token { return next_token.value(); }

auto amun::Parser::is_previous_kind(TokenKind kind) -> bool { return peek_previous().kind == kind; }

auto amun::Parser::is_current_kind(TokenKind kind) -> bool { return peek_current().kind == kind; }

auto amun::Parser::is_next_kind(TokenKind kind) -> bool { return peek_next().kind == kind; }

auto amun::Parser::consume_kind(TokenKind kind, const char* message) -> Token
{
    if (is_current_kind(kind)) {
        advanced_token();
        return previous_token.value();
    }
    context->diagnostics.report_error(peek_current().position, message);
    throw "Stop";
}

void amun::Parser::assert_kind(TokenKind kind, const char* message)
{
    if (is_current_kind(kind)) {
        advanced_token();
        return;
    }

    auto location = peek_current().position;
    if (kind == TokenKind::TOKEN_SEMICOLON) {
        location = peek_previous().position;
    }
    context->diagnostics.report_error(location, message);
    throw "Stop";
}

auto amun::Parser::is_source_available() -> bool
{
    return peek_current().kind != TokenKind::TOKEN_END_OF_FILE;
}
