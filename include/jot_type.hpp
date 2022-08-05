#pragma once

#include "jot_token.hpp"

#include <memory>
#include <sstream>
#include <utility>
#include <vector>

enum TypeKind {
    Number,
    Pointer,
    Function,
    Array,
    Enumeration,
    Named,
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

class JotNumber : public JotType {
  public:
    JotNumber(Token token, NumberKind kind) : token(token), kind(kind) {}

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

  private:
    Token token;
    std::shared_ptr<JotType> point_to;
};

class JotArrayType : public JotType {
  public:
    JotArrayType(std::shared_ptr<JotType> element_type, size_t size)
        : element_type(element_type), size(size) {}

    std::shared_ptr<JotType> get_element_type() { return element_type; }

    size_t get_size() { return size; }

    Token get_type_token() override { return element_type->get_type_token(); }

    std::string type_literal() override { return "[" + element_type->type_literal() + "]"; }

    TypeKind get_type_kind() override { return TypeKind::Array; }

    TokenSpan get_type_position() override { return element_type->get_type_token().get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

  private:
    std::shared_ptr<JotType> element_type;
    size_t size;
};

class JotFunctionType : public JotType {
  public:
    JotFunctionType(Token name, std::vector<std::shared_ptr<JotType>> parameters,
                    std::shared_ptr<JotType> return_type)
        : name(name), parameters(parameters), return_type(return_type) {}

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

  private:
    Token name;
    std::vector<std::shared_ptr<JotType>> parameters;
    std::shared_ptr<JotType> return_type;
};

class JotEnumType : public JotType {
  public:
    JotEnumType(Token name, std::vector<Token> values) : name(name), values(values) {}

    Token get_type_token() override { return name; }

    std::string type_literal() override { return "enum"; }

    TypeKind get_type_kind() override { return TypeKind::Enumeration; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
    std::vector<Token> values;
};

class JotNamedType : public JotType {
  public:
    JotNamedType(Token name) : name(name) {}

    Token get_type_token() override { return name; }

    std::string type_literal() override { return "Named " + name.get_literal(); }

    TypeKind get_type_kind() override { return TypeKind::Named; }

    TokenSpan get_type_position() override { return name.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

  private:
    Token name;
};

class JotVoid : public JotType {
  public:
    explicit JotVoid(Token token) : token(token) {}

    Token get_type_token() override { return token; }

    std::string type_literal() override { return "void"; }

    TypeKind get_type_kind() override { return TypeKind::Void; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

  private:
    Token token;
};

class JotNull : public JotType {
  public:
    explicit JotNull(Token token) : token(token) {}

    Token get_type_token() override { return token; }

    std::string type_literal() override { return "null"; }

    TypeKind get_type_kind() override { return TypeKind::Null; }

    TokenSpan get_type_position() override { return token.get_span(); }

    bool equals(const std::shared_ptr<JotType> &other) override;

  private:
    Token token;
};