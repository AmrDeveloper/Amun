#include "../include/jot_type.hpp"

bool is_jot_types_equals(const std::shared_ptr<JotType>& type,
                         const std::shared_ptr<JotType>& other)
{
    const auto type_kind = type->type_kind;
    const auto other_kind = other->type_kind;

    if (type_kind == TypeKind::Number && other->type_kind == TypeKind::Number) {
        auto type_number = std::static_pointer_cast<JotNumberType>(type);
        auto other_number = std::static_pointer_cast<JotNumberType>(other);
        return type_number->number_kind == other_number->number_kind;
    }

    if (type_kind == TypeKind::Pointer && other->type_kind == TypeKind::Pointer) {
        auto type_ptr = std::static_pointer_cast<JotPointerType>(type);
        auto other_ptr = std::static_pointer_cast<JotPointerType>(other);
        return is_jot_types_equals(type_ptr->base_type, other_ptr->base_type);
    }

    if (type_kind == TypeKind::Array && other->type_kind == TypeKind::Array) {
        auto type_array = std::static_pointer_cast<JotArrayType>(type);
        auto other_array = std::static_pointer_cast<JotArrayType>(other);
        return type_array->size == other_array->size &&
               is_jot_types_equals(type_array->element_type, other_array->element_type);
    }

    if (type_kind == TypeKind::Function && other->type_kind == TypeKind::Function) {
        auto type_function = std::static_pointer_cast<JotFunctionType>(type);
        auto other_function = std::static_pointer_cast<JotFunctionType>(other);
        return is_jot_functions_types_equals(type_function, other_function);
    }

    if (type_kind == TypeKind::Structure && other->type_kind == TypeKind::Structure) {
        auto type_struct = std::static_pointer_cast<JotStructType>(type);
        auto other_struct = std::static_pointer_cast<JotStructType>(other);
        return jot_type_literal(type_struct) == jot_type_literal(other_struct);
    }

    if (type_kind == TypeKind::EnumerationElement &&
        other->type_kind == TypeKind::EnumerationElement) {
        auto type_element = std::static_pointer_cast<JotEnumElementType>(type);
        auto other_element = std::static_pointer_cast<JotEnumElementType>(other);
        return type_element->name == other_element->name;
    }

    return type_kind == other_kind;
}

inline bool is_jot_functions_types_equals(const std::shared_ptr<JotFunctionType>& type,
                                          const std::shared_ptr<JotFunctionType>& other)
{
    const auto type_parameters = type->parameters;
    const auto other_parameters = other->parameters;
    const auto type_parameters_size = type_parameters.size();
    if (type_parameters_size != other_parameters.size())
        return false;
    for (size_t i = 0; i < type_parameters_size; i++) {
        if (!is_jot_types_equals(type_parameters[i], other_parameters[i]))
            return false;
    }
    return is_jot_types_equals(type->return_type, other->return_type);
}

bool can_jot_types_casted(const std::shared_ptr<JotType>& from, const std::shared_ptr<JotType>& to)
{
    const auto from_kind = from->type_kind;
    const auto to_kind = to->type_kind;

    if (from_kind == TypeKind::Void || from_kind == TypeKind::None ||
        from_kind == TypeKind::Enumeration || from_kind == TypeKind::EnumerationElement ||
        from_kind == TypeKind::Function) {
        return false;
    }

    if (to_kind == TypeKind::Void || to_kind == TypeKind::None ||
        to_kind == TypeKind::Enumeration || to_kind == TypeKind::EnumerationElement ||
        to_kind == TypeKind::Function) {
        return false;
    }

    if (from_kind == TypeKind::Number && to_kind == TypeKind::Number) {
        return true;
    }

    if (from_kind == TypeKind::Pointer) {
        auto from_pointer = std::static_pointer_cast<JotPointerType>(from);
        if (from_pointer->base_type->type_kind == TypeKind::Void)
            return true;

        if (to_kind == TypeKind::Pointer) {
            auto to_ptr = std::static_pointer_cast<JotPointerType>(to);
            return to_ptr->base_type->type_kind == TypeKind::Void;
        }

        return false;
    }

    if (from_kind == TypeKind::Array && to_kind == TypeKind::Pointer) {
        auto from_array = std::static_pointer_cast<JotArrayType>(from);
        auto to_pointer = std::static_pointer_cast<JotPointerType>(to);
        return is_jot_types_equals(from_array->element_type, to_pointer->base_type);
    }

    return false;
}

std::string jot_type_literal(const std::shared_ptr<JotType>& type)
{
    const auto type_kind = type->type_kind;
    if (type_kind == TypeKind::Number) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        return jot_number_kind_literal(number_type->number_kind);
    }

    if (type_kind == TypeKind::Array) {
        auto array_type = std::static_pointer_cast<JotArrayType>(type);
        return "[" + std::to_string(array_type->size) + "]" +
               jot_type_literal(array_type->element_type);
    }

    if (type_kind == TypeKind::Pointer) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(type);
        return "*" + jot_type_literal(pointer_type->base_type);
    }

    if (type_kind == TypeKind::Function) {
        const auto        function_type = std::static_pointer_cast<JotFunctionType>(type);
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

    if (type_kind == TypeKind::Structure) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        return struct_type->name;
    }

    if (type_kind == TypeKind::Enumeration) {
        auto enum_type = std::static_pointer_cast<JotEnumType>(type);
        return enum_type->name.literal;
    }

    if (type_kind == TypeKind::EnumerationElement) {
        auto enum_element = std::static_pointer_cast<JotEnumElementType>(type);
        return enum_element->name;
    }

    if (type_kind == TypeKind::None) {
        return "none";
    }

    if (type_kind == TypeKind::Void) {
        return "void";
    }

    if (type_kind == TypeKind::Null) {
        return "null";
    }

    return "";
}

inline const char* jot_number_kind_literal(NumberKind kind)
{
    switch (kind) {
    case NumberKind::Integer1: return "Int1";
    case NumberKind::Integer8: return "Int8";
    case NumberKind::Integer16: return "Int16";
    case NumberKind::Integer32: return "Int32";
    case NumberKind::Integer64: return "Int64";
    case NumberKind::UInteger8: return "UInt8";
    case NumberKind::UInteger16: return "UInt16";
    case NumberKind::UInteger32: return "UInt32";
    case NumberKind::UInteger64: return "UInt64";
    case NumberKind::Float32: return "Float32";
    case NumberKind::Float64: return "Float64";
    default: return "Number";
    }
}

bool is_number_type(std::shared_ptr<JotType> type) { return type->type_kind == TypeKind::Number; }

bool is_integer_type(std::shared_ptr<JotType> type)
{
    if (type->type_kind == TypeKind::Number) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        auto number_kind = number_type->number_kind;
        return number_kind != NumberKind::Float32 && number_kind != NumberKind::Float64;
    }
    return false;
}

bool is_enum_element_type(std::shared_ptr<JotType> type)
{
    return type->type_kind == TypeKind::EnumerationElement;
}

bool is_boolean_type(std::shared_ptr<JotType> type)
{
    if (type->type_kind == TypeKind::Number) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        return number_type->number_kind == NumberKind::Integer1;
    }
    return false;
}

bool is_function_type(std::shared_ptr<JotType> type)
{
    return type->type_kind == TypeKind::Function;
}

bool is_function_pointer_type(std::shared_ptr<JotType> type)
{
    if (is_pointer_type(type)) {
        auto pointer = std::static_pointer_cast<JotPointerType>(type);
        return is_function_type(pointer->base_type);
    }
    return false;
}

bool is_pointer_type(std::shared_ptr<JotType> type) { return type->type_kind == TypeKind::Pointer; }

bool is_void_type(std::shared_ptr<JotType> type) { return type->type_kind == TypeKind::Void; }

bool is_null_type(std::shared_ptr<JotType> type) { return type->type_kind == TypeKind::Null; }

bool is_none_type(std::shared_ptr<JotType> type)
{
    const auto type_kind = type->type_kind;

    if (type_kind == TypeKind::Array) {
        auto array_type = std::static_pointer_cast<JotArrayType>(type);
        return array_type->element_type->type_kind == TypeKind::None;
    }

    if (type_kind == TypeKind::Pointer) {
        auto array_type = std::static_pointer_cast<JotPointerType>(type);
        return array_type->base_type->type_kind == TypeKind::None;
    }

    return type->type_kind == TypeKind::None;
}
