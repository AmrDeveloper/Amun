#pragma once

#include "jot_ast.hpp"
#include "jot_ast_visitor.hpp"
#include "jot_symboltable.hpp"
#include "jot_type.hpp"

#include <llvm/IR/Constant.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Type.h>

#include <any>
#include <map>
#include <memory>
#include <stack>
#include <vector>

class DeferCall {
  public:
    virtual void generate_call() = 0;
};

// LLVM Context, Builder and Current Module
static llvm::LLVMContext llvm_context;
static llvm::IRBuilder<> Builder(llvm_context);

// LLVM Integer types
static auto llvm_int1_type = llvm::Type::getInt1Ty(llvm_context);
static auto llvm_int8_type = llvm::Type::getInt8Ty(llvm_context);
static auto llvm_int16_type = llvm::Type::getInt16Ty(llvm_context);
static auto llvm_int32_type = llvm::Type::getInt32Ty(llvm_context);
static auto llvm_int64_type = llvm::Type::getInt64Ty(llvm_context);

static auto llvm_int64_ptr_type = llvm::Type::getInt64PtrTy(llvm_context);

// LLVM Floating pointer types
static auto llvm_float32_type = llvm::Type::getFloatTy(llvm_context);
static auto llvm_float64_type = llvm::Type::getDoubleTy(llvm_context);

// LLVM Void type
static auto llvm_void_type = llvm::Type::getVoidTy(llvm_context);

// LLVM 1 bit integer with zero value (false)
static auto false_value = Builder.getInt1(false);

// LLVM 32 bit integer with zero value
static auto zero_int32_value = Builder.getInt32(0);

class DeferFunctionCall : public DeferCall {
  public:
    DeferFunctionCall(llvm::Function *function, std::vector<llvm::Value *> arguments)
        : function(function), arguments(arguments) {}
    llvm::Function *function;
    std::vector<llvm::Value *> arguments;

    void generate_call() override { Builder.CreateCall(function, arguments); }
};

class DeferFunctionPtrCall : public DeferCall {
  public:
    DeferFunctionPtrCall(llvm::FunctionType *function_type, llvm::Value *callee,
                         std::vector<llvm::Value *> arguments)
        : function_type(function_type), callee(callee), arguments(arguments) {}
    llvm::FunctionType *function_type;
    llvm::Value *callee;
    std::vector<llvm::Value *> arguments;

    void generate_call() override { Builder.CreateCall(function_type, callee, arguments); }
};

class JotLLVMBackend : public TreeVisitor {
  public:
    JotLLVMBackend() {
        alloca_inst_global_scope = std::make_shared<JotSymbolTable>();
        alloca_inst_scope = alloca_inst_global_scope;
    }

    std::unique_ptr<llvm::Module> compile(std::string module_name,
                                          std::shared_ptr<CompilationUnit> compilation_unit);

    std::any visit(BlockStatement *node) override;

    std::any visit(FieldDeclaration *node) override;

    std::any visit(FunctionPrototype *node) override;

    std::any visit(FunctionDeclaration *node) override;

    std::any visit(EnumDeclaration *node) override;

    std::any visit(IfStatement *node) override;

    std::any visit(WhileStatement *node) override;

    std::any visit(SwitchStatement *node) override;

    std::any visit(ReturnStatement *node) override;

    std::any visit(DeferStatement *node) override;

    std::any visit(BreakStatement *node) override;

    std::any visit(ContinueStatement *node) override;

    std::any visit(ExpressionStatement *node) override;

    std::any visit(IfExpression *node) override;

    std::any visit(GroupExpression *node) override;

    std::any visit(AssignExpression *node) override;

    std::any visit(BinaryExpression *node) override;

    std::any visit(ShiftExpression *node) override;

    std::any visit(ComparisonExpression *node) override;

    std::any visit(LogicalExpression *node) override;

    std::any visit(PrefixUnaryExpression *node) override;

    std::any visit(PostfixUnaryExpression *node) override;

    std::any visit(CallExpression *node) override;

    std::any visit(CastExpression *node) override;

    std::any visit(IndexExpression *node) override;

    std::any visit(EnumAccessExpression *node) override;

    std::any visit(LiteralExpression *node) override;

    std::any visit(NumberExpression *node) override;

    std::any visit(ArrayExpression *node) override;

    std::any visit(StringExpression *node) override;

    std::any visit(CharacterExpression *node) override;

    std::any visit(BooleanExpression *node) override;

    std::any visit(NullExpression *node) override;

  private:
    llvm::Value *llvm_node_value(std::any any_value);

    llvm::Value *llvm_resolve_value(std::any any_value);

    llvm::Value *llvm_number_value(std::string value_litearl, NumberKind size);

    llvm::Value *llvm_boolean_value(bool value);

    llvm::Value *llvm_character_value(char character);

    llvm::Value *llvm_type_default_value(std::shared_ptr<JotType> type);

    llvm::Type *llvm_type_from_jot_type(std::shared_ptr<JotType> type);

    llvm::Value *create_llvm_integers_bianry(TokenKind op, llvm::Value *left, llvm::Value *right);

    llvm::Value *create_llvm_floats_bianry(TokenKind op, llvm::Value *left, llvm::Value *right);

    llvm::Value *create_llvm_integers_comparison(TokenKind op, llvm::Value *left,
                                                 llvm::Value *right);

    llvm::Value *create_llvm_floats_comparison(TokenKind op, llvm::Value *left, llvm::Value *right);

    llvm::Value *create_llvm_value_increment(std::shared_ptr<Expression> right, bool is_prefix);

    llvm::Value *create_llvm_value_decrement(std::shared_ptr<Expression> right, bool is_prefix);

    llvm::Constant *resolve_constant_expression(FieldDeclaration *expression);

    llvm::Constant *resolve_constant_index_expression(std::shared_ptr<IndexExpression> expression);

    llvm::Constant *resolve_constant_if_expression(std::shared_ptr<IfExpression> expression);

    llvm::AllocaInst *create_entry_block_alloca(llvm::Function *function,
                                                const std::string var_name, llvm::Type *type);

    void create_switch_case_branch(llvm::SwitchInst *switch_inst, llvm::Function *current_function,
                                   llvm::BasicBlock *basic_block,
                                   std::shared_ptr<SwitchCase> switch_case);

    llvm::Function *lookup_function(std::string name);

    void execute_defer_calls();

    void clear_defer_calls_stack();

    void push_alloca_inst_scope();

    void pop_alloca_inst_scope();

    std::unique_ptr<llvm::Module> llvm_module;

    std::map<std::string, std::shared_ptr<FunctionPrototype>> functions_table;
    std::map<std::string, llvm::Function *> llvm_functions;

    std::vector<std::shared_ptr<DeferCall>> defer_calls_stack;
    std::stack<llvm::BasicBlock *> break_blocks_stack;
    std::stack<llvm::BasicBlock *> continue_blocks_stack;

    std::shared_ptr<JotSymbolTable> alloca_inst_global_scope;
    std::shared_ptr<JotSymbolTable> alloca_inst_scope;

    bool has_return_statement = false;
    bool has_break_or_continue_statement = false;
};