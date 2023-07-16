#pragma once

#include "amun_ast_visitor.hpp"
#include "amun_primitives.hpp"
#include "amun_token.hpp"

#include <any>
#include <memory>
#include <utility>
#include <vector>

enum class AstNodeType {
    AST_NODE,

    // Statements
    AST_BLOCK,
    AST_FIELD_DECLARAION,
    AST_DESTRUCTURING_DECLARAION,
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
    AST_BITWISE,
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
    AST_VECTOR,
    AST_STRING,
    AST_LITERAL,
    AST_NUMBER,
    AST_CHARACTER,
    AST_BOOL,
    AST_NULL,
    AST_UNDEFINED,
};

struct AstNode {
    virtual auto get_ast_node_type() -> AstNodeType = 0;
};

class Statement : public AstNode {
  public:
    virtual auto accept(StatementVisitor* visitor) -> std::any = 0;
    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_NODE; }
};

class Expression : public AstNode {
  public:
    virtual auto get_type_node() -> Shared<amun::Type> = 0;
    virtual auto set_type_node(Shared<amun::Type> new_type) -> void = 0;
    virtual auto accept(ExpressionVisitor* visitor) -> std::any = 0;
    virtual auto is_constant() -> bool = 0;
    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_NODE; }
};

struct CompilationUnit {
    explicit CompilationUnit(std::vector<Shared<Statement>> nodes) : tree_nodes(std::move(nodes)) {}
    std::vector<Shared<Statement>> tree_nodes;
};

class BlockStatement : public Statement {
  public:
    explicit BlockStatement(std::vector<Shared<Statement>> nodes) : statements(std::move(nodes)) {}

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_BLOCK; }

    std::vector<Shared<Statement>> statements;
};

struct Parameter {
    Parameter(Token name, Shared<amun::Type> type) : name(std::move(name)), type(std::move(type)) {}
    Token name;
    Shared<amun::Type> type;
};

class FieldDeclaration : public Statement {
  public:
    FieldDeclaration(Token name, Shared<amun::Type> type, Shared<Expression> value, bool global)
        : name(std::move(name)), type(std::move(type)), value(std::move(value)), is_global(global)
    {
        has_explicit_type = this->type->type_kind != amun::TypeKind::NONE;
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FIELD_DECLARAION; }

    Token name;
    Shared<amun::Type> type;
    Shared<Expression> value;
    bool is_global;
    bool has_explicit_type;
};

class DestructuringDeclaraion : public Statement {
  public:
    DestructuringDeclaraion(std::vector<Token> names, std::vector<Shared<amun::Type>> types,
                            Shared<Expression> value, Token equal_token, bool is_global)
        : names(std::move(names)), types(std::move(types)), value(std::move(value)),
          equal_token(std::move(equal_token)), is_global(is_global)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override
    {
        return AstNodeType::AST_DESTRUCTURING_DECLARAION;
    }

    std::vector<Token> names;
    std::vector<Shared<amun::Type>> types;
    Shared<Expression> value;
    Token equal_token;
    bool is_global;
};

class ConstDeclaration : public Statement {
  public:
    ConstDeclaration(Token name, Shared<Expression> value)
        : name(std::move(name)), value(std::move(value))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FIELD_DECLARAION; }

    Token name;
    Shared<Expression> value;
};

class FunctionPrototype : public Statement {
  public:
    FunctionPrototype(Token name, std::vector<Shared<Parameter>> parameters,
                      Shared<amun::Type> return_type, bool external = false, bool varargs = false,
                      Shared<amun::Type> varargs_type = {}, bool is_generic = false,
                      std::vector<std::string> generic_parameters = {})
        : name(std::move(name)), parameters(std::move(parameters)),
          return_type(std::move(return_type)), is_external(external), has_varargs(varargs),
          varargs_type(std::move(varargs_type)), is_generic(is_generic),
          generic_parameters(std::move(generic_parameters))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_PROTOTYPE; }

    Token name;
    std::vector<Shared<Parameter>> parameters;
    Shared<amun::Type> return_type;
    bool is_external;

    bool has_varargs;
    Shared<amun::Type> varargs_type;

    bool is_generic;
    std::vector<std::string> generic_parameters;
};

class IntrinsicPrototype : public Statement {
  public:
    IntrinsicPrototype(Token name, std::string native_name,
                       std::vector<Shared<Parameter>> parameters, Shared<amun::Type> return_type,
                       bool varargs, Shared<amun::Type> varargs_type)
        : name(std::move(name)), native_name(std::move(native_name)),
          parameters(std::move(parameters)), return_type(std::move(return_type)), varargs(varargs),
          varargs_type(std::move(varargs_type))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_INTRINSIC; }

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
        : prototype(std::move(prototype)), body(std::move(body))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FUNCTION; }

    Shared<FunctionPrototype> prototype;
    Shared<Statement> body;
};

class OperatorFunctionDeclaraion : public Statement {
  public:
    OperatorFunctionDeclaraion(Token op, Shared<FunctionDeclaration> function)
        : op(std::move(op)), function(std::move(function))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_OPERATOR_FUNCTION; }

    Token op;
    Shared<FunctionDeclaration> function;
};

class StructDeclaration : public Statement {
  public:
    explicit StructDeclaration(Shared<amun::StructType> type) : struct_type(std::move(type)) {}

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_STRUCT; }

    Shared<amun::StructType> struct_type;
};

class EnumDeclaration : public Statement {
  public:
    EnumDeclaration(Token name, Shared<amun::EnumType> type)
        : name(std::move(name)), enum_type(type)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_ENUM; }

    Token name;
    Shared<amun::Type> enum_type;
};

class ConditionalBlock {
  public:
    ConditionalBlock(Token position, Shared<Expression> condition, Shared<Statement> body)
        : position(std::move(position)), condition(std::move(condition)), body(std::move(body))
    {
    }

    Token position;
    Shared<Expression> condition;
    Shared<Statement> body;
};

class IfStatement : public Statement {
  public:
    IfStatement(std::vector<Shared<ConditionalBlock>> conditional_blocks, bool has_else)
        : conditional_blocks(std::move(conditional_blocks)), has_else(has_else)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_IF_STATEMENT; }

    std::vector<Shared<ConditionalBlock>> conditional_blocks;
    bool has_else;
};

class ForRangeStatement : public Statement {
  public:
    ForRangeStatement(Token position, std::string element_name, Shared<Expression> range_start,
                      Shared<Expression> range_end, Shared<Expression> step, Shared<Statement> body)
        : position(std::move(position)), element_name(std::move(element_name)),
          range_start(std::move(range_start)), range_end(std::move(range_end)),
          step(std::move(step)), body(std::move(body))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FOR_RANGE; }

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

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FOR_RANGE; }

    Token position;
    std::string element_name;
    std::string index_name;
    Shared<Expression> collection;
    Shared<Statement> body;
};

class ForeverStatement : public Statement {
  public:
    ForeverStatement(Token position, Shared<Statement> body)
        : position(std::move(position)), body(std::move(body))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_FOR_EVER; }

    Token position;
    Shared<Statement> body;
};

class WhileStatement : public Statement {
  public:
    WhileStatement(Token position, Shared<Expression> condition, Shared<Statement> body)
        : keyword(std::move(position)), condition(std::move(condition)), body(std::move(body))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_WHILE; }

    Token keyword;
    Shared<Expression> condition;
    Shared<Statement> body;
};

class SwitchCase {
  public:
    SwitchCase(Token position, std::vector<Shared<Expression>> values, Shared<Statement> body)
        : position(std::move(position)), values(std::move(values)), body(std::move(body))
    {
    }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<Statement> body;
};

class SwitchStatement : public Statement {
  public:
    SwitchStatement(Token position, Shared<Expression> argument,
                    std::vector<Shared<SwitchCase>> cases, Shared<SwitchCase> default_case)
        : keyword(std::move(position)), argument(std::move(argument)), cases(std::move(cases)),
          default_case(std::move(default_case))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_SWITCH_STATEMENT; }

    Token keyword;
    Shared<Expression> argument;
    std::vector<Shared<SwitchCase>> cases;
    Shared<SwitchCase> default_case;

    bool should_perform_complete_check = false;
};

class ReturnStatement : public Statement {
  public:
    ReturnStatement(Token position, Shared<Expression> value, bool contain_value)
        : keyword(std::move(position)), value(std::move(value)), has_value(contain_value)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_RETURN; }

    Token keyword;
    Shared<Expression> value;
    bool has_value;
};

class DeferStatement : public Statement {
  public:
    explicit DeferStatement(Shared<CallExpression> call) : call_expression(std::move(call)) {}

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_DEFER; }

    Shared<CallExpression> call_expression;
};

class BreakStatement : public Statement {
  public:
    BreakStatement(Token token, bool has_times, int times)
        : keyword(std::move(token)), has_times(has_times), times(times)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_BREAK; }

    Token keyword;
    bool has_times;
    int times;
};

class ContinueStatement : public Statement {
  public:
    ContinueStatement(Token token, bool has_times, int times)
        : keyword(std::move(token)), has_times(has_times), times(times)
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_CONTINUE; }

    Token keyword;
    bool has_times;
    int times;
};

class ExpressionStatement : public Statement {
  public:
    explicit ExpressionStatement(Shared<Expression> expression) : expression(std::move(expression))
    {
    }

    auto accept(StatementVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override
    {
        return AstNodeType::AST_EXPRESSION_STATEMENT;
    }

    Shared<Expression> expression;
};

class IfExpression : public Expression {
  public:
    IfExpression(Token if_token, Token else_token, Shared<Expression> condition,
                 Shared<Expression> if_expression, Shared<Expression> else_expression)
        : if_token(std::move(if_token)), else_token(std::move(else_token)),
          condition(std::move(condition)), if_expression(if_expression),
          else_expression(std::move(else_expression))
    {
        type = if_expression->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override
    {
        return condition->is_constant() && if_expression->is_constant() &&
               else_expression->is_constant();
    }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_IF_EXPRESSION; }

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
        : keyword(std::move(switch_token)), argument(std::move(argument)),
          switch_cases(std::move(switch_cases)), switch_cases_values(switch_cases_values),
          default_value(std::move(default_value))
    {
        type = switch_cases_values[0]->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override
    {
        if (argument->is_constant()) {
            for (auto& switch_case : switch_cases) {
                if (!switch_case->is_constant()) {
                    return false;
                }
            }

            for (auto& switch_value : switch_cases_values) {
                if (!switch_value->is_constant()) {
                    return false;
                }
            }
            return true;
        }
        return false;
    }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_SWITCH_EXPRESSION; }

    Token keyword;
    Shared<Expression> argument;
    std::vector<Shared<Expression>> switch_cases;
    std::vector<Shared<Expression>> switch_cases_values;
    Shared<Expression> default_value;
    Shared<amun::Type> type;
};

class GroupExpression : public Expression {
  public:
    explicit GroupExpression(Shared<Expression> expression) : expression(expression)
    {
        type = expression->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return expression->get_type_node(); }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return expression->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_GROUP; }

    Shared<Expression> expression;
    Shared<amun::Type> type;
};

class TupleExpression : public Expression {
  public:
    TupleExpression(Token position, std::vector<Shared<Expression>> values)
        : position(std::move(position)), values(std::move(values))
    {
        type = amun::none_type;
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_TUPLE; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<amun::Type> type;
};

class AssignExpression : public Expression {
  public:
    AssignExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(std::move(left)), operator_token(std::move(token)), right(right)
    {
        type = right->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return false; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_ASSIGN; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class BinaryExpression : public Expression {
  public:
    BinaryExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(std::move(left)), operator_token(std::move(token)), right(right)
    {
        type = right->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return left->is_constant() and right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_BINARY; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class BitwiseExpression : public Expression {
  public:
    BitwiseExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(std::move(left)), operator_token(std::move(token)), right(right)
    {
        type = right->get_type_node();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return left->is_constant() and right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_BITWISE; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class ComparisonExpression : public Expression {
  public:
    ComparisonExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(std::move(left)), operator_token(std::move(token)), right(std::move(right))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return left->is_constant() and right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_COMPARISON; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type = amun::i1_type;
};

class LogicalExpression : public Expression {
  public:
    LogicalExpression(Shared<Expression> left, Token token, Shared<Expression> right)
        : left(std::move(left)), operator_token(token), right(right)
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return left->is_constant() and right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_LOGICAL; }

    Shared<Expression> left;
    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type = amun::i1_type;
};

class PrefixUnaryExpression : public Expression {
  public:
    PrefixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(std::move(token)), right(right), type(right->get_type_node())
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_PREFIX_UNARY; }

    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class PostfixUnaryExpression : public Expression {
  public:
    PostfixUnaryExpression(Token token, Shared<Expression> right)
        : operator_token(std::move(token)), right(right), type(right->get_type_node())
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return right->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_POSTFIX_UNARY; }

    Token operator_token;
    Shared<Expression> right;
    Shared<amun::Type> type;
};

class CallExpression : public Expression {
  public:
    CallExpression(Token position, Shared<Expression> callee,
                   std::vector<Shared<Expression>> arguments,
                   std::vector<Shared<amun::Type>> generic_arguments = {})
        : position(std::move(position)), callee(callee), arguments(std::move(arguments)),
          type(callee->get_type_node()), generic_arguments(std::move(generic_arguments))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return false; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_CALL; }

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

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override
    {
        for (const auto& argument : arguments) {
            if (!argument->is_constant()) {
                return false;
            }
        }
        return true;
    }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_INIT; }

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
        auto function_type =
            std::make_shared<amun::FunctionType>(position, parameters_types, return_type);

        lambda_type = std::make_shared<amun::PointerType>(function_type);
    }

    auto get_type_node() -> Shared<amun::Type> override { return lambda_type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { lambda_type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_LAMBDA; }

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
        : dot_token(std::move(dot_token)), callee(std::move(callee)),
          field_name(std::move(field_name))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_DOT; }

    auto is_constant() -> bool override { return is_constants_; }

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
        : position(std::move(position)), type(std::move(type)), value(std::move(value))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return value->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_CAST; }

    Token position;
    Shared<amun::Type> type;
    Shared<Expression> value;
};

class TypeSizeExpression : public Expression {
  public:
    explicit TypeSizeExpression(Shared<amun::Type> type) : type(std::move(type)) {}

    auto get_type_node() -> Shared<amun::Type> override { return amun::i64_type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override {}

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_TYPE_SIZE; }

    Shared<amun::Type> type;
};

class TypeAlignExpression : public Expression {
  public:
    explicit TypeAlignExpression(Shared<amun::Type> type) : type(std::move(type)) {}

    auto get_type_node() -> Shared<amun::Type> override { return amun::i64_type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override {}

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_TYPE_SIZE; }

    Shared<amun::Type> type;
};

class ValueSizeExpression : public Expression {
  public:
    explicit ValueSizeExpression(Shared<Expression> value) : value(std::move(value)) {}

    auto get_type_node() -> Shared<amun::Type> override { return amun::i64_type; }

    void set_type_node(Shared<amun::Type> new_type) override {}

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_VALUE_SIZE; }

    Shared<Expression> value;
};

class IndexExpression : public Expression {
  public:
    IndexExpression(Token position, Shared<Expression> value, Shared<Expression> index)
        : position(std::move(position)), value(std::move(value)), index(index)
    {
        type = std::make_shared<amun::NoneType>();
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return index->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_INDEX; }

    Token position;
    Shared<Expression> value;
    Shared<Expression> index;
    Shared<amun::Type> type;
};

class EnumAccessExpression : public Expression {
  public:
    EnumAccessExpression(Token enum_name, Token element_name, int index,
                         Shared<amun::Type> element_type)
        : enum_name(std::move(enum_name)), element_name(std::move(element_name)),
          enum_element_index(index), element_type(std::move(element_type))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return element_type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { element_type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_ENUM_ELEMENT; }

    Token enum_name;
    Token element_name;
    int enum_element_index;
    Shared<amun::Type> element_type;
};

class ArrayExpression : public Expression {
  public:
    ArrayExpression(Token position, std::vector<Shared<Expression>> values)
        : position(std::move(position)), values(values)
    {
        auto size = values.size();
        auto element_type = size == 0 ? amun::none_type : values[0]->get_type_node();
        type = std::make_shared<amun::StaticArrayType>(element_type, size);

        // Check if all values of array are constant or not
        for (auto& value : values) {
            if (!value->is_constant()) {
                is_constants_array = false;
                break;
            }
        }
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return is_constants_array; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_ARRAY; }

    Token position;
    std::vector<Shared<Expression>> values;
    Shared<amun::Type> type;
    bool is_constants_array = true;
};

class VectorExpression : public Expression {
  public:
    explicit VectorExpression(Shared<ArrayExpression> array) : array(std::move(array)) {}

    auto get_type_node() -> Shared<amun::Type> override
    {
        auto type = array->get_type_node();
        auto array_type = std::static_pointer_cast<amun::StaticArrayType>(type);
        return std::make_shared<amun::StaticVectorType>(array_type);
    }

    auto set_type_node(Shared<amun::Type> type) -> void override { array->set_type_node(type); }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return array->is_constant(); }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_VECTOR; }

    Shared<ArrayExpression> array;
};

class StringExpression : public Expression {
  public:
    explicit StringExpression(Token value) : value(std::move(value)) {}

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_STRING; }

    Token value;
    Shared<amun::Type> type = amun::i8_ptr_type;
};

class LiteralExpression : public Expression {
  public:
    explicit LiteralExpression(Token name) : name(std::move(name)) {}

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    void set_type_node(Shared<amun::Type> new_type) override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    void set_constant(bool is_constants) { constants = is_constants; }

    auto is_constant() -> bool override { return constants; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_LITERAL; }

    Token name;
    Shared<amun::Type> type = amun::none_type;
    bool constants = false;
};

class NumberExpression : public Expression {
  public:
    NumberExpression(Token value, Shared<amun::Type> type)
        : value(std::move(value)), type(std::move(type))
    {
    }

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_NUMBER; }

    Token value;
    Shared<amun::Type> type;
};

class CharacterExpression : public Expression {
  public:
    explicit CharacterExpression(Token value) : value(std::move(value)) {}

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_CHARACTER; }

    Token value;
    Shared<amun::Type> type = amun::i8_type;
};

class BooleanExpression : public Expression {
  public:
    explicit BooleanExpression(Token value) : value(std::move(value)) {}

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_BOOL; }

    Token value;
    Shared<amun::Type> type = amun::i1_type;
};

class NullExpression : public Expression {
  public:
    explicit NullExpression(Token value) : value(std::move(value)) {}

    auto get_type_node() -> Shared<amun::Type> override { return type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_NULL; }

    Token value;
    Shared<amun::Type> type = amun::null_type;
    Shared<amun::Type> null_base_type = amun::i32_ptr_type;
};

class UndefinedExpression : public Expression {
  public:
    explicit UndefinedExpression(Token keyword) : keyword(std::move(keyword)) {}

    auto get_type_node() -> Shared<amun::Type> override { return base_type; }

    auto set_type_node(Shared<amun::Type> new_type) -> void override { base_type = new_type; }

    auto accept(ExpressionVisitor* visitor) -> std::any override { return visitor->visit(this); }

    auto is_constant() -> bool override { return true; }

    auto get_ast_node_type() -> AstNodeType override { return AstNodeType::AST_UNDEFINED; }

    Token keyword;
    Shared<amun::Type> base_type = amun::none_type;
};