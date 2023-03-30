#include "../include/jot_type.hpp"

auto is_jot_types_equals(const Shared<JotType>& type, const Shared<JotType>& other) -> bool
{
    const auto type_kind = type->type_kind;
    const auto other_kind = other->type_kind;

    if (type_kind == TypeKind::NUMBER && other->type_kind == TypeKind::NUMBER) {
        auto type_number = std::static_pointer_cast<JotNumberType>(type);
        auto other_number = std::static_pointer_cast<JotNumberType>(other);
        return type_number->number_kind == other_number->number_kind;
    }

    if (type_kind == TypeKind::POINTER && other->type_kind == TypeKind::POINTER) {
        auto type_ptr = std::static_pointer_cast<JotPointerType>(type);
        auto other_ptr = std::static_pointer_cast<JotPointerType>(other);
        return is_jot_types_equals(type_ptr->base_type, other_ptr->base_type);
    }

    if (type_kind == TypeKind::ARRAY && other->type_kind == TypeKind::ARRAY) {
        auto type_array = std::static_pointer_cast<JotArrayType>(type);
        auto other_array = std::static_pointer_cast<JotArrayType>(other);
        return type_array->size == other_array->size &&
               is_jot_types_equals(type_array->element_type, other_array->element_type);
    }

    if (type_kind == TypeKind::FUNCTION && other->type_kind == TypeKind::FUNCTION) {
        auto type_function = std::static_pointer_cast<JotFunctionType>(type);
        auto other_function = std::static_pointer_cast<JotFunctionType>(other);
        return is_jot_functions_types_equals(type_function, other_function);
    }

    if (type_kind == TypeKind::STRUCT && other->type_kind == TypeKind::STRUCT) {
        auto type_struct = std::static_pointer_cast<JotStructType>(type);
        auto other_struct = std::static_pointer_cast<JotStructType>(other);
        return jot_type_literal(type_struct) == jot_type_literal(other_struct);
    }

    if (type_kind == TypeKind::ENUM_ELEMENT && other->type_kind == TypeKind::ENUM_ELEMENT) {
        auto type_element = std::static_pointer_cast<JotEnumElementType>(type);
        auto other_element = std::static_pointer_cast<JotEnumElementType>(other);
        return type_element->name == other_element->name;
    }

    if (type_kind == TypeKind::GENERIC_STRUCT && other->type_kind == TypeKind::GENERIC_STRUCT) {
        auto type_element = std::static_pointer_cast<JotGenericStructType>(type);
        auto other_element = std::static_pointer_cast<JotGenericStructType>(other);
        if (!is_jot_types_equals(type_element->struct_type, other_element->struct_type)) {
            return false;
        }

        auto type_parameters = type_element->parameters;
        auto other_parameters = other_element->parameters;
        if (type_parameters.size() != other_parameters.size()) {
            return false;
        }

        for (size_t i = 0; i < type_parameters.size(); i++) {
            if (!is_jot_types_equals(type_parameters[i], other_parameters[i])) {
                return false;
            }
        }

        return true;
    }

    return type_kind == other_kind;
}

inline auto is_jot_functions_types_equals(const Shared<JotFunctionType>& type,
                                          const Shared<JotFunctionType>& other) -> bool
{
    const auto type_parameters = type->parameters;
    const auto other_parameters = other->parameters;
    const auto type_parameters_size = type_parameters.size();
    if (type_parameters_size != other_parameters.size()) {
        return false;
    }
    for (size_t i = 0; i < type_parameters_size; i++) {
        if (!is_jot_types_equals(type_parameters[i], other_parameters[i])) {
            return false;
        }
    }
    return is_jot_types_equals(type->return_type, other->return_type);
}

auto can_jot_types_casted(const Shared<JotType>& from, const Shared<JotType>& to) -> bool
{
    const auto from_kind = from->type_kind;
    const auto to_kind = to->type_kind;

    if (from_kind == TypeKind::VOID || from_kind == TypeKind::NONE || from_kind == TypeKind::ENUM ||
        from_kind == TypeKind::ENUM_ELEMENT || from_kind == TypeKind::FUNCTION) {
        return false;
    }

    if (to_kind == TypeKind::VOID || to_kind == TypeKind::NONE || to_kind == TypeKind::ENUM ||
        to_kind == TypeKind::ENUM_ELEMENT || to_kind == TypeKind::FUNCTION) {
        return false;
    }

    if (from_kind == TypeKind::NUMBER && to_kind == TypeKind::NUMBER) {
        return true;
    }

    if (from_kind == TypeKind::POINTER) {
        auto from_pointer = std::static_pointer_cast<JotPointerType>(from);
        if (from_pointer->base_type->type_kind == TypeKind::VOID) {
            return true;
        }
        if (to_kind == TypeKind::POINTER) {
            auto to_ptr = std::static_pointer_cast<JotPointerType>(to);
            return to_ptr->base_type->type_kind == TypeKind::VOID;
        }

        return false;
    }

    if (from_kind == TypeKind::ARRAY && to_kind == TypeKind::POINTER) {
        auto from_array = std::static_pointer_cast<JotArrayType>(from);
        auto to_pointer = std::static_pointer_cast<JotPointerType>(to);
        return is_jot_types_equals(from_array->element_type, to_pointer->base_type);
    }

    return false;
}

auto jot_type_literal(const Shared<JotType>& type) -> std::string
{
    const auto type_kind = type->type_kind;
    if (type_kind == TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        return jot_number_kind_literal(number_type->number_kind);
    }

    if (type_kind == TypeKind::ARRAY) {
        auto array_type = std::static_pointer_cast<JotArrayType>(type);
        return "[" + std::to_string(array_type->size) + "]" +
               jot_type_literal(array_type->element_type);
    }

    if (type_kind == TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(type);
        return "*" + jot_type_literal(pointer_type->base_type);
    }

    if (type_kind == TypeKind::FUNCTION) {
        const auto function_type = std::static_pointer_cast<JotFunctionType>(type);

        std::stringstream string_stream;
        string_stream << "(";
        for (auto& parameter : function_type->parameters) {
            string_stream << " ";
            string_stream << jot_type_literal(parameter);
            string_stream << " ";
        }
        string_stream << ")";
        string_stream << " -> ";
        string_stream << jot_type_literal(function_type->return_type);
        return string_stream.str();
    }

    if (type_kind == TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        return struct_type->name;
    }

    if (type_kind == TypeKind::ENUM) {
        auto enum_type = std::static_pointer_cast<JotEnumType>(type);
        return enum_type->name.literal;
    }

    if (type_kind == TypeKind::ENUM_ELEMENT) {
        auto enum_element = std::static_pointer_cast<JotEnumElementType>(type);
        return enum_element->name;
    }

    if (type_kind == TypeKind::GENERIC_STRUCT) {
        auto generic_struct = std::static_pointer_cast<JotGenericStructType>(type);

        std::stringstream string_stream;
        string_stream << jot_type_literal(generic_struct->struct_type);
        string_stream << "<";
        auto parameters = generic_struct->parameters;
        auto count = parameters.size();
        size_t parameter_index = 0;
        for (const auto& parameter : generic_struct->parameters) {
            string_stream << jot_type_literal(parameter);
            if (++parameter_index < count) {
                string_stream << ",";
            }
        }
        string_stream << ">";
        return string_stream.str();
    }

    if (type_kind == TypeKind::GENERIC_PARAMETER) {
        auto generic_parameter = std::static_pointer_cast<JotGenericParameterType>(type);
        return generic_parameter->name;
    }

    if (type_kind == TypeKind::NONE) {
        return "none";
    }

    if (type_kind == TypeKind::VOID) {
        return "void";
    }

    if (type_kind == TypeKind::NILL) {
        return "null";
    }

    return "";
}

inline auto jot_number_kind_literal(NumberKind kind) -> const char*
{
    switch (kind) {
    case NumberKind::INTEGER_1: return "Int1";
    case NumberKind::INTEGER_8: return "Int8";
    case NumberKind::INTEGER_16: return "Int16";
    case NumberKind::INTEGER_32: return "Int32";
    case NumberKind::INTEGER_64: return "Int64";
    case NumberKind::U_INTEGER_8: return "UInt8";
    case NumberKind::U_INTEGER_16: return "UInt16";
    case NumberKind::U_INTEGER_32: return "UInt32";
    case NumberKind::U_INTEGER_64: return "UInt64";
    case NumberKind::FLOAT_32: return "Float32";
    case NumberKind::FLOAT_64: return "Float64";
    default: return "Number";
    }
}

auto is_number_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::NUMBER; }

auto is_integer_type(Shared<JotType> type) -> bool
{
    if (type->type_kind == TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        auto number_kind = number_type->number_kind;
        return number_kind != NumberKind::FLOAT_32 && number_kind != NumberKind::FLOAT_64;
    }
    return false;
}

auto is_enum_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::ENUM; }

auto is_enum_element_type(Shared<JotType> type) -> bool
{
    return type->type_kind == TypeKind::ENUM_ELEMENT;
}

auto is_struct_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::STRUCT; }

auto is_generic_struct_type(Shared<JotType> type) -> bool
{
    return type->type_kind == TypeKind::GENERIC_STRUCT;
}

auto is_boolean_type(Shared<JotType> type) -> bool
{
    if (type->type_kind == TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        return number_type->number_kind == NumberKind::INTEGER_1;
    }
    return false;
}

auto is_function_type(Shared<JotType> type) -> bool
{
    return type->type_kind == TypeKind::FUNCTION;
}

auto is_function_pointer_type(Shared<JotType> type) -> bool
{
    if (is_pointer_type(type)) {
        auto pointer = std::static_pointer_cast<JotPointerType>(type);
        return is_function_type(pointer->base_type);
    }
    return false;
}

auto is_pointer_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::POINTER; }

auto is_void_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::VOID; }

auto is_null_type(Shared<JotType> type) -> bool { return type->type_kind == TypeKind::NILL; }

auto is_none_type(Shared<JotType> type) -> bool
{
    const auto type_kind = type->type_kind;

    if (type_kind == TypeKind::ARRAY) {
        auto array_type = std::static_pointer_cast<JotArrayType>(type);
        return array_type->element_type->type_kind == TypeKind::NONE;
    }

    if (type_kind == TypeKind::POINTER) {
        auto array_type = std::static_pointer_cast<JotPointerType>(type);
        return array_type->base_type->type_kind == TypeKind::NONE;
    }

    return type->type_kind == TypeKind::NONE;
}

auto is_pointer_of_type(Shared<JotType> type, Shared<JotType> base) -> bool
{
    if (type->type_kind != TypeKind::POINTER) {
        return false;
    }
    auto pointer_type = std::static_pointer_cast<JotPointerType>(type);
    return is_jot_types_equals(pointer_type->base_type, base);
}

auto is_array_of_type(Shared<JotType> type, Shared<JotType> base) -> bool
{
    if (type->type_kind != TypeKind::ARRAY) {
        return false;
    }
    auto array_type = std::static_pointer_cast<JotArrayType>(type);
    return is_jot_types_equals(array_type->element_type, base);
}