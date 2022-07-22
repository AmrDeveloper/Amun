#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_symboltable.hpp"

#include <unordered_map>

class JotTypeChecker : public TreeVisitor {
  public:
    void check_compilation_unit(std::shared_ptr<CompilationUnit> compilation_unit);

    std::any visit(BlockStatement *node) override;

    std::any visit(FieldDeclaration *node) override;

    std::any visit(FunctionPrototype *node) override;

    std::any visit(FunctionDeclaration *node) override;

    std::any visit(WhileStatement *node) override;

    std::any visit(ReturnStatement *node) override;

    std::any visit(ExpressionStatement *node) override;

    std::any visit(GroupExpression *node) override;

    std::any visit(BinaryExpression *node) override;

    std::any visit(UnaryExpression *node) override;

    std::any visit(CallExpression *node) override;

    std::any visit(LiteralExpression *node) override;

    std::any visit(NumberExpression *node) override;

    std::any visit(CharacterExpression *node) override;

    std::any visit(BooleanExpression *node) override;

    std::any visit(NullExpression *node) override;

    bool is_number_type(std::shared_ptr<JotType> type);

    bool is_pointer_type(std::shared_ptr<JotType> type);

    bool is_same_type(std::shared_ptr<JotType> left, std::shared_ptr<JotType> right);

  private:
    JotSymbolTable symbol_table;
};