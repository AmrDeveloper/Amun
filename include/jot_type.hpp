#pragma once

#include "jot_token.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum class TypeKind {
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

class JotType {
  public:
    virtual std::string type_literal() = 0;
    virtual TypeKind    get_type_kind() = 0;
    virtual bool        equals(const std::shared_ptr<JotType>& other) = 0;
    virtual bool        castable(const std::shared_ptr<JotType>& other) = 0;
};

enum class NumberKind {
    Integer1,
    Integer8,
    Integer16,
    Integer32,
    Integer64,
    Float32,
    Float64,
};

class JotNumberType : public JotType {
  public:
    JotNumberType(NumberKind kind) : kind(kind) {}

    NumberKind get_kind() { return kind; }

    std::string type_literal() override
    {
        switch (kind) {
        case NumberKind::Integer1: return "Int1";
        case NumberKind::Integer8: return "Int8";
        case NumberKind::Integer16: return "Int16";
        case NumberKind::Integer32: return "Int32";
        case NumberKind::Integer64: return "Int64";
        case NumberKind::Float32: return "Float32";
        case NumberKind::Float64: return "Float64";
        default: return "Number";
        }
    }

    TypeKind get_type_kind() override { return TypeKind::Number; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

    bool is_boolean() { return kind == NumberKind::Integer1; }

    bool is_integer() { return not is_float(); }

    bool is_float() { return kind == NumberKind::Float32 || kind == NumberKind::Float64; }

  private:
      NumberKind kind;
};

class JotPointerType : public JotType {
  public:
    JotPointerType(std::shared_ptr<JotType> point_to) : point_to(point_to) {}

    std::shared_ptr<JotType> get_point_to() { return point_to; }

    std::string type_literal() override { return "*" + point_to->type_literal(); }

    TypeKind get_type_kind() override { return TypeKind::Pointer; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

  private:
    std::shared_ptr<JotType> point_to;
};

class JotArrayType : public JotType {
  public:
    JotArrayType(std::shared_ptr<JotType> element_type, size_t size)
        : element_type(element_type), size(size)
    {
    }

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    void set_element_type(std::shared_ptr<JotType> new_type) { element_type = new_type; }

    size_t get_size() { return size; }

    std::string type_literal() override
    {
        return "[" + std::to_string(size) + "]" + element_type->type_literal();
    }

    TypeKind get_type_kind() override { return TypeKind::Array; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

  private:
    std::shared_ptr<JotType> element_type;
    size_t                   size;
};

class JotFunctionType : public JotType {
  public:
    JotFunctionType(Token name, std::vector<std::shared_ptr<JotType>> parameters,
                    std::shared_ptr<JotType> return_type, bool varargs = false,
                    std::shared_ptr<JotType> varargs_type = nullptr)
        : name(name), parameters(parameters), return_type(return_type), varargs(varargs),
          varargs_type(varargs_type)
    {
    }

    std::vector<std::shared_ptr<JotType>> get_parameters() { return parameters; }

    std::shared_ptr<JotType> get_return_type() { return return_type; }

    std::string type_literal() override
    {
        std::stringstream function_type_literal;
        function_type_literal << "(";
        for (auto& parameter : parameters) {
            function_type_literal << " ";
            function_type_literal << parameter->type_literal();
            function_type_literal << " ";
        }
        function_type_literal << ")";
        function_type_literal << " -> ";
        function_type_literal << return_type->type_literal();
        return function_type_literal.str();
    }

    TypeKind get_type_kind() override { return TypeKind::Function; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

    bool has_varargs() { return varargs; }

    std::shared_ptr<JotType> get_varargs_type() { return varargs_type; }

  private:
    Token                                 name;
    std::vector<std::shared_ptr<JotType>> parameters;
    std::shared_ptr<JotType>              return_type;
    bool                                  varargs;
    std::shared_ptr<JotType>              varargs_type;
};

class JotStructType : public JotType {
  public:
    JotStructType(std::string name, std::unordered_map<std::string, int> fields_names,
                  std::vector<std::shared_ptr<JotType>> types)
        : name(std::move(name)), fields_names(fields_names), fields_types(types)
    {
    }

    std::string get_name() { return name; }

    std::unordered_map<std::string, int> get_fields_names() { return fields_names; }

    std::vector<std::shared_ptr<JotType>> get_fields_types() { return fields_types; }

    std::string type_literal() override { return name; }

    TypeKind get_type_kind() override { return TypeKind::Structure; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

  private:
    std::string                                 name;
    std::unordered_map<std::string, int>  fields_names;
    std::vector<std::shared_ptr<JotType>> fields_types;
};

class JotEnumType : public JotType {
  public:
    JotEnumType(Token name, std::unordered_map<std::string, int> values,
                std::shared_ptr<JotType> element_type)
        : name(name), values(values), element_type(element_type)
    {
    }

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    std::unordered_map<std::string, int> get_enum_values() { return values; }

    std::string type_literal() override { return "enum " + name.get_literal(); }

    TypeKind get_type_kind() override { return TypeKind::Enumeration; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

  private:
    Token                                name;
    std::unordered_map<std::string, int> values;
    std::shared_ptr<JotType>             element_type;
};

class JotEnumElementType : public JotType {
  public:
    JotEnumElementType(Token name, std::shared_ptr<JotType> element_type)
        : name(name), element_type(element_type)
    {
    }

    std::string get_name() { return name.get_literal(); }

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    std::string type_literal() override { return "enum " + name.get_literal(); }

    TypeKind get_type_kind() override { return TypeKind::EnumerationElement; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;

  private:
    Token                    name;
    std::shared_ptr<JotType> element_type;
};

class JotNoneType : public JotType {
  public:
    std::string type_literal() override { return "None Type"; }

    TypeKind get_type_kind() override { return TypeKind::None; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;
};

class JotVoidType : public JotType {
  public:
    std::string type_literal() override { return "void"; }

    TypeKind get_type_kind() override { return TypeKind::Void; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;
};

class JotNullType : public JotType {
  public:
    std::string type_literal() override { return "null"; }

    TypeKind get_type_kind() override { return TypeKind::Null; }

    bool equals(const std::shared_ptr<JotType>& other) override;

    bool castable(const std::shared_ptr<JotType>& other) override;
};