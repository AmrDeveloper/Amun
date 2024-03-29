#include "../include/amun_name_mangle.hpp"
#include "../include/amun_parser.hpp"

#include <unordered_map>

auto amun::Parser::parse_type() -> Shared<amun::Type>
{
    if (is_current_kind(TokenKind::TOKEN_AT)) {
        return pares_types_directive();
    }
    return parse_type_with_prefix();
}

auto amun::Parser::parse_type_with_prefix() -> Shared<amun::Type>
{
    // Parse function pointer type
    if (is_current_kind(TokenKind::TOKEN_FUN)) {
        return parse_function_ptr_type();
    }

    // Parse pointer type
    if (is_current_kind(TokenKind::TOKEN_STAR)) {
        return parse_pointer_to_type();
    }

    // Parse tuple type
    if (is_current_kind(TokenKind::TOKEN_OPEN_PAREN)) {
        return parse_tuple_type();
    }

    // Parse Fixed size array type
    if (is_current_kind(TokenKind::TOKEN_OPEN_BRACKET)) {
        return parse_fixed_size_array_type();
    }

    return parse_type_with_postfix();
}

auto amun::Parser::parse_pointer_to_type() -> Shared<amun::Type>
{
    assert_kind(TokenKind::TOKEN_STAR, "Pointer type must be started with *");
    return std::make_shared<amun::PointerType>(parse_type_with_prefix());
}

auto amun::Parser::parse_function_ptr_type() -> Shared<amun::Type>
{
    assert_kind(TokenKind::TOKEN_FUN, "Expect `fun` keyword at the start of function ptr");
    auto paren = peek_current();
    auto parameters_types = parse_list_of_types();
    auto return_type = parse_type();
    auto function_type = std::make_shared<amun::FunctionType>(paren, parameters_types, return_type);
    return std::make_shared<amun::PointerType>(function_type);
}

auto amun::Parser::parse_tuple_type() -> Shared<amun::Type>
{
    auto paren = peek_current();
    auto field_types = parse_list_of_types();

    if (field_types.size() < 2) {
        context->diagnostics.report_error(paren.position, "Tuple type must has at least 2 types");
        throw "Stop";
    }

    auto tuple_type = std::make_shared<amun::TupleType>("", field_types);
    tuple_type->name = mangle_tuple_type(tuple_type);
    return tuple_type;
}

auto amun::Parser::parse_list_of_types() -> std::vector<Shared<amun::Type>>
{
    assert_kind(TokenKind::TOKEN_OPEN_PAREN, "Expect `(` before types");

    std::vector<Shared<amun::Type>> types;
    while (is_source_available() && !is_current_kind(TokenKind::TOKEN_CLOSE_PAREN)) {
        types.push_back(parse_type());
        if (is_current_kind(TokenKind::TOKEN_COMMA)) {
            advanced_token();
        }
        else {
            break;
        }
    }

    assert_kind(TokenKind::TOKEN_CLOSE_PAREN, "Expect `)` after types");

    return types;
}

auto amun::Parser::parse_fixed_size_array_type() -> Shared<amun::Type>
{
    auto bracket =
        consume_kind(TokenKind::TOKEN_OPEN_BRACKET, "Expect [ for fixed size array type");

    if (is_current_kind(TokenKind::TOKEN_CLOSE_BRACKET)) {
        context->diagnostics.report_error(peek_current().position,
                                          "Fixed array type must have implicit size [n]");
        throw "Stop";
    }

    // Can't use `parse_number_expression` directly because it may be a const identifier
    // it will resolved to number for example
    //
    // const SIZE = 10;
    // var array : [SIZE]int64;
    //
    const auto size_expression = parse_expression();
    if (size_expression->get_ast_node_type() != AstNodeType::AST_NUMBER) {
        context->diagnostics.report_error(bracket.position,
                                          "Array size must be an integer constants");
        throw "Stop";
    }

    const auto size = std::dynamic_pointer_cast<NumberExpression>(size_expression);
    if (!amun::is_integer_type(size->get_type_node())) {
        context->diagnostics.report_error(bracket.position,
                                          "Array size must be an integer constants");
        throw "Stop";
    }

    auto number_value = std::atoi(size->value.literal.c_str());
    assert_kind(TokenKind::TOKEN_CLOSE_BRACKET, "Expect ] after array size.");
    auto element_type = parse_type();

    // Check if array element type is not void
    if (amun::is_void_type(element_type)) {
        auto void_token = peek_previous();
        context->diagnostics.report_error(void_token.position,
                                          "Can't declare array with incomplete type 'void'");
        throw "Stop";
    }

    // Check if array element type is none (incomplete)
    if (amun::is_types_equals(element_type, amun::none_type)) {
        auto type_token = peek_previous();
        context->diagnostics.report_error(type_token.position,
                                          "Can't declare array with incomplete type");
        throw "Stop";
    }

    return std::make_shared<amun::StaticArrayType>(element_type, number_value);
}

auto amun::Parser::parse_type_with_postfix() -> Shared<amun::Type>
{
    auto type = parse_generic_struct_type();

    // Report useful message when user create pointer type with prefix `*` like in C
    if (is_current_kind(TokenKind::TOKEN_STAR)) {
        context->diagnostics.report_error(peek_previous().position,
                                          "In pointer type `*` must be before the type like *" +
                                              amun::get_type_literal(type));
        throw "Stop";
    }

    return type;
}

auto amun::Parser::parse_generic_struct_type() -> Shared<amun::Type>
{
    auto primary_type = parse_primary_type();

    // Parse generic struct type with types parameters
    if (is_current_kind(TokenKind::TOKEN_SMALLER)) {
        if (primary_type->type_kind == amun::TypeKind::STRUCT) {
            auto smaller_token = peek_current();
            auto struct_type = std::static_pointer_cast<amun::StructType>(primary_type);

            // Prevent use non generic struct type with any type parameters
            if (!struct_type->is_generic) {
                context->diagnostics.report_error(
                    smaller_token.position,
                    "Non generic struct type don't accept any types parameters");
                throw "Stop";
            }

            auto generic_parameters = parse_generic_arguments_if_exists();

            // Make sure generic struct types is used with correct number of parameters
            if (struct_type->generic_parameters.size() != generic_parameters.size()) {
                context->diagnostics.report_error(
                    smaller_token.position,
                    "Invalid number of generic parameters expect " +
                        std::to_string(struct_type->generic_parameters.size()) + " but got " +
                        std::to_string(generic_parameters.size()));
                throw "Stop";
            }

            return std::make_shared<amun::GenericStructType>(struct_type, generic_parameters);
        }

        context->diagnostics.report_error(peek_previous().position,
                                          "Only structures can accept generic parameters");
        throw "Stop";
    }

    // Assert that generic structs types must be created with parameters types
    if (primary_type->type_kind == amun::TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<amun::StructType>(primary_type);
        if (struct_type->is_generic) {
            auto struct_name = peek_previous();
            context->diagnostics.report_error(
                struct_name.position, "Generic struct type must be used with parameters types " +
                                          struct_name.literal + "<..>");
            throw "Stop";
        }
    }

    return primary_type;
}

auto amun::Parser::parse_primary_type() -> Shared<amun::Type>
{
    // Check if this type is an identifier
    if (is_current_kind(TokenKind::TOKEN_IDENTIFIER)) {
        return parse_identifier_type();
    }

    // Show helpful diagnostic error message for varargs case
    if (is_current_kind(TokenKind::TOKEN_VARARGS)) {
        context->diagnostics.report_error(peek_current().position,
                                          "Varargs not supported as function pointer parameter");
        throw "Stop";
    }

    context->diagnostics.report_error(peek_current().position, "Expected type name");
    throw "Stop";
}

auto amun::Parser::parse_identifier_type() -> Shared<amun::Type>
{
    Token symbol_token = consume_kind(TokenKind::TOKEN_IDENTIFIER, "Expect identifier as type");
    std::string type_literal = symbol_token.literal;

    // Check if this time is primitive
    if (primitive_types.contains(type_literal)) {
        return primitive_types[type_literal];
    }

    // Check if this type is structure type
    if (context->structures.contains(type_literal)) {
        return context->structures[type_literal];
    }

    // Check if this type is enumeration type
    if (context->enumerations.contains(type_literal)) {
        auto enum_type = context->enumerations[type_literal];
        auto enum_element_type =
            std::make_shared<amun::EnumElementType>(symbol_token.literal, enum_type->element_type);
        return enum_element_type;
    }

    // Struct with field that has his type for example LinkedList Node struct
    // Current mark it un solved then solve it after building the struct type itself
    if (type_literal == current_struct_name) {
        current_struct_unknown_fields++;
        return amun::none_type;
    }

    // Check if this is a it generic type parameter
    if (generic_parameters_names.contains(type_literal)) {
        return std::make_shared<amun::GenericParameterType>(type_literal);
    }

    // Check if this identifier is a type alias key
    if (context->type_alias_table.contains(type_literal)) {
        return context->type_alias_table.resolve_alias(type_literal);
    }

    // This type is not permitive, structure or enumerations
    context->diagnostics.report_error(peek_current().position,
                                      "Cannot find type `" + type_literal + "` in this scope");
    throw "Stop";
}