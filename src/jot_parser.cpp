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

auto JotParser::parse_compilation_unit() -> Shared<CompilationUnit>
{
    std::vector<Shared<Statement>> tree_nodes;
    try {
        while (is_source_available()) {
            // Handle importing std file
            if (is_current_kind(TokenKind::ImportKeyword)) {
                auto module_tree_node = parse_import_declaration();
                append_vectors(tree_nodes, module_tree_node);
                continue;
            }

            // Handle loading user file
            if (is_current_kind(TokenKind::LoadKeyword)) {
                auto module_tree_node = parse_load_declaration();
                append_vectors(tree_nodes, module_tree_node);
                continue;
            }

            // Handle type alias declarations
            if (is_current_kind(TokenKind::TypeKeyword)) {
                parse_type_alias_declaration();
                continue;
            }

            tree_nodes.push_back(parse_declaration_statement());
        }
    }
    catch (const char* message) {
    }
    return std::make_shared<CompilationUnit>(tree_nodes);
}

auto JotParser::parse_import_declaration() -> std::vector<Shared<Statement>>
{
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // import { <string> <string> }
        advanced_token();
        std::vector<Shared<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name = consume_kind(
                TokenKind::String, "Expect string as library name after import statement");
            std::string library_path = "../lib/" + library_name.literal + ".jot";

            if (context->source_manager.is_path_registered(library_path)) {
                continue;
            }

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic_error(
                    library_name.position, "No standard library with name " + library_name.literal);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            append_vectors(tree_nodes, nodes);
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as library name after import statement");
    std::string library_path = "../lib/" + library_name.literal + ".jot";

    if (context->source_manager.is_path_registered(library_path)) {
        return std::vector<Shared<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic_error(
            library_name.position, "No standard library with name " + library_name.literal);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

auto JotParser::parse_load_declaration() -> std::vector<Shared<Statement>>
{
    advanced_token();
    if (is_current_kind(TokenKind::OpenBrace)) {
        // load { <string> <string> }
        advanced_token();
        std::vector<Shared<Statement>> tree_nodes;
        while (is_source_available() && not is_current_kind(TokenKind::CloseBrace)) {
            auto library_name =
                consume_kind(TokenKind::String, "Expect string as file name after load statement");

            std::string library_path = file_parent_path + library_name.literal + ".jot";

            if (context->source_manager.is_path_registered(library_path)) {
                continue;
            }

            if (not is_file_exists(library_path)) {
                context->diagnostics.add_diagnostic_error(library_name.position,
                                                          "Path not exists " + library_path);
                throw "Stop";
            }

            auto nodes = parse_single_source_file(library_path);
            append_vectors(tree_nodes, nodes);
        }
        assert_kind(TokenKind::CloseBrace, "Expect Close brace `}` after import block");
        return tree_nodes;
    }

    auto library_name =
        consume_kind(TokenKind::String, "Expect string as file name after load statement");

    std::string library_path = file_parent_path + library_name.literal + ".jot";

    if (context->source_manager.is_path_registered(library_path)) {
        return std::vector<Shared<Statement>>();
    }

    if (not is_file_exists(library_path)) {
        context->diagnostics.add_diagnostic_error(library_name.position,
                                                  "Path not exists " + library_path);
        throw "Stop";
    }

    return parse_single_source_file(library_path);
}

auto JotParser::parse_type_alias_declaration() -> void
{
    auto type_token = consume_kind(TokenKind::TypeKeyword, "Expect type keyword");
    auto alias_token = consume_kind(TokenKind::Symbol, "Expect identifier for type alias");

    // Make sure alias name is unique
    if (context->type_alias_table.contains(alias_token.literal)) {
        context->diagnostics.add_diagnostic_error(
            alias_token.position, "There already a type with name " + alias_token.literal);
        throw "Stop";
    }

    assert_kind(TokenKind::Equal, "Expect = after alias name");
    auto actual_type = parse_type();

    if (is_enum_type(actual_type)) {
        context->diagnostics.add_diagnostic_error(alias_token.position,
                                                  "You can't use type alias for enum name");
        throw "Stop";
    }

    if (is_enum_element_type(actual_type)) {
        context->diagnostics.add_diagnostic_error(alias_token.position,
                                                  "You can't use type alias for enum element");
        throw "Stop";
    }

    assert_kind(TokenKind::Semicolon, "Expect ; after actual type");
    context->type_alias_table.define_alias(alias_token.literal, actual_type);
}

auto JotParser::parse_single_source_file(std::string& path) -> std::vector<Shared<Statement>>
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

auto JotParser::parse_declaration_statement() -> Shared<Statement>
{
    switch (peek_current().kind) {
    case TokenKind::PrefixKeyword:
    case TokenKind::InfixKeyword:
    case TokenKind::PostfixKeyword: {
        auto call_kind = PREFIX_FUNCTION;
        if (peek_current().kind == TokenKind::InfixKeyword) {
            call_kind = INFIX_FUNCTION;
        }
        else if (peek_current().kind == TokenKind::PostfixKeyword) {
            call_kind = POSTFIX_FUNCTION;
        }
        advanced_token();

        if (is_current_kind(TokenKind::ExternKeyword)) {
            return parse_function_prototype(call_kind, true);
        }
        else if (is_current_kind(TokenKind::FunKeyword)) {
            return parse_function_declaration(call_kind);
        }

        context->diagnostics.add_diagnostic_error(
            peek_current().position, "Prefix, Infix, postfix keyword used only with functions");
        throw "Stop";
    }
    case TokenKind::ExternKeyword: {
        return parse_function_prototype(NORMAL_FUNCTION, true);
    }
    case TokenKind::IntrinsicKeyword: {
        return parse_intrinsic_prototype();
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

auto JotParser::parse_statement() -> Shared<Statement>
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

auto JotParser::parse_field_declaration(bool is_global) -> Shared<FieldDeclaration>
{
    assert_kind(TokenKind::VarKeyword, "Expect var keyword.");
    auto name = consume_kind(TokenKind::Symbol, "Expect identifier as variable name.");
    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        auto               type = parse_type();
        Shared<Expression> initalizer;
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

auto JotParser::parse_intrinsic_prototype() -> Shared<IntrinsicPrototype>
{
    auto intrinsic_keyword = consume_kind(TokenKind::IntrinsicKeyword, "Expect intrinsic keyword");

    std::string intrinsic_name;
    if (is_current_kind(TokenKind::OpenParen)) {
        advanced_token();
        auto intrinsic_token = consume_kind(TokenKind::String, "Expect intrinsic native name.");
        intrinsic_name = intrinsic_token.literal;
        if (!is_valid_intrinsic_name(intrinsic_name)) {
            context->diagnostics.add_diagnostic_error(
                intrinsic_token.position, "Intrinsic name can't have space or be empty");
            throw "Stop";
        }
        assert_kind(TokenKind::CloseParen, "Expect ) after native intrinsic name.");
    }

    assert_kind(TokenKind::FunKeyword, "Expect function keyword.");
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as function name.");
    if (intrinsic_name.empty()) {
        intrinsic_name = name.literal;
    }

    bool                           has_varargs = false;
    Shared<JotType>                varargs_type = nullptr;
    std::vector<Shared<Parameter>> parameters;
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

    // Register current function declaration kind
    context->functions[name.literal] = FunctionDeclarationKind::NORMAL_FUNCTION;

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
    if (return_type->type_kind == TypeKind::ARRAY) {
        context->diagnostics.add_diagnostic_error(
            name.position, "Function cannot return array type " + jot_type_literal(return_type));
        throw "Stop";
    }

    assert_kind(TokenKind::Semicolon, "Expect ; after external function declaration");

    return std::make_shared<IntrinsicPrototype>(name, intrinsic_name, parameters, return_type,
                                                has_varargs, varargs_type);
}

auto JotParser::parse_function_prototype(FunctionDeclarationKind kind, bool is_external)
    -> Shared<FunctionPrototype>
{
    if (is_external) {
        assert_kind(TokenKind::ExternKeyword, "Expect external keyword");
    }

    assert_kind(TokenKind::FunKeyword, "Expect function keyword.");
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as function name.");

    bool                           has_varargs = false;
    Shared<JotType>                varargs_type = nullptr;
    std::vector<Shared<Parameter>> parameters;
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
    if (return_type->type_kind == TypeKind::ARRAY) {
        context->diagnostics.add_diagnostic_error(
            name.position, "Function cannot return array type " + jot_type_literal(return_type));
        throw "Stop";
    }

    if (is_external) {
        assert_kind(TokenKind::Semicolon, "Expect ; after external function declaration");
    }
    return std::make_shared<FunctionPrototype>(name, parameters, return_type, is_external,
                                               has_varargs, varargs_type);
}

auto JotParser::parse_function_declaration(FunctionDeclarationKind kind)
    -> Shared<FunctionDeclaration>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::FUNCTION_SCOPE;

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

auto JotParser::parse_structure_declaration(bool is_packed) -> Shared<StructDeclaration>
{
    auto struct_token = consume_kind(TokenKind::StructKeyword, "Expect struct keyword");
    auto struct_name = consume_kind(TokenKind::Symbol, "Expect Symbol as struct name");
    auto struct_name_str = struct_name.literal;

    // Make sure this name is unique
    if (context->structures.contains(struct_name_str)) {
        context->diagnostics.add_diagnostic_error(
            struct_name.position, "There is already struct with name " + struct_name_str);
        throw "Stop";
    }

    // Make sure this name is unique and no alias use it
    if (context->type_alias_table.contains(struct_name_str)) {
        context->diagnostics.add_diagnostic_error(
            struct_name.position, "There is already a type with name " + struct_name_str);
        throw "Stop";
    }

    current_struct_name = struct_name.literal;

    std::vector<std::string> generics_parameters;

    // Parse generic parameters declarations if they exists
    bool is_generic_struct = is_current_kind(TokenKind::Smaller);
    if (is_generic_struct) {
        advanced_token();
        while (is_source_available() && !is_current_kind(TokenKind::Greater)) {
            auto parameter = consume_kind(TokenKind::Symbol, "Expect parameter name");

            if (!generic_parameters_names.insert(parameter.literal).second) {
                context->diagnostics.add_diagnostic_error(
                    parameter.position,
                    "You already declared generic parameter with name " + parameter.literal);
                throw "Stop";
            }

            generics_parameters.push_back(parameter.literal);

            if (is_current_kind(TokenKind::Comma)) {
                advanced_token();
            }
        }
        assert_kind(TokenKind::Greater, "Expect > after struct type parameters");
    }

    std::vector<std::string>     fields_names;
    std::vector<Shared<JotType>> fields_types;
    assert_kind(TokenKind::OpenBrace, "Expect { after struct name");
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        auto field_name = consume_kind(TokenKind::Symbol, "Expect Symbol as struct name");

        if (is_contains(fields_names, field_name.literal)) {
            context->diagnostics.add_diagnostic_error(field_name.position,
                                                      "There is already struct member with name " +
                                                          field_name.literal);
            throw "Stop";
        }

        fields_names.push_back(field_name.literal);
        auto field_type = parse_type();

        // Handle Incomplete field type case
        if (field_type->type_kind == TypeKind::NONE) {
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
        std::make_shared<JotStructType>(struct_name_str, fields_names, fields_types,
                                        generics_parameters, is_packed, is_generic_struct);

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
    context->type_alias_table.define_alias(struct_name_str, structure_type);
    current_struct_name = "";
    generic_parameters_names.clear();
    return std::make_shared<StructDeclaration>(structure_type);
}

auto JotParser::parse_enum_declaration() -> Shared<EnumDeclaration>
{
    auto enum_token = consume_kind(TokenKind::EnumKeyword, "Expect enum keyword");
    auto enum_name = consume_kind(TokenKind::Symbol, "Expect Symbol as enum name");

    Shared<JotType> element_type = nullptr;

    if (is_current_kind(TokenKind::Colon)) {
        advanced_token();
        element_type = parse_type();
    }
    else {
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
            if (explicit_values.contains(explicit_value)) {
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

auto JotParser::parse_parameter() -> Shared<Parameter>
{
    Token name = consume_kind(TokenKind::Symbol, "Expect identifier as parameter name.");
    auto  type = parse_type();
    return std::make_shared<Parameter>(name, type);
}

auto JotParser::parse_block_statement() -> Shared<BlockStatement>
{
    consume_kind(TokenKind::OpenBrace, "Expect { on the start of block.");
    std::vector<Shared<Statement>> statements;
    while (is_source_available() && !is_current_kind(TokenKind::CloseBrace)) {
        statements.push_back(parse_statement());
    }
    consume_kind(TokenKind::CloseBrace, "Expect } on the end of block.");
    return std::make_shared<BlockStatement>(statements);
}

auto JotParser::parse_return_statement() -> Shared<ReturnStatement>
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

auto JotParser::parse_defer_statement() -> Shared<DeferStatement>
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

auto JotParser::parse_break_statement() -> Shared<BreakStatement>
{
    auto break_token = consume_kind(TokenKind::BreakKeyword, "Expect break keyword.");

    if (current_ast_scope != AstNodeScope::CONDITION_SCOPE or loop_levels_stack.top() == 0) {
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

auto JotParser::parse_continue_statement() -> Shared<ContinueStatement>
{
    auto continue_token = consume_kind(TokenKind::ContinueKeyword, "Expect continue keyword.");

    if (current_ast_scope != AstNodeScope::CONDITION_SCOPE or loop_levels_stack.top() == 0) {
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

auto JotParser::parse_if_statement() -> Shared<IfStatement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::CONDITION_SCOPE;

    auto if_token = consume_kind(TokenKind::IfKeyword, "Expect If keyword.");
    auto condition = parse_expression();
    auto then_block = parse_statement();
    auto conditional_block = std::make_shared<ConditionalBlock>(if_token, condition, then_block);
    std::vector<Shared<ConditionalBlock>> conditional_blocks;
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

auto JotParser::parse_for_statement() -> Shared<Statement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::CONDITION_SCOPE;

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
        Shared<Expression> step = nullptr;
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

auto JotParser::parse_while_statement() -> Shared<WhileStatement>
{
    auto parent_node_scope = current_ast_scope;
    current_ast_scope = AstNodeScope::CONDITION_SCOPE;

    auto keyword = consume_kind(TokenKind::WhileKeyword, "Expect while keyword.");
    auto condition = parse_expression();

    loop_levels_stack.top() += 1;
    auto body = parse_statement();
    loop_levels_stack.top() -= 1;

    current_ast_scope = parent_node_scope;
    return std::make_shared<WhileStatement>(keyword, condition, body);
}

auto JotParser::parse_switch_statement() -> Shared<SwitchStatement>
{
    auto keyword = consume_kind(TokenKind::SwitchKeyword, "Expect switch keyword.");
    auto argument = parse_expression();
    assert_kind(TokenKind::OpenBrace, "Expect { after switch value");

    std::vector<Shared<SwitchCase>> switch_cases;
    Shared<SwitchCase>              default_branch = nullptr;
    bool                            has_default_branch = false;
    while (is_source_available() and !is_current_kind(TokenKind::CloseBrace)) {
        std::vector<Shared<Expression>> values;
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
        auto right_arrow = consume_kind(TokenKind::RightArrow, "Expect -> after branch value");
        auto branch = parse_statement();
        auto switch_case = std::make_shared<SwitchCase>(right_arrow, values, branch);
        switch_cases.push_back(switch_case);
    }
    assert_kind(TokenKind::CloseBrace, "Expect } after switch Statement last branch");
    return std::make_shared<SwitchStatement>(keyword, argument, switch_cases, default_branch);
}

auto JotParser::parse_expression_statement() -> Shared<ExpressionStatement>
{
    auto expression = parse_expression();
    assert_kind(TokenKind::Semicolon, "Expect semicolon `;` after field declaration");
    return std::make_shared<ExpressionStatement>(expression);
}

auto JotParser::parse_expression() -> Shared<Expression> { return parse_assignment_expression(); }

auto JotParser::parse_assignment_expression() -> Shared<Expression>
{
    auto expression = parse_logical_or_expression();
    if (is_assignments_operator_token(peek_current())) {
        auto               assignments_token = peek_and_advance_token();
        Shared<Expression> right_value;
        auto               assignments_token_kind = assignments_token.kind;
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

auto JotParser::parse_logical_or_expression() -> Shared<Expression>
{
    auto expression = parse_logical_and_expression();
    while (is_current_kind(TokenKind::LogicalOr)) {
        auto or_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, or_token, right);
    }
    return expression;
}

auto JotParser::parse_logical_and_expression() -> Shared<Expression>
{
    auto expression = parse_equality_expression();
    while (is_current_kind(TokenKind::LogicalAnd)) {
        auto and_token = peek_and_advance_token();
        auto right = parse_equality_expression();
        expression = std::make_shared<LogicalExpression>(expression, and_token, right);
    }
    return expression;
}

auto JotParser::parse_equality_expression() -> Shared<Expression>
{
    auto expression = parse_comparison_expression();
    while (is_current_kind(TokenKind::EqualEqual) || is_current_kind(TokenKind::BangEqual)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_comparison_expression();
        expression = std::make_shared<ComparisonExpression>(expression, operator_token, right);
    }
    return expression;
}

auto JotParser::parse_comparison_expression() -> Shared<Expression>
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

auto JotParser::parse_shift_expression() -> Shared<Expression>
{
    auto expression = parse_term_expression();
    while (true) {
        if (is_current_kind(TokenKind::LeftShift)) {
            auto operator_token = peek_and_advance_token();
            auto right = parse_term_expression();
            expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
            continue;
        }

        if (is_current_kind(TokenKind::Greater) && is_next_kind(TokenKind::Greater)) {
            advanced_token();
            auto operator_token = peek_and_advance_token();
            operator_token.kind = TokenKind::RightShift;
            auto right = parse_term_expression();
            expression = std::make_shared<ShiftExpression>(expression, operator_token, right);
            continue;
        }

        break;
    }

    return expression;
}

auto JotParser::parse_term_expression() -> Shared<Expression>
{
    auto expression = parse_factor_expression();
    while (is_current_kind(TokenKind::Plus) || is_current_kind(TokenKind::Minus)) {
        Token operator_token = peek_and_advance_token();
        auto  right = parse_factor_expression();
        expression = std::make_shared<BinaryExpression>(expression, operator_token, right);
    }
    return expression;
}

auto JotParser::parse_factor_expression() -> Shared<Expression>
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

auto JotParser::parse_enum_access_expression() -> Shared<Expression>
{
    auto expression = parse_infix_call_expression();
    if (is_current_kind(TokenKind::ColonColon)) {
        auto colons_token = peek_and_advance_token();
        if (auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression)) {
            auto enum_name = literal->get_name();
            if (context->enumerations.contains(enum_name.literal)) {
                auto enum_type = context->enumerations[enum_name.literal];
                auto element =
                    consume_kind(TokenKind::Symbol, "Expect identifier as enum field name");

                auto enum_values = enum_type->values;
                if (not enum_values.contains(element.literal)) {
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

auto JotParser::parse_infix_call_expression() -> Shared<Expression>
{
    auto expression = parse_prefix_expression();
    auto current_token_literal = peek_current().literal;

    // Parse Infix function call as a call expression
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, INFIX_FUNCTION)) {
        auto                            symbol_token = peek_current();
        auto                            literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(expression);
        arguments.push_back(parse_infix_call_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }

    return expression;
}

auto JotParser::parse_prefix_expression() -> Shared<Expression>
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

auto JotParser::parse_prefix_call_expression() -> Shared<Expression>
{
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, PREFIX_FUNCTION)) {
        Token                           symbol_token = peek_current();
        auto                            literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(parse_prefix_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_postfix_increment_or_decrement();
}

auto JotParser::parse_postfix_increment_or_decrement() -> Shared<Expression>
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

auto JotParser::parse_call_or_access_expression() -> Shared<Expression>
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
            auto                            position = peek_and_advance_token();
            std::vector<Shared<Expression>> arguments;
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

auto JotParser::parse_enumeration_attribute_expression() -> Shared<Expression>
{
    auto expression = parse_postfix_call_expression();
    if (is_current_kind(TokenKind::Dot) and
        expression->get_ast_node_type() == AstNodeType::LiteralExpr) {
        auto literal = std::dynamic_pointer_cast<LiteralExpression>(expression);
        auto literal_str = literal->get_name().literal;
        if (context->enumerations.contains(literal_str)) {
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

auto JotParser::parse_postfix_call_expression() -> Shared<Expression>
{
    auto expression = parse_initializer_expression();
    auto current_token_literal = peek_current().literal;
    if (is_current_kind(TokenKind::Symbol) and
        is_function_declaration_kind(current_token_literal, POSTFIX_FUNCTION)) {
        Token                           symbol_token = peek_current();
        auto                            literal = parse_literal_expression();
        std::vector<Shared<Expression>> arguments;
        arguments.push_back(expression);
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return expression;
}

auto JotParser::parse_initializer_expression() -> Shared<Expression>
{
    if (is_current_kind(TokenKind::Symbol) &&
        context->type_alias_table.contains(current_token->literal)) {
        auto resolved_type = context->type_alias_table.resolve_alias(current_token->literal);
        if (is_struct_type(resolved_type) || is_generic_struct_type(resolved_type)) {
            if (is_next_kind(TokenKind::OpenBrace) || is_next_kind(TokenKind::Smaller)) {
                auto type = parse_type();
                auto token =
                    consume_kind(TokenKind::OpenBrace, "Expect { at the start of initializer");
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
        }
    }

    return parse_function_call_with_lambda_argument();
}

auto JotParser::parse_function_call_with_lambda_argument() -> Shared<Expression>
{
    if (is_current_kind(TokenKind::Symbol) and is_next_kind(TokenKind::OpenBrace) and
        is_function_declaration_kind(current_token->literal, NORMAL_FUNCTION)) {
        auto symbol_token = peek_current();
        auto literal = parse_literal_expression();

        std::vector<Shared<Expression>> arguments;
        arguments.push_back(parse_lambda_expression());
        return std::make_shared<CallExpression>(symbol_token, literal, arguments);
    }
    return parse_primary_expression();
}

auto JotParser::parse_primary_expression() -> Shared<Expression>
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

auto JotParser::parse_lambda_expression() -> Shared<LambdaExpression>
{
    auto open_paren =
        consume_kind(TokenKind::OpenBrace, "Expect { at the start of lambda expression");

    std::vector<Shared<Parameter>> parameters;

    Shared<JotType> return_type;

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
    std::vector<Shared<Statement>> body;
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

auto JotParser::parse_number_expression() -> Shared<NumberExpression>
{
    auto number_token = peek_and_advance_token();
    auto number_kind = get_number_kind(number_token.kind);
    auto number_type = std::make_shared<JotNumberType>(number_kind);
    return std::make_shared<NumberExpression>(number_token, number_type);
}

auto JotParser::parse_literal_expression() -> Shared<LiteralExpression>
{
    Token symbol_token = peek_and_advance_token();
    auto  type = std::make_shared<JotNoneType>();
    return std::make_shared<LiteralExpression>(symbol_token, type);
}

auto JotParser::parse_if_expression() -> Shared<IfExpression>
{
    Token if_token = peek_and_advance_token();
    auto  condition = parse_expression();
    auto  then_value = parse_expression();
    Token else_token =
        consume_kind(TokenKind::ElseKeyword, "Expect `else` keyword after then value.");
    auto else_value = parse_expression();
    return std::make_shared<IfExpression>(if_token, else_token, condition, then_value, else_value);
}

auto JotParser::parse_switch_expression() -> Shared<SwitchExpression>
{
    auto keyword = consume_kind(TokenKind::SwitchKeyword, "Expect switch keyword.");
    auto argument = parse_expression();
    assert_kind(TokenKind::OpenBrace, "Expect { after switch value");
    std::vector<Shared<Expression>> cases;
    std::vector<Shared<Expression>> values;
    Shared<Expression>              default_value;
    bool                            has_default_branch = false;
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

auto JotParser::parse_group_expression() -> Shared<GroupExpression>
{
    Token position = peek_and_advance_token();
    auto  expression = parse_expression();
    assert_kind(TokenKind::CloseParen, "Expect ) after in the end of call expression");
    return std::make_shared<GroupExpression>(position, expression);
}

auto JotParser::parse_array_expression() -> Shared<ArrayExpression>
{
    Token                           position = peek_and_advance_token();
    std::vector<Shared<Expression>> values;
    while (is_source_available() && not is_current_kind(CloseBracket)) {
        values.push_back(parse_expression());
        if (is_current_kind(TokenKind::Comma)) {
            advanced_token();
        }
    }
    assert_kind(TokenKind::CloseBracket, "Expect ] at the end of array values");
    return std::make_shared<ArrayExpression>(position, values);
}

auto JotParser::parse_cast_expression() -> Shared<CastExpression>
{
    auto cast_token = consume_kind(TokenKind::CastKeyword, "Expect cast keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after cast keyword");
    auto cast_to_type = parse_type();
    assert_kind(TokenKind::CloseParen, "Expect `)` after cast type");
    auto expression = parse_expression();
    return std::make_shared<CastExpression>(cast_token, cast_to_type, expression);
}

auto JotParser::parse_type_size_expression() -> Shared<TypeSizeExpression>
{
    auto token = consume_kind(TokenKind::TypeSizeKeyword, "Expect type_size keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after type_size keyword");
    auto type = parse_type();
    assert_kind(TokenKind::CloseParen, "Expect `)` after type_size type");
    return std::make_shared<TypeSizeExpression>(token, type);
}

auto JotParser::parse_value_size_expression() -> Shared<ValueSizeExpression>
{
    auto token = consume_kind(TokenKind::ValueSizeKeyword, "Expect value_size keyword");
    assert_kind(TokenKind::OpenParen, "Expect `(` after value_size keyword");
    auto value = parse_expression();
    assert_kind(TokenKind::CloseParen, "Expect `)` after value_size type");
    return std::make_shared<ValueSizeExpression>(token, value);
}

auto JotParser::get_number_kind(TokenKind token) -> NumberKind
{
    switch (token) {
    case TokenKind::Integer: return NumberKind::INTEGER_64;
    case TokenKind::Integer1Type: return NumberKind::INTEGER_1;
    case TokenKind::Integer8Type: return NumberKind::INTEGER_8;
    case TokenKind::Integer16Type: return NumberKind::INTEGER_16;
    case TokenKind::Integer32Type: return NumberKind::INTEGER_32;
    case TokenKind::Integer64Type: return NumberKind::INTEGER_64;
    case TokenKind::UInteger8Type: return NumberKind::U_INTEGER_8;
    case TokenKind::UInteger16Type: return NumberKind::U_INTEGER_16;
    case TokenKind::UInteger32Type: return NumberKind::U_INTEGER_32;
    case TokenKind::UInteger64Type: return NumberKind::U_INTEGER_64;
    case TokenKind::Float: return NumberKind::FLOAT_64;
    case TokenKind::Float32Type: return NumberKind::FLOAT_32;
    case TokenKind::Float64Type: return NumberKind::FLOAT_64;
    default: {
        context->diagnostics.add_diagnostic_error(peek_current().position,
                                                  "Token kind is not a number");
        throw "Stop";
    }
    }
}

auto JotParser::is_function_declaration_kind(std::string& fun_name, FunctionDeclarationKind kind)
    -> bool
{
    if (context->functions.contains(fun_name)) {
        return context->functions[fun_name] == kind;
    }
    return false;
}

auto JotParser::is_valid_intrinsic_name(std::string& name) -> bool
{
    if (name.empty()) {
        return false;
    }

    if (name.find(' ') != std::string::npos) {
        return false;
    }

    return true;
}

auto JotParser::advanced_token() -> void
{
    previous_token = current_token;
    current_token = next_token;
    next_token = tokenizer.scan_next_token();
}

auto JotParser::peek_and_advance_token() -> Token
{
    auto current = peek_current();
    advanced_token();
    return current;
}

auto JotParser::peek_previous() -> Token { return previous_token.value(); }

auto JotParser::peek_current() -> Token { return current_token.value(); }

auto JotParser::peek_next() -> Token { return next_token.value(); }

auto JotParser::is_previous_kind(TokenKind kind) -> bool { return peek_previous().kind == kind; }

auto JotParser::is_current_kind(TokenKind kind) -> bool { return peek_current().kind == kind; }

auto JotParser::is_next_kind(TokenKind kind) -> bool { return peek_next().kind == kind; }

auto JotParser::consume_kind(TokenKind kind, const char* message) -> Token
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

auto JotParser::is_source_available() -> bool
{
    return peek_current().kind != TokenKind::EndOfFile;
}
