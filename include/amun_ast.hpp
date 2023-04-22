#pragma once

#include "amun_ast_visitor.hpp"
#include "amun_primitives.hpp"
#include "amun_token.hpp"

#include <any>
#include <memory>
#include <vector>

enum class AstNodeType {
    AST_NODE,

    // Statements
    AST_BLOCK,
    AST_FIELD_DECLARAION,
    AST_PROTOTYPE,
    AST_INTRINSIC,
    AST_FUNCTION,
    AST_OPERATOR_FUNCTION,
    AST_STRUCT,
    AST_ENUM,
    AST_IF_STATEMENT,
    AST_SWITCH_STATEMENT,
    AST_FOR_RANGE,
    AST_FOR_EACH,
    AST_FOR_EVER,
    AST_WHILE,
    AST_RETURN,
    AST_DEFER,
    AST_BREAK,
    AST_CONTINUE,
    AST_EXPRESSION_STATEMENT,

    // Expressions
    AST_IF_EXPRESSION,
    AST_SWITCH_EXPRESSION,
    AST_GROUP,
    AST_TUPLE,
    AST_ASSIGN,
    AST_BINARY,
    AST_SHIFT,
    AST_COMPARISON,
    AST_LOGICAL,
    AST_PREFIX_UNARY,
    AST_POSTFIX_UNARY,
    AST_CALL,
    AST_INIT,
    AST_LAMBDA,
    AST_DOT,
    AST_CAST,
    AST_TYPE_SIZE,
    AST_VALUE_SIZE,
    AST_INDEX,
    AST_ENUM_ELEMENT,
    AST_ARRAY,
    AST_STRING,
    AST_LITERAL,
    AST_NUMBER,
    AST_CHARACTER,
    AST_BOOL,
    AST_NULL
};

class AstNode {
  public:
    virtual AstNodeType get_ast_node_type() = 0;
};

class Statement : public AstNode {
  public:
    virtual std::any accept(StatementVisitor* visitor) = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::AST_NODE; }
};

class Expression : public AstNode {
  public:
    virtual Shared<amun::Type> get_type_node() = 0;
    virtual void set_type_node(Shared<amun::Type> new_type) = 0;
    virtual std::any accept(ExpressionVisitor* visitor) = 0;
    virtual bool is_constant() = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::AST_NODE; }
};

class CompilationUnit {
  public:
    CompilationUnit(std::vector<Shared<Statement>> tree_nodes) : tree_nodes(tree_nodes) {}

    std::vector<Shared<Statement>> get_tree_nodes() { return tree_nodes; }

    std::vector<Shared<Statement>> tree_nodes;
};

class BlockStatement : public Statement {
  public:
    BlockStatement(std::vector<Shared<Statement>> nodes) : statements(nodes) {}

    std::vector<Shared<Statement>> get_nodes() { return statements; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_BLOCK; }

    std::vector<Shared<Statement>> statements;
};

struct Parameter {
    Parameter(Token name, Shared<amun::Type> type) : name(std::move(name)), type(std::move(type)) {}
    Token name;
    Shared<amun::Type> type;
};

class FieldDeclaration : public Statement {
  public:
    FieldDeclaration(Token name, Shared<amun::Type> type, Shared<Expression> value,
                     bool global = false)
        : name(name), type(type), value(value), global(global)
    {
    }

    Token get_name() { return name; }

    void set_type(Shared<amun::Type> resolved_type) { type = resolved_type; }

    Shared<amun::Type> get_type() { return type; }

    Shared<Expression> get_value() { return value; }

    bool is_global() { return global; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FIELD_DECLARAION; }

    Token name;
    Shared<amun::Type> type;
    Shared<Expression> value;
    bool global;
};

class ConstDeclaration : public Statement {
  public:
    ConstDeclaration(Token name, Shared<Expression> value)
        : name(std::move(name)), value(std::move(value))
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FIELD_DECLARAION; }

    Token name;
    Shared<Expression> value;
};

class FunctionPrototype : public Statement {
  public:
    FunctionPrototype(Token name, std::vector<Shared<Parameter>> parameters,
                      Shared<amun::Type> return_type, bool external = false, bool varargs = false,
                      Shared<amun::Type> varargs_type = {}, bool is_generic = false,
                      std::vector<std::string> generic_parameters = {})
        : name(name), parameters(parameters), return_type(return_type), external(external),
          varargs(varargs), varargs_type(varargs_type), is_generic(is_generic),
          generic_parameters(generic_parameters)
    {
    }

    Token get_name() { return name; }

    std::vector<Shared<Parameter>> get_parameters() { return parameters; }

    Shared<amun::Type> get_return_type() { return return_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    bool is_external() { return external; }

    bool has_varargs() { return varargs; }

    Shared<amun::Type> get_varargs_type() { return varargs_type; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_PROTOTYPE; }

    Token name;
    std::vector<Shared<Parameter>> parameters;
    Shared<amun::Type> return_type;
    bool external;

    bool varargs;
    Shared<amun::Type> varargs_type;

    bool is_generic;
    std::vector<std::string> generic_parameters;
};

class IntrinsicPrototype : public Statement {
  public:
    IntrinsicPrototype(Token name, std::string native_name,
                       std::vector<Shared<Parameter>> parameters, Shared<amun::Type> return_type,
                       bool varargs, Shared<amun::Type> varargs_type)
        : name(name), native_name(native_name), parameters(parameters), return_type(return_type),
          varargs(varargs), varargs_type(varargs_type)
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_INTRINSIC; }

    Token name;
    std::string native_name;
    std::vector<Shared<Parameter>> parameters;
    Shared<amun::Type> return_type;

    bool varargs;
    Shared<amun::Type> varargs_type;
};

class FunctionDeclaration : public Statement {
  public:
    FunctionDeclaration(Shared<FunctionPrototype> prototype, Shared<Statement> body)
        : prototype(prototype), body(body)
    {
    }

    Shared<FunctionPrototype> get_prototype() { return prototype; }

    Shared<Statement> get_body() { return body; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FUNCTION; }

    Shared<FunctionPrototype> prototype;
    Shared<Statement> body;
};

class OperatorFunctionDeclaraion : public Statement {
  public:
    OperatorFunctionDeclaraion(Token op, Shared<FunctionDeclaration> function)
        : op(std::move(op)), function(std::move(function))
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_OPERATOR_FUNCTION; }

    Token op;
    Shared<FunctionDeclaration> function;
};

class StructDeclaration : public Statement {
  public:
    StructDeclaration(Shared<amun::StructType> struct_type) : struct_type(struct_type) {}

    Shared<amun::StructType> get_struct_type() { return struct_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_STRUCT; }

    Shared<amun::StructType> struct_type;
};

class EnumDeclaration : public Statement {
  public:
    EnumDeclaration(Token name, Shared<amun::EnumType> enum_type) : name(name), enum_type(enum_type)
    {
    }

    Token get_name() { return name; }

    Shared<amun::Type> get_enum_type() { return enum_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_ENUM; }

    Token name;
    Shared<amun::Type> enum_type;
};

class ConditionalBlock {
  public:
    ConditionalBlock(Token position, Shared<Expression> condition, Shared<Statement> body)
        : position(position), condition(condition), body(body)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_condition() { return condition; }

    Shared<Statement> get_body() { return body; }

    Token position;
    Shared<Expression> condition;
    Shared<Statement> body;
};

class IfStatement : public Statement {
  public:
    IfStatement(std::vector<Shared<ConditionalBlock>> conditional_blocks, bool has_else)
        : conditional_blocks(conditional_blocks), has_else(has_else)
    {
    }

    std::vector<Shared<ConditionalBlock>> get_conditional_blocks() { return conditional_blocks; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_IF_STATEMENT; }

    bool has_else_branch() { return has_else; }

    std::vector<Shared<ConditionalBlock>> conditional_blocks;
    bool has_else;
};

class ForRangeStatement : public Statement {
  public:
    ForRangeStatement(Token position, std::string element_name, Shared<Expression> range_start,
                      Shared<Expression> range_end, Shared<Expression> step, Shared<Statement> body)
        : position(position), element_name(std::move(element_name)),
          range_start(std::move(range_start)), range_end(std::move(range_end)),
          step(std::move(step)), body(std::move(body))
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FOR_RANGE; }

    Token position;
    std::string element_name;
    Shared<Expression> range_start;
    Shared<Expression> range_end;
    Shared<Expression> step;
    Shared<Statement> body;
};

class ForEachStatement : public Statement {
  public:
    ForEachStatement(Token position, std::string element_name, std::string index_name,
                     Shared<Expression> collection, Shared<Statement> body)
        : position(std::move(position)), element_name(std::move(element_name)),
          index_name(std::move(index_name)), collection(std::move(collection)), body(body)
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FOR_RANGE; }

    Token position;
    std::string element_name;
    std::string index_name;
    Shared<Expression> collection;
    Shared<Statement> body;
};

class ForeverStatement : public Statement {
  public:
    ForeverStatement(Token position, Shared<Statement> body)
        : position(position), body(std::move(body))
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_FOR_EVER; }

    Token position;
    Shared<Statement> body;
};

class WhileStatement : public Statement {
  public:
    WhileStatement(Token position, Shared<Expression> condition, Shared<Statement> body)
        : position(position), condition(condition), body(body)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_condition() { return condition; }

    Shared<Statement> get_body() { return body; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_WHILE; }

    Token position;
    Shared<Expression> condition;
    Shared<Statement> body;
};

class SwitchCase {
  public:
    SwitchCase(Token position, std::vector<Shared<Expression>> values, Shared<Statement> body)
        : position(position), values(values), body(body)
    {
    }

    Token get_position() { return position; }

    std::vector<Shared<Expression>> get_values() { return values; }

    Shared<Statement> get_body() { return body; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<Statement> body;
};

class SwitchStatement : public Statement {
  public:
    SwitchStatement(Token position, Shared<Expression> argument,
                    std::vector<Shared<SwitchCase>> cases, Shared<SwitchCase> default_case)
        : position(position), argument(argument), cases(cases), default_case(default_case)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_argument() { return argument; }

    std::vector<Shared<SwitchCase>> get_cases() { return cases; }

    Shared<SwitchCase> get_default_case() { return default_case; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_SWITCH_STATEMENT; }

    Token position;
    Shared<Expression> argument;
    std::vector<Shared<SwitchCase>> cases;
    Shared<SwitchCase> default_case;
};

class ReturnStatement : public Statement {
  public:
    ReturnStatement(Token position, Shared<Expression> value, bool contain_value)
        : position(position), value(value), contain_value(contain_value)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> return_value() { return value; }

    bool has_value() { return contain_value; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_RETURN; }

    Token position;
    Shared<Expression> value;
    bool contain_value;
};

class DeferStatement : public Statement {
  public:
    DeferStatement(Token position, Shared<CallExpression> call) : position(position), call(call) {}

    Token get_position() { return position; }

    Shared<CallExpression> get_call_expression() { return call; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_DEFER; }

    Token position;
    Shared<CallExpression> call;
};

class BreakStatement : public Statement {
  public:
    BreakStatement(Token token, bool has_times, int times)
        : break_token(token), has_times(has_times), times(times)
    {
    }

    Token get_position() { return break_token; }

    bool is_has_times() { return has_times; }

    int get_times() { return times; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_BREAK; }

    Token break_token;
    bool has_times;
    int times;
};

class ContinueStatement : public Statement {
  public:
    ContinueStatement(Token token, bool has_times, int times)
        : break_token(token), has_times(has_times), times(times)
    {
    }

    Token get_position() { return break_token; }

    bool is_has_times() { return has_times; }

    int get_times() { return times; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_CONTINUE; }

    Token break_token;
    bool has_times;

    int times;
};

class ExpressionStatement : public Statement {
  public:
    ExpressionStatement(Shared<Expression> expression) : expression(expression) {}

    Shared<Expression> get_expression() { return expression; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_EXPRESSION_STATEMENT; }

    Shared<Expression> expression;
};

class IfExpression : public Expression {
  public:
    IfExpression(Token if_token, Token else_token, Shared<Expression> condition,
                 Shared<Expression> if_expression, Shared<Expression> else_expression)
        : if_token(if_token), else_token(else_token), condition(condition),
          if_expression(if_expression), else_expression(else_expression)
    {
        type = if_expression->get_type_node();
    }

    Token get_if_position() { return if_token; }

    Token get_else_position() { return else_token; }

    Shared<Expression> get_condition() { return condition; }

    Shared<Expression> get_if_value() { return if_expression; }

    Shared<Expression> get_else_value() { return else_expression; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        return condition->is_constant() && if_expression->is_constant() &&
               else_expression->is_constant();
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_IF_EXPRESSION; }

    Token if_token;
    Token else_token;
    Shared<Expression> condition;
    Shared<Expression> if_expression;
    Shared<Expression> else_expression;
    Shared<amun::Type> type;
};

class SwitchExpression : public Expression {
  public:
    SwitchExpression(Token switch_token, Shared<Expression> argument,
                     std::vector<Shared<Expression>> switch_cases,
                     std::vector<Shared<Expression>> switch_cases_values,
                     Shared<Expression> default_value)
        : switch_token(switch_token), argument(argument), switch_cases(switch_cases),
          switch_cases_values(switch_cases_values), default_value(default_value)
    {
        type = switch_cases_values[0]->get_type_node();
    }

    Token get_position() { return switch_token; }

    Shared<Expression> get_argument() { return argument; }

    std::vector<Shared<Expression>> get_switch_cases() { return switch_cases; }

    std::vector<Shared<Expression>> get_switch_cases_values() { return switch_cases_values; }

    Shared<Expression> get_default_case_value() { return default_value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        if (argument->is_constant()) {
            for (auto& switch_case : switch_cases) {
                if (not switch_case->is_constant()) {
                    return false;
                }
            }

            for (auto& switch_value : switch_cases_values) {
                if (not switch_value->is_constant()) {
                    return false;
                }
            }
        }
        return true;
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_SWITCH_EXPRESSION; }

    Token switch_token;
    Shared<Expression> argument;
    std::vector<Shared<Expression>> switch_cases;
    std::vector<Shared<Expression>> switch_cases_values;
    Shared<Expression> default_value;
    Shared<amun::Type> type;
};

class GroupExpression : public Expression {
  public:
    GroupExpression(Token position, Shared<Expression> expression)
        : position(position), expression(expression)
    {
        type = expression->get_type_node();
    }

    Token get_position() { return position; }

    Shared<Expression> get_expression() { return expression; }

    Shared<amun::Type> get_type_node() override { return expression->get_type_node(); }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return expression->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_GROUP; }

    Token position;
    Shared<Expression> expression;
    Shared<amun::Type> type;
};

class TupleExpression : public Expression {
  public:
    TupleExpression(Token position, std::vector<Shared<Expression>> values)
        : position(position), values(values)
    {
        type = amun::none_type;
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_TUPLE; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<amun::Type> type;
};

class AssignExpression : public Expression {
  public:
    AssignExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right)
    {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_ASSIGN; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class BinaryExpression : public Expression {
  public:
    BinaryExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right)
    {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_BINARY; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class ShiftExpression : public Expression {
  public:
    ShiftExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right)
    {
        type = right->get_type_node();
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_SHIFT; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class ComparisonExpression : public Expression {
  public:
    ComparisonExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right), type(amun::i1_type)
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_COMPARISON; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class LogicalExpression : public Expression {
  public:
    LogicalExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right), type(amun::i1_type)
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_LOGICAL; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class PrefixUnaryExpression : public Expression {
  public:
    PrefixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(token), right(right), type(right->get_type_node())
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_PREFIX_UNARY; }

    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class PostfixUnaryExpression : public Expression {
  public:
    PostfixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(token), right(right), type(right->get_type_node())
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_POSTFIX_UNARY; }

    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class CallExpression : public Expression {
  public:
    CallExpression(Token position, Shared<Expression> callee,
                   std::vector<Shared<Expression>> arguments,
                   std::vector<Shared<amun::Type>> generic_arguments = {})
        : position(position), callee(callee), arguments(arguments), type(callee->get_type_node()),
          generic_arguments(generic_arguments)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_callee() { return callee; }

    std::vector<Shared<Expression>> get_arguments() { return arguments; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_CALL; }

    Token position;
    Shared<Expression> callee;
    std::vector<Shared<Expression>> arguments;
    Shared<amun::Type> type;
    std::vector<Shared<amun::Type>> generic_arguments;
};

class InitializeExpression : public Expression {
  public:
    InitializeExpression(Token position, Shared<amun::Type> type,
                         std::vector<Shared<Expression>> arguments)
        : position(std::move(position)), type(std::move(type)), arguments(std::move(arguments))
    {
    }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        for (auto& argument : arguments) {
            if (not argument->is_constant()) {
                return false;
            }
        }
        return true;
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_INIT; }

    Token position;
    Shared<amun::Type> type;
    std::vector<Shared<Expression>> arguments;
};

class LambdaExpression : public Expression {
  public:
    LambdaExpression(Token position, std::vector<Shared<Parameter>> parameters,
                     Shared<amun::Type> return_type, Shared<BlockStatement> body)
        : position(std::move(position)), explicit_parameters(std::move(parameters)),
          return_type(return_type), body(std::move(body))
    {
        std::vector<Shared<amun::Type>> parameters_types;
        parameters_types.reserve(explicit_parameters.size());
        for (auto& parameter : explicit_parameters) {
            parameters_types.push_back(parameter->type);
        }
        auto function_type =
            std::make_shared<amun::FunctionType>(position, parameters_types, return_type);
        lambda_type = std::make_shared<amun::PointerType>(function_type);
    }

    Shared<amun::Type> get_type_node() override { return lambda_type; }

    void set_type_node(Shared<amun::Type> new_type) override { lambda_type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_LAMBDA; }

    Token position;
    std::vector<Shared<Parameter>> explicit_parameters;
    std::vector<std::string> implict_parameters_names;
    std::vector<Shared<amun::Type>> implict_parameters_types;
    Shared<amun::Type> return_type;
    Shared<BlockStatement> body;
    Shared<amun::Type> lambda_type;
};

class DotExpression : public Expression {
  public:
    DotExpression(Token dot_token, Shared<Expression> callee, Token field_name)
        : dot_token(dot_token), callee(callee), field_name(field_name)
    {
    }

    Token get_position() { return dot_token; }

    Shared<Expression> get_callee() { return callee; }

    Token get_field_name() { return field_name; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_DOT; }

    bool is_constant() override { return is_constants_; }

    int field_index = 0;
    bool is_constants_ = false;

    Token dot_token;
    Shared<Expression> callee;
    Token field_name;
    Shared<amun::Type> type;
};

class CastExpression : public Expression {
  public:
    CastExpression(Token position, Shared<amun::Type> type, Shared<Expression> value)
        : position(position), type(type), value(value)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return value->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_CAST; }

    Token position;
    Shared<amun::Type> type;
    Shared<Expression> value;
};

class TypeSizeExpression : public Expression {
  public:
    TypeSizeExpression(Token position, Shared<amun::Type> type)
        : position(position), type(type), node_type(amun::i64_type)
    {
    }

    Token get_position() { return position; }

    Shared<amun::Type> get_type_node() override { return node_type; }

    void set_type_node(Shared<amun::Type> new_type) override { node_type = new_type; }

    Shared<amun::Type> get_type() { return type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_TYPE_SIZE; }

    Token position;
    Shared<amun::Type> type;
    Shared<amun::Type> node_type;
};

class ValueSizeExpression : public Expression {
  public:
    ValueSizeExpression(Token position, Shared<Expression> value)
        : position(position), value(value), node_type(amun::i64_type)
    {
    }

    Token get_position() { return position; }

    Shared<amun::Type> get_type_node() override { return node_type; }

    void set_type_node(Shared<amun::Type> new_type) override { node_type = new_type; }

    Shared<Expression> get_value() { return value; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_VALUE_SIZE; }

    Token position;
    Shared<Expression> value;
    Shared<amun::Type> node_type;
};

class IndexExpression : public Expression {
  public:
    IndexExpression(Token position, Shared<Expression> value, Shared<Expression> index)
        : position(position), value(value), index(index)
    {
        type = std::make_shared<amun::NoneType>();
    }

    Token get_position() { return position; }

    Shared<Expression> get_value() { return value; }

    Shared<Expression> get_index() { return index; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        // TODO: Should be calculated depend if value and index are constants
        return index->is_constant();
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_INDEX; }

    Token position;
    Shared<Expression> value;
    Shared<Expression> index;
    Shared<amun::Type> type;
};

class EnumAccessExpression : public Expression {
  public:
    EnumAccessExpression(Token enum_name, Token element_name, int index,
                         Shared<amun::Type> element_type)
        : enum_name(enum_name), element_name(element_name), index(index), element_type(element_type)
    {
    }

    Token get_enum_name() { return enum_name; }

    int get_enum_element_index() { return index; }

    Shared<amun::Type> get_type_node() override { return element_type; }

    void set_type_node(Shared<amun::Type> new_type) override { element_type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_ENUM_ELEMENT; }

    Token enum_name;
    Token element_name;
    int index;
    Shared<amun::Type> element_type;
};

class ArrayExpression : public Expression {
  public:
    ArrayExpression(Token position, std::vector<Shared<Expression>> values)
        : position(position), values(values)
    {
        auto size = values.size();
        auto element_type = size == 0 ? amun::none_type : values[0]->get_type_node();
        type = std::make_shared<amun::StaticArrayType>(element_type, size);

        // Check if all values of array are constant or not
        for (auto& value : values) {
            if (not value->is_constant()) {
                is_constants_array = false;
                break;
            }
        }
    }

    Token get_position() { return position; }

    std::vector<Shared<Expression>> get_values() { return values; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return is_constants_array; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_ARRAY; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<amun::Type> type;
    bool is_constants_array = true;
};

class StringExpression : public Expression {
  public:
    StringExpression(Token value) : value(value), type(amun::i8_ptr_type) {}

    Token get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_STRING; }

    Token value;
    Shared<amun::Type> type;
};

class LiteralExpression : public Expression {
  public:
    LiteralExpression(Token name, Shared<amun::Type> type) : name(name), type(type) {}

    Token get_name() { return name; }

    void set_type(Shared<amun::Type> resolved_type) { type = resolved_type; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    void set_constant(bool is_constants) { constants = is_constants; }

    bool is_constant() override { return constants; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_LITERAL; }

    Token name;
    Shared<amun::Type> type;
    bool constants = false;
};

class NumberExpression : public Expression {
  public:
    NumberExpression(Token value, Shared<amun::Type> type) : value(value), type(type) {}

    Token get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_NUMBER; }

    Token value;
    Shared<amun::Type> type;
};

class CharacterExpression : public Expression {
  public:
    CharacterExpression(Token value) : value(value), type(amun::i8_type) {}

    Token get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_CHARACTER; }

    Token value;
    Shared<amun::Type> type;
};

class BooleanExpression : public Expression {
  public:
    BooleanExpression(Token value) : value(value), type(amun::i1_type) {}

    Token get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_BOOL; }

    Token value;
    Shared<amun::Type> type;
};

class NullExpression : public Expression {
  public:
    NullExpression(Token value)
        : value(value), type(amun::null_type), null_base_type(amun::i32_ptr_type)
    {
    }

    Token get_value() { return value; }

    Shared<amun::Type> get_type_node() override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AST_NULL; }

    Token value;
    Shared<amun::Type> type;

  public:
    Shared<amun::Type> null_base_type;
};
