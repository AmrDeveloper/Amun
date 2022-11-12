#pragma once

#include <any>
#include <memory>

class BlockStatement;
class FieldDeclaration;
class FunctionPrototype;
class FunctionDeclaration;
class StructDeclaration;
class EnumDeclaration;
class IfStatement;
class WhileStatement;
class SwitchStatement;
class ReturnStatement;
class DeferStatement;
class BreakStatement;
class ContinueStatement;
class ExpressionStatement;

class StatementVisitor {
  public:
    virtual std::any visit(BlockStatement *node) = 0;

    virtual std::any visit(FieldDeclaration *node) = 0;

    virtual std::any visit(FunctionPrototype *node) = 0;

    virtual std::any visit(FunctionDeclaration *node) = 0;

    virtual std::any visit(StructDeclaration *node) = 0;

    virtual std::any visit(EnumDeclaration *node) = 0;

    virtual std::any visit(IfStatement *node) = 0;

    virtual std::any visit(WhileStatement *node) = 0;

    virtual std::any visit(SwitchStatement *node) = 0;

    virtual std::any visit(ReturnStatement *node) = 0;

    virtual std::any visit(DeferStatement *node) = 0;

    virtual std::any visit(BreakStatement *node) = 0;

    virtual std::any visit(ContinueStatement *node) = 0;

    virtual std::any visit(ExpressionStatement *node) = 0;
};

class IfExpression;
class SwitchExpression;
class GroupExpression;
class AssignExpression;
class BinaryExpression;
class ShiftExpression;
class ComparisonExpression;
class LogicalExpression;
class PrefixUnaryExpression;
class PostfixUnaryExpression;
class CallExpression;
class DotExpression;
class CastExpression;
class TypeSizeExpression;
class ValueSizeExpression;
class IndexExpression;
class EnumAccessExpression;
class LiteralExpression;
class NumberExpression;
class ArrayExpression;
class StringExpression;
class CharacterExpression;
class BooleanExpression;
class NullExpression;

class ExpressionVisitor {
  public:
    virtual std::any visit(IfExpression *node) = 0;

    virtual std::any visit(SwitchExpression *node) = 0;

    virtual std::any visit(GroupExpression *node) = 0;

    virtual std::any visit(AssignExpression *node) = 0;

    virtual std::any visit(BinaryExpression *node) = 0;

    virtual std::any visit(ShiftExpression *node) = 0;

    virtual std::any visit(ComparisonExpression *node) = 0;

    virtual std::any visit(LogicalExpression *node) = 0;

    virtual std::any visit(PrefixUnaryExpression *node) = 0;

    virtual std::any visit(PostfixUnaryExpression *node) = 0;

    virtual std::any visit(CallExpression *node) = 0;

    virtual std::any visit(DotExpression *node) = 0;

    virtual std::any visit(CastExpression *node) = 0;

    virtual std::any visit(TypeSizeExpression *node) = 0;

    virtual std::any visit(ValueSizeExpression *node) = 0;

    virtual std::any visit(IndexExpression *node) = 0;

    virtual std::any visit(EnumAccessExpression *node) = 0;

    virtual std::any visit(LiteralExpression *node) = 0;

    virtual std::any visit(NumberExpression *node) = 0;

    virtual std::any visit(StringExpression *node) = 0;

    virtual std::any visit(ArrayExpression *node) = 0;

    virtual std::any visit(CharacterExpression *node) = 0;

    virtual std::any visit(BooleanExpression *node) = 0;

    virtual std::any visit(NullExpression *node) = 0;
};

class TreeVisitor : public StatementVisitor, public ExpressionVisitor {};