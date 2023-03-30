#pragma once

#include "jot_token.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

enum class TypeKind : int8 {
    NUMBER,
    POINTER,
    FUNCTION,
    ARRAY,
    STRUCT,
    ENUM,
    ENUM_ELEMENT,
    GENERIC_PARAMETER,
    GENERIC_STRUCT,
    NONE,
    VOID,
    NILL,
};

struct JotType {
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

struct JotNumberType : public JotType {
    explicit JotNumberType(NumberKind kind) : number_kind(kind) { type_kind = TypeKind::NUMBER; }
    NumberKind number_kind;
};

struct JotPointerType : public JotType {
    explicit JotPointerType(Shared<JotType> base_type) : base_type(std::move(base_type))
    {
        type_kind = TypeKind::POINTER;
    }

    Shared<JotType> base_type;
};

struct JotArrayType : public JotType {
    JotArrayType(Shared<JotType> element_type, size_t size)
        : element_type(std::move(element_type)), size(size)
    {
        type_kind = TypeKind::ARRAY;
    }

    Shared<JotType> element_type;
    size_t size;
};

struct JotFunctionType : public JotType {
    JotFunctionType(Token name, std::vector<Shared<JotType>> parameters,
                    Shared<JotType> return_type, bool varargs = false,
                    Shared<JotType> varargs_type = nullptr, bool is_intrinsic = false,
                    bool is_generic = false, std::vector<std::string> generic_names = {})
        : name(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(return_type)), has_varargs(varargs),
          varargs_type(std::move(varargs_type)), is_intrinsic(is_intrinsic), is_generic(is_generic),
          generic_names(generic_names)
    {
        type_kind = TypeKind::FUNCTION;
    }

    Token name;
    std::vector<Shared<JotType>> parameters;
    Shared<JotType> return_type;
    int implicit_parameters_count = 0;

    bool has_varargs;
    Shared<JotType> varargs_type;

    bool is_intrinsic;

    bool is_generic;
    std::vector<std::string> generic_names;
};

struct JotStructType : public JotType {
    JotStructType(std::string name, std::vector<std::string> fields_names,
                  std::vector<Shared<JotType>> types, std::vector<std::string> generic_parameters,
                  bool is_packed, bool is_generic)
        : name(std::move(name)), fields_names(std::move(fields_names)),
          fields_types(std::move(types)), generic_parameters(std::move(generic_parameters)),
          is_packed(is_packed), is_generic(is_generic)
    {
        type_kind = TypeKind::STRUCT;
    }

    std::string name;
    std::vector<std::string> fields_names;
    std::vector<Shared<JotType>> fields_types;
    std::vector<std::string> generic_parameters;
    bool is_packed;
    bool is_generic;
};

struct JotEnumType : public JotType {
    JotEnumType(Token name, std::unordered_map<std::string, int> values,
                Shared<JotType> element_type)
        : name(std::move(name)), values(std::move(values)), element_type(std::move(element_type))
    {
        type_kind = TypeKind::ENUM;
    }

    Token name;
    std::unordered_map<std::string, int> values;
    Shared<JotType> element_type;
};

struct JotEnumElementType : public JotType {
    JotEnumElementType(std::string name, Shared<JotType> element_type)
        : name(std::move(name)), element_type(std::move(element_type))
    {
        type_kind = TypeKind::ENUM_ELEMENT;
    }

    std::string name;
    Shared<JotType> element_type;
};

struct JotGenericParameterType : public JotType {
    explicit JotGenericParameterType(std::string name) : name(std::move(name))
    {
        type_kind = TypeKind::GENERIC_PARAMETER;
    }
    std::string name;
};

struct JotGenericStructType : public JotType {
    JotGenericStructType(Shared<JotStructType> struct_type, std::vector<Shared<JotType>> parameters)
        : struct_type(std::move(struct_type)), parameters(std::move(parameters))
    {
        type_kind = TypeKind::GENERIC_STRUCT;
    }
    Shared<JotStructType> struct_type;
    std::vector<Shared<JotType>> parameters;
};

struct JotNoneType : public JotType {
    JotNoneType() { type_kind = TypeKind::NONE; }
};

struct JotVoidType : public JotType {
    JotVoidType() { type_kind = TypeKind::VOID; }
};

struct JotNullType : public JotType {
    JotNullType() { type_kind = TypeKind::NILL; }
};

auto is_jot_types_equals(const Shared<JotType>& type, const Shared<JotType>& other) -> bool;

auto is_jot_functions_types_equals(const std::shared_ptr<JotFunctionType>& type,
                                   const std::shared_ptr<JotFunctionType>& other) -> bool;

auto can_jot_types_casted(const Shared<JotType>& from, const Shared<JotType>& to) -> bool;

auto jot_type_literal(const Shared<JotType>& type) -> std::string;

auto jot_number_kind_literal(NumberKind kind) -> const char*;

auto is_number_type(Shared<JotType> type) -> bool;

auto is_integer_type(Shared<JotType> type) -> bool;

auto is_enum_type(Shared<JotType> type) -> bool;

auto is_enum_element_type(Shared<JotType> type) -> bool;

auto is_struct_type(Shared<JotType> type) -> bool;

auto is_generic_struct_type(Shared<JotType> type) -> bool;

auto is_boolean_type(Shared<JotType> type) -> bool;

auto is_function_type(Shared<JotType> type) -> bool;

auto is_function_pointer_type(Shared<JotType> type) -> bool;

auto is_pointer_type(Shared<JotType> type) -> bool;

auto is_void_type(Shared<JotType> type) -> bool;

auto is_null_type(Shared<JotType> type) -> bool;

auto is_none_type(Shared<JotType> type) -> bool;

auto is_pointer_of_type(Shared<JotType> type, Shared<JotType> base) -> bool;

auto is_array_of_type(Shared<JotType> type, Shared<JotType> base) -> bool;