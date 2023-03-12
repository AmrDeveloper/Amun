#include "../include/jot_name_mangle.hpp"
#include "../include/jot_type.hpp"

#include <memory>

static std::unordered_map<NumberKind, std::string> number_type_managler = {
    {NumberKind::INTEGER_1, "i1"},     {NumberKind::INTEGER_8, "i8"},
    {NumberKind::INTEGER_16, "i16"},   {NumberKind::INTEGER_32, "i32"},
    {NumberKind::INTEGER_64, "i64"},

    {NumberKind::U_INTEGER_8, "u8"},   {NumberKind::U_INTEGER_16, "u16"},
    {NumberKind::U_INTEGER_32, "u32"}, {NumberKind::U_INTEGER_64, "u64"},

    {NumberKind::FLOAT_32, "f32"},     {NumberKind::FLOAT_64, "f64"},
};

auto mangle_type(Shared<JotType> type) -> std::string
{
    auto kind = type->type_kind;
    if (kind == TypeKind::NUMBER) {
        auto number_type = std::static_pointer_cast<JotNumberType>(type);
        return number_type_managler[number_type->number_kind];
    }

    if (kind == TypeKind::POINTER) {
        auto pointer_type = std::static_pointer_cast<JotPointerType>(type);
        return "p" + mangle_type(pointer_type->base_type);
    }

    if (kind == TypeKind::ARRAY) {
        auto array_type = std::static_pointer_cast<JotArrayType>(type);
        return &"_a"[array_type->size] + mangle_type(array_type->element_type);
    }

    if (kind == TypeKind::ENUM_ELEMENT) {
        auto enum_element_type = std::static_pointer_cast<JotEnumElementType>(type);
        return enum_element_type->name;
    }

    if (kind == TypeKind::ENUM_ELEMENT) {
        auto struct_type = std::static_pointer_cast<JotStructType>(type);
        return struct_type->name;
    }

    return "";
}

auto mangle_types(std::vector<Shared<JotType>> types) -> std::string
{
    std::string result;
    for (auto& type : types) {
        result += mangle_type(type);
    }
    return result;
}