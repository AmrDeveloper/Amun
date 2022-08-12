#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_context.hpp"
#include "jot_symboltable.hpp"

#include <memory>
#include <unordered_map>

class JotTypeChecker : public TreeVisitor {
  public:
    JotTypeChecker(std::shared_ptr<JotContext> context) : context(context) {
        global_scope = std::make_shared<JotSymbolTable>();
        symbol_table = global_scope;
    }

    void check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit);

    std::any visit(BlockStatement *node) override;

    std::any visit(FieldDeclaration *node) override;

    std::any visit(FunctionPrototype *node) override;

    std::any visit(FunctionDeclaration *node) override;

    std::any visit(EnumDeclaration *node) override;

    std::any visit(IfStatement *node) override;

    std::any visit(WhileStatement *node) override;

    std::any visit(ReturnStatement *node) override;

    std::any visit(ExpressionStatement *node) override;

    std::any visit(IfExpression *node) override;

    std::any visit(GroupExpression *node) override;

    std::any visit(AssignExpression *node) override;

    std::any visit(BinaryExpression *node) override;

    std::any visit(ComparisonExpression *node) override;

    std::any visit(LogicalExpression *node) override;

    std::any visit(UnaryExpression *node) override;

    std::any visit(CallExpression *node) override;

    std::any visit(LiteralExpression *node) override;

    std::any visit(NumberExpression *node) override;

    std::any visit(StringExpression *node) override;

    std::any visit(CharacterExpression *node) override;

    std::any visit(BooleanExpression *node) override;

    std::any visit(NullExpression *node) override;

    std::shared_ptr<JotType> node_jot_type(std::any any_type);

    bool is_number_type(const std::shared_ptr<JotType> &);

    bool is_pointer_type(const std::shared_ptr<JotType> &);

    bool is_same_type(const std::shared_ptr<JotType> &, const std::shared_ptr<JotType> &);

    void push_new_scope();

    void pop_current_scope();

  private:
    std::shared_ptr<JotContext> context;
    std::shared_ptr<JotSymbolTable> global_scope;
    std::shared_ptr<JotSymbolTable> symbol_table;
};