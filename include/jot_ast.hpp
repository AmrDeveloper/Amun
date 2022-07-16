#pragma once

#include "jot_ast_type.hpp"
#include "jot_token.hpp"
#include "jot_type.hpp"

#include <vector>
#include <memory>

class Statement {

};

class Expression {
public:
    virtual TypeNode get_type_node() = 0;
};

class CompilationUnit {
public:
    CompilationUnit(std::vector<std::shared_ptr<Statement>> tree_nodes) 
        : tree_nodes(tree_nodes) {}

    std::vector<std::shared_ptr<Statement>> get_tree_nodes() { return tree_nodes; }

private:
    std::vector<std::shared_ptr<Statement>> tree_nodes;
};

class BlockStatement : public Statement {
public:
    BlockStatement(std::vector<std::shared_ptr<Statement>> nodes) 
        : nodes(nodes) {}
    
    std::vector<std::shared_ptr<Statement>> get_nodes() { return nodes; }

private:
    std::vector<std::shared_ptr<Statement>> nodes;
};

class Parameter {
public:
    Parameter(Token name, TypeNode type) : name(name), type(type) {}

    Token get_name() { return name; }

    TypeNode get_type() { return type; }

private:
    Token name;
    TypeNode type;
};

class FieldDeclaration : public Statement {
public:
    FieldDeclaration(Token name, TypeNode type, std::shared_ptr<Expression> value)
        : name(name), type(type), value(value) {}

    Token get_name() { return name; }

    TypeNode get_type() { return type; }

    std::shared_ptr<Expression> get_value() { return value; }

private:
    Token name;
    TypeNode type;
    std::shared_ptr<Expression> value;
};

class FunctionPrototype : public Statement {
public:
    FunctionPrototype(Token name, std::vector<std::shared_ptr<Parameter>> parameters, TypeNode return_type) 
        : name(name), parameters(parameters), return_type(return_type) {}

    Token get_name() { return name; }

    std::vector<std::shared_ptr<Parameter>> get_parameters() { return parameters; }

    TypeNode get_return_type() { return return_type; }

private:
    Token name;
    std::vector<std::shared_ptr<Parameter>> parameters;
    TypeNode return_type;
};

class FunctionDeclaration : public Statement {
public:
    FunctionDeclaration(std::shared_ptr<FunctionPrototype> prototype, std::shared_ptr<Statement> body)
        : prototype(prototype), body(body) {}

    std::shared_ptr<FunctionPrototype> get_prototype() { return prototype; }

    std::shared_ptr<Statement> get_body() { return body; }

private:
    std::shared_ptr<FunctionPrototype> prototype;
    std::shared_ptr<Statement> body;
};

class WhileStatement : public Statement {
public:
    WhileStatement(Token position, std::shared_ptr<Expression> condition, std::shared_ptr<Statement> body)
        : position(position), condition(condition), body(body) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_condition() { return condition; }

    std::shared_ptr<Statement> get_body() { return body; }
private:
    Token position;
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Statement> body;
};

class ReturnStatement : public Statement {
public:
    ReturnStatement(Token position, std::shared_ptr<Expression> value)
        : position(position), value(value) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> return_value() { return value; }
private:
    Token position;
    std::shared_ptr<Expression> value;
};

class ExpressionStatement : public Statement {
public:
    ExpressionStatement(std::shared_ptr<Expression> expression) : expression(expression) {}

    std::shared_ptr<Expression> get_expression() { return expression; }

private:
    std::shared_ptr<Expression> expression;
};

class GroupExpression : public Expression {
public:
    GroupExpression(Token position, std::shared_ptr<Expression> expression)
        : position(position), expression(expression) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_expression() { return expression; }

    TypeNode get_type_node() override { return expression->get_type_node(); }
private:
    Token position;
    std::shared_ptr<Expression> expression;
};

class BinaryExpression : public Expression {
public:
    BinaryExpression(std::shared_ptr<Expression> left, Token token, std::shared_ptr<Expression> right)
    : left(left), operator_token(token), right(right) {}

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    TypeNode get_type_node() override { return right->get_type_node(); }

private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
};

class UnaryExpression : public Expression {
public:
    UnaryExpression(Token token, std::shared_ptr<Expression> right)
        : operator_token(token), right(right) {}

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    TypeNode get_type_node() override { return right->get_type_node(); }

private:
    Token operator_token;
    std::shared_ptr<Expression> right;
};

class CallExpression : public Expression {
public:
    CallExpression(Token position, std::shared_ptr<Expression> callee, std::vector<std::shared_ptr<Expression>> arguments)
        : position(position), callee(callee), arguments(arguments) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_callee() { return callee; }

    std::vector<std::shared_ptr<Expression>> get_arguments() { return arguments; }

    TypeNode get_type_node() override { return callee->get_type_node(); }

private:
    Token position;
    std::shared_ptr<Expression> callee;
    std::vector<std::shared_ptr<Expression>> arguments;
};

class LiteralExpression : public Expression {
public:
    LiteralExpression(Token name)
        : name(name), type(TypeNode(JotNull(), name.get_span())) {}

    Token get_name() { return name; }

    TypeNode get_type_node() override { return type; }

private:
    Token name;
    TypeNode type;
};

class NumberExpression : public Expression {
public:
    NumberExpression(Token value)
        : value(value), type(TypeNode(
            JotNumber(value.get_kind() == TokenKind::Integer ?
            NumberKind::Integer64
            : NumberKind::Float64),
            value.get_span())) {}

    Token get_value() { return value; }

    TypeNode get_type_node() override { return type; }

private:
    Token value;
    TypeNode type;
};

class CharacterExpression : public Expression {
public:
    CharacterExpression(Token value)
        : value(value), type(TypeNode(JotNumber(NumberKind::Integer8), value.get_span())) {}

    Token get_value() { return value; }

    TypeNode get_type_node() override { return type; }

private:
    Token value;
    TypeNode type;
};

class BooleanExpression : public Expression {
public:
    BooleanExpression(Token value)
        : value(value), type(TypeNode(JotNumber(NumberKind::Integer1), value.get_span())) {}

    Token get_value() { return value; }

    TypeNode get_type_node() override { return type; }

private:
    Token value;
    TypeNode type;
};

class NullExpression : public Expression {
public:
    NullExpression(Token value)
        : value(value), type(TypeNode(JotNull(), value.get_span())) {}

    Token get_value() { return value; }

    TypeNode get_type_node() override { return type; }

private:
    Token value;
    TypeNode type;
};

