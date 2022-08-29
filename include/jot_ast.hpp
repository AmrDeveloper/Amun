#pragma once

#include "jot_ast_visitor.hpp"
#include "jot_token.hpp"
#include "jot_type.hpp"

#include <any>
#include <memory>
#include <vector>

enum class AstNodeType {
    Node,
    Block,
    Field,
    Prototype,
    Function,
    Enum,
    If,
    While,
    Return,
    Defer,
    Break,
    Continue,
    Expression,

    IfExpr,
    GroupExpr,
    AssignExpr,
    BinaryExpr,
    ShiftExpr,
    ComparisonExpr,
    LogicalExpr,
    UnaryExpr,
    CallExpr,
    CastExpr,
    IndexExpr,
    EnumElementExpr,
    ArrayExpr,
    StringExpr,
    LiteralExpr,
    NumberExpr,
    CharExpr,
    BoolExpr,
    NullExpr
};

class AstNode {
  public:
    virtual AstNodeType get_ast_node_type() = 0;
};

class Statement : public AstNode {
  public:
    virtual std::any accept(StatementVisitor *visitor) = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::Node; }
};

class Expression : public AstNode {
  public:
    virtual std::shared_ptr<JotType> get_type_node() = 0;
    virtual void set_type_node(std::shared_ptr<JotType> new_type) = 0;
    virtual std::any accept(ExpressionVisitor *visitor) = 0;
    virtual bool is_constant() = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::Node; }
};

class CompilationUnit {
  public:
    CompilationUnit(std::vector<std::shared_ptr<Statement>> tree_nodes) : tree_nodes(tree_nodes) {}

    std::vector<std::shared_ptr<Statement>> get_tree_nodes() { return tree_nodes; }

  private:
    std::vector<std::shared_ptr<Statement>> tree_nodes;
};

class BlockStatement : public Statement {
  public:
    BlockStatement(std::vector<std::shared_ptr<Statement>> nodes) : nodes(nodes) {}

    std::vector<std::shared_ptr<Statement>> get_nodes() { return nodes; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Block; }

  private:
    std::vector<std::shared_ptr<Statement>> nodes;
};

class Parameter {
  public:
    Parameter(Token name, std::shared_ptr<JotType> type) : name(name), type(type) {}

    Token get_name() { return name; }

    std::shared_ptr<JotType> get_type() { return type; }

  private:
    Token name;
    std::shared_ptr<JotType> type;
};

class FieldDeclaration : public Statement {
  public:
    FieldDeclaration(Token name, std::shared_ptr<JotType> type, std::shared_ptr<Expression> value,
                     bool global = false)
        : name(name), type(type), value(value), global(global) {}

    Token get_name() { return name; }

    void set_type(std::shared_ptr<JotType> resolved_type) { type = resolved_type; }

    std::shared_ptr<JotType> get_type() { return type; }

    std::shared_ptr<Expression> get_value() { return value; }

    bool is_global() { return global; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Field; }

  private:
    Token name;
    std::shared_ptr<JotType> type;
    std::shared_ptr<Expression> value;
    bool global;
};

enum FunctionCallKind {
    Normal,
    Prefix,
    Infix,
    Postfix,
};

class FunctionPrototype : public Statement {
  public:
    FunctionPrototype(Token name, std::vector<std::shared_ptr<Parameter>> parameters,
                      std::shared_ptr<JotType> return_type, FunctionCallKind call_kind,
                      bool external)
        : name(name), parameters(parameters), return_type(return_type), call_kind(call_kind),
          external(external) {}

    Token get_name() { return name; }

    std::vector<std::shared_ptr<Parameter>> get_parameters() { return parameters; }

    std::shared_ptr<JotType> get_return_type() { return return_type; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    FunctionCallKind get_call_kind() { return call_kind; }

    bool is_external() { return external; }

    AstNodeType get_ast_node_type() override { return AstNodeType::Prototype; }

  private:
    Token name;
    std::vector<std::shared_ptr<Parameter>> parameters;
    std::shared_ptr<JotType> return_type;
    FunctionCallKind call_kind;
    bool external;
};

class FunctionDeclaration : public Statement {
  public:
    FunctionDeclaration(std::shared_ptr<FunctionPrototype> prototype,
                        std::shared_ptr<Statement> body)
        : prototype(prototype), body(body) {}

    std::shared_ptr<FunctionPrototype> get_prototype() { return prototype; }

    std::shared_ptr<Statement> get_body() { return body; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Function; }

  private:
    std::shared_ptr<FunctionPrototype> prototype;
    std::shared_ptr<Statement> body;
};

class EnumDeclaration : public Statement {
  public:
    EnumDeclaration(Token name, std::shared_ptr<JotType> enum_type)
        : name(name), enum_type(enum_type) {}

    Token get_name() { return name; }

    std::shared_ptr<JotType> get_enum_type() { return enum_type; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Enum; }

  private:
    Token name;
    std::shared_ptr<JotType> enum_type;
};

class ConditionalBlock {
  public:
    ConditionalBlock(Token position, std::shared_ptr<Expression> condition,
                     std::shared_ptr<Statement> body)
        : position(position), condition(condition), body(body) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_condition() { return condition; }

    std::shared_ptr<Statement> get_body() { return body; }

  private:
    Token position;
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Statement> body;
};

class IfStatement : public Statement {
  public:
    IfStatement(std::vector<std::shared_ptr<ConditionalBlock>> conditional_blocks)
        : conditional_blocks(conditional_blocks) {}

    std::vector<std::shared_ptr<ConditionalBlock>> get_conditional_blocks() {
        return conditional_blocks;
    }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::If; }

  private:
    std::vector<std::shared_ptr<ConditionalBlock>> conditional_blocks;
};

class WhileStatement : public Statement {
  public:
    WhileStatement(Token position, std::shared_ptr<Expression> condition,
                   std::shared_ptr<Statement> body)
        : position(position), condition(condition), body(body) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_condition() { return condition; }

    std::shared_ptr<Statement> get_body() { return body; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::While; }

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

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Return; }

  private:
    Token position;
    std::shared_ptr<Expression> value;
};

class DeferStatement : public Statement {
  public:
    DeferStatement(Token position, std::shared_ptr<CallExpression> call)
        : position(position), call(call) {}

    Token get_position() { return position; }

    std::shared_ptr<CallExpression> get_call_expression() { return call; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Defer; }

  private:
    Token position;
    std::shared_ptr<CallExpression> call;
};

class BreakStatement : public Statement {
  public:
    BreakStatement(Token token) : break_token(token) {}

    Token get_position() { return break_token; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Break; }

  private:
    Token break_token;
};

class ContinueStatement : public Statement {
  public:
    ContinueStatement(Token token) : break_token(token) {}

    Token get_position() { return break_token; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Continue; }

  private:
    Token break_token;
};

class ExpressionStatement : public Statement {
  public:
    ExpressionStatement(std::shared_ptr<Expression> expression) : expression(expression) {}

    std::shared_ptr<Expression> get_expression() { return expression; }

    std::any accept(StatementVisitor *visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Expression; }

  private:
    std::shared_ptr<Expression> expression;
};

class IfExpression : public Expression {
  public:
    IfExpression(Token if_token, Token else_token, std::shared_ptr<Expression> condition,
                 std::shared_ptr<Expression> if_expression,
                 std::shared_ptr<Expression> else_expression)
        : if_token(if_token), else_token(else_token), condition(condition),
          if_expression(if_expression), else_expression(else_expression) {
        type = if_expression->get_type_node();
    }

    Token get_if_position() { return if_token; }

    Token get_else_position() { return else_token; }

    std::shared_ptr<Expression> get_condition() { return condition; }

    std::shared_ptr<Expression> get_if_value() { return if_expression; }

    std::shared_ptr<Expression> get_else_value() { return else_expression; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::IfExpr; }

  private:
    Token if_token;
    Token else_token;
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> if_expression;
    std::shared_ptr<Expression> else_expression;
    std::shared_ptr<JotType> type;
};

class GroupExpression : public Expression {
  public:
    GroupExpression(Token position, std::shared_ptr<Expression> expression)
        : position(position), expression(expression) {
        type = expression->get_type_node();
    }

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_expression() { return expression; }

    std::shared_ptr<JotType> get_type_node() override { return expression->get_type_node(); }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return expression->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::GroupExpr; }

  private:
    Token position;
    std::shared_ptr<Expression> expression;
    std::shared_ptr<JotType> type;
};

class AssignExpression : public Expression {
  public:
    AssignExpression(std::shared_ptr<Expression> left, Token token,
                     std::shared_ptr<Expression> right)
        : left(left), operator_token(token), right(right) {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AssignExpr; }

  private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class BinaryExpression : public Expression {
  public:
    BinaryExpression(std::shared_ptr<Expression> left, Token token,
                     std::shared_ptr<Expression> right)
        : left(left), operator_token(token), right(right) {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::BinaryExpr; }

  private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class ShiftExpression : public Expression {
  public:
    ShiftExpression(std::shared_ptr<Expression> left, Token token,
                    std::shared_ptr<Expression> right)
        : left(left), operator_token(token), right(right) {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::ShiftExpr; }

  private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class ComparisonExpression : public Expression {
  public:
    ComparisonExpression(std::shared_ptr<Expression> left, Token token,
                         std::shared_ptr<Expression> right)
        : left(left), operator_token(token), right(right) {
        type = std::make_shared<JotNumberType>(operator_token, NumberKind::Integer1);
    }

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::ComparisonExpr; }

  private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class LogicalExpression : public Expression {
  public:
    LogicalExpression(std::shared_ptr<Expression> left, Token token,
                      std::shared_ptr<Expression> right)
        : left(left), operator_token(token), right(right) {
        type = std::make_shared<JotNumberType>(operator_token, NumberKind::Integer1);
    }

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<Expression> get_left() { return left; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::LogicalExpr; }

  private:
    std::shared_ptr<Expression> left;
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class UnaryExpression : public Expression {
  public:
    UnaryExpression(Token token, std::shared_ptr<Expression> right)
        : operator_token(token), right(right), type(right->get_type_node()) {}

    Token get_operator_token() { return operator_token; }

    std::shared_ptr<Expression> get_right() { return right; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::UnaryExpr; }

  private:
    Token operator_token;
    std::shared_ptr<Expression> right;
    std::shared_ptr<JotType> type;
};

class CallExpression : public Expression {
  public:
    CallExpression(Token position, std::shared_ptr<Expression> callee,
                   std::vector<std::shared_ptr<Expression>> arguments)
        : position(position), callee(callee), arguments(arguments), type(callee->get_type_node()) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_callee() { return callee; }

    std::vector<std::shared_ptr<Expression>> get_arguments() { return arguments; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::CallExpr; }

  private:
    Token position;
    std::shared_ptr<Expression> callee;
    std::vector<std::shared_ptr<Expression>> arguments;
    std::shared_ptr<JotType> type;
};

class CastExpression : public Expression {
  public:
    CastExpression(Token position, std::shared_ptr<JotType> type, std::shared_ptr<Expression> value)
        : position(position), type(type), value(value) {}

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return value->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::CastExpr; }

  private:
    Token position;
    std::shared_ptr<JotType> type;
    std::shared_ptr<Expression> value;
};

class IndexExpression : public Expression {
  public:
    IndexExpression(Token position, std::shared_ptr<Expression> value,
                    std::shared_ptr<Expression> index)
        : position(position), value(value), index(index) {
        type = std::make_shared<JotNoneType>(position);
    }

    Token get_position() { return position; }

    std::shared_ptr<Expression> get_value() { return value; }

    std::shared_ptr<Expression> get_index() { return index; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::IndexExpr; }

  private:
    Token position;
    std::shared_ptr<Expression> value;
    std::shared_ptr<Expression> index;
    std::shared_ptr<JotType> type;
};

class EnumAccessExpression : public Expression {
  public:
    EnumAccessExpression(Token enum_name, Token element_name, int index,
                         std::shared_ptr<JotType> element_type)
        : enum_name(enum_name), element_name(element_name), index(index),
          element_type(element_type) {}

    Token get_enum_name() { return enum_name; }

    int get_enum_element_index() { return index; }

    std::shared_ptr<JotType> get_type_node() override { return element_type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { element_type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::EnumElementExpr; }

  private:
    Token enum_name;
    Token element_name;
    int index;
    std::shared_ptr<JotType> element_type;
};

class ArrayExpression : public Expression {
  public:
    ArrayExpression(Token position, std::vector<std::shared_ptr<Expression>> values)
        : position(position), values(values) {
        auto size = values.size();
        auto element_type =
            size == 0 ? std::make_shared<JotNoneType>(position) : values[0]->get_type_node();
        type = std::make_shared<JotArrayType>(element_type, size);
    }

    Token get_position() { return position; }

    std::vector<std::shared_ptr<Expression>> get_values() { return values; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

  private:
    Token position;
    std::vector<std::shared_ptr<Expression>> values;
    std::shared_ptr<JotType> type;
};

class StringExpression : public Expression {
  public:
    StringExpression(Token value) : value(value) {
        auto element_type = std::make_shared<JotNumberType>(value, NumberKind::Integer8);
        type = std::make_shared<JotPointerType>(value, element_type);
    }

    Token get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::StringExpr; }

  private:
    Token value;
    std::shared_ptr<JotType> type;
};

class LiteralExpression : public Expression {
  public:
    LiteralExpression(Token name, std::shared_ptr<JotType> type) : name(name), type(type) {}

    Token get_name() { return name; }

    void set_type(std::shared_ptr<JotType> resolved_type) { type = resolved_type; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::LiteralExpr; }

  private:
    Token name;
    std::shared_ptr<JotType> type;
};

class NumberExpression : public Expression {
  public:
    NumberExpression(Token value, std::shared_ptr<JotType> type) : value(value), type(type) {}

    Token get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::NumberExpr; }

  private:
    Token value;
    std::shared_ptr<JotType> type;
};

class CharacterExpression : public Expression {
  public:
    CharacterExpression(Token value)
        : value(value), type(std::make_shared<JotNumberType>(value, NumberKind::Integer8)) {}

    Token get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::CharExpr; }

  private:
    Token value;
    std::shared_ptr<JotType> type;
};

class BooleanExpression : public Expression {
  public:
    BooleanExpression(Token value)
        : value(value), type(std::make_shared<JotNumberType>(value, NumberKind::Integer1)) {}

    Token get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::BoolExpr; }

  private:
    Token value;
    std::shared_ptr<JotType> type;
};

class NullExpression : public Expression {
  public:
    NullExpression(Token value) : value(value), type(std::make_shared<JotNullType>(value)) {}

    Token get_value() { return value; }

    std::shared_ptr<JotType> get_type_node() override { return type; }

    void set_type_node(std::shared_ptr<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor *visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::NullExpr; }

  private:
    Token value;
    std::shared_ptr<JotType> type;
};
