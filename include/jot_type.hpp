#pragma once

#include "jot_token.hpp"

#include <memory>
#include <utility>
#include <vector>

enum TypeKind {
    Number,
    Pointer,
    Function,
    Unary,
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
    JotNumber(Token token, NumberKind kind)
        : token(token), kind(kind) {}

    NumberKind get_kind() { return kind; }

    Token get_type_token() override {
        return token;
    }

    std::string type_literal() override {
        return token.get_literal();
    }

    TypeKind get_type_kind() override {
        return TypeKind::Number;
    }

    TokenSpan get_type_position() override {
        return token.get_span();
    }

private:
    Token token;
    NumberKind kind;
};

class JotPointerType : public JotType {
public:
    JotPointerType(Token token, std::shared_ptr<JotType> point_to)
        : token(token), point_to(point_to) {}

    std::shared_ptr<JotType> get_point_to() { return point_to; }

    Token get_type_token() override {
        return token;
    }

    std::string type_literal() override {
        return "*" + token.get_literal();
    }

    TypeKind get_type_kind() override {
        return TypeKind::Pointer;
    }

    TokenSpan get_type_position() override {
        return token.get_span();
    }
private:
    Token token;
    std::shared_ptr<JotType> point_to;
};

class JotFunctionType : public JotType {
public:
    JotFunctionType(Token name, std::vector<std::shared_ptr<JotType>> parameters,std::shared_ptr<JotType> return_type)
        : name(name), parameters(parameters), return_type(return_type) {}

    std::vector<std::shared_ptr<JotType>> get_parameters() { return parameters; }

    std::shared_ptr<JotType> get_return_type() { return return_type; }

    Token get_type_token() override {
        return name;
    }

    std::string type_literal() override {
        return name.get_literal();
    }

    TypeKind get_type_kind() override {
        return TypeKind::Function;
    }

    TokenSpan get_type_position() override {
        return name.get_span();
    }

private:
    Token name;
    std::vector<std::shared_ptr<JotType>> parameters;
    std::shared_ptr<JotType> return_type;
};

class JotUnaryType : public JotType {
public:
    JotUnaryType(Token unary_operator, std::shared_ptr<JotType> type)
        : unary_operator(unary_operator), type(type) {}

    Token get_type_token() override {
        return type->get_type_token();
    }

    std::string type_literal() override {
        return unary_operator.get_kind_literal() + type->type_literal();
    }

    TypeKind get_type_kind() override {
        return TypeKind::Unary;
    }

    TokenSpan get_type_position() override {
        return unary_operator.get_span();
    }

private:
    Token unary_operator;
    std::shared_ptr<JotType> type;
};

class JotNamedType : public JotType {
public:
    JotNamedType(Token name) : name(name) {}

    Token get_type_token() override {
        return name;
    }

    std::string type_literal() override {
        return name.get_literal();
    }

    TypeKind get_type_kind() override {
        return TypeKind::Named;
    }

    TokenSpan get_type_position() override {
        return name.get_span();
    }

private:
    Token name;
};

class JotVoid : public JotType {
public:
    explicit JotVoid(Token token) : token(token) {}

    Token get_type_token() override {
        return token;
    }

    std::string type_literal() override {
        return "void";
    }

    TypeKind get_type_kind() override {
        return TypeKind::Void;
    }

    TokenSpan get_type_position() override {
        return token.get_span();
    }

private:
    Token token;
};

class JotNull : public JotType {
public:
    explicit JotNull(Token token) : token(token) {}

    Token get_type_token() override {
        return token;
    }

    std::string type_literal() override {
        return "null";
    }

    TypeKind get_type_kind() override {
        return TypeKind::Null;
    }

    TokenSpan get_type_position() override {
        return token.get_span();
    }

private:
    Token token;
};