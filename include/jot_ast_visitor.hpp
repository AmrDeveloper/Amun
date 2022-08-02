#pragma once

#include <any>
#include <memory>

class BlockStatement;
class FieldDeclaration;
class ExternalPrototype;
class FunctionPrototype;
class FunctionDeclaration;
class EnumDeclaration;
class IfStatement;
class WhileStatement;
class ReturnStatement;
class ExpressionStatement;

class StatementVisitor {
  public:
    virtual std::any visit(BlockStatement *node) = 0;

    virtual std::any visit(FieldDeclaration *node) = 0;

    virtual std::any visit(ExternalPrototype *node) = 0;

    virtual std::any visit(FunctionPrototype *node) = 0;

    virtual std::any visit(FunctionDeclaration *node) = 0;

    virtual std::any visit(EnumDeclaration *node) = 0;

    virtual std::any visit(IfStatement *node) = 0;

    virtual std::any visit(WhileStatement *node) = 0;

    virtual std::any visit(ReturnStatement *node) = 0;

    virtual std::any visit(ExpressionStatement *node) = 0;
};

class IfExpression;
class GroupExpression;
class AssignExpression;
class BinaryExpression;
class ComparisonExpression;
class LogicalExpression;
class UnaryExpression;
class CallExpression;
class LiteralExpression;
class NumberExpression;
class StringExpression;
class CharacterExpression;
class BooleanExpression;
class NullExpression;

class ExpressionVisitor {
  public:
    virtual std::any visit(IfExpression *node) = 0;

    virtual std::any visit(GroupExpression *node) = 0;

    virtual std::any visit(AssignExpression *node) = 0;

    virtual std::any visit(BinaryExpression *node) = 0;

    virtual std::any visit(ComparisonExpression *node) = 0;

    virtual std::any visit(LogicalExpression *node) = 0;

    virtual std::any visit(UnaryExpression *node) = 0;

    virtual std::any visit(CallExpression *node) = 0;

    virtual std::any visit(LiteralExpression *node) = 0;

    virtual std::any visit(NumberExpression *node) = 0;

    virtual std::any visit(StringExpression *node) = 0;

    virtual std::any visit(CharacterExpression *node) = 0;

    virtual std::any visit(BooleanExpression *node) = 0;

    virtual std::any visit(NullExpression *node) = 0;
};

class TreeVisitor : public StatementVisitor, public ExpressionVisitor {};