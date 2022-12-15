#pragma once

#include "jot_token.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum class TypeKind : short {
    Number,
    Pointer,
    Function,
    Array,
    Structure,
    Enumeration,
    EnumerationElement,
    None,
    Void,
    Null,
};

struct JotType {
    TypeKind type_kind = TypeKind::None;
};

enum class NumberKind : short {
    Integer1,
    Integer8,
    Integer16,
    Integer32,
    Integer64,
    Float32,
    Float64,
};

struct JotNumberType : public JotType {
    JotNumberType(NumberKind kind) : number_kind(kind) { type_kind = TypeKind::Number; }
    NumberKind number_kind;
};

struct JotPointerType : public JotType {
    JotPointerType(std::shared_ptr<JotType> base_type) : base_type(std::move(base_type))
    {
        type_kind = TypeKind::Pointer;
    }

    std::shared_ptr<JotType> base_type;
};

struct JotArrayType : public JotType {
    JotArrayType(std::shared_ptr<JotType> element_type, size_t size)
        : element_type(std::move(element_type)), size(size)
    {
        type_kind = TypeKind::Array;
    }

    std::shared_ptr<JotType> element_type;
    size_t                   size;
};

struct JotFunctionType : public JotType {
    JotFunctionType(Token name, std::vector<std::shared_ptr<JotType>> parameters,
                    std::shared_ptr<JotType> return_type, bool varargs = false,
                    std::shared_ptr<JotType> varargs_type = nullptr)
        : name(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(return_type)), has_varargs(varargs),
          varargs_type(std::move(varargs_type))
    {
        type_kind = TypeKind::Function;
    }

    Token                                 name;
    std::vector<std::shared_ptr<JotType>> parameters;
    std::shared_ptr<JotType>              return_type;
    bool                                  has_varargs;
    std::shared_ptr<JotType>              varargs_type;
};

struct JotStructType : public JotType {
    JotStructType(std::string name, std::unordered_map<std::string, int> fields_names,
                  std::vector<std::shared_ptr<JotType>> types)
        : name(std::move(name)), fields_names(std::move(fields_names)),
          fields_types(std::move(types))
    {
        type_kind = TypeKind::Structure;
    }

    std::string                           name;
    std::unordered_map<std::string, int>  fields_names;
    std::vector<std::shared_ptr<JotType>> fields_types;
};

struct JotEnumType : public JotType {
    JotEnumType(Token name, std::unordered_map<std::string, int> values,
                std::shared_ptr<JotType> element_type)
        : name(name), values(std::move(values)), element_type(std::move(element_type))
    {
        type_kind = TypeKind::Enumeration;
    }

    Token                                name;
    std::unordered_map<std::string, int> values;
    std::shared_ptr<JotType>             element_type;
};

struct JotEnumElementType : public JotType {
    JotEnumElementType(std::string name, std::shared_ptr<JotType> element_type)
        : name(name), element_type(std::move(element_type))
    {
        type_kind = TypeKind::EnumerationElement;
    }

    std::string              name;
    std::shared_ptr<JotType> element_type;
};

struct JotNoneType : public JotType {
    JotNoneType() { type_kind = TypeKind::None; }
};

struct JotVoidType : public JotType {
    JotVoidType() { type_kind = TypeKind::Void; }
};

struct JotNullType : public JotType {
    JotNullType() { type_kind = TypeKind::Null; }
};

bool is_jot_types_equals(const std::shared_ptr<JotType>& type,
                         const std::shared_ptr<JotType>& other);

bool is_jot_functions_types_equals(const std::shared_ptr<JotFunctionType>& type,
                                   const std::shared_ptr<JotFunctionType>& other);

bool can_jot_types_casted(const std::shared_ptr<JotType>& from, const std::shared_ptr<JotType>& to);

std::string jot_type_literal(const std::shared_ptr<JotType>& type);

const char* jot_number_kind_literal(NumberKind kind);

bool is_number_type(std::shared_ptr<JotType> type);

bool is_integer_type(std::shared_ptr<JotType> type);

bool is_enum_element_type(std::shared_ptr<JotType> type);

bool is_boolean_type(std::shared_ptr<JotType> type);

bool is_pointer_type(std::shared_ptr<JotType> type);

bool is_null_type(std::shared_ptr<JotType> type);

bool is_none_type(std::shared_ptr<JotType> type);