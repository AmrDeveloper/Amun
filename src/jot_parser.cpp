#include "../include/jot_parser.hpp"
#include "../include/jot_files.hpp"
#include "../include/jot_logger.hpp"
#include "../include/jot_type.hpp"

#include <algorithm>
#include <cassert>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

std::shared_ptr<CompilationUnit> JotParser::parse_compilation_unit()
{
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
    }
    catch (const char* message) {
    }
    return std::make_shared<CompilationUnit>(tree_nodes);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_import_declaration()
{
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // import { <string> <string> }
        advanced_token();
        std::vector<std::shared_ptr<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name = consume_kind(
                TokenKind::String, "Expect string as library name after import statement");
            std::string library_path = "../lib/" + library_name.literal + ".jot";

            if (context->source_manager.is_path_registered(library_path.c_str()))
                continue;

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic_error(
                    library_name.position, "No standard library with name " + library_name.literal);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            merge_tree_nodes(tree_nodes, nodes);
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as library name after import statement");
    std::string library_path = "../lib/" + library_name.literal + ".jot";

    if (context->source_manager.is_path_registered(library_path.c_str())) {
        return std::vector<std::shared_ptr<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic_error(
            library_name.position, "No standard library with name " + library_name.literal);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_load_declaration()
{
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // load { <string> <string> }
        advanced_token();
        std::vector<std::shared_ptr<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name =
                consume_kind(TokenKind::String, "Expect string as file name after load statement");

            std::string library_path = file_parent_path + library_name.literal + ".jot";

            if (context->source_manager.is_path_registered(library_path.c_str()))
                continue;

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic_error(library_name.position,
                                                          "Path not exists " + library_path);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            merge_tree_nodes(tree_nodes, nodes);
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as file name after load statement");

    std::string library_path = file_parent_path + library_name.literal + ".jot";

    if (context->source_manager.is_path_registered(library_path)) {
        return std::vector<std::shared_ptr<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic_error(library_name.position,
                                                  "Path not exists " + library_path);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

std::vector<std::shared_ptr<Statement>> JotParser::parse_single_source_file(std::string& path)
{
    const char*  file_name = path.c_str();
    std::string  source_content = read_file_content(file_name);
    int          file_id = context->source_manager.register_source_path(path);
    JotTokenizer tokenizer(file_id, source_content);
    JotParser    parser(context, tokenizer);
    auto         compilation_unit = parser.parse_compilation_unit();
    if (context->diagnostics.get_errors_number() > 0) {
        throw "Stop";
    }
    return compilation_unit->get_tree_nodes();
}

void JotParser::merge_tree_nodes(std::vector<std::shared_ptr<Statement>>& distany,
                                 std::vector<std::shared_ptr<Statement>>& source)
{
    distany.insert(std::end(distany), std::begin(source), std::end(source));
}

std::shared_ptr<Statement> JotParser::parse_declaration_statement()
{
    switch (peek_current().kind) {
    case TokenKind::PrefixKeyword:
    case TokenKind::InfixKeyword:
    case TokenKind::PostfixKeyword: {
        auto call_kind = PREFIX_FUNCTION;
        if (peek_current().kind == TokenKind::InfixKeyword)
            call_kind = INFIX_FUNCTION;
        else if (peek_current().kind == TokenKind::PostfixKeyword)
            call_kind = POSTFIX_FUNCTION;
        advanced_token();

        if (is_current_kind(TokenKind::ExternKeyword))
            return parse_function_prototype(call_kind, true);
        else if (is_current_kind(TokenKind::FunKeyword))
            return parse_function_declaration(call_kind);

        context->diagnostics.add_diagnostic_error(
            peek_current().position, "Prefix, Infix, postfix keyword used only with functions");
        throw "Stop";
    }
    case TokenKind::ExternKeyword: {
        return parse_function_prototype(NORMAL_FUNCTION, true);
    }
    case TokenKind::FunKeyword: {
        return parse_function_declaration(NORMAL_FUNCTION);
    }
    case TokenKind::VarKeyword: {
        return parse_field_declaration(true);
    }
    case TokenKind::StructKeyword: {
        return parse_structure_declaration(false);
    }
    case TokenKind::PackedKeyword: {
        advanced_token();
        return parse_structure_declaration(true);
    }
    case TokenKind::EnumKeyword: {
        return parse_enum_declaration();
    }
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().position,
                                                  "Invalid top level declaration statement");
        throw "Stop";
    }
    }
}

std::shared_ptr<Statement> JotParser::parse_statement()
{
    switch (peek_current().kind) {
    case TokenKind::VarKeyword: {
        return parse_field_declaration(false);
    }
    case TokenKind::IfKeyword: {
        return parse_if_statement();
    }
    case TokenKind::ForKeyword: {
        return parse_for_statement();
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

std::shared_ptr<FieldDeclaration> JotParser::parse_field_declaration(bool is_global)
{
    assert_kind(TokenKind::VarKeyword, "Expect var keyword.");
    auto name = consume_kind(TokenKind::Symbol, "Expect identifier as variable name.");
    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        auto                        type = parse_type();
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
    return std::make_shared<FieldDeclaration>(name, jot_none_ty, value, is_global);
}

std::shared_ptr<FunctionPrototype> JotParser::parse_function_prototype(FunctionDeclarationKind kind,
                                                                       bool is_external)
{
    if (is_external) {
        assert_kind(TokenKind::ExternKeyword, "Expect external keyword");
    }

    assert_kind(TokenKind::FunKeyword, "Expect function keyword.");
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as function name.");

    bool                                    has_varargs = false;
    std::shared_ptr<JotType>                varargs_type = nullptr;
    std::vector<std::shared_ptr<Parameter>> parameters;
    if (is_current_kind(TokenKind::OpenParen)) {
        advanced_token();
        while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
            if (has_varargs) {
                context->diagnostics.add_diagnostic_error(
                    previous_token->position, "Varargs must be the last parameter in the function");
                throw "Stop";
            }

            if (is_current_kind(TokenKind::VarargsKeyword)) {
                advanced_token();
                if (is_current_kind(TokenKind::Symbol) && current_token->literal == "Any") {
                    advanced_token();
                }
                else {
                    varargs_type = parse_type();
                }
                has_varargs = true;
                continue;
            }

            parameters.push_back(parse_parameter());
            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::CloseParen, "Expect ) after function parameters.");
    }

    auto parameters_size = parameters.size();

    if (kind == PREFIX_FUNCTION && parameters_size != 1) {
        context->diagnostics.add_diagnostic_error(
            name.position, "Prefix function must have exactly one parameter");
        throw "Stop";
    }

    if (kind == INFIX_FUNCTION && parameters_size != 2) {
        context->diagnostics.add_diagnostic_error(name.position,
                                                  "Infix function must have exactly Two parameter");
        throw "Stop";
    }

    if (kind == POSTFIX_FUNCTION && parameters_size != 1) {
        context->diagnostics.add_diagnostic_error(
            name.position, "Postfix function must have exactly one parameter");
        throw "Stop";
    }

    // Register current function declaration kind
    context->functions[name.literal] = kind;

    // If function prototype has no explicit return type,
    // make return type to be void
    Shared<JotType> return_type;
    if (is_current_kind(TokenKind::Semicolon) || is_current_kind(TokenKind::OpenBrace)) {
        return_type = jot_void_ty;
    }
    else {
        return_type = parse_type();
    }

    // Function can't return fixed size array, you can use pointer format to return allocated array
    if (return_type->type_kind == TypeKind::Array) {
        context->diagnostics.add_diagnostic_error(
            name.position, "Function cannot return array type " + jot_type_literal(return_type));
        throw "Stop";
    }

    if (is_external)
        assert_kind(TokenKind::Semicolon, "Expect ; after external function declaration");
    return std::make_shared<FunctionPrototype>(name, parameters, return_type, is_external,
                                               has_varargs, varargs_type);
}

std::shared_ptr<FunctionDeclaration>
JotParser::parse_function_declaration(FunctionDeclarationKind kind)
{
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
        loop_levels_stack.push(0);
        auto block = parse_block_statement();
        loop_levels_stack.pop();
        auto close_brace = previous_token;

        // If function return type is void and has no return at the end, emit one
        if (is_void_type(prototype->get_return_type()) &&
            (block->statements.empty() ||
             block->statements.back()->get_ast_node_type() != AstNodeType::Return)) {
            auto void_return =
                std::make_shared<ReturnStatement>(close_brace.value(), nullptr, false);
            block->statements.push_back(void_return);
        }

        current_ast_scope = parent_node_scope;
        return std::make_shared<FunctionDeclaration>(prototype, block);
    }

    context->diagnostics.add_diagnostic_error(peek_current().position,
                                              "Invalid function declaration body");
    throw "Stop";
}

std::shared_ptr<StructDeclaration> JotParser::parse_structure_declaration(bool is_packed)
{
    auto struct_token = consume_kind(TokenKind::StructKeyword, "Expect struct keyword");
    auto struct_name = consume_kind(TokenKind::Symbol, "Expect Symbol as struct name");
    auto struct_name_str = struct_name.literal;
    current_struct_name = struct_name.literal;
    size_t                                field_index = 0;
    std::unordered_map<std::string, int>  fields_names;
    std::vector<std::shared_ptr<JotType>> fields_types;
    assert_kind(TokenKind::OpenBrace, "Expect { after struct name");
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto field_name = consume_kind(TokenKind::Symbol, "Expect Symbol as struct name");
        if (fields_names.contains(field_name.literal)) {
            context->diagnostics.add_diagnostic_error(field_name.position,
                                                      "There is already struct member with name " +
                                                          field_name.literal);
            throw "Stop";
        }

        fields_names[field_name.literal] = field_index++;
        auto field_type = parse_type();

        // Handle Incomplete field type case
        if (field_type->type_kind == TypeKind::None) {
            context->diagnostics.add_diagnostic_error(
                field_name.position, "Field type isn't fully defined yet, you can't use it "
                                     "until it defined but you can use *" +
                                         struct_name_str);
            throw "Stop";
        }

        fields_types.push_back(field_type);
        assert_kind(TokenKind::Semicolon, "Expect ; at the end of struct field declaration");
    }
    assert_kind(TokenKind::CloseBrace, "Expect } in the end of struct declaration");
    auto structure_type =
        std::make_shared<JotStructType>(struct_name_str, fields_names, fields_types, is_packed);

    if (context->structures.count(struct_name_str)) {
        context->diagnostics.add_diagnostic_error(
            struct_name.position, "There is already struct with name " + struct_name_str);
        throw "Stop";
    }

    // Resolve un solved types
    // This code will executed only if there are field with type of pointer to the current struct
    // In parsing type function we return a pointer to jot none type,
    // Now we replace it with pointer to current struct type after it created
    if (current_struct_unknown_fields > 0) {
        // Pointer to current struct type
        auto struct_pointer_ty = std::make_shared<JotPointerType>(structure_type);

        const auto fields_size = fields_types.size();
        for (size_t i = 0; i < fields_size; i++) {
            const auto field_type = fields_types[i];

            // If Field type is pointer to none that mean it point to struct itself
            if (is_pointer_of_type(field_type, jot_none_ty)) {
                // Update field type from *None to *Itself
                structure_type->fields_types[i] = struct_pointer_ty;
                current_struct_unknown_fields--;
                continue;
            }

            // Update field type from [?]*None to [?]*Itself
            if (is_array_of_type(field_type, jot_none_ptr_ty)) {
                auto array_type = std::static_pointer_cast<JotArrayType>(field_type);
                array_type->element_type = struct_pointer_ty;

                structure_type->fields_types[i] = array_type;
                current_struct_unknown_fields--;
                continue;
            }
        }
    }

    assert(current_struct_unknown_fields == 0);

    context->structures[struct_name_str] = structure_type;
    current_struct_name = "";
    return std::make_shared<StructDeclaration>(structure_type);
}

std::shared_ptr<EnumDeclaration> JotParser::parse_enum_declaration()
{
    auto enum_token = consume_kind(TokenKind::EnumKeyword, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::Symbol, "Expect Symbol as enum name");

    std::shared_ptr<JotType> element_type = nullptr;

    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        element_type = parse_type();
    }
    else {
        // TODO: if we split the type from his position we can declare this type once
        // Default enumeration element type is integer 32
        element_type = jot_int32_ty;
    }

    assert_kind(TokenKind::OpenBrace, "Expect { after enum name");
    std::vector<Token>                   enum_values;
    std::unordered_map<std::string, int> enum_values_indexes;
    std::set<int>                        explicit_values;
    int                                  index = 0;
    bool                                 has_explicit_values = false;

    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto enum_value = consume_kind(TokenKind::Symbol, "Expect Symbol as enum value");
        enum_values.push_back(enum_value);
        auto enum_field_literal = enum_value.literal;

        if (enum_values_indexes.contains(enum_field_literal)) {
            context->diagnostics.add_diagnostic_error(
                enum_value.position, "Can't declare 2 elements with the same name");
            throw "Stop";
        }

        if (is_current_kind(TokenKind::Equal)) {
            advanced_token();
            auto field_value = parse_expression();
            if (field_value->get_ast_node_type() != AstNodeType::NumberExpr) {
                context->diagnostics.add_diagnostic_error(
                    enum_value.position, "Enum field explicit value must be integer expression");
                throw "Stop";
            }

            auto number_expr = std::dynamic_pointer_cast<NumberExpression>(field_value);
            auto number_value_token = number_expr->get_value();
            if (is_float_number_token(number_value_token)) {
                context->diagnostics.add_diagnostic_error(
                    enum_value.position,
                    "Enum field explicit value must be integer value not float");
                throw "Stop";
            }

            auto explicit_value = std::stoi(number_value_token.literal);
            if (explicit_values.count(explicit_value)) {
                context->diagnostics.add_diagnostic_error(
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
                context->diagnostics.add_diagnostic_error(
                    enum_value.position,
                    "You must add explicit value to all enum fields or to no one");
                throw "Stop";
            }

            enum_values_indexes[enum_field_literal] = index++;
        }

        if (is_current_kind(TokenKind::Comma)) {
            advanced_token();
        }
    }
    assert_kind(TokenKind::CloseBrace, "Expect } in the end of enum declaration");
    auto enum_type = std::make_shared<JotEnumType>(enum_name, enum_values_indexes, element_type);
    context->enumerations[enum_name.literal] = enum_type;
    return std::make_shared<EnumDeclaration>(enum_name, enum_type);
}

std::shared_ptr<Parameter> JotParser::parse_parameter()
{
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as parameter name.");
    auto  type = parse_type();
    return std::make_shared<Parameter>(name, type);
}

std::shared_ptr<BlockStatement> JotParser::parse_block_statement()
{
    consume_kind(TokenKind::OpenBrace, "Expect { on the start of block.");
    std::vector<std::shared_ptr<Statement>> statements;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        statements.push_back(parse_statement());
    }
    consume_kind(TokenKind::CloseBrace, "Expect } on the end of block.");
    return std::make_shared<BlockStatement>(statements);
}

std::shared_ptr<ReturnStatement> JotParser::parse_return_statement()
{
    auto keyword = consume_kind(TokenKind::ReturnKeyword, "Expect return keyword.");
    if (is_current_kind(TokenKind::Semicolon)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return keyword");
        return std::make_shared<ReturnStatement>(keyword, nullptr, false);
    }
    auto value = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after return statement");
    return std::make_shared<ReturnStatement>(keyword, value, true);
}

std::shared_ptr<DeferStatement> JotParser::parse_defer_statement()
{
    auto defer_token = consume_kind(TokenKind::DeferKeyword, "Expect Defer keyword.");
    auto expression = parse_expression();
    if (auto call_expression = std::dynamic_pointer_cast<CallExpression>(expression)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after defer call statement");
        return std::make_shared<DeferStatement>(defer_token, call_expression);
    }

    context->diagnostics.add_diagnostic_error(defer_token.position,
                                              "defer keyword expect call expression");
    throw "Stop";
}

std::shared_ptr<BreakStatement> JotParser::parse_break_statement()
{
    auto break_token = consume_kind(TokenKind::BreakKeyword, "Expect break keyword.");

    if (current_ast_scope != AstNodeScope::ConditionalScope or loop_levels_stack.top() == 0) {
        context->diagnostics.add_diagnostic_error(
            break_token.position, "break keyword can only be used inside at last one while loop");
        throw "Stop";
    }

    if (is_current_kind(TokenKind::Semicolon)) {
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after break call statement");
        return std::make_shared<BreakStatement>(break_token, false, 1);
    }

    auto break_times = parse_expression();
    if (auto number_expr = std::dynamic_pointer_cast<NumberExpression>(break_times)) {
        auto number_value = number_expr->get_value();
        if (is_float_number_token(number_value)) {
            context->diagnostics.add_diagnostic_error(
                break_token.position,
                "expect break keyword times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.literal);
        if (times_int < 1) {
            context->diagnostics.add_diagnostic_error(
                break_token.position, "expect break times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_levels_stack.top()) {
            context->diagnostics.add_diagnostic_error(
                break_token.position, "break times can't be bigger than the number of loops you "
                                      "have, expect less than or equals " +
                                          std::to_string(loop_levels_stack.top()));
            throw "Stop";
        }

        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after brea statement");
        return std::make_shared<BreakStatement>(break_token, true, times_int);
    }

    context->diagnostics.add_diagnostic_error(break_token.position,
                                              "break keyword times must be a number");
    throw "Stop";
}

std::shared_ptr<ContinueStatement> JotParser::parse_continue_statement()
{
    auto continue_token = consume_kind(TokenKind::ContinueKeyword, "Expect continue keyword.");

    if (current_ast_scope != AstNodeScope::ConditionalScope or loop_levels_stack.top() == 0) {
        context->diagnostics.add_diagnostic_error(
            continue_token.position,
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
        auto number_kind = number_value.kind;
        if (number_kind == TokenKind::Float or number_kind == TokenKind::Float32Type or
            number_kind == TokenKind::Float64Type) {

            context->diagnostics.add_diagnostic_error(
                continue_token.position,
                "expect continue times to be integer but found floating pointer value");
            throw "Stop";
        }

        int times_int = std::stoi(number_value.literal);
        if (times_int < 1) {
            context->diagnostics.add_diagnostic_error(
                continue_token.position,
                "expect continue times must be positive value and at last 1");
            throw "Stop";
        }

        if (times_int > loop_levels_stack.top()) {
            context->diagnostics.add_diagnostic_error(
                continue_token.position,
                "continue times can't be bigger than the number of loops you "
                "have, expect less than or equals " +
                    std::to_string(loop_levels_stack.top()));
            throw "Stop";
        }

        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after brea statement");
        return std::make_shared<ContinueStatement>(continue_token, true, times_int);
    }

    context->diagnostics.add_diagnostic_error(continue_token.position,
                                              "continue keyword times must be a number");
    throw "Stop";
}

std::shared_ptr<IfStatement> JotParser::parse_if_statement()
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::ConditionalScope;

    auto if_token = consume_kind(TokenKind::IfKeyword, "Expect If keyword.");
    auto condition = parse_expression();
    auto then_block = parse_statement();
    auto conditional_block = std::make_shared<ConditionalBlock>(if_token, condition, then_block);
    std::vector<std::shared_ptr<ConditionalBlock>> conditional_blocks;
    conditional_blocks.push_back(conditional_block);

    bool has_else_branch = false;
    while (is_source_available() && is_current_kind(TokenKind::ElseKeyword)) {
        auto else_token = consume_kind(TokenKind::ElseKeyword, "Expect else keyword.");
        if (is_current_kind(TokenKind::IfKeyword)) {
            advanced_token();
            auto elif_condition = parse_expression();
            auto elif_block = parse_statement();
            auto elif_condition_block =
                std::make_shared<ConditionalBlock>(else_token, elif_condition, elif_block);
            conditional_blocks.push_back(elif_condition_block);
            continue;
        }

        if (has_else_branch) {
            context->diagnostics.add_diagnostic_error(else_token.position,
                                                      "You already declared else branch");
            throw "Stop";
        }

        auto true_value_token = else_token;
        true_value_token.kind = TokenKind::TrueKeyword;
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

std::shared_ptr<Statement> JotParser::parse_for_statement()
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::ConditionalScope;

    auto keyword = consume_kind(TokenKind::ForKeyword, "Expect for keyword.");

    // Parse for ever statement
    if (is_current_kind(TokenKind::OpenBrace)) {
        loop_levels_stack.top() += 1;
        auto body = parse_statement();
        loop_levels_stack.top() -= 1;
        current_ast_scope = parent_node_scope;
        return std::make_shared<ForeverStatement>(keyword, body);
    }

    // Parse optional element name or it as default
    std::string name = "it";
    auto        expr = parse_expression();
    if (is_current_kind(TokenKind::Colon)) {
        if (expr->get_ast_node_type() != AstNodeType::LiteralExpr) {
            context->diagnostics.add_diagnostic_error(keyword.position,
                                                      "Optional named variable must be identifier");
            throw "Stop";
        }

        // Previous token is the LiteralExpression that contains variable name
        name = previous_token->literal;
        // Consume the colon
        advanced_token();
        expr = parse_expression();
    }

    // For range statemnet for x -> y
    if (is_current_kind(TokenKind::DotDot)) {
        advanced_token();
        const auto range_end = parse_expression();

        // If there is : after range the mean we have custom step expression
        // If not use nullptr as default value for step (Handled in the Backend)
        std::shared_ptr<Expression> step = nullptr;
        if (is_current_kind(TokenKind::Colon)) {
            advanced_token();
            step = parse_expression();
        }

        loop_levels_stack.top() += 1;
        auto body = parse_statement();
        loop_levels_stack.top() -= 1;

        current_ast_scope = parent_node_scope;
        return std::make_shared<ForRangeStatement>(keyword, name, expr, range_end, step, body);
    }

    // Parse For each statement

    loop_levels_stack.top() += 1;
    auto body = parse_statement();
    loop_levels_stack.top() -= 1;

    current_ast_scope = parent_node_scope;

    return std::make_shared<ForEachStatement>(keyword, name, expr, body);
}

std::shared_ptr<WhileStatement> JotParser::parse_while_statement()
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::ConditionalScope;

    auto keyword = consume_kind(TokenKind::WhileKeyword, "Expect while keyword.");
    auto condition = parse_expression();

    loop_levels_stack.top() += 1;
    auto body = parse_statement();
    loop_levels_stack.top() -= 1;

    current_ast_scope = parent_node_scope;
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

std::shared_ptr<SwitchStatement> JotParser::parse_switch_statement()
{
    auto keyword = consume_kind(TokenKind::SwitchKeyword, "Expect switch keyword.");
    auto argument = parse_expression();
    assert_kind(TokenKind::OpenBrace, "Expect { after switch value");

    std::vector<std::shared_ptr<SwitchCase>> switch_cases;
    std::shared_ptr<SwitchCase>              default_branch = nullptr;
    bool                                     has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::CloseBrace)) {
        std::vector<std::shared_ptr<Expression>> values;
        if (is_current_kind(TokenKind::ElseKeyword)) {
            if (has_default_branch) {
                context->diagnostics.add_diagnostic_error(
                    keyword.position, "Switch statementscan't has more than one default branch");
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

std::shared_ptr<ExpressionStatement> JotParser::parse_expression_statement()
{
    auto expression = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<ExpressionStatement>(expression);
}

std::shared_ptr<Expression> JotParser::parse_expression() { return parse_assignment_expression(); }

std::shared_ptr<Expression> JotParser::parse_assignment_expression()
{
    auto expression = parse_logical_or_expression();
    if (is_assignments_operator_token(peek_current())) {
        auto                        assignments_token = peek_and_advance_token();
        std::shared_ptr<Expression> right_value;
        auto                        assignments_token_kind = assignments_token.kind;
        if (assignments_token_kind == TokenKind::Equal) {
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

std::shared_ptr<Expression> JotParser::parse_logical_or_expression()
{
    auto expression = parse_logical_and_expression();
    while (is_current_kind(TokenKind::LogicalOr)) {
        auto or_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, or_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_logical_and_expression()
{
    auto expression = parse_equality_expression();
    while (is_current_kind(TokenKind::LogicalAnd)) {
        auto and_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, and_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_equality_expression()
{
    auto expression = parse_comparison_expression();
    while (is_current_kind(TokenKind::EqualEqual) || is_current_kind(TokenKind::BangEqual)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_comparison_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_comparison_expression()
{
    auto expression = parse_shift_expression();
    while (is_current_kind(TokenKind::Greater) || is_current_kind(TokenKind::GreaterEqual) ||
           is_current_kind(TokenKind::Smaller) || is_current_kind(TokenKind::SmallerEqual)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_shift_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_shift_expression()
{
    auto expression = parse_term_expression();
    while (is_current_kind(TokenKind::RightShift) || is_current_kind(TokenKind::LeftShift)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_term_expression();
        expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_term_expression()
{
    auto expression = parse_factor_expression();
    while (is_current_kind(TokenKind::Plus) || is_current_kind(TokenKind::Minus)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_factor_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_factor_expression()
{
    auto expression = parse_enum_access_expression();
    while (is_current_kind(TokenKind::Star) || is_current_kind(TokenKind::Slash) ||
           is_current_kind(TokenKind::Percent)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_enum_access_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_enum_access_expression()
{
    auto expression = parse_infix_call_expression();
    if (is_current_kind(TokenKind::ColonColon)) {
        auto colons_token = peek_and_advance_token();
        if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression)) {
            auto enum_name = literal->get_name();
            if (context->enumerations.count(enum_name.literal)) {
                auto enum_type = context->enumerations[enum_name.literal];
                auto element =
                    consume_kind(TokenKind::Symbol, "Expect identifier as enum field name");

                auto enum_values = enum_type->values;
                if (not enum_values.count(element.literal)) {
                    context->diagnostics.add_diagnostic_error(
                        element.position, "Can't find element with name " + element.literal +
                                              " in enum " + enum_name.literal);
                    throw "Stop";
                }

                int  index = enum_values[element.literal];
                auto enum_element_type = std::make_shared<JotEnumElementType>(
                    enum_name.literal, enum_type->element_type);
                return std::make_shared<EnumAccessExpression>(enum_name, element, index,
                                                              enum_element_type);
            }
            else {
                context->diagnostics.add_diagnostic_error(enum_name.position,
                                                          "Can't find enum declaration with name " +
                                                              enum_name.literal);
                throw "Stop";
            }
        }
        else {
            context->diagnostics.add_diagnostic_error(colons_token.position,
                                                      "Expect identifier as Enum name");
            throw "Stop";
        }
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_infix_call_expression()
{
    auto expression = parse_prefix_expression();
    auto current_token_literal = peek_current().literal;

    // Parse Infix function call as a call expression
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, INFIX_FUNCTION)) {
        auto                                     symbol_token = peek_current();
        auto                                     literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(expression);
        arguments.push_back(parse_infix_call_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }

    return expression;
}

std::shared_ptr<Expression> JotParser::parse_prefix_expression()
{
    if (is_unary_operator_token(peek_current())) {
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        return std::make_shared<PrefixUnaryExpression>(token, right);
    }

    if (is_current_kind(TokenKind::PlusPlus) or is_current_kind(TokenKind::MinusMinus)) {
        auto token = peek_and_advance_token();
        auto right = parse_prefix_expression();
        auto ast_node_type = right->get_ast_node_type();
        if ((ast_node_type == AstNodeType::LiteralExpr) or
            (ast_node_type == AstNodeType::IndexExpr) or (ast_node_type == AstNodeType::DotExpr)) {
            return std::make_shared<PrefixUnaryExpression>(token, right);
        }

        context->diagnostics.add_diagnostic_error(
            token.position,
            "Unary ++ or -- expect right expression to be variable or index expression");
        throw "Stop";
    }

    return parse_prefix_call_expression();
}

std::shared_ptr<Expression> JotParser::parse_prefix_call_expression()
{
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, PREFIX_FUNCTION)) {
        Token                                    symbol_token = peek_current();
        auto                                     literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(parse_prefix_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_postfix_increment_or_decrement();
}

std::shared_ptr<Expression> JotParser::parse_postfix_increment_or_decrement()
{
    auto expression = parse_call_or_access_expression();

    if (is_current_kind(TokenKind::PlusPlus) or is_current_kind(TokenKind::MinusMinus)) {
        auto token = peek_and_advance_token();
        auto ast_node_type = expression->get_ast_node_type();
        if ((ast_node_type == AstNodeType::LiteralExpr) or
            (ast_node_type == AstNodeType::IndexExpr) or (ast_node_type == AstNodeType::DotExpr)) {
            return std::make_shared<PostfixUnaryExpression>(token, expression);
        }
        context->diagnostics.add_diagnostic_error(
            token.position,
            "Unary ++ or -- expect left expression to be variable or index expression");
        throw "Stop";
    }

    return expression;
}

// TODO: Need a better name
std::shared_ptr<Expression> JotParser::parse_call_or_access_expression()
{
    auto expression = parse_enumeration_attribute_expression();
    while (is_current_kind(TokenKind::Dot) || is_current_kind(TokenKind::OpenParen) ||
           is_current_kind(TokenKind::OpenBracket)) {

        // Parse structure field access expression
        if (is_current_kind(TokenKind::Dot)) {
            auto dot_token = peek_and_advance_token();
            auto field_name = consume_kind(TokenKind::Symbol, "Expect literal as field name");
            expression = std::make_shared<DotExpression>(dot_token, expression, field_name);
            continue;
        }

        // Parse function call expression
        if (is_current_kind(TokenKind::OpenParen)) {
            auto                                     position = peek_and_advance_token();
            std::vector<std::shared_ptr<Expression>> arguments;
            while (not is_current_kind(TokenKind::CloseParen)) {
                arguments.push_back(parse_expression());
                if (is_current_kind(TokenKind::Comma)) {
                    advanced_token();
                }
            }

            assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");

            // Add support for optional lambda after call expression
            if (is_current_kind(TokenKind::OpenBrace)) {
                arguments.push_back(parse_lambda_expression());
            }

            expression = std::make_shared<CallExpression>(position, expression, arguments);
            continue;
        }

        // Parse Index Expression
        if (is_current_kind(TokenKind::OpenBracket)) {
            auto position = peek_and_advance_token();
            auto index = parse_expression();
            assert_kind(TokenKind::CloseBracket, "Expect ] after index value");
            expression = std::make_shared<IndexExpression>(position, expression, index);
            continue;
        }
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_enumeration_attribute_expression()
{
    auto expression = parse_postfix_call_expression();
    if (is_current_kind(TokenKind::Dot) and
        expression->get_ast_node_type() == AstNodeType::LiteralExpr) {
        auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression);
        auto literal_str = literal->get_name().literal;
        if (context->enumerations.count(literal_str)) {
            auto dot_token = peek_and_advance_token();
            auto attribute = consume_kind(TokenKind::Symbol, "Expect attribute name for enum");
            auto attribute_str = attribute.literal;
            if (attribute_str == "count") {
                auto  count = context->enumerations[literal_str]->values.size();
                Token number_token = {TokenKind::Integer, attribute.position,
                                      std::to_string(count)};
                auto  number_type = jot_int64_ty;
                return std::make_shared<NumberExpression>(number_token, number_type);
            }

            context->diagnostics.add_diagnostic_error(
                attribute.position, "Un supported attribute for enumeration type");
            throw "Stop";
        }
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_postfix_call_expression()
{
    auto expression = parse_initializer_expression();
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, POSTFIX_FUNCTION)) {
        Token                                    symbol_token = peek_current();
        auto                                     literal = parse_literal_expression();
        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(expression);
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return expression;
}

std::shared_ptr<Expression> JotParser::parse_initializer_expression()
{
    if (is_current_kind(TokenKind::Symbol) and is_next_kind(TokenKind::OpenBrace) and
        context->structures.count(current_token->literal)) {
        auto type = parse_type();
        auto token = consume_kind(TokenKind::OpenBrace, "Expect { at the start of initializer");
        std::vector<std::shared_ptr<Expression>> arguments;
        while (not is_current_kind(TokenKind::CloseBrace)) {
            arguments.push_back(parse_expression());
            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            }
            else {
                break;
            }
        }
        assert_kind(TokenKind::CloseBrace, "Expect } at the end of initializer");
        return std::make_shared<InitializeExpression>(token, type, arguments);
    }

    return parse_function_call_with_lambda_argument();
}

std::shared_ptr<Expression> JotParser::parse_function_call_with_lambda_argument()
{
    if (is_current_kind(TokenKind::Symbol) and is_next_kind(TokenKind::OpenBrace) and
        is_function_declaration_kind(current_token->literal, NORMAL_FUNCTION)) {
        auto symbol_token = peek_current();
        auto literal = parse_literal_expression();

        std::vector<std::shared_ptr<Expression>> arguments;
        arguments.push_back(parse_lambda_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_primary_expression();
}

std::shared_ptr<Expression> JotParser::parse_primary_expression()
{
    auto current_token_kind = peek_current().kind;
    switch (current_token_kind) {
    case TokenKind::Integer:
    case TokenKind::Integer1Type:
    case TokenKind::Integer8Type:
    case TokenKind::Integer16Type:
    case TokenKind::Integer32Type:
    case TokenKind::Integer64Type:
    case TokenKind::UInteger8Type:
    case TokenKind::UInteger16Type:
    case TokenKind::UInteger32Type:
    case TokenKind::UInteger64Type:
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
    case TokenKind::OpenBrace: {
        return parse_lambda_expression();
    }
    case TokenKind::IfKeyword: {
        return parse_if_expression();
    }
    case TokenKind::SwitchKeyword: {
        return parse_switch_expression();
    }
    case TokenKind::CastKeyword: {
        return parse_cast_expression();
    }
    case TokenKind::TypeSizeKeyword: {
        return parse_type_size_expression();
    }
    case TokenKind::ValueSizeKeyword: {
        return parse_value_size_expression();
    }
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().position,
                                                  "Unexpected or unsupported expression");
        throw "Stop";
    }
    }
}

std::shared_ptr<LambdaExpression> JotParser::parse_lambda_expression()
{
    auto open_paren =
        consume_kind(TokenKind::OpenBrace, "Expect { at the start of lambda expression");

    std::vector<std::shared_ptr<Parameter>> parameters;

    std::shared_ptr<JotType> return_type;

    if (is_current_kind(TokenKind::OpenParen)) {
        advanced_token();

        // Parse Parameters
        while (not is_current_kind(TokenKind::CloseParen)) {
            parameters.push_back(parse_parameter());

            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            }
            else {
                break;
            }
        }

        assert_kind(TokenKind::CloseParen, "Expect ) after lambda parameters");
        return_type = parse_type();
        assert_kind(TokenKind::RightArrow, "Expect -> after lambda return type");
    }
    else {
        // Default return type for lambda expression
        return_type = jot_void_ty;
    }

    loop_levels_stack.push(0);

    // Parse lambda expression body
    std::vector<std::shared_ptr<Statement>> body;
    while (not is_current_kind(TokenKind::CloseBrace)) {
        body.push_back(parse_statement());
    }

    loop_levels_stack.pop();

    auto lambda_body = std::make_shared<BlockStatement>(body);

    auto close_brace =
        consume_kind(TokenKind::CloseBrace, "Expect } at the end of lambda expression");

    // If lambda body has no statements or end without return statement
    // we can implicity insert return statement without value
    if (is_void_type(return_type) &&
        (body.empty() || body.back()->get_ast_node_type() != AstNodeType::Return)) {
        auto void_return = std::make_shared<ReturnStatement>(close_brace, nullptr, false);
        lambda_body->statements.push_back(void_return);
    }

    return std::make_shared<LambdaExpression>(open_paren, parameters, return_type, lambda_body);
}

std::shared_ptr<NumberExpression> JotParser::parse_number_expression()
{
    auto number_token = peek_and_advance_token();
    auto number_kind = get_number_kind(number_token.kind);
    auto number_type = std::make_shared<JotNumberType>(number_kind);
    return std::make_shared<NumberExpression>(number_token, number_type);
}

std::shared_ptr<LiteralExpression> JotParser::parse_literal_expression()
{
    Token symbol_token = peek_and_advance_token();
    auto  type = std::make_shared<JotNoneType>();
    return std::make_shared<LiteralExpression>(symbol_token, type);
}

std::shared_ptr<IfExpression> JotParser::parse_if_expression()
{
    Token if_token = peek_and_advance_token();
    auto  condition = parse_expression();
    auto  then_value = parse_expression();
    Token else_token =
        consume_kind(TokenKind::ElseKeyword, "Expect `else` keyword after then value.");
    auto else_value = parse_expression();
    return std::make_shared<IfExpression>(if_token, else_token, condition, then_value, else_value);
}

std::shared_ptr<SwitchExpression> JotParser::parse_switch_expression()
{
    auto keyword = consume_kind(TokenKind::SwitchKeyword, "Expect switch keyword.");
    auto argument = parse_expression();
    assert_kind(TokenKind::OpenBrace, "Expect { after switch value");
    std::vector<std::shared_ptr<Expression>> cases;
    std::vector<std::shared_ptr<Expression>> values;
    std::shared_ptr<Expression>              default_value;
    bool                                     has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::CloseBrace)) {
        if (is_current_kind(TokenKind::ElseKeyword)) {
            if (has_default_branch) {
                context->diagnostics.add_diagnostic_error(
                    keyword.position, "Switch expression can't has more than one default branch");
                throw "Stop";
            }
            assert_kind(TokenKind::ElseKeyword, "Expect else keyword in switch default branch");
            assert_kind(TokenKind::RightArrow,
                        "Expect -> after else keyword in switch default branch");
            default_value = parse_expression();
            assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after switch case value");
            has_default_branch = true;
            continue;
        }

        do {
            if (is_current_kind(TokenKind::Comma)) {
                if (cases.size() > values.size()) {
                    advanced_token();
                }
                else {
                    context->diagnostics.add_diagnostic_error(
                        current_token->position,
                        "In Switch expression can't use `,` with no value before it");
                    throw "Stop";
                }
            }

            auto case_expression = parse_expression();
            auto case_node_type = case_expression->get_ast_node_type();
            if (case_node_type == AstNodeType::NumberExpr ||
                case_node_type == AstNodeType::EnumElementExpr) {
                cases.push_back(case_expression);
                continue;
            }

            // Assert that case node type must be valid type
            context->diagnostics.add_diagnostic_error(
                keyword.position, "Switch expression case must be integer or enum element");
            throw "Stop";
        } while (is_current_kind(TokenKind::Comma));

        size_t case_values_count = cases.size() - values.size();
        auto   rightArrow = consume_kind(TokenKind::RightArrow, "Expect -> after branch value");
        auto   right_value_expression = parse_expression();
        for (size_t i = 0; i < case_values_count; i++) {
            values.push_back(right_value_expression);
        }
        assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after switch case value");
    }

    if (not has_default_branch) {
        context->diagnostics.add_diagnostic_error(keyword.position,
                                                  "Switch expression must has a default case");
        throw "Stop";
    }

    if (cases.empty()) {
        context->diagnostics.add_diagnostic_error(
            keyword.position, "Switch expression must has at last one case and default case");
        throw "Stop";
    }

    assert_kind(TokenKind::CloseBrace, "Expect } after switch Statement last branch");
    return std::make_shared<SwitchExpression>(keyword, argument, cases, values, default_value);
}

std::shared_ptr<GroupExpression> JotParser::parse_group_expression()
{
    Token position = peek_and_advance_token();
    auto  expression = parse_expression();
    assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
    return std::make_shared<GroupExpression>(position, expression);
}

std::shared_ptr<ArrayExpression> JotParser::parse_array_expression()
{
    Token                                    position = peek_and_advance_token();
    std::vector<std::shared_ptr<Expression>> values;
    while (is_source_available() && not is_current_kind(CloseBracket)) {
        values.push_back(parse_expression());
        if (is_current_kind(TokenKind::Comma))
            advanced_token();
    }
    assert_kind(TokenKind::CloseBracket, "Expect ] at the end of array values");
    return std::make_shared<ArrayExpression>(position, values);
}

std::shared_ptr<CastExpression> JotParser::parse_cast_expression()
{
    auto cast_token = consume_kind(TokenKind::CastKeyword, "Expect cast keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after cast keyword");
    auto cast_to_type = parse_type();
    assert_kind(TokenKind::CloseParen, "Expect `)` after cast type");
    auto expression = parse_expression();
    return std::make_shared<CastExpression>(cast_token, cast_to_type, expression);
}

std::shared_ptr<TypeSizeExpression> JotParser::parse_type_size_expression()
{
    auto token = consume_kind(TokenKind::TypeSizeKeyword, "Expect type_size keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after type_size keyword");
    auto type = parse_type();
    assert_kind(TokenKind::CloseParen, "Expect `)` after type_size type");
    return std::make_shared<TypeSizeExpression>(token, type);
}

std::shared_ptr<ValueSizeExpression> JotParser::parse_value_size_expression()
{
    auto token = consume_kind(TokenKind::ValueSizeKeyword, "Expect value_size keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after value_size keyword");
    auto value = parse_expression();
    assert_kind(TokenKind::CloseParen, "Expect `)` after value_size type");
    return std::make_shared<ValueSizeExpression>(token, value);
}

std::shared_ptr<JotType> JotParser::parse_type() { return parse_type_with_prefix(); }

std::shared_ptr<JotType> JotParser::parse_type_with_prefix()
{
    // Parse pointer type
    if (is_current_kind(TokenKind::Star)) {
        return parse_pointer_to_type();
    }

    // Parse function pointer type
    if (is_current_kind(TokenKind::OpenParen)) {
        return parse_function_type();
    }

    // Parse Fixed size array type
    if (is_current_kind(TokenKind::OpenBracket)) {
        return parse_fixed_size_array_type();
    }

    return parse_type_with_postfix();
}

std::shared_ptr<JotType> JotParser::parse_pointer_to_type()
{
    auto star_token = consume_kind(TokenKind::Star, "Pointer type must start with *");
    auto base_type = parse_type_with_prefix();
    return std::make_shared<JotPointerType>(base_type);
}

std::shared_ptr<JotType> JotParser::parse_function_type()
{
    auto paren = consume_kind(TokenKind::OpenParen, "Function type expect to start with {");

    std::vector<std::shared_ptr<JotType>> parameters_types;
    while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
        parameters_types.push_back(parse_type());
        if (is_current_kind(TokenKind::Comma))
            advanced_token();
    }
    assert_kind(TokenKind::CloseParen, "Expect ) after function type parameters");
    auto return_type = parse_type();
    return std::make_shared<JotFunctionType>(paren, parameters_types, return_type);
}

std::shared_ptr<JotType> JotParser::parse_fixed_size_array_type()
{
    auto bracket = consume_kind(TokenKind::OpenBracket, "Expect [ for fixed size array type");

    if (is_current_kind(TokenKind::CloseBracket)) {
        context->diagnostics.add_diagnostic_error(peek_current().position,
                                                  "Fixed array type must have implicit size [n]");
        throw "Stop";
    }

    const auto size = parse_number_expression();
    if (!is_integer_type(size->get_type_node())) {
        context->diagnostics.add_diagnostic_error(bracket.position,
                                                  "Array size must be an integer constants");
        throw "Stop";
    }
    auto number_value = std::atoi(size->get_value().literal.c_str());
    assert_kind(TokenKind::CloseBracket, "Expect ] after array size.");
    auto element_type = parse_type();

    // Check if array element type is not void
    if (is_void_type(element_type)) {
        auto void_token = peek_previous();
        context->diagnostics.add_diagnostic_error(
            void_token.position, "Can't declare array with incomplete type 'void'");
        throw "Stop";
    }

    // Check if array element type is none (incomplete)
    if (is_jot_types_equals(element_type, jot_none_ty)) {
        auto type_token = peek_previous();
        context->diagnostics.add_diagnostic_error(type_token.position,
                                                  "Can't declare array with incomplete type");
        throw "Stop";
    }

    return std::make_shared<JotArrayType>(element_type, number_value);
}

std::shared_ptr<JotType> JotParser::parse_type_with_postfix()
{
    auto primary_type = parse_primary_type();
    // TODO: Can used later to provide sume postfix features such as ? for null safty
    return primary_type;
}

std::shared_ptr<JotType> JotParser::parse_primary_type()
{
    if (is_current_kind(TokenKind::Symbol)) {
        return parse_identifier_type();
    }

    // Show helpful diagnostic error message for varargs case
    if (is_current_kind(TokenKind::VarargsKeyword)) {
        context->diagnostics.add_diagnostic_error(
            peek_current().position, "Varargs not supported as function pointer parameter");
        throw "Stop";
    }

    context->diagnostics.add_diagnostic_error(peek_current().position, "Expected symbol as type");
    throw "Stop";
}

std::shared_ptr<JotType> JotParser::parse_identifier_type()
{
    Token       symbol_token = consume_kind(TokenKind::Symbol, "Expect identifier as type");
    std::string type_literal = symbol_token.literal;

    if (type_literal == "int16") {
        return jot_int16_ty;
    }

    if (type_literal == "uint16") {
        return jot_uint16_ty;
    }

    if (type_literal == "int32") {
        return jot_int32_ty;
    }

    if (type_literal == "uint32") {
        return jot_uint32_ty;
    }

    if (type_literal == "int64") {
        return jot_int64_ty;
    }

    if (type_literal == "uint64") {
        return jot_uint64_ty;
    }

    if (type_literal == "float32") {
        return jot_float32_ty;
    }

    if (type_literal == "float64") {
        return jot_float64_ty;
    }

    if (type_literal == "char" || type_literal == "int8") {
        return jot_int8_ty;
    }

    if (type_literal == "uchar" || type_literal == "uint8") {
        return jot_uint8_ty;
    }

    if (type_literal == "bool" || type_literal == "int1") {
        return jot_int1_ty;
    }

    if (type_literal == "void") {
        return jot_void_ty;
    }

    // Check if this type is structure type
    if (context->structures.count(type_literal)) {
        return context->structures[type_literal];
    }

    // Check if this type is enumeration type
    if (context->enumerations.count(type_literal)) {
        auto enum_type = context->enumerations[type_literal];
        auto enum_element_type =
            std::make_shared<JotEnumElementType>(symbol_token.literal, enum_type->element_type);
        return enum_element_type;
    }

    // Struct with field that has his type for example LinkedList Node struct
    // Current mark it un solved then solve it after building the struct type itself
    if (type_literal == current_struct_name) {
        current_struct_unknown_fields++;
        return jot_none_ty;
    }

    // This type is not permitive, structure or enumerations
    context->diagnostics.add_diagnostic_error(peek_current().position,
                                              "Unexpected identifier type");
    throw "Stop";
}

NumberKind JotParser::get_number_kind(TokenKind token)
{
    switch (token) {
    case TokenKind::Integer: return NumberKind::Integer64;
    case TokenKind::Integer1Type: return NumberKind::Integer1;
    case TokenKind::Integer8Type: return NumberKind::Integer8;
    case TokenKind::Integer16Type: return NumberKind::Integer16;
    case TokenKind::Integer32Type: return NumberKind::Integer32;
    case TokenKind::Integer64Type: return NumberKind::Integer64;
    case TokenKind::UInteger8Type: return NumberKind::UInteger8;
    case TokenKind::UInteger16Type: return NumberKind::UInteger16;
    case TokenKind::UInteger32Type: return NumberKind::UInteger32;
    case TokenKind::UInteger64Type: return NumberKind::UInteger64;
    case TokenKind::Float: return NumberKind::Float64;
    case TokenKind::Float32Type: return NumberKind::Float32;
    case TokenKind::Float64Type: return NumberKind::Float64;
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().position,
                                                  "Token kind is not a number");
        throw "Stop";
    }
    }
}

bool JotParser::is_function_declaration_kind(std::string& fun_name, FunctionDeclarationKind kind)
{
    if (context->functions.count(fun_name)) {
        return context->functions[fun_name] == kind;
    }
    return false;
}

void JotParser::advanced_token()
{
    previous_token = current_token;
    current_token = next_token;
    next_token = tokenizer.scan_next_token();
}

Token JotParser::peek_and_advance_token()
{
    auto current = peek_current();
    advanced_token();
    return current;
}

Token JotParser::peek_previous() { return previous_token.value(); }

Token JotParser::peek_current() { return current_token.value(); }

Token JotParser::peek_next() { return next_token.value(); }

bool JotParser::is_previous_kind(TokenKind kind) { return peek_previous().kind == kind; }

bool JotParser::is_current_kind(TokenKind kind) { return peek_current().kind == kind; }

bool JotParser::is_next_kind(TokenKind kind) { return peek_next().kind == kind; }

Token JotParser::consume_kind(TokenKind kind, const char* message)
{
    if (is_current_kind(kind)) {
        advanced_token();
        return previous_token.value();
    }
    context->diagnostics.add_diagnostic_error(peek_current().position, message);
    throw "Stop";
}

void JotParser::assert_kind(TokenKind kind, const char* message)
{
    if (is_current_kind(kind)) {
        advanced_token();
        return;
    }

    auto location = peek_current().position;
    if (kind == TokenKind::Semicolon) {
        location = peek_previous().position;
    }
    context->diagnostics.add_diagnostic_error(location, message);
    throw "Stop";
}

bool JotParser::is_source_available() { return peek_current().kind != TokenKind::EndOfFile; }
