#include "../include/amun_type.hpp"
#include "../include/amun_primitives.hpp"

auto amun::is_types_equals(const Shared<amun::Type>& type, const Shared<amun::Type>& other) -> bool
{
    const auto type_kind = type->type_kind;
    const auto other_kind = other->type_kind;

    if (type_kind == amun::TypeKind::NUMBER && other->type_kind == amun::TypeKind::NUMBER) {
        auto type_number = std::static_pointer_cast<amun::NumberType>(type);
        auto other_number = std::static_pointer_cast<amun::NumberType>(other);
        return type_number->number_kind == other_number->number_kind;
    }

    if (type_kind == amun::TypeKind::POINTER && other->type_kind == amun::TypeKind::POINTER) {
        auto type_ptr = std::static_pointer_cast<amun::PointerType>(type);
        auto other_ptr = std::static_pointer_cast<amun::PointerType>(other);
        return amun::is_types_equals(type_ptr->base_type, other_ptr->base_type);
    }

    if (type_kind == amun::TypeKind::STATIC_ARRAY &&
        other->type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto type_array = std::static_pointer_cast<amun::StaticArrayType>(type);
        auto other_array = std::static_pointer_cast<amun::StaticArrayType>(other);
        return type_array->size == other_array->size &&
               amun::is_types_equals(type_array->element_type, other_array->element_type);
    }

    if (type_kind == amun::TypeKind::STATIC_VECTOR &&
        other->type_kind == amun::TypeKind::STATIC_VECTOR) {
        auto type_vector = std::static_pointer_cast<amun::StaticVectorType>(type);
        auto other_vector = std::static_pointer_cast<amun::StaticVectorType>(other);
        return is_types_equals(type_vector->array, other_vector->array);
    }

    if (type_kind == amun::TypeKind::FUNCTION && other->type_kind == amun::TypeKind::FUNCTION) {
        auto type_function = std::static_pointer_cast<amun::FunctionType>(type);
        auto other_function = std::static_pointer_cast<amun::FunctionType>(other);
        return amun::is_functions_types_equals(type_function, other_function);
    }

    if (type_kind == amun::TypeKind::STRUCT && other->type_kind == amun::TypeKind::STRUCT) {
        auto type_struct = std::static_pointer_cast<amun::StructType>(type);
        auto other_struct = std::static_pointer_cast<amun::StructType>(other);
        return amun::get_type_literal(type_struct) == amun::get_type_literal(other_struct);
    }

    if (type_kind == amun::TypeKind::TUPLE && other->type_kind == amun::TypeKind::TUPLE) {
        auto type_tuple = std::static_pointer_cast<amun::TupleType>(type);
        auto other_tuple = std::static_pointer_cast<amun::TupleType>(other);
        return amun::get_type_literal(type_tuple) == amun::get_type_literal(other_tuple);
    }

    if (type_kind == amun::TypeKind::ENUM_ELEMENT &&
        other->type_kind == amun::TypeKind::ENUM_ELEMENT) {
        auto type_element = std::static_pointer_cast<amun::EnumElementType>(type);
        auto other_element = std::static_pointer_cast<amun::EnumElementType>(other);
        return type_element->enum_name == other_element->enum_name;
    }

    if (type_kind == amun::TypeKind::GENERIC_STRUCT &&
        other->type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto type_element = std::static_pointer_cast<amun::GenericStructType>(type);
        auto other_element = std::static_pointer_cast<amun::GenericStructType>(other);
        if (!amun::is_types_equals(type_element->struct_type, other_element->struct_type)) {
            return false;
        }

        auto type_parameters = type_element->parameters;
        auto other_parameters = other_element->parameters;
        if (type_parameters.size() != other_parameters.size()) {
            return false;
        }

        for (size_t i = 0; i < type_parameters.size(); i++) {
            if (!amun::is_types_equals(type_parameters[i], other_parameters[i])) {
                return false;
            }
        }

        return true;
    }

    return type_kind == other_kind;
}

auto amun::is_functions_types_equals(const Shared<amun::FunctionType>& type,
                                     const Shared<amun::FunctionType>& other) -> bool
{
    const auto type_parameters = type->parameters;
    const auto other_parameters = other->parameters;
    const auto type_parameters_size = type_parameters.size();
    if (type_parameters_size != other_parameters.size()) {
        return false;
    }
    for (size_t i = 0; i < type_parameters_size; i++) {
        if (!amun::is_types_equals(type_parameters[i], other_parameters[i])) {
            return false;
        }
    }
    return amun::is_types_equals(type->return_type, other->return_type);
}

auto amun::can_types_casted(const Shared<amun::Type>& from, const Shared<amun::Type>& to) -> bool
{
    const auto from_kind = from->type_kind;
    const auto to_kind = to->type_kind;

    // Catch casting from un castable type
    if (from_kind == amun::TypeKind::VOID || from_kind == amun::TypeKind::NONE ||
        from_kind == amun::TypeKind::ENUM || from_kind == amun::TypeKind::ENUM_ELEMENT ||
        from_kind == amun::TypeKind::FUNCTION) {
        return false;
    }

    // Catch casting to un castable type
    if (to_kind == amun::TypeKind::VOID || to_kind == amun::TypeKind::NONE ||
        to_kind == amun::TypeKind::ENUM || to_kind == amun::TypeKind::ENUM_ELEMENT ||
        to_kind == amun::TypeKind::FUNCTION) {
        return false;
    }

    // Casting between numbers
    if (from_kind == amun::TypeKind::NUMBER && to_kind == amun::TypeKind::NUMBER) {
        return true;
    }

    // Allow casting to and from void pointer type
    if (amun::is_pointer_of_type(from, amun::void_type) ||
        amun::is_pointer_of_type(to, amun::void_type)) {
        return true;
    }

    // Casting Array to pointer of the same elemnet type
    if (from_kind == amun::TypeKind::STATIC_ARRAY && to_kind == amun::TypeKind::POINTER) {
        auto from_array = std::static_pointer_cast<amun::StaticArrayType>(from);
        auto to_pointer = std::static_pointer_cast<amun::PointerType>(to);
        return amun::is_types_equals(from_array->element_type, to_pointer->base_type);
    }

    return false;
}

auto amun::get_type_literal(const Shared<amun::Type>& type) -> std::string
{
    const auto type_kind = type->type_kind;
    if (type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return amun::get_number_kind_literal(number_type->number_kind);
    }

    if (type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
        return "[" + std::to_string(array_type->size) + "]" +
               amun::get_type_literal(array_type->element_type);
    }

    if (type_kind == amun::TypeKind::STATIC_VECTOR) {
        auto vector_type = std::static_pointer_cast<amun::StaticVectorType>(type);
        return "@vec" + get_type_literal(vector_type->array);
    }

    if (type_kind == amun::TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<amun::PointerType>(type);
        return "*" + amun::get_type_literal(pointer_type->base_type);
    }

    if (type_kind == amun::TypeKind::FUNCTION) {
        const auto function_type = std::static_pointer_cast<amun::FunctionType>(type);

        std::stringstream string_stream;
        string_stream << "(";
        for (auto& parameter : function_type->parameters) {
            string_stream << " ";
            string_stream << amun::get_type_literal(parameter);
            string_stream << " ";
        }
        string_stream << ")";
        string_stream << " -> ";
        string_stream << amun::get_type_literal(function_type->return_type);
        return string_stream.str();
    }

    if (type_kind == amun::TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<amun::StructType>(type);
        return struct_type->name;
    }

    if (type_kind == amun::TypeKind::TUPLE) {
        auto tuple_type = std::static_pointer_cast<amun::TupleType>(type);
        std::stringstream string_stream;
        string_stream << "(";
        auto number_of_fields = tuple_type->fields_types.size();
        auto current_filed_index = 0;
        for (auto& parameter : tuple_type->fields_types) {
            current_filed_index++;
            string_stream << amun::get_type_literal(parameter);
            if (current_filed_index < number_of_fields) {
                string_stream << ", ";
            }
        }
        string_stream << ")";
        return string_stream.str();
    }

    if (type_kind == amun::TypeKind::ENUM) {
        auto enum_type = std::static_pointer_cast<amun::EnumType>(type);
        return enum_type->name.literal;
    }

    if (type_kind == amun::TypeKind::ENUM_ELEMENT) {
        auto enum_element = std::static_pointer_cast<amun::EnumElementType>(type);
        return enum_element->enum_name;
    }

    if (type_kind == amun::TypeKind::GENERIC_STRUCT) {
        auto generic_struct = std::static_pointer_cast<amun::GenericStructType>(type);

        std::stringstream string_stream;
        string_stream << amun::get_type_literal(generic_struct->struct_type);
        string_stream << "<";
        auto parameters = generic_struct->parameters;
        auto count = parameters.size();
        size_t parameter_index = 0;
        for (const auto& parameter : generic_struct->parameters) {
            string_stream << amun::get_type_literal(parameter);
            if (++parameter_index < count) {
                string_stream << ",";
            }
        }
        string_stream << ">";
        return string_stream.str();
    }

    if (type_kind == amun::TypeKind::GENERIC_PARAMETER) {
        auto generic_parameter = std::static_pointer_cast<amun::GenericParameterType>(type);
        return generic_parameter->name;
    }

    if (type_kind == amun::TypeKind::NONE) {
        return "none";
    }

    if (type_kind == amun::TypeKind::VOID) {
        return "void";
    }

    if (type_kind == amun::TypeKind::NILL) {
        return "null";
    }

    return "";
}

auto amun::get_number_kind_literal(amun::NumberKind kind) -> const char*
{
    switch (kind) {
    case amun::NumberKind::INTEGER_1: return "Int1";
    case amun::NumberKind::INTEGER_8: return "Int8";
    case amun::NumberKind::INTEGER_16: return "Int16";
    case amun::NumberKind::INTEGER_32: return "Int32";
    case amun::NumberKind::INTEGER_64: return "Int64";
    case amun::NumberKind::U_INTEGER_8: return "UInt8";
    case amun::NumberKind::U_INTEGER_16: return "UInt16";
    case amun::NumberKind::U_INTEGER_32: return "UInt32";
    case amun::NumberKind::U_INTEGER_64: return "UInt64";
    case amun::NumberKind::FLOAT_32: return "Float32";
    case amun::NumberKind::FLOAT_64: return "Float64";
    default: return "Number";
    }
}

auto amun::is_number_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::NUMBER;
}

auto amun::is_integer_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        auto number_kind = number_type->number_kind;
        return number_kind != amun::NumberKind::FLOAT_32 &&
               number_kind != amun::NumberKind::FLOAT_64;
    }
    return false;
}

auto amun::is_signed_integer_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        auto number_kind = number_type->number_kind;
        return number_kind == amun::NumberKind::INTEGER_1 ||
               number_kind == amun::NumberKind::INTEGER_8 ||
               number_kind == amun::NumberKind::INTEGER_16 ||
               number_kind == amun::NumberKind::INTEGER_32 ||
               number_kind == amun::NumberKind::INTEGER_64;
    }
    return false;
}

auto amun::is_unsigned_integer_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        auto number_kind = number_type->number_kind;
        return number_kind == amun::NumberKind::U_INTEGER_8 ||
               number_kind == amun::NumberKind::U_INTEGER_16 ||
               number_kind == amun::NumberKind::U_INTEGER_32 ||
               number_kind == amun::NumberKind::U_INTEGER_64;
    }
    return false;
}

auto amun::is_integer1_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return number_type->number_kind == amun::NumberKind::INTEGER_1;
    }
    return false;
}

auto amun::is_integer32_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return number_type->number_kind == amun::NumberKind::INTEGER_32;
    }
    return false;
}

auto amun::is_integer64_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return number_type->number_kind == amun::NumberKind::INTEGER_64;
    }
    return false;
}

auto amun::is_enum_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::ENUM;
}

auto amun::is_enum_element_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::ENUM_ELEMENT;
}

auto amun::is_struct_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::STRUCT;
}

auto amun::is_generic_struct_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::GENERIC_STRUCT;
}

auto amun::is_tuple_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::TUPLE;
}

auto amun::is_boolean_type(Shared<amun::Type> type) -> bool
{
    if (type->type_kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return number_type->number_kind == amun::NumberKind::INTEGER_1;
    }
    return false;
}

auto amun::is_function_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::FUNCTION;
}

auto amun::is_function_pointer_type(Shared<amun::Type> type) -> bool
{
    if (amun::is_pointer_type(type)) {
        auto pointer = std::static_pointer_cast<amun::PointerType>(type);
        return amun::is_function_type(pointer->base_type);
    }
    return false;
}

auto amun::is_array_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::STATIC_ARRAY;
}

auto amun::is_pointer_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::POINTER;
}

auto amun::is_void_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::VOID;
}

auto amun::is_null_type(Shared<amun::Type> type) -> bool
{
    return type->type_kind == amun::TypeKind::NILL;
}

auto amun::is_none_type(Shared<amun::Type> type) -> bool
{
    const auto type_kind = type->type_kind;

    if (type_kind == amun::TypeKind::STATIC_ARRAY) {
        auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
        return array_type->element_type->type_kind == amun::TypeKind::NONE;
    }

    if (type_kind == amun::TypeKind::POINTER) {
        auto array_type = std::static_pointer_cast<amun::PointerType>(type);
        return array_type->base_type->type_kind == amun::TypeKind::NONE;
    }

    return type->type_kind == amun::TypeKind::NONE;
}

auto amun::is_pointer_of_type(Shared<amun::Type> type, Shared<amun::Type> base) -> bool
{
    if (type->type_kind != amun::TypeKind::POINTER) {
        return false;
    }
    auto pointer_type = std::static_pointer_cast<amun::PointerType>(type);
    return amun::is_types_equals(pointer_type->base_type, base);
}

auto amun::is_array_of_type(Shared<amun::Type> type, Shared<amun::Type> base) -> bool
{
    if (type->type_kind != amun::TypeKind::STATIC_ARRAY) {
        return false;
    }
    auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
    return amun::is_types_equals(array_type->element_type, base);
}