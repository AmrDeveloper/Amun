#pragma once

#include <any>
#include <memory>

class BlockStatement;
class FieldDeclaration;
class ConstDeclaration;
class FunctionPrototype;
class IntrinsicPrototype;
class FunctionDeclaration;
class OperatorFunctionDeclaraion;
class StructDeclaration;
class EnumDeclaration;
class IfStatement;
class ForRangeStatement;
class ForEachStatement;
class ForeverStatement;
class WhileStatement;
class SwitchStatement;
class ReturnStatement;
class DeferStatement;
class BreakStatement;
class ContinueStatement;
class ExpressionStatement;

class StatementVisitor {
  public:
    virtual auto visit(BlockStatement* node) -> std::any = 0;

    virtual auto visit(FieldDeclaration* node) -> std::any = 0;

    virtual auto visit(ConstDeclaration* node) -> std::any = 0;

    virtual auto visit(FunctionPrototype* node) -> std::any = 0;

    virtual auto visit(OperatorFunctionDeclaraion* node) -> std::any = 0;

    virtual auto visit(IntrinsicPrototype* node) -> std::any = 0;

    virtual auto visit(FunctionDeclaration* node) -> std::any = 0;

    virtual auto visit(StructDeclaration* node) -> std::any = 0;

    virtual auto visit(EnumDeclaration* node) -> std::any = 0;

    virtual auto visit(IfStatement* node) -> std::any = 0;

    virtual auto visit(ForRangeStatement* node) -> std::any = 0;

    virtual auto visit(ForEachStatement* node) -> std::any = 0;

    virtual auto visit(ForeverStatement* node) -> std::any = 0;

    virtual auto visit(WhileStatement* node) -> std::any = 0;

    virtual auto visit(SwitchStatement* node) -> std::any = 0;

    virtual auto visit(ReturnStatement* node) -> std::any = 0;

    virtual auto visit(DeferStatement* node) -> std::any = 0;

    virtual auto visit(BreakStatement* node) -> std::any = 0;

    virtual auto visit(ContinueStatement* node) -> std::any = 0;

    virtual auto visit(ExpressionStatement* node) -> std::any = 0;
};

class IfExpression;
class SwitchExpression;
class GroupExpression;
class TupleExpression;
class AssignExpression;
class BinaryExpression;
class BitwiseExpression;
class ComparisonExpression;
class LogicalExpression;
class PrefixUnaryExpression;
class PostfixUnaryExpression;
class CallExpression;
class InitializeExpression;
class LambdaExpression;
class DotExpression;
class CastExpression;
class TypeSizeExpression;
class TypeAlignExpression;
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
class UndefinedExpression;

class ExpressionVisitor {
  public:
    virtual auto visit(IfExpression* node) -> std::any = 0;

    virtual auto visit(SwitchExpression* node) -> std::any = 0;

    virtual auto visit(GroupExpression* node) -> std::any = 0;

    virtual auto visit(TupleExpression* node) -> std::any = 0;

    virtual auto visit(AssignExpression* node) -> std::any = 0;

    virtual auto visit(BinaryExpression* node) -> std::any = 0;

    virtual auto visit(BitwiseExpression* node) -> std::any = 0;

    virtual auto visit(ComparisonExpression* node) -> std::any = 0;

    virtual auto visit(LogicalExpression* node) -> std::any = 0;

    virtual auto visit(PrefixUnaryExpression* node) -> std::any = 0;

    virtual auto visit(PostfixUnaryExpression* node) -> std::any = 0;

    virtual auto visit(CallExpression* node) -> std::any = 0;

    virtual auto visit(InitializeExpression* node) -> std::any = 0;

    virtual auto visit(LambdaExpression* node) -> std::any = 0;

    virtual auto visit(DotExpression* node) -> std::any = 0;

    virtual auto visit(CastExpression* node) -> std::any = 0;

    virtual auto visit(TypeSizeExpression* node) -> std::any = 0;

    virtual auto visit(TypeAlignExpression* node) -> std::any = 0;

    virtual auto visit(ValueSizeExpression* node) -> std::any = 0;

    virtual auto visit(IndexExpression* node) -> std::any = 0;

    virtual auto visit(EnumAccessExpression* node) -> std::any = 0;

    virtual auto visit(LiteralExpression* node) -> std::any = 0;

    virtual auto visit(NumberExpression* node) -> std::any = 0;

    virtual auto visit(StringExpression* node) -> std::any = 0;

    virtual auto visit(ArrayExpression* node) -> std::any = 0;

    virtual auto visit(CharacterExpression* node) -> std::any = 0;

    virtual auto visit(BooleanExpression* node) -> std::any = 0;

    virtual auto visit(NullExpression* node) -> std::any = 0;

    virtual auto visit(UndefinedExpression* node) -> std::any = 0;
};

class TreeVisitor : public StatementVisitor, public ExpressionVisitor {};