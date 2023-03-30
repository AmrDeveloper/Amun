#pragma once

#include "jot_ast_visitor.hpp"
#include "jot_primitives.hpp"
#include "jot_token.hpp"

#include <any>
#include <memory>
#include <vector>

/*
 * This enum types will be very useful in the refactor
 * and make it easy to check for the type without castring
 */
enum class AstNodeType {
    Node,
    Block,
    Field,
    Prototype,
    Intrinsic,
    Function,
    Struct,
    Enum,
    If,
    ForRange,
    ForEach,
    Forever,
    While,
    Switch,
    Return,
    Defer,
    Break,
    Continue,
    Expression,

    IfExpr,
    SwitchExpr,
    GroupExpr,
    AssignExpr,
    BinaryExpr,
    ShiftExpr,
    ComparisonExpr,
    LogicalExpr,
    PrefixUnaryExpr,
    PostfixUnaryExpr,
    CallExpr,
    InitExpr,
    LambdaExpr,
    DotExpr,
    CastExpr,
    TypeSizeExpr,
    ValueSizeExpr,
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
    virtual std::any accept(StatementVisitor* visitor) = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::Node; }
};

class Expression : public AstNode {
  public:
    virtual Shared<JotType> get_type_node() = 0;
    virtual void set_type_node(Shared<JotType> new_type) = 0;
    virtual std::any accept(ExpressionVisitor* visitor) = 0;
    virtual bool is_constant() = 0;
    AstNodeType get_ast_node_type() { return AstNodeType::Node; }
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

    AstNodeType get_ast_node_type() override { return AstNodeType::Block; }

    std::vector<Shared<Statement>> statements;
};

struct Parameter {
    Parameter(Token name, Shared<JotType> type) : name(std::move(name)), type(std::move(type)) {}

    Token name;
    Shared<JotType> type;
};

class FieldDeclaration : public Statement {
  public:
    FieldDeclaration(Token name, Shared<JotType> type, Shared<Expression> value,
                     bool global = false)
        : name(name), type(type), value(value), global(global)
    {
    }

    Token get_name() { return name; }

    void set_type(Shared<JotType> resolved_type) { type = resolved_type; }

    Shared<JotType> get_type() { return type; }

    Shared<Expression> get_value() { return value; }

    bool is_global() { return global; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Field; }

    Token name;
    Shared<JotType> type;
    Shared<Expression> value;
    bool global;
};

class FunctionPrototype : public Statement {
  public:
    FunctionPrototype(Token name, std::vector<Shared<Parameter>> parameters,
                      Shared<JotType> return_type, bool external, bool varargs,
                      Shared<JotType> varargs_type, bool is_generic = false,
                      std::vector<std::string> generic_parameters = {})
        : name(name), parameters(parameters), return_type(return_type), external(external),
          varargs(varargs), varargs_type(varargs_type), is_generic(is_generic),
          generic_parameters(generic_parameters)
    {
    }

    Token get_name() { return name; }

    std::vector<Shared<Parameter>> get_parameters() { return parameters; }

    Shared<JotType> get_return_type() { return return_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    bool is_external() { return external; }

    bool has_varargs() { return varargs; }

    Shared<JotType> get_varargs_type() { return varargs_type; }

    AstNodeType get_ast_node_type() override { return AstNodeType::Prototype; }

    Token name;
    std::vector<Shared<Parameter>> parameters;
    Shared<JotType> return_type;
    bool external;

    bool varargs;
    Shared<JotType> varargs_type;

    bool is_generic;
    std::vector<std::string> generic_parameters;
};

class IntrinsicPrototype : public Statement {
  public:
    IntrinsicPrototype(Token name, std::string native_name,
                       std::vector<Shared<Parameter>> parameters, Shared<JotType> return_type,
                       bool varargs, Shared<JotType> varargs_type)
        : name(name), native_name(native_name), parameters(parameters), return_type(return_type),
          varargs(varargs), varargs_type(varargs_type)
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Intrinsic; }

    Token name;
    std::string native_name;
    std::vector<Shared<Parameter>> parameters;
    Shared<JotType> return_type;

    bool varargs;
    Shared<JotType> varargs_type;
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

    AstNodeType get_ast_node_type() override { return AstNodeType::Function; }

    Shared<FunctionPrototype> prototype;
    Shared<Statement> body;
};

class StructDeclaration : public Statement {
  public:
    StructDeclaration(Shared<JotStructType> struct_type) : struct_type(struct_type) {}

    Shared<JotStructType> get_struct_type() { return struct_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Struct; }

    Shared<JotStructType> struct_type;
};

class EnumDeclaration : public Statement {
  public:
    EnumDeclaration(Token name, Shared<JotEnumType> enum_type) : name(name), enum_type(enum_type) {}

    Token get_name() { return name; }

    Shared<JotType> get_enum_type() { return enum_type; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Enum; }

    Token name;
    Shared<JotType> enum_type;
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

    AstNodeType get_ast_node_type() override { return AstNodeType::If; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::ForRange; }

    Token position;
    std::string element_name;
    Shared<Expression> range_start;
    Shared<Expression> range_end;
    Shared<Expression> step;
    Shared<Statement> body;
};

class ForEachStatement : public Statement {
  public:
    ForEachStatement(Token position, std::string element_name, Shared<Expression> collection,
                     Shared<Statement> body)
        : position(position), element_name(std::move(element_name)), collection(collection),
          body(body)
    {
    }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::ForRange; }

    Token position;
    std::string element_name;
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

    AstNodeType get_ast_node_type() override { return AstNodeType::Forever; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::While; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::Switch; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::Return; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::Defer; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::Break; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::Continue; }

    Token break_token;
    bool has_times;

    int times;
};

class ExpressionStatement : public Statement {
  public:
    ExpressionStatement(Shared<Expression> expression) : expression(expression) {}

    Shared<Expression> get_expression() { return expression; }

    std::any accept(StatementVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::Expression; }

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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        return condition->is_constant() && if_expression->is_constant() &&
               else_expression->is_constant();
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::IfExpr; }

    Token if_token;
    Token else_token;
    Shared<Expression> condition;
    Shared<Expression> if_expression;
    Shared<Expression> else_expression;
    Shared<JotType> type;
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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::SwitchExpr; }

    Token switch_token;
    Shared<Expression> argument;
    std::vector<Shared<Expression>> switch_cases;
    std::vector<Shared<Expression>> switch_cases_values;
    Shared<Expression> default_value;
    Shared<JotType> type;
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

    Shared<JotType> get_type_node() override { return expression->get_type_node(); }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return expression->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::GroupExpr; }

    Token position;
    Shared<Expression> expression;
    Shared<JotType> type;
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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::AssignExpr; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::BinaryExpr; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::ShiftExpr; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
};

class ComparisonExpression : public Expression {
  public:
    ComparisonExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right), type(jot_int1_ty)
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::ComparisonExpr; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
};

class LogicalExpression : public Expression {
  public:
    LogicalExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(left), operator_token(token), right(right), type(jot_int1_ty)
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<Expression> get_left() { return left; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return left->is_constant() and right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::LogicalExpr; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
};

class PrefixUnaryExpression : public Expression {
  public:
    PrefixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(token), right(right), type(right->get_type_node())
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::PrefixUnaryExpr; }

    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
};

class PostfixUnaryExpression : public Expression {
  public:
    PostfixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(token), right(right), type(right->get_type_node())
    {
    }

    Token get_operator_token() { return operator_token; }

    Shared<Expression> get_right() { return right; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return right->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::PostfixUnaryExpr; }

    Token operator_token;
    Shared<Expression> right;
    Shared<JotType> type;
};

class CallExpression : public Expression {
  public:
    CallExpression(Token position, Shared<Expression> callee,
                   std::vector<Shared<Expression>> arguments,
                   std::vector<Shared<JotType>> generic_arguments = {})
        : position(position), callee(callee), arguments(arguments), type(callee->get_type_node()),
          generic_arguments(generic_arguments)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_callee() { return callee; }

    std::vector<Shared<Expression>> get_arguments() { return arguments; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return false; }

    AstNodeType get_ast_node_type() override { return AstNodeType::CallExpr; }

    Token position;
    Shared<Expression> callee;
    std::vector<Shared<Expression>> arguments;
    Shared<JotType> type;
    std::vector<Shared<JotType>> generic_arguments;
};

class InitializeExpression : public Expression {
  public:
    InitializeExpression(Token position, Shared<JotType> type,
                         std::vector<Shared<Expression>> arguments)
        : position(std::move(position)), type(std::move(type)), arguments(std::move(arguments))
    {
    }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

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

    AstNodeType get_ast_node_type() override { return AstNodeType::InitExpr; }

    Token position;
    Shared<JotType> type;
    std::vector<Shared<Expression>> arguments;
};

class LambdaExpression : public Expression {
  public:
    LambdaExpression(Token position, std::vector<Shared<Parameter>> parameters,
                     Shared<JotType> return_type, Shared<BlockStatement> body)
        : position(std::move(position)), explicit_parameters(std::move(parameters)),
          return_type(return_type), body(std::move(body))
    {
        std::vector<Shared<JotType>> parameters_types;
        parameters_types.reserve(explicit_parameters.size());
        for (auto& parameter : explicit_parameters) {
            parameters_types.push_back(parameter->type);
        }
        auto function_type =
            std::make_shared<JotFunctionType>(position, parameters_types, return_type);
        lambda_type = std::make_shared<JotPointerType>(function_type);
    }

    Shared<JotType> get_type_node() override { return lambda_type; }

    void set_type_node(Shared<JotType> new_type) override { lambda_type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::LambdaExpr; }

    Token position;
    std::vector<Shared<Parameter>> explicit_parameters;
    std::vector<std::string> implict_parameters_names;
    std::vector<Shared<JotType>> implict_parameters_types;
    Shared<JotType> return_type;
    Shared<BlockStatement> body;
    Shared<JotType> lambda_type;
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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    AstNodeType get_ast_node_type() override { return AstNodeType::DotExpr; }

    bool is_constant() override { return is_constants_; }

    int field_index = 0;
    bool is_constants_ = false;

    Token dot_token;
    Shared<Expression> callee;
    Token field_name;
    Shared<JotType> type;
};

class CastExpression : public Expression {
  public:
    CastExpression(Token position, Shared<JotType> type, Shared<Expression> value)
        : position(position), type(type), value(value)
    {
    }

    Token get_position() { return position; }

    Shared<Expression> get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return value->is_constant(); }

    AstNodeType get_ast_node_type() override { return AstNodeType::CastExpr; }

    Token position;
    Shared<JotType> type;
    Shared<Expression> value;
};

class TypeSizeExpression : public Expression {
  public:
    TypeSizeExpression(Token position, Shared<JotType> type)
        : position(position), type(type), node_type(jot_int64_ty)
    {
    }

    Token get_position() { return position; }

    Shared<JotType> get_type_node() override { return node_type; }

    void set_type_node(Shared<JotType> new_type) override { node_type = new_type; }

    Shared<JotType> get_type() { return type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::TypeSizeExpr; }

    Token position;
    Shared<JotType> type;
    Shared<JotType> node_type;
};

class ValueSizeExpression : public Expression {
  public:
    ValueSizeExpression(Token position, Shared<Expression> value)
        : position(position), value(value), node_type(jot_int64_ty)
    {
    }

    Token get_position() { return position; }

    Shared<JotType> get_type_node() override { return node_type; }

    void set_type_node(Shared<JotType> new_type) override { node_type = new_type; }

    Shared<Expression> get_value() { return value; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::ValueSizeExpr; }

    Token position;
    Shared<Expression> value;
    Shared<JotType> node_type;
};

class IndexExpression : public Expression {
  public:
    IndexExpression(Token position, Shared<Expression> value, Shared<Expression> index)
        : position(position), value(value), index(index)
    {
        type = std::make_shared<JotNoneType>();
    }

    Token get_position() { return position; }

    Shared<Expression> get_value() { return value; }

    Shared<Expression> get_index() { return index; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override
    {
        // TODO: Should be calculated depend if value and index are constants
        return index->is_constant();
    }

    AstNodeType get_ast_node_type() override { return AstNodeType::IndexExpr; }

    Token position;
    Shared<Expression> value;
    Shared<Expression> index;
    Shared<JotType> type;
};

class EnumAccessExpression : public Expression {
  public:
    EnumAccessExpression(Token enum_name, Token element_name, int index,
                         Shared<JotType> element_type)
        : enum_name(enum_name), element_name(element_name), index(index), element_type(element_type)
    {
    }

    Token get_enum_name() { return enum_name; }

    int get_enum_element_index() { return index; }

    Shared<JotType> get_type_node() override { return element_type; }

    void set_type_node(Shared<JotType> new_type) override { element_type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::EnumElementExpr; }

    Token enum_name;
    Token element_name;
    int index;
    Shared<JotType> element_type;
};

class ArrayExpression : public Expression {
  public:
    ArrayExpression(Token position, std::vector<Shared<Expression>> values)
        : position(position), values(values)
    {
        auto size = values.size();
        auto element_type = size == 0 ? jot_none_ty : values[0]->get_type_node();
        type = std::make_shared<JotArrayType>(element_type, size);

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

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return is_constants_array; }

    AstNodeType get_ast_node_type() override { return AstNodeType::ArrayExpr; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<JotType> type;
    bool is_constants_array = true;
};

class StringExpression : public Expression {
  public:
    StringExpression(Token value) : value(value), type(jot_int8ptr_ty) {}

    Token get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::StringExpr; }

    Token value;
    Shared<JotType> type;
};

class LiteralExpression : public Expression {
  public:
    LiteralExpression(Token name, Shared<JotType> type) : name(name), type(type) {}

    Token get_name() { return name; }

    void set_type(Shared<JotType> resolved_type) { type = resolved_type; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    void set_constant(bool is_constants) { constants = is_constants; }

    bool is_constant() override { return constants; }

    AstNodeType get_ast_node_type() override { return AstNodeType::LiteralExpr; }

    Token name;
    Shared<JotType> type;
    bool constants = false;
};

class NumberExpression : public Expression {
  public:
    NumberExpression(Token value, Shared<JotType> type) : value(value), type(type) {}

    Token get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::NumberExpr; }

    Token value;
    Shared<JotType> type;
};

class CharacterExpression : public Expression {
  public:
    CharacterExpression(Token value) : value(value), type(jot_int8_ty) {}

    Token get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::CharExpr; }

    Token value;
    Shared<JotType> type;
};

class BooleanExpression : public Expression {
  public:
    BooleanExpression(Token value) : value(value), type(jot_int1_ty) {}

    Token get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::BoolExpr; }

    Token value;
    Shared<JotType> type;
};

class NullExpression : public Expression {
  public:
    NullExpression(Token value) : value(value), type(jot_null_ty), null_base_type(jot_int32ptr_ty)
    {
    }

    Token get_value() { return value; }

    Shared<JotType> get_type_node() override { return type; }

    void set_type_node(Shared<JotType> new_type) override { type = new_type; }

    std::any accept(ExpressionVisitor* visitor) override { return visitor->visit(this); }

    bool is_constant() override { return true; }

    AstNodeType get_ast_node_type() override { return AstNodeType::NullExpr; }

    Token value;
    Shared<JotType> type;

  public:
    Shared<JotType> null_base_type;
};
