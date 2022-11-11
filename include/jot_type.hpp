#pragma once

#include "jot_token.hpp"

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

enum TypeKind {
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
    virtual Token get_type_token() = 0;
    virtual std::string type_literal() = 0;
    virtual TypeKind get_type_kind() = 0;
    virtual TokenSpan get_type_position() = 0;
    virtual bool equals(const std::shared_ptr<JotType> &other) = 0;
    virtual bool castable(const std::shared_ptr<JotType> &other) = 0;
};

enum NumberKind {
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
    JotNumberType(Token token, NumberKind kind) : token(token), kind(kind) {}

    NumberKind get_kind() { return kind; }

    Token get_type_token() override { return token; }

    std::string type_literal() override {
        switch (kind) {
        case Integer1: return "Int1";
        case Integer8: return "Int8";
        case Integer16: return "Int16";
        case Integer32: return "Int32";
        case Integer64: return "Int64";
        case Float32: return "Float32";
        case Float64: return "Float64";
        default: return "Number";
        }
    }

    TypeKind get_type_kind() override { return TypeKind::Number; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

    bool is_boolean() { return kind == NumberKind::Integer1; }

    bool is_integer() { return not is_float(); }

    bool is_float() { return kind == NumberKind::Float32 || kind == NumberKind::Float64; }

  private:
    Token token;
    NumberKind kind;
};

class JotPointerType : public JotType {
  public:
    JotPointerType(Token token, std::shared_ptr<JotType> point_to)
        : token(token), point_to(point_to) {}

    std::shared_ptr<JotType> get_point_to() { return point_to; }

    Token get_type_token() override { return token; }

    std::string type_literal() override { return "*" + point_to->type_literal(); }

    TypeKind get_type_kind() override { return TypeKind::Pointer; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token token;
    std::shared_ptr<JotType> point_to;
};

class JotArrayType : public JotType {
  public:
    JotArrayType(std::shared_ptr<JotType> element_type, size_t size)
        : element_type(element_type), size(size) {}

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    void set_element_type(std::shared_ptr<JotType> new_type) { element_type = new_type; }

    size_t get_size() { return size; }

    Token get_type_token() override { return element_type->get_type_token(); }

    std::string type_literal() override {
        return "[" + std::to_string(size) + "]" + element_type->type_literal();
    }

    TypeKind get_type_kind() override { return TypeKind::Array; }

    TokenSpan get_type_position() override { return element_type->get_type_token().get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    std::shared_ptr<JotType> element_type;
    size_t size;
};

class JotFunctionType : public JotType {
  public:
    JotFunctionType(Token name, std::vector<std::shared_ptr<JotType>> parameters,
                    std::shared_ptr<JotType> return_type, bool varargs = false)
        : name(name), parameters(parameters), return_type(return_type), varargs(varargs) {}

    std::vector<std::shared_ptr<JotType>> get_parameters() { return parameters; }

    std::shared_ptr<JotType> get_return_type() { return return_type; }

    Token get_type_token() override { return name; }

    std::string type_literal() override {
        std::stringstream function_type_literal;
        function_type_literal << "(";
        for (auto &parameter : parameters) {
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

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

    bool has_varargs() { return varargs; }

  private:
    Token name;
    std::vector<std::shared_ptr<JotType>> parameters;
    std::shared_ptr<JotType> return_type;
    bool varargs;
};

class JotStructType : public JotType {
  public:
    JotStructType(Token name, std::vector<Token> names, std::vector<std::shared_ptr<JotType>> types)
        : name(name), fields_names(names), fields_types(types) {}

    Token get_type_token() override { return name; }

    std::vector<Token> get_fields_names() { return fields_names; }

    std::vector<std::shared_ptr<JotType>> get_fields_types() { return fields_types; }

    std::string type_literal() override { return "struct"; }

    TypeKind get_type_kind() override { return TypeKind::Structure; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
    std::vector<Token> fields_names;
    std::vector<std::shared_ptr<JotType>> fields_types;
};

class JotEnumType : public JotType {
  public:
    JotEnumType(Token name, std::unordered_map<std::string, int> values,
                std::shared_ptr<JotType> element_type)
        : name(name), values(values), element_type(element_type) {}

    Token get_type_token() override { return name; }

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    std::unordered_map<std::string, int> get_enum_values() { return values; }

    std::string type_literal() override { return "enum"; }

    TypeKind get_type_kind() override { return TypeKind::Enumeration; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
    std::unordered_map<std::string, int> values;
    std::shared_ptr<JotType> element_type;
};

class JotEnumElementType : public JotType {
  public:
    JotEnumElementType(Token name, std::shared_ptr<JotType> element_type)
        : name(name), element_type(element_type) {}

    Token get_type_token() override { return name; }

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    std::string type_literal() override { return "enum " + name.get_literal(); }

    TypeKind get_type_kind() override { return TypeKind::EnumerationElement; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
    std::shared_ptr<JotType> element_type;
};

class JotNoneType : public JotType {
  public:
    JotNoneType(Token name) : name(name) {}

    Token get_type_token() override { return name; }

    std::string type_literal() override { return "None Type"; }

    TypeKind get_type_kind() override { return TypeKind::None; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
};

class JotVoidType : public JotType {
  public:
    explicit JotVoidType(Token token) : token(token) {}

    Token get_type_token() override { return token; }

    std::string type_literal() override { return "void"; }

    TypeKind get_type_kind() override { return TypeKind::Void; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token token;
};

class JotNullType : public JotType {
  public:
    explicit JotNullType(Token token) : token(token) {}

    Token get_type_token() override { return token; }

    std::string type_literal() override { return "null"; }

    TypeKind get_type_kind() override { return TypeKind::Null; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

    bool castable(const std::shared_ptr<JotType> &other) override;

  private:
    Token token;
};