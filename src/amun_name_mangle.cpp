#include "../include/amun_name_mangle.hpp"
#include "../include/amun_type.hpp"

#include <memory>
#include <string>

static std::unordered_map<amun::NumberKind, std::string> number_type_managler = {
    {amun::NumberKind::INTEGER_1, "i1"},     {amun::NumberKind::INTEGER_8, "i8"},
    {amun::NumberKind::INTEGER_16, "i16"},   {amun::NumberKind::INTEGER_32, "i32"},
    {amun::NumberKind::INTEGER_64, "i64"},

    {amun::NumberKind::U_INTEGER_8, "u8"},   {amun::NumberKind::U_INTEGER_16, "u16"},
    {amun::NumberKind::U_INTEGER_32, "u32"}, {amun::NumberKind::U_INTEGER_64, "u64"},

    {amun::NumberKind::FLOAT_32, "f32"},     {amun::NumberKind::FLOAT_64, "f64"},
};

auto mangle_type(Shared<amun::Type> type) -> std::string
{
    auto kind = type->type_kind;
    if (kind == amun::TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<amun::NumberType>(type);
        return number_type_managler[number_type->number_kind];
    }

    if (kind == amun::TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<amun::PointerType>(type);
        return "p" + mangle_type(pointer_type->base_type);
    }

    if (kind == amun::TypeKind::STATIC_ARRAY) {
        auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
        return "_a" + std::to_string(array_type->size) + mangle_type(array_type->element_type);
    }

    if (kind == amun::TypeKind::ENUM_ELEMENT) {
        auto enum_element_type = std::static_pointer_cast<amun::EnumElementType>(type);
        return enum_element_type->enum_name;
    }

    if (kind == amun::TypeKind::STRUCT) {
        auto struct_type = std::static_pointer_cast<amun::StructType>(type);
        return struct_type->name;
    }

    if (kind == amun::TypeKind::TUPLE) {
        auto tuple_type = std::static_pointer_cast<amun::TupleType>(type);
        return mangle_tuple_type(tuple_type);
    }

    return "";
}

auto mangle_tuple_type(Shared<amun::TupleType> type) -> std::string
{
    return "_tuple_" + mangle_types(type->fields_types);
}

auto mangle_operator_function(TokenKind op, std::vector<Shared<amun::Type>> parameters)
    -> std::string
{
    auto operator_literal = overloading_operator_literal[op];
    std::string operator_function_name = "_operator_" + operator_literal;
    for (const auto& parameter : parameters) {
        operator_function_name += mangle_type(parameter);
    }
    return operator_function_name;
}

auto mangle_types(std::vector<Shared<amun::Type>> types) -> std::string
{
    std::string result;
    for (auto& type : types) {
        result += mangle_type(type);
    }
    return result;
}