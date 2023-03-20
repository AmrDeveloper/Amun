#include "../include/jot_parser.hpp"

#include <unordered_map>

static std::unordered_map<std::string, Shared<JotType>> primitive_types = {
    {"int1", jot_int1_ty},       {"bool", jot_int1_ty},
    {"char", jot_int8_ty},       {"uchar", jot_uint8_ty},

    {"int8", jot_int8_ty},       {"int16", jot_int16_ty},
    {"int32", jot_int32_ty},     {"int64", jot_int64_ty},

    {"uint8", jot_uint8_ty},     {"uint16", jot_uint16_ty},
    {"uint32", jot_uint32_ty},   {"uint64", jot_uint64_ty},

    {"float32", jot_float32_ty}, {"float64", jot_float64_ty},

    {"void", jot_void_ty},
};

auto JotParser::parse_type() -> Shared<JotType> { return parse_type_with_prefix(); }

auto JotParser::parse_type_with_prefix() -> Shared<JotType>
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

auto JotParser::parse_pointer_to_type() -> Shared<JotType>
{
    auto star_token = consume_kind(TokenKind::Star, "Pointer type must start with *");
    auto base_type = parse_type_with_prefix();
    return std::make_shared<JotPointerType>(base_type);
}

auto JotParser::parse_function_type() -> Shared<JotType>
{
    auto paren = consume_kind(TokenKind::OpenParen, "Function type expect to start with {");

    std::vector<Shared<JotType>> parameters_types;
    while (is_source_available() && not is_current_kind(TokenKind::CloseParen)) {
        parameters_types.push_back(parse_type());
        if (is_current_kind(TokenKind::Comma)) {
            advanced_token();
        }
    }
    assert_kind(TokenKind::CloseParen, "Expect ) after function type parameters");
    auto return_type = parse_type();
    return std::make_shared<JotFunctionType>(paren, parameters_types, return_type);
}

auto JotParser::parse_fixed_size_array_type() -> Shared<JotType>
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

auto JotParser::parse_type_with_postfix() -> Shared<JotType> { return parse_generic_struct_type(); }

auto JotParser::parse_generic_struct_type() -> Shared<JotType>
{
    auto primary_type = parse_primary_type();

    // Parse generic struct type with types parameters
    if (is_current_kind(TokenKind::Smaller)) {
        if (primary_type->type_kind == TypeKind::STRUCT) {
            auto smaller_token = consume_kind(TokenKind::Smaller, "Expect < after struct name");
            auto struct_type = std::static_pointer_cast<JotStructType>(primary_type);

            // Prevent use non generic struct type with any type parameters
            if (!struct_type->is_generic) {
                context->diagnostics.add_diagnostic_error(
                    smaller_token.position,
                    "Non generic struct type don't accept any types parameters");
                throw "Stop";
            }

            std::vector<Shared<JotType>> generic_parameters;
            while (is_source_available() && !is_current_kind(TokenKind::Greater)) {
                auto parameter = parse_type();
                generic_parameters.push_back(parameter);

                if (is_current_kind(TokenKind::Comma)) {
                    advanced_token();
                }
            }

            assert_kind(TokenKind::Greater, "Expect > After generic types parameters");

            // Make sure generic struct types is used with correct number of parameters
            if (struct_type->generic_parameters.size() != generic_parameters.size()) {
                context->diagnostics.add_diagnostic_error(
                    smaller_token.position,
                    "Invalid number of generic parameters expect " +
                        std::to_string(struct_type->generic_parameters.size()) + " but got " +
                        std::to_string(generic_parameters.size()));
                throw "Stop";
            }

            return std::make_shared<JotGenericStructType>(struct_type, generic_parameters);
        }

        context->diagnostics.add_diagnostic_error(peek_previous().position,
                                                  "Only structures can accept generic parameters");
        throw "Stop";
    }

    // Assert that generic structs types must be created with parameters types
    if (primary_type->type_kind == TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<JotStructType>(primary_type);
        if (struct_type->is_generic) {
            auto struct_name = peek_previous();
            context->diagnostics.add_diagnostic_error(
                struct_name.position, "Generic struct type must be used with parameters types " +
                                          struct_name.literal + "<..>");
            throw "Stop";
        }
    }

    return primary_type;
}

auto JotParser::parse_primary_type() -> Shared<JotType>
{
    // Check if this type is an identifier
    if (is_current_kind(TokenKind::Symbol)) {
        return parse_identifier_type();
    }

    // Show helpful diagnostic error message for varargs case
    if (is_current_kind(TokenKind::VarargsKeyword)) {
        context->diagnostics.add_diagnostic_error(
            peek_current().position, "Varargs not supported as function pointer parameter");
        throw "Stop";
    }

    context->diagnostics.add_diagnostic_error(peek_current().position, "Expected symbol as type ");
    throw "Stop";
}

auto JotParser::parse_identifier_type() -> Shared<JotType>
{
    Token       symbol_token = consume_kind(TokenKind::Symbol, "Expect identifier as type");
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
            std::make_shared<JotEnumElementType>(symbol_token.literal, enum_type->element_type);
        return enum_element_type;
    }

    // Struct with field that has his type for example LinkedList Node struct
    // Current mark it un solved then solve it after building the struct type itself
    if (type_literal == current_struct_name) {
        current_struct_unknown_fields++;
        return jot_none_ty;
    }

    // Check if this is a it generic type parameter
    if (generic_parameters_names.contains(type_literal)) {
        return std::make_shared<JotGenericParameterType>(type_literal);
    }

    // Check if this identifier is a type alias key
    if (context->type_alias_table.contains(type_literal)) {
        return context->type_alias_table.resolve_alias(type_literal);
    }

    // This type is not permitive, structure or enumerations
    context->diagnostics.add_diagnostic_error(peek_current().position,
                                              "Unexpected identifier type");
    throw "Stop";
}