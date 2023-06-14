#pragma once

#include "amun_token.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace amun {

enum class TypeKind : int8 {
    NUMBER,
    POINTER,
    FUNCTION,
    STATIC_ARRAY,
    STATIC_VECTOR,
    STRUCT,
    TUPLE,
    ENUM,
    ENUM_ELEMENT,
    GENERIC_PARAMETER,
    GENERIC_STRUCT,
    NONE,
    VOID,
    NILL,
};

struct Type {
    TypeKind type_kind = TypeKind::NONE;
};

enum class NumberKind : int8 {
    INTEGER_1,
    INTEGER_8,
    INTEGER_16,
    INTEGER_32,
    INTEGER_64,
    U_INTEGER_8,
    U_INTEGER_16,
    U_INTEGER_32,
    U_INTEGER_64,
    FLOAT_32,
    FLOAT_64,
};

static std::unordered_map<NumberKind, int> number_kind_width = {
    {NumberKind::INTEGER_1, 1},     {NumberKind::INTEGER_8, 8},     {NumberKind::INTEGER_16, 16},
    {NumberKind::INTEGER_32, 32},   {NumberKind::INTEGER_64, 64},

    {NumberKind::U_INTEGER_8, 8},   {NumberKind::U_INTEGER_16, 16}, {NumberKind::U_INTEGER_32, 32},
    {NumberKind::U_INTEGER_64, 64},

    {NumberKind::FLOAT_32, 32},     {NumberKind::FLOAT_64, 64},
};

struct NumberType : public Type {
    explicit NumberType(NumberKind kind) : number_kind(kind) { type_kind = TypeKind::NUMBER; }
    NumberKind number_kind;
};

struct PointerType : public Type {
    explicit PointerType(Shared<Type> base_type) : base_type(std::move(base_type))
    {
        type_kind = TypeKind::POINTER;
    }

    Shared<Type> base_type;
};

struct StaticArrayType : public Type {
    StaticArrayType(Shared<Type> element_type, size_t size)
        : element_type(std::move(element_type)), size(size)
    {
        type_kind = TypeKind::STATIC_ARRAY;
    }

    Shared<Type> element_type;
    size_t size;
};

struct StaticVectorType : public Type {
    explicit StaticVectorType(Shared<StaticArrayType> array) : array(std::move(array))
    {
        type_kind = TypeKind::STATIC_VECTOR;
    }
    Shared<StaticArrayType> array;
};

struct FunctionType : public Type {
    FunctionType(Token name, std::vector<Shared<Type>> parameters, Shared<Type> return_type,
                 bool varargs = false, Shared<Type> varargs_type = nullptr,
                 bool is_intrinsic = false, bool is_generic = false,
                 std::vector<std::string> generic_names = {})
        : name(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(return_type)), has_varargs(varargs),
          varargs_type(std::move(varargs_type)), is_intrinsic(is_intrinsic), is_generic(is_generic),
          generic_names(std::move(generic_names))
    {
        type_kind = TypeKind::FUNCTION;
    }

    Token name;
    std::vector<Shared<Type>> parameters;
    Shared<Type> return_type;
    int implicit_parameters_count = 0;

    bool has_varargs;
    Shared<Type> varargs_type;

    bool is_intrinsic;

    bool is_generic;
    std::vector<std::string> generic_names;
};

struct StructType : public Type {
    StructType(std::string name, std::vector<std::string> fields_names,
               std::vector<Shared<Type>> types, std::vector<std::string> generic_parameters = {},
               bool is_packed = false, bool is_generic = false, bool is_extern = false)
        : name(std::move(name)), fields_names(std::move(fields_names)),
          fields_types(std::move(types)), generic_parameters(std::move(generic_parameters)),
          is_packed(is_packed), is_generic(is_generic), is_extern(is_extern)
    {
        type_kind = TypeKind::STRUCT;
    }

    std::string name;
    std::vector<std::string> fields_names;
    std::vector<Shared<Type>> fields_types;
    std::vector<std::string> generic_parameters;
    std::vector<Shared<Type>> generic_parameters_types = {};
    bool is_packed;
    bool is_generic;
    bool is_extern;
};

struct TupleType : public Type {
    TupleType(std::string name, std::vector<Shared<Type>> fields_types)
        : name(std::move(name)), fields_types(std::move(fields_types))
    {
        type_kind = TypeKind::TUPLE;
    }
    std::string name;
    std::vector<Shared<Type>> fields_types;
};

struct EnumType : public Type {
    EnumType(Token name, std::unordered_map<std::string, int> values, Shared<Type> element_type)
        : name(std::move(name)), values(std::move(values)), element_type(std::move(element_type))
    {
        type_kind = TypeKind::ENUM;
    }

    Token name;
    std::unordered_map<std::string, int> values;
    Shared<Type> element_type;
};

struct EnumElementType : public Type {
    EnumElementType(std::string enum_name, Shared<Type> element_type)
        : enum_name(std::move(enum_name)), element_type(std::move(element_type))
    {
        type_kind = TypeKind::ENUM_ELEMENT;
    }

    std::string enum_name;
    Shared<Type> element_type;
};

struct GenericParameterType : public Type {
    explicit GenericParameterType(std::string name) : name(std::move(name))
    {
        type_kind = TypeKind::GENERIC_PARAMETER;
    }
    std::string name;
};

struct GenericStructType : public Type {
    GenericStructType(Shared<StructType> struct_type, std::vector<Shared<Type>> parameters)
        : struct_type(std::move(struct_type)), parameters(std::move(parameters))
    {
        type_kind = TypeKind::GENERIC_STRUCT;
    }
    Shared<StructType> struct_type;
    std::vector<Shared<Type>> parameters;
};

struct NoneType : public Type {
    NoneType() { type_kind = TypeKind::NONE; }
};

struct VoidType : public Type {
    VoidType() { type_kind = TypeKind::VOID; }
};

struct NullType : public Type {
    NullType() { type_kind = TypeKind::NILL; }
};

auto is_types_equals(const Shared<Type>& type, const Shared<Type>& other) -> bool;

auto is_functions_types_equals(const std::shared_ptr<FunctionType>& type,
                               const std::shared_ptr<FunctionType>& other) -> bool;

auto can_types_casted(const Shared<Type>& from, const Shared<Type>& to) -> bool;

auto get_type_literal(const Shared<Type>& type) -> std::string;

auto get_number_kind_literal(NumberKind kind) -> const char*;

auto is_number_type(Shared<Type> type) -> bool;

auto is_signed_integer_type(Shared<Type> type) -> bool;

auto is_unsigned_integer_type(Shared<Type> type) -> bool;

auto is_integer_type(Shared<Type> type) -> bool;

auto is_integer1_type(Shared<Type> type) -> bool;

auto is_integer32_type(Shared<Type> type) -> bool;

auto is_integer64_type(Shared<Type> type) -> bool;

auto is_enum_type(Shared<Type> type) -> bool;

auto is_enum_element_type(Shared<Type> type) -> bool;

auto is_struct_type(Shared<Type> type) -> bool;

auto is_generic_struct_type(Shared<Type> type) -> bool;

auto is_tuple_type(Shared<amun::Type> type) -> bool;

auto is_boolean_type(Shared<Type> type) -> bool;

auto is_function_type(Shared<Type> type) -> bool;

auto is_function_pointer_type(Shared<Type> type) -> bool;

auto is_array_type(Shared<amun::Type> type) -> bool;

auto is_pointer_type(Shared<Type> type) -> bool;

auto is_void_type(Shared<Type> type) -> bool;

auto is_null_type(Shared<Type> type) -> bool;

auto is_none_type(Shared<Type> type) -> bool;

auto is_pointer_of_type(Shared<Type> type, Shared<Type> base) -> bool;

auto is_array_of_type(Shared<Type> type, Shared<Type> base) -> bool;

} // namespace amun