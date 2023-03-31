#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_basic.hpp"
#include "jot_context.hpp"
#include "jot_scoped_map.hpp"
#include "jot_type.hpp"

#include <memory>
#include <stack>
#include <unordered_map>
#include <utility>

class JotTypeChecker : public TreeVisitor {
  public:
    explicit JotTypeChecker(Shared<JotContext> context) : context(std::move(context))
    {
        types_table.push_new_scope();
    }

    auto check_compilation_unit(Shared<CompilationUnit> compilation_unit) -> void;

    auto visit(BlockStatement* node) -> std::any override;

    auto visit(FieldDeclaration* node) -> std::any override;

    auto visit(FunctionPrototype* node) -> std::any override;

    auto visit(IntrinsicPrototype* node) -> std::any override;

    auto visit(FunctionDeclaration* node) -> std::any override;

    auto visit(StructDeclaration* node) -> std::any override;

    auto visit(EnumDeclaration* node) -> std::any override;

    auto visit(IfStatement* node) -> std::any override;

    auto visit(ForRangeStatement* node) -> std::any override;

    auto visit(ForEachStatement* node) -> std::any override;

    auto visit(ForeverStatement* node) -> std::any override;

    auto visit(WhileStatement* node) -> std::any override;

    auto visit(SwitchStatement* node) -> std::any override;

    auto visit(ReturnStatement* node) -> std::any override;

    auto visit(DeferStatement* node) -> std::any override;

    auto visit(BreakStatement* node) -> std::any override;

    auto visit(ContinueStatement* node) -> std::any override;

    auto visit(ExpressionStatement* node) -> std::any override;

    auto visit(IfExpression* node) -> std::any override;

    auto visit(SwitchExpression* node) -> std::any override;

    auto visit(GroupExpression* node) -> std::any override;

    auto visit(AssignExpression* node) -> std::any override;

    auto visit(BinaryExpression* node) -> std::any override;

    auto visit(ShiftExpression* node) -> std::any override;

    auto visit(ComparisonExpression* node) -> std::any override;

    auto visit(LogicalExpression* node) -> std::any override;

    auto visit(PrefixUnaryExpression* node) -> std::any override;

    auto visit(PostfixUnaryExpression* node) -> std::any override;

    auto visit(CallExpression* node) -> std::any override;

    auto visit(InitializeExpression* node) -> std::any override;

    auto visit(LambdaExpression* node) -> std::any override;

    auto visit(DotExpression* node) -> std::any override;

    auto visit(CastExpression* node) -> std::any override;

    auto visit(TypeSizeExpression* node) -> std::any override;

    auto visit(ValueSizeExpression* node) -> std::any override;

    auto visit(IndexExpression* node) -> std::any override;

    auto visit(EnumAccessExpression* node) -> std::any override;

    auto visit(LiteralExpression* node) -> std::any override;

    auto visit(NumberExpression* node) -> std::any override;

    auto visit(ArrayExpression* node) -> std::any override;

    auto visit(StringExpression* node) -> std::any override;

    auto visit(CharacterExpression* node) -> std::any override;

    auto visit(BooleanExpression* node) -> std::any override;

    auto visit(NullExpression* node) -> std::any override;

    auto node_jot_type(std::any any_type) -> Shared<JotType>;

    auto is_same_type(const Shared<JotType>& left, const Shared<JotType>& right) -> bool;

    auto check_number_limits(const char* literal, NumberKind kind) -> bool;

    auto resolve_generic_struct(Shared<JotType> type) -> Shared<JotType>;

    auto check_missing_return_statement(Shared<Statement> node) -> bool;

    auto check_parameters_types(TokenSpan location, std::vector<Shared<Expression>>& arguments,
                                std::vector<Shared<JotType>>& parameters, bool has_varargs,
                                Shared<JotType> varargs_type, int implicit_parameters_count)
        -> void;

    auto check_valid_assignment_right_side(Shared<Expression> node, TokenSpan position) -> void;

    auto push_new_scope() -> void;

    auto pop_current_scope() -> void;

  private:
    Shared<JotContext> context;
    JotScopedMap<std::string, std::any> types_table;

    // Generic function declaraions and parameters
    std::unordered_map<std::string, FunctionDeclaration*> generic_functions_declaraions;
    std::unordered_map<std::string, Shared<JotType>> generic_types;

    // Used to track the return types of functions and inner lambda expression
    std::stack<Shared<JotType>> return_types_stack;

    // Flag that tell us when we are inside lambda expression body
    bool is_inside_lambda_body;
    std::stack<std::vector<std::pair<std::string, Shared<JotType>>>> lambda_implicit_parameters;
};